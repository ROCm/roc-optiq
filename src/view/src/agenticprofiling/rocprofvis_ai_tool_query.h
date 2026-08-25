// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"
#include "model/rocprofvis_tables_model.h"
#include "rocprofvis_controller_enums.h"

#include <cstdint>
#include <string>

namespace RocProfVis
{
namespace View
{

/**
 * @brief Turns the query-shaped arguments of a tool call into the SQL fragments
 * the data provider takes.
 *
 * These arguments come from the model, so everything here treats them as
 * hostile input. Column names and operators are checked against fixed
 * whitelists rather than escaped, string values are quoted, and LIKE patterns
 * have their wildcards escaped so a value cannot silently widen the match. The
 * result is appended to a query the caller built, so no function here emits the
 * WHERE keyword itself.
 *
 * This lives apart from the tool bodies because it is the one place a bad
 * argument could turn into bad SQL, and because it can be exercised on its own.
 */

// Longest filter list a single call may carry.
constexpr size_t ASSISTANT_MAX_FILTERS = 8;

// Builds the boolean fragment for a tool's "filters" argument, or an empty
// string when there are none. Writes to error_out and returns empty when an
// argument is malformed; callers must check error_out rather than the return
// value, since no filters is also a valid empty result.
std::string BuildAssistantWhereClause(const jt::Json& args, std::string& error_out);

// Reads the "group_by" argument, checked against the same column whitelist as
// the filters. Empty when absent. Leaves error_out alone unless the column is
// not one we allow, and does nothing when error_out is already set.
std::string AssistantGroupByFromArgs(const jt::Json& args, std::string& error_out);

// Turns a "sort_by" column name into its index in the table's own header,
// falling back when the name is absent or does not name a column of this table.
uint64_t ResolveAssistantSortColumn(const TablesModel& tables, TableType type,
                                    const std::string& name, uint64_t fallback);

// Reads "sort_order", keeping the calling tool's own default when absent.
rocprofvis_controller_sort_order_t AssistantSortOrderFromArgs(
    const jt::Json& args, rocprofvis_controller_sort_order_t fallback);

// The columns a tool argument may name, as a comma-separated list. Used to tell
// the model what it should have used when it names something else.
std::string AssistantAllowedQueryColumnList();

}  // namespace View
}  // namespace RocProfVis
