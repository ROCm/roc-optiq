// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_tool_query.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "rocprofvis_core_string_utils.h"
#include "rocprofvis_json_utils.h"

namespace RocProfVis
{
namespace View
{

namespace
{

// SQLite has no default LIKE escape character, so one is named explicitly and
// the pattern is escaped to match. See EscapeLikeWildcards.
constexpr char ASSISTANT_LIKE_ESCAPE_CHAR = '\\';

// Columns a tool argument may filter, group, and sort on. Filters reach the
// database as a SQL fragment, so only names on this list are accepted and the
// values beside them are quoted by QuoteSqlLiteral. Adding a column here widens
// what the model can query, so treat it as an interface change.
const char* const ASSISTANT_QUERY_COLUMNS[] = {
    "name",       "category",  "duration",   "start",      "end",
    "id",         "__uuid",    "PID",        "TID",        "queue",
    "stream",     "node",      "nodeId",     "size",       "address",
    "SrcAddr",    "value",     "counter",    "arguments",  "GridSizeX",
    "GridSizeY",  "GridSizeZ", "WGSizeX",    "WGSizeY",    "WGSizeZ",
    "LDSSize",    "ScratchSize", "StaticLDSSize", "StaticScratchSize",
    "AgentAbsoluteIndex", "AgentType", "AgentTypeIndex", "AgentName",
    "SrcAgentAbsoluteIndex", "SrcAgentType", "SrcAgentTypeIndex",
    "SrcAgentName", "__trackId", "__streamTrackId",
};

enum class QueryWildcard
{
    kNone,
    kContains,
    kPrefix
};

struct QueryOperator
{
    const char*   key;
    const char*   sql;
    QueryWildcard wildcard;
};

const QueryOperator ASSISTANT_QUERY_OPERATORS[] = {
    { "=", "=", QueryWildcard::kNone },
    { "==", "=", QueryWildcard::kNone },
    { "!=", "!=", QueryWildcard::kNone },
    { "<>", "!=", QueryWildcard::kNone },
    { "<", "<", QueryWildcard::kNone },
    { "<=", "<=", QueryWildcard::kNone },
    { ">", ">", QueryWildcard::kNone },
    { ">=", ">=", QueryWildcard::kNone },
    { "contains", "LIKE", QueryWildcard::kContains },
    { "starts_with", "LIKE", QueryWildcard::kPrefix },
};

// Reports whether a column name is one a tool argument is allowed to name.
bool
IsAllowedQueryColumn(const std::string& name)
{
    for(const char* column : ASSISTANT_QUERY_COLUMNS)
    {
        if(name == column)
        {
            return true;
        }
    }
    return false;
}

// Looks up a filter operator, so only known spellings reach the query.
const QueryOperator*
FindQueryOperator(const std::string& key)
{
    for(const QueryOperator& op : ASSISTANT_QUERY_OPERATORS)
    {
        if(key == op.key)
        {
            return &op;
        }
    }
    return nullptr;
}

// Renders a string as a SQL literal: single quotes doubled, control characters
// dropped, so a value cannot terminate the literal and inject syntax.
std::string
QuoteSqlLiteral(const std::string& value)
{
    std::string quoted = "'";
    for(char c : value)
    {
        if(static_cast<unsigned char>(c) < 0x20)
        {
            continue;
        }
        if(c == '\'')
        {
            quoted += '\'';
        }
        quoted += c;
    }
    quoted += '\'';
    return quoted;
}

// Escapes the two characters LIKE treats as wildcards, plus the escape
// character itself. Without this a value containing % or _ - which real kernel
// names do - would silently widen the match instead of being looked for
// literally. Pairs with the ESCAPE clause BuildAssistantWhereClause appends.
std::string
EscapeLikeWildcards(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for(char c : value)
    {
        if(c == ASSISTANT_LIKE_ESCAPE_CHAR || c == '%' || c == '_')
        {
            escaped += ASSISTANT_LIKE_ESCAPE_CHAR;
        }
        escaped += c;
    }
    return escaped;
}

// Prints a double at full precision so a filter value survives the trip.
std::string
FormatNumberLiteral(double value)
{
    std::ostringstream out;
    out << std::setprecision(17) << value;
    return out.str();
}

// Renders one filter's value as a SQL literal. Sets pattern_out when the value
// became a LIKE pattern, which is what tells the caller to add ESCAPE.
bool
FilterLiteral(jt::Json& value, QueryWildcard wildcard, const std::string& column,
              std::string& literal_out, bool& pattern_out, std::string& error_out)
{
    pattern_out = false;
    if(value.isString())
    {
        std::string text = value.getString();
        if(wildcard != QueryWildcard::kNone)
        {
            // Escape first, then add our own wildcards, so only the ones we add
            // are treated as wildcards.
            text        = EscapeLikeWildcards(text);
            pattern_out = true;
        }
        if(wildcard == QueryWildcard::kContains)
        {
            text = "%" + text + "%";
        }
        else if(wildcard == QueryWildcard::kPrefix)
        {
            text += "%";
        }
        literal_out = QuoteSqlLiteral(text);
        return true;
    }
    if(value.isLong())
    {
        literal_out = std::to_string(value.getLong());
        return true;
    }
    if(value.isDouble())
    {
        literal_out = FormatNumberLiteral(value.getDouble());
        return true;
    }
    error_out = "Filter on \"" + column + "\" needs a string or number value.";
    return false;
}

}  // namespace

std::string
AssistantAllowedQueryColumnList()
{
    std::ostringstream out;
    bool               first = true;
    for(const char* column : ASSISTANT_QUERY_COLUMNS)
    {
        if(!first)
        {
            out << ", ";
        }
        first = false;
        out << column;
    }
    return out.str();
}

std::string
BuildAssistantWhereClause(const jt::Json& args, std::string& error_out)
{
    jt::Json& mutable_args = const_cast<jt::Json&>(args);
    if(!mutable_args.contains("filters"))
    {
        return std::string();
    }
    // Present but the wrong shape is a mistake, not "no filters". Ignoring it
    // would run the unfiltered query and report the answer as though it had
    // been filtered - the same trap the per-entry check below exists for.
    if(!mutable_args["filters"].isArray())
    {
        error_out = "filters must be an array of {column, op, value} objects.";
        return std::string();
    }

    std::vector<jt::Json>& entries = mutable_args["filters"].getArray();
    if(entries.size() > ASSISTANT_MAX_FILTERS)
    {
        error_out = "Too many filters. Use at most " +
                    std::to_string(ASSISTANT_MAX_FILTERS) + ".";
        return std::string();
    }

    std::ostringstream out;
    size_t             used = 0;
    for(jt::Json& entry : entries)
    {
        // Skipping a malformed entry quietly would run a broader query than the
        // model asked for and report it as the one it wanted, so say so instead.
        if(!entry.isObject())
        {
            error_out = "Every filter must be an object with column, op, and value.";
            return std::string();
        }

        const std::string column = JsonUtils::GetString(entry, "column", "");
        if(!IsAllowedQueryColumn(column))
        {
            error_out = "Unknown filter column \"" + column +
                        "\". Allowed columns: " + AssistantAllowedQueryColumnList();
            return std::string();
        }

        const QueryOperator* op =
            FindQueryOperator(Core::String::to_lower_copy(JsonUtils::GetString(entry, "op", "=")));
        if(op == nullptr)
        {
            error_out = "Unknown filter op. Use =, !=, <, <=, >, >=, contains, "
                        "or starts_with.";
            return std::string();
        }

        if(!entry.contains("value"))
        {
            error_out = "Filter on \"" + column + "\" is missing value.";
            return std::string();
        }

        std::string literal;
        bool        pattern_match = false;
        if(!FilterLiteral(entry["value"], op->wildcard, column, literal, pattern_match,
                          error_out))
        {
            return std::string();
        }

        if(used > 0)
        {
            out << " AND ";
        }
        out << column << " " << op->sql << " " << literal;
        if(pattern_match)
        {
            out << " ESCAPE '" << ASSISTANT_LIKE_ESCAPE_CHAR << "'";
        }
        ++used;
    }
    return out.str();
}

std::string
AssistantGroupByFromArgs(const jt::Json& args, std::string& error_out)
{
    const std::string group = JsonUtils::GetString(args, "group_by", "");
    if(group.empty() || !error_out.empty())
    {
        return std::string();
    }
    if(!IsAllowedQueryColumn(group))
    {
        error_out = "Unknown group_by column \"" + group +
                    "\". Allowed columns: " + AssistantAllowedQueryColumnList();
        return std::string();
    }
    return group;
}

uint64_t
ResolveAssistantSortColumn(const TablesModel& tables, TableType type,
                           const std::string& name, uint64_t fallback)
{
    if(name.empty())
    {
        return fallback;
    }
    const std::vector<std::string>& header  = tables.GetTableHeader(type);
    const std::string               lowered = Core::String::to_lower_copy(name);
    for(size_t i = 0; i < header.size(); ++i)
    {
        if(Core::String::to_lower_copy(header[i]) == lowered)
        {
            return static_cast<uint64_t>(i);
        }
    }
    return fallback;
}

uint64_t
ResolveAssistantSortColumnNamed(const TablesModel& tables, TableType type,
                                const std::string& name,
                                const std::string& fallback_name,
                                uint64_t           fallback_index)
{
    // Ask for the column by name first. An index is only right for the column
    // order the table happens to have today, and a wrong one sorts by something
    // else without saying so. The index stays as a last resort for the first
    // query of a session, when no header has been read yet.
    const uint64_t resolved = ResolveAssistantSortColumn(tables, type, name,
                                                         ASSISTANT_SORT_COLUMN_UNKNOWN);
    if(resolved != ASSISTANT_SORT_COLUMN_UNKNOWN)
    {
        return resolved;
    }
    const uint64_t by_name = ResolveAssistantSortColumn(tables, type, fallback_name,
                                                        ASSISTANT_SORT_COLUMN_UNKNOWN);
    return by_name == ASSISTANT_SORT_COLUMN_UNKNOWN ? fallback_index : by_name;
}

rocprofvis_controller_sort_order_t
AssistantSortOrderFromArgs(const jt::Json&                    args,
                           rocprofvis_controller_sort_order_t fallback)
{
    const std::string order = Core::String::to_lower_copy(JsonUtils::GetString(args, "sort_order", ""));
    if(order == "asc" || order == "ascending")
    {
        return kRPVControllerSortOrderAscending;
    }
    if(order == "desc" || order == "descending")
    {
        return kRPVControllerSortOrderDescending;
    }
    return fallback;
}

}  // namespace View
}  // namespace RocProfVis
