// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_markdown.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <unordered_map>
#include <vector>

#include "imgui_internal.h"

namespace RocProfVis
{
namespace View
{

namespace
{

// Lengths in units of the text size, so the layout scales with the font.
constexpr float LIST_INDENT         = 1.6f;
constexpr float QUOTE_INDENT        = 1.0f;
constexpr float QUOTE_BAR_WIDTH     = 0.2f;
constexpr float BLOCK_GAP           = 0.6f;
constexpr float LIST_GAP            = 0.2f;
constexpr float HEADING_GAP         = 0.9f;
constexpr float RULE_HEIGHT         = 1.0f;
constexpr float CODE_PADDING        = 0.5f;
constexpr float INLINE_CODE_PADDING = 0.15f;
constexpr float CELL_PADDING        = 0.4f;
constexpr float MIN_COLUMN_WIDTH    = 3.0f;
constexpr float MARKER_GAP          = 0.4f;
constexpr float BULLET_RADIUS       = 0.17f;
constexpr float TASK_BOX_SIZE       = 0.7f;
// Fractions of a font's ascent, down from the top of the line.
constexpr float STRIKE_POSITION = 0.65f;
// Horizontal slant per pixel of height for faux italic.
constexpr float ITALIC_SHEAR = 0.2f;
// Pixels.
constexpr float LINE_THICKNESS   = 1.0f;
constexpr float FAUX_BOLD_OFFSET = 1.0f;
constexpr float BOX_ROUNDING     = 2.0f;
// The check mark in a done task box, as fractions of the box.
constexpr ImVec2 CHECK_MARK[] = { ImVec2(0.2f, 0.52f), ImVec2(0.42f, 0.74f),
                                  ImVec2(0.8f, 0.28f) };

constexpr size_t  MAX_HEADING_LEVEL  = 6;
constexpr size_t  MIN_FENCE_LENGTH   = 3;
constexpr size_t  MIN_RULE_LENGTH    = 3;
constexpr size_t  MAX_ORDERED_DIGITS = 9;
constexpr size_t  MAX_LIST_SPACES    = 4;
constexpr size_t  MAX_EMPHASIS_RUN   = 3;
constexpr size_t  TASK_MARKER_LENGTH = 3;
constexpr uint8_t MAX_NESTING        = 16;
// Bounds the recursion of inline matching on hostile input.
constexpr int32_t       MAX_INLINE_DEPTH = 16;
constexpr unsigned char FIRST_NON_ASCII  = 0x80;
constexpr int32_t       DECIMAL_BASE     = 10;
// A tab advances to the next multiple of this many columns.
constexpr int32_t TAB_COLUMNS             = 4;
constexpr int32_t INDENTED_CODE_COLUMNS   = 4;
constexpr int32_t QUOTE_MARKER_MAX_INDENT = 3;
// ImGui reports 1 for a click, 2 for a double-click, 3 for a triple-click.
constexpr int32_t CLICKS_PER_WORD = 2;
constexpr int32_t CLICKS_PER_LINE = 3;
constexpr float   HALF            = 0.5f;
constexpr float   TWICE           = 2.0f;
// Offsets into the drawn text are uint32_t.
constexpr size_t MAX_MARKDOWN_BYTES = 0xffffffffu;
// Past this many steps the parse stops and the view draws the source as plain
// text, so a hostile reply cannot stall the UI thread.
constexpr size_t MIN_PARSE_STEPS      = 100000;
constexpr size_t PARSE_STEPS_PER_BYTE = 64;

constexpr size_t      NPOS        = std::string_view::npos;
constexpr const char* BULLET_TEXT = "\xE2\x80\xA2 ";  // U+2022 and a space

static_assert(sizeof(markdown_style_t::heading_scale) /
                      sizeof(markdown_style_t::heading_scale[0]) ==
                  MAX_HEADING_LEVEL,
              "one scale per heading level");

// ---------------------------------------------------------------- document

enum SpanFlags : uint32_t
{
    kBold   = 1u << 0,
    kItalic = 1u << 1,
    kStrike = 1u << 2,
    kCode   = 1u << 3,
    // Set by layout when no face of its own draws the style.
    kFauxBold   = 1u << 4,
    kFauxItalic = 1u << 5,
};

// A run of one style, as a range of document_t::text.
struct span_t
{
    uint32_t begin = 0;
    uint32_t end   = 0;
    uint32_t flags = 0;
    int32_t  link  = -1;
};

enum class BlockKind : uint8_t
{
    kParagraph,
    kHeading,
    kCode,
    kRule,
    kTable
};

enum class Alignment : uint8_t
{
    kLeft,
    kCenter,
    kRight
};

enum class Marker : uint8_t
{
    kNone,
    kBullet,
    kNumber,
    kTask,
    kTaskDone
};

struct cell_t
{
    std::vector<span_t> spans;
};

struct block_t
{
    BlockKind kind   = BlockKind::kParagraph;
    uint8_t   level  = 0;  // heading level, 1 to 6
    uint8_t   quote  = 0;  // block quote depth
    uint8_t   list   = 0;  // list nesting depth
    Marker    marker = Marker::kNone;
    // The marker as it copies: "• ", "3. ", "[x] ".
    span_t marker_span;
    // A list block straight after another, with no blank line between.
    bool                tight = false;
    std::vector<span_t> spans;
    // Tables: the header row first, then one alignment per column.
    std::vector<std::vector<cell_t>> rows;
    std::vector<Alignment>           align;
};

struct document_t
{
    // What selection copies: the text as drawn, blocks on their own lines and
    // table cells split by tabs.
    std::string              text;
    std::vector<block_t>     blocks;
    std::vector<std::string> links;
};

// ----------------------------------------------------------------- helpers

bool
is_blank(char c)
{
    return c == ' ' || c == '\t';
}

// The ends of the text count as whitespace.
bool
is_space(char c)
{
    return c == '\0' || c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool
is_punct(char c)
{
    return std::ispunct(static_cast<unsigned char>(c)) != 0;
}

bool
is_word_char(char c)
{
    const unsigned char byte = static_cast<unsigned char>(c);
    return std::isalnum(byte) != 0 || c == '_' || byte >= FIRST_NON_ASCII;
}

bool
starts_with(std::string_view text, std::string_view prefix)
{
    return text.substr(0, prefix.size()) == prefix;
}

size_t
run_length(std::string_view text, size_t at)
{
    size_t length = 0;
    while(at + length < text.size() && text[at + length] == text[at])
    {
        ++length;
    }
    return length;
}

// Bytes of leading spaces and tabs, and the column they reach. A tab advances
// to the next multiple of TAB_COLUMNS, so a list nested with tabs keeps its
// depth.
struct column_indent_t
{
    size_t  bytes   = 0;
    int32_t columns = 0;
};

column_indent_t
measure_indent(std::string_view text)
{
    column_indent_t out;
    while(out.bytes < text.size())
    {
        const char c = text[out.bytes];
        if(c == ' ')
        {
            ++out.columns;
            ++out.bytes;
        }
        else if(c == '\t')
        {
            const int32_t stop = TAB_COLUMNS - (out.columns % TAB_COLUMNS);
            out.columns += stop;
            ++out.bytes;
        }
        else
        {
            break;
        }
    }
    return out;
}

std::string_view
strip_columns(std::string_view text, int32_t columns)
{
    int32_t got   = 0;
    size_t  bytes = 0;
    while(bytes < text.size() && got < columns)
    {
        if(text[bytes] == ' ')
        {
            ++got;
            ++bytes;
        }
        else if(text[bytes] == '\t')
        {
            const int32_t stop = TAB_COLUMNS - (got % TAB_COLUMNS);
            got += stop;
            ++bytes;
        }
        else
        {
            break;
        }
    }
    return text.substr(bytes);
}

std::string_view
trim(std::string_view text)
{
    const size_t first = std::min(text.find_first_not_of(" \t"), text.size());
    const size_t last  = text.find_last_not_of(" \t");
    return last == NPOS ? std::string_view() : text.substr(first, last + 1 - first);
}

ImVec2
offset_pos(const ImVec2& origin, float x, float y)
{
    return ImVec2(origin.x + x, origin.y + y);
}

// Strips up to max_depth block quote markers and reports how many there were.
std::string_view
strip_quotes(std::string_view line, uint8_t max_depth, uint8_t& depth)
{
    depth = 0;
    while(depth < max_depth)
    {
        const column_indent_t indent = measure_indent(line);
        if(indent.columns > QUOTE_MARKER_MAX_INDENT || indent.bytes >= line.size() ||
           line[indent.bytes] != '>')
        {
            break;
        }
        ++depth;
        line.remove_prefix(indent.bytes + 1);
        if(!line.empty() && (line[0] == ' ' || line[0] == '\t'))
        {
            line.remove_prefix(1);
        }
    }
    return line;
}

// Three or more of one of - * _, with nothing else but spaces.
bool
is_rule(std::string_view text)
{
    const std::string_view trimmed   = trim(text);
    const char             marker    = trimmed.empty() ? '\0' : trimmed[0];
    const char             allowed[] = { marker, ' ', '\0' };
    return (marker == '-' || marker == '*' || marker == '_') &&
           trimmed.find_first_not_of(allowed) == NPOS &&
           static_cast<size_t>(std::count(trimmed.begin(), trimmed.end(), marker)) >=
               MIN_RULE_LENGTH;
}

// The heading level an underline of = or - gives the paragraph above, or 0.
size_t
setext_level(std::string_view text)
{
    const std::string_view trimmed = trim(text);
    const char             marker  = trimmed.empty() ? '\0' : trimmed[0];
    size_t                 level   = 0;
    if((marker == '=' || marker == '-') && trimmed.find_first_not_of(marker) == NPOS)
    {
        level = marker == '=' ? 1 : 2;
    }
    return level;
}

// The length of a list marker and the space after it, or 0.
size_t
list_marker(std::string_view text, bool& ordered, int32_t& number)
{
    size_t digits = 0;
    number        = 0;
    while(digits < text.size() && digits <= MAX_ORDERED_DIGITS &&
          std::isdigit(static_cast<unsigned char>(text[digits])) != 0)
    {
        number = number * DECIMAL_BASE + static_cast<int32_t>(text[digits] - '0');
        ++digits;
    }
    ordered = digits > 0 && digits <= MAX_ORDERED_DIGITS && digits < text.size() &&
              (text[digits] == '.' || text[digits] == ')');
    const bool bullet =
        !text.empty() && (text[0] == '-' || text[0] == '*' || text[0] == '+');
    size_t marker = 0;
    if(ordered)
    {
        marker = digits + 1;
    }
    else if(bullet)
    {
        marker = 1;
    }
    const bool spaced = marker > 0 && (marker == text.size() || is_blank(text[marker]));
    return spaced && !is_rule(text) ? marker : 0;
}

std::string_view
heading_text(std::string_view text)
{
    text = trim(text);
    // A closing run of # counts only after a space.
    const size_t last = text.find_last_not_of('#');
    if(last == NPOS)
    {
        text = std::string_view();
    }
    else if(last + 1 < text.size() && is_blank(text[last]))
    {
        text = trim(text.substr(0, last));
    }
    return text;
}

void
split_row(std::string_view row, std::vector<std::string_view>& cells)
{
    cells.clear();
    row = trim(row);
    if(!row.empty() && row.front() == '|')
    {
        row.remove_prefix(1);
    }
    if(!row.empty() && row.back() == '|' &&
       (row.size() < 2 || row[row.size() - 2] != '\\'))
    {
        row.remove_suffix(1);
    }
    size_t start = 0;
    for(size_t at = 0; at <= row.size(); ++at)
    {
        if(at == row.size() || (row[at] == '|' && (at == 0 || row[at - 1] != '\\')))
        {
            cells.push_back(trim(row.substr(start, at - start)));
            start = at + 1;
        }
    }
}

// A table's delimiter row, such as |---|:-:|--:|, and its alignments.
bool
delimiter_row(std::string_view row, std::vector<Alignment>& align,
              std::vector<std::string_view>& cells)
{
    align.clear();
    split_row(row, cells);
    bool valid = row.find_first_of("|-") != NPOS;
    for(std::string_view cell : cells)
    {
        const bool             left  = !cell.empty() && cell.front() == ':';
        const bool             right = cell.size() > 1 && cell.back() == ':';
        const std::string_view dashes =
            cell.substr(left ? 1 : 0, cell.size() - (left ? 1 : 0) - (right ? 1 : 0));
        valid = valid && !dashes.empty() && dashes.find_first_not_of('-') == NPOS;
        if(left && right)
        {
            align.push_back(Alignment::kCenter);
        }
        else if(right)
        {
            align.push_back(Alignment::kRight);
        }
        else
        {
            align.push_back(Alignment::kLeft);
        }
    }
    return valid;
}

struct flanking_t
{
    bool open  = false;
    bool close = false;
};

// CommonMark's rule for a run of * _ or ~: it opens emphasis when the text it
// wraps starts right after it, and closes when that text ends right before
// it, which keeps `a * b` and `float*, int*` as written. An underscore also
// refuses to open or close inside a word, so snake_case stays whole.
flanking_t
flank(std::string_view text, size_t at, size_t length)
{
    const char before = at > 0 ? text[at - 1] : '\0';
    const char after  = at + length < text.size() ? text[at + length] : '\0';
    const bool left =
        !is_space(after) && (!is_punct(after) || is_space(before) || is_punct(before));
    const bool right =
        !is_space(before) && (!is_punct(before) || is_space(after) || is_punct(after));
    flanking_t result;
    result.open  = left;
    result.close = right;
    if(text[at] == '_')
    {
        result.open  = left && (!right || is_punct(before));
        result.close = right && (!left || is_punct(after));
    }
    return result;
}

// Scratch for one parse. The closer map records runs already searched, so a
// string of unmatched markers is linear instead of exponential. Tick runs are
// indexed once per inline string.
struct parse_state_t
{
    size_t                                          steps  = 0;
    size_t                                          limit  = 0;
    bool                                            failed = false;
    std::unordered_map<uint64_t, size_t>            closers;
    std::unordered_map<size_t, std::vector<size_t>> tick_runs;
    const char*                                     tick_data = nullptr;
    size_t                                          tick_size = 0;
};

bool
charge(parse_state_t& state)
{
    ++state.steps;
    if(state.steps > state.limit)
    {
        state.failed = true;
        return false;
    }
    return true;
}

void
index_ticks(parse_state_t& state, std::string_view text)
{
    if(state.tick_data == text.data() && state.tick_size == text.size())
    {
        return;
    }
    state.tick_runs.clear();
    state.tick_data = text.data();
    state.tick_size = text.size();
    size_t at       = 0;
    while(at < text.size() && !state.failed)
    {
        if(!charge(state))
        {
            return;
        }
        if(text[at] != '`')
        {
            ++at;
            continue;
        }
        size_t length = 0;
        while(at + length < text.size() && text[at + length] == '`')
        {
            ++length;
        }
        state.tick_runs[length].push_back(at);
        at += length;
    }
}

// Where a run of exactly `ticks` backticks starts, at or after `from`.
// The search starts after an opening run, so a longer run is not matched by
// a suffix of itself.
size_t
find_ticks(parse_state_t& state, std::string_view text, size_t from, size_t ticks)
{
    if(state.failed)
    {
        return NPOS;
    }
    index_ticks(state, text);
    const std::unordered_map<size_t, std::vector<size_t>>::const_iterator found =
        state.tick_runs.find(ticks);
    if(found == state.tick_runs.end())
    {
        return NPOS;
    }
    const std::vector<size_t>::const_iterator at =
        std::lower_bound(found->second.begin(), found->second.end(), from);
    return at == found->second.end() ? NPOS : *at;
}

// Past the code span starting at `at`, or past its backticks if it never
// closes.
size_t
skip_code_span(parse_state_t& state, std::string_view text, size_t at)
{
    const size_t ticks = run_length(text, at);
    const size_t close = find_ticks(state, text, at + ticks, ticks);
    return close == NPOS ? at + ticks : close + ticks;
}

size_t
emphasis_width(char marker, size_t run)
{
    return marker == '~' ? run : std::min(run, MAX_EMPHASIS_RUN);
}

uint64_t
closer_key(size_t from, char marker, size_t width, int32_t depth)
{
    const uint64_t which = marker == '*' ? 1u : (marker == '_' ? 2u : 3u);
    return (static_cast<uint64_t>(from) << 16) | (which << 8) |
           (static_cast<uint64_t>(width) << 4) | static_cast<uint32_t>(depth);
}

// Where the run closing `width` markers starts, scanning from `from`. Pairs
// nested inside are skipped whole, so the nearest opener gets the nearest
// closer. A failed search is remembered, so unmatched openers are not
// explored again.
size_t
find_closer(parse_state_t& state, std::string_view text, size_t from, char marker,
            size_t width, int32_t depth)
{
    if(state.failed)
    {
        return NPOS;
    }
    const uint64_t key = closer_key(from, marker, width, depth);
    const std::unordered_map<uint64_t, size_t>::const_iterator known =
        state.closers.find(key);
    if(known != state.closers.end())
    {
        return known->second;
    }

    size_t at    = from;
    size_t close = NPOS;
    while(close == NPOS && at < text.size())
    {
        if(!charge(state))
        {
            return NPOS;
        }
        if(text[at] == '\\')
        {
            at += 2;
        }
        else if(text[at] == '`')
        {
            at = skip_code_span(state, text, at);
        }
        else if(text[at] != marker)
        {
            ++at;
        }
        else
        {
            const size_t     run  = run_length(text, at);
            const flanking_t side = flank(text, at, run);
            size_t           next = at + run;
            if(side.close && run >= width && (marker != '~' || run == width))
            {
                close = at;
            }
            else if(side.open && depth < MAX_INLINE_DEPTH)
            {
                const size_t nested_width = emphasis_width(marker, run);
                const size_t nested =
                    find_closer(state, text, at + run, marker, nested_width, depth + 1);
                if(state.failed)
                {
                    return NPOS;
                }
                if(nested != NPOS)
                {
                    next = nested + nested_width;
                }
            }
            at = next;
        }
    }
    state.closers.emplace(key, close);
    return close;
}

// The ']' closing the '[' at `open`, past nested brackets, escapes, and code.
size_t
find_bracket(parse_state_t& state, std::string_view text, size_t open)
{
    int32_t level = 0;
    size_t  at    = open;
    size_t  close = NPOS;
    while(close == NPOS && at < text.size())
    {
        if(!charge(state))
        {
            return NPOS;
        }
        if(text[at] == '\\')
        {
            at += 2;
        }
        else if(text[at] == '`')
        {
            at = skip_code_span(state, text, at);
        }
        else
        {
            if(text[at] == '[')
            {
                ++level;
            }
            else if(text[at] == ']')
            {
                --level;
                if(level == 0)
                {
                    close = at;
                }
            }
            ++at;
        }
    }
    return close;
}

struct destination_t
{
    size_t begin = 0;
    size_t end   = 0;
    // Just past the closing parenthesis.
    size_t after = 0;
};

size_t
skip_link_space(std::string_view text, size_t at)
{
    return std::min(text.find_first_not_of(" \t\n", at), text.size());
}

// The (destination "title") after a link label, starting at `at`.
bool
parse_destination(std::string_view text, size_t at, destination_t& out)
{
    bool   valid = at < text.size() && text[at] == '(';
    size_t i     = valid ? skip_link_space(text, at + 1) : text.size();
    if(valid && i < text.size() && text[i] == '<')
    {
        const size_t close = text.find('>', i + 1);
        valid              = close != NPOS;
        out.begin          = i + 1;
        out.end            = valid ? close : i + 1;
        i                  = valid ? close + 1 : text.size();
    }
    else if(valid)
    {
        out.begin      = i;
        int32_t parens = 0;
        while(i < text.size() && !is_space(text[i]) && (text[i] != ')' || parens > 0))
        {
            if(text[i] == '(')
            {
                ++parens;
            }
            else if(text[i] == ')')
            {
                --parens;
            }
            i += text[i] == '\\' ? 2 : 1;
        }
        i       = std::min(i, text.size());
        out.end = i;
    }
    i = skip_link_space(text, i);
    if(valid && i < text.size() && (text[i] == '"' || text[i] == '\''))
    {
        const size_t close = text.find(text[i], i + 1);
        valid              = close != NPOS;
        i                  = valid ? skip_link_space(text, close + 1) : text.size();
    }
    valid     = valid && i < text.size() && text[i] == ')';
    out.after = i + 1;
    return valid;
}

// The length of a bare http://, https://, or www. URL at `at`, or 0. Trailing
// punctuation and an unmatched ')' belong to the sentence around it.
size_t
bare_url_length(std::string_view text, size_t at)
{
    const std::string_view rest = text.substr(at);
    const bool starts = starts_with(rest, "https://") || starts_with(rest, "http://") ||
                        starts_with(rest, "www.");
    const bool bounded = at == 0 || is_space(text[at - 1]) || text[at - 1] == '(' ||
                         text[at - 1] == '*' || text[at - 1] == '_';
    size_t end = 0;
    if(starts && bounded)
    {
        while(end < rest.size() && !is_space(rest[end]) && rest[end] != '<')
        {
            ++end;
        }
        size_t opened =
            static_cast<size_t>(std::count(rest.begin(), rest.begin() + end, '('));
        size_t closed =
            static_cast<size_t>(std::count(rest.begin(), rest.begin() + end, ')'));
        while(end > 0 && (std::strchr("?!.,:;*_~'\"", rest[end - 1]) != nullptr ||
                          (rest[end - 1] == ')' && closed > opened)))
        {
            if(rest[end - 1] == ')')
            {
                --closed;
            }
            --end;
        }
        end = rest.find('.') < end ? end : 0;
    }
    return end;
}

bool
is_autolink(std::string_view inner)
{
    const bool spaced = inner.find_first_of(" \t\n<") != NPOS;
    const bool scheme = starts_with(inner, "http://") || starts_with(inner, "https://") ||
                        starts_with(inner, "mailto:");
    const bool email = inner.find('@') != NPOS && inner.find('.') != NPOS;
    return !inner.empty() && !spaced && (scheme || email);
}

bool
equals_ignore_case(std::string_view text, std::string_view lower)
{
    if(text.size() != lower.size())
    {
        return false;
    }
    for(size_t i = 0; i < text.size(); ++i)
    {
        const unsigned char byte = static_cast<unsigned char>(text[i]);
        const char          folded =
            byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte - 'A' + 'a') : text[i];
        if(folded != lower[i])
        {
            return false;
        }
    }
    return true;
}

bool
is_break_tag(std::string_view inner)
{
    const std::string_view trimmed = trim(inner);
    return equals_ignore_case(trimmed, "br") || equals_ignore_case(trimmed, "br/") ||
           equals_ignore_case(trimmed, "br /");
}

struct entity_t
{
    std::string_view name;
    std::string_view text;
};

constexpr entity_t ENTITIES[] = {
    { "&amp;", "&" }, { "&lt;", "<" },   { "&gt;", ">" },   { "&quot;", "\"" },
    { "&#39;", "'" }, { "&apos;", "'" }, { "&nbsp;", " " },
};

// ------------------------------------------------------------------ parser

class Parser
{
public:
    explicit Parser(document_t& document)
    : m_doc(document)
    {}

    // False when the text is too large to index or the step budget runs out.
    // The caller then draws the source as plain text.
    bool Parse(std::string_view source)
    {
        assert(source.size() <= MAX_MARKDOWN_BYTES);
        if(source.size() > MAX_MARKDOWN_BYTES)
        {
            m_state.failed = true;
            return false;
        }
        m_state.limit = std::max(source.size() * PARSE_STEPS_PER_BYTE, MIN_PARSE_STEPS);
        size_t start  = 0;
        while(start <= source.size() && !m_state.failed)
        {
            if(!charge(m_state))
            {
                break;
            }
            const size_t     end  = std::min(source.find('\n', start), source.size());
            std::string_view line = source.substr(start, end - start);
            if(!line.empty() && line.back() == '\r')
            {
                line.remove_suffix(1);
            }
            Line(line);
            start = end + 1;
        }
        Flush();
        return !m_state.failed;
    }

private:
    struct ListLevel
    {
        int32_t marker_indent  = 0;
        int32_t content_indent = 0;
        bool    ordered        = false;
        int32_t number         = 0;
    };

    void Line(std::string_view line)
    {
        if(m_state.failed)
        {
            return;
        }
        if(m_fence_length > 0 && !CodeLine(line))
        {
            return;
        }
        if(m_indented)
        {
            if(ContinueIndented(line))
            {
                return;
            }
            m_indented = false;
        }

        uint8_t               quote   = 0;
        std::string_view      rest    = strip_quotes(line, MAX_NESTING, quote);
        const column_indent_t indent  = measure_indent(rest);
        std::string_view      content = rest.substr(indent.bytes);
        if(trim(content).empty())
        {
            Flush();
            m_table = -1;
            m_blank = true;
            return;
        }

        // A paragraph carries on through a line with fewer quote markers,
        // unless that line starts a block of its own.
        if(m_open && quote < m_quote && !StartsBlock(content))
        {
            Continue(content);
            return;
        }
        if(quote != m_quote)
        {
            Flush();
            m_table = -1;
            m_lists.clear();
            m_quote = quote;
        }
        if(m_table >= 0 && content.find('|') != NPOS)
        {
            AddRow(m_doc.blocks[static_cast<size_t>(m_table)], content);
            m_blank = false;
            return;
        }
        m_table = -1;

        // Four columns of indent at the root is a code block. It does not
        // interrupt a paragraph, and a list item keeps its own indent.
        if(!m_open && m_lists.empty() && indent.columns >= INDENTED_CODE_COLUMNS)
        {
            Flush();
            Open(BlockKind::kCode);
            m_indented   = true;
            m_code_lines = 0;
            ContinueIndented(line);
            m_blank = false;
            return;
        }

        const bool in_item =
            m_lists.empty() || indent.columns >= m_lists.back().content_indent;
        if(m_open && in_item && setext_level(content) > 0)
        {
            block_t& heading = Open(BlockKind::kHeading);
            heading.level    = static_cast<uint8_t>(setext_level(content));
            assert(heading.level >= 1 && heading.level <= MAX_HEADING_LEVEL);
            Inline(heading.spans, m_paragraph, 0, -1, 0);
            m_paragraph.clear();
            m_open  = false;
            m_blank = false;
            return;
        }

        int32_t content_indent = indent.columns;
        if(!ListItem(content, indent.columns, content_indent))
        {
            if(m_open && !StartsBlock(content))
            {
                Continue(content);
                m_blank = false;
                return;
            }
            while(!m_lists.empty() && indent.columns < m_lists.back().content_indent)
            {
                m_lists.pop_back();
            }
        }
        Content(content, content_indent);
        m_blank = false;
    }

    // True when this line belongs to the open indented code block.
    bool ContinueIndented(std::string_view line)
    {
        uint8_t          quote = 0;
        std::string_view rest  = strip_quotes(line, m_quote, quote);
        if(quote < m_quote)
        {
            return false;
        }
        block_t& code = m_doc.blocks.back();
        if(trim(rest).empty())
        {
            if(m_code_lines > 0)
            {
                Append(code.spans, "\n", kCode, -1);
            }
            ++m_code_lines;
            return true;
        }
        const column_indent_t indent = measure_indent(rest);
        if(indent.columns < INDENTED_CODE_COLUMNS)
        {
            return false;
        }
        if(m_code_lines > 0)
        {
            Append(code.spans, "\n", kCode, -1);
        }
        Append(code.spans, strip_columns(rest, INDENTED_CODE_COLUMNS), kCode, -1);
        ++m_code_lines;
        return true;
    }

    // Returns true when the line closed the fence and still needs reading.
    bool CodeLine(std::string_view line)
    {
        uint8_t               quote        = 0;
        std::string_view      rest         = strip_quotes(line, m_fence_quote, quote);
        bool                  done         = quote < m_fence_quote;
        const column_indent_t fence_indent = measure_indent(rest);
        const int32_t         strip    = std::min(fence_indent.columns, m_fence_indent);
        rest                           = strip_columns(rest, strip);
        const std::string_view trimmed = trim(rest);
        const size_t           run =
            !trimmed.empty() && trimmed[0] == m_fence ? run_length(trimmed, 0) : 0;
        if(!done && run >= m_fence_length && run == trimmed.size())
        {
            m_fence_length = 0;
        }
        else if(!done)
        {
            block_t& code = m_doc.blocks.back();
            if(m_code_lines > 0)
            {
                Append(code.spans, "\n", kCode, -1);
            }
            Append(code.spans, rest, kCode, -1);
            ++m_code_lines;
        }
        if(done)
        {
            m_fence_length = 0;
        }
        return done;
    }

    // Opens a list item when the line starts with a marker, leaving its text
    // in `content`.
    bool ListItem(std::string_view& content, int32_t indent, int32_t& content_indent)
    {
        bool         ordered = false;
        int32_t      number  = 0;
        const size_t marker  = list_marker(content, ordered, number);
        if(marker == 0)
        {
            return false;
        }

        Flush();
        m_table = -1;
        ListLevel sibling;
        bool      has_sibling = false;
        while(!m_lists.empty() && indent < m_lists.back().content_indent)
        {
            sibling     = m_lists.back();
            has_sibling = true;
            m_lists.pop_back();
        }
        const column_indent_t spaces = measure_indent(content.substr(marker));
        ListLevel             level;
        level.marker_indent = indent;
        const int32_t gap =
            std::max(std::min(spaces.columns, static_cast<int32_t>(MAX_LIST_SPACES)),
                     int32_t{ 1 });
        level.content_indent = indent + static_cast<int32_t>(marker) + gap;
        level.ordered        = ordered;
        level.number         = number;
        // A list counts on from its first number, whatever the next ones say.
        if(ordered && has_sibling && sibling.ordered && sibling.marker_indent == indent)
        {
            level.number = sibling.number + 1;
        }
        if(m_lists.size() < MAX_NESTING)
        {
            m_lists.push_back(level);
        }
        content        = content.substr(std::min(marker + spaces.bytes, content.size()));
        content_indent = level.content_indent;
        m_marker       = ordered ? Marker::kNumber : Marker::kBullet;
        m_number       = level.number;

        const bool task = content.size() >= TASK_MARKER_LENGTH && content[0] == '[' &&
                          content[2] == ']' &&
                          (content[1] == ' ' || content[1] == 'x' || content[1] == 'X') &&
                          (content.size() == TASK_MARKER_LENGTH ||
                           is_blank(content[TASK_MARKER_LENGTH]));
        if(task)
        {
            m_marker = content[1] == ' ' ? Marker::kTask : Marker::kTaskDone;
            content  = trim(content.substr(TASK_MARKER_LENGTH));
        }
        return true;
    }

    // Whether a line, quote markers and indent gone, interrupts a paragraph.
    bool StartsBlock(std::string_view content) const
    {
        bool         ordered = false;
        int32_t      number  = 0;
        const size_t hashes =
            !content.empty() && content[0] == '#' ? run_length(content, 0) : 0;
        const bool heading = hashes > 0 && hashes <= MAX_HEADING_LEVEL &&
                             (hashes == content.size() || is_blank(content[hashes]));
        const bool fence = !content.empty() && (content[0] == '`' || content[0] == '~') &&
                           run_length(content, 0) >= MIN_FENCE_LENGTH;
        return heading || fence || is_rule(content) ||
               list_marker(content, ordered, number) > 0 || TableStarts(content);
    }

    // Whether the open paragraph is a table header and this its delimiter row.
    bool TableStarts(std::string_view content) const
    {
        if(!(m_open && m_paragraph.find('\n') == std::string::npos &&
             m_paragraph.find('|') != std::string::npos))
        {
            return false;
        }
        if(!delimiter_row(content, m_align_scratch, m_cells))
        {
            return false;
        }
        split_row(m_paragraph, m_header_cells);
        return m_align_scratch.size() == m_header_cells.size();
    }

    void Content(std::string_view content, int32_t indent)
    {
        const char   first  = content.empty() ? '\0' : content[0];
        const size_t fence  = (first == '`' || first == '~') ? run_length(content, 0) : 0;
        const size_t hashes = first == '#' ? run_length(content, 0) : 0;
        if(fence >= MIN_FENCE_LENGTH &&
           (first == '~' || content.find('`', fence) == NPOS))
        {
            Flush();
            Open(BlockKind::kCode);
            m_fence        = first;
            m_fence_length = fence;
            m_fence_indent = indent;
            m_fence_quote  = m_quote;
            m_code_lines   = 0;
        }
        else if(is_rule(content))
        {
            Flush();
            Open(BlockKind::kRule);
        }
        else if(hashes > 0 && hashes <= MAX_HEADING_LEVEL &&
                (hashes == content.size() || is_blank(content[hashes])))
        {
            Flush();
            block_t& heading = Open(BlockKind::kHeading);
            heading.level    = static_cast<uint8_t>(hashes);
            assert(heading.level >= 1 && heading.level <= MAX_HEADING_LEVEL);
            Inline(heading.spans, heading_text(content.substr(hashes)), 0, -1, 0);
        }
        else if(TableStarts(content))
        {
            const std::string header = m_paragraph;
            m_paragraph.clear();
            m_open         = false;
            block_t& table = Open(BlockKind::kTable);
            table.align    = m_align_scratch;
            m_table        = static_cast<int32_t>(m_doc.blocks.size() - 1);
            AddRow(table, header);
        }
        else if(m_open)
        {
            Continue(content);
        }
        else
        {
            m_paragraph.assign(content.data(), content.size());
            m_open            = true;
            m_paragraph_blank = m_blank;
        }
    }

    void Continue(std::string_view content)
    {
        m_paragraph += '\n';
        m_paragraph.append(content.data(), content.size());
    }

    void Flush()
    {
        if(m_open)
        {
            block_t& block = Open(BlockKind::kParagraph);
            Inline(block.spans, m_paragraph, 0, -1, 0);
            m_paragraph.clear();
            m_open = false;
        }
    }

    block_t& Open(BlockKind kind)
    {
        const bool blank        = m_open ? m_paragraph_blank : m_blank;
        const bool follows_item = !m_doc.blocks.empty() && m_doc.blocks.back().list > 0;
        if(!m_doc.text.empty())
        {
            m_doc.text += '\n';
        }
        block_t& block = m_doc.blocks.emplace_back();
        block.kind     = kind;
        block.quote    = m_quote;
        block.list     = static_cast<uint8_t>(m_lists.size());
        block.tight    = !blank && follows_item && block.list > 0;
        block.marker   = m_marker;
        if(m_marker != Marker::kNone)
        {
            std::string marker = BULLET_TEXT;
            if(m_marker == Marker::kNumber)
            {
                marker = std::to_string(m_number) + ". ";
            }
            else if(m_marker == Marker::kTask)
            {
                marker = "[ ] ";
            }
            else if(m_marker == Marker::kTaskDone)
            {
                marker = "[x] ";
            }
            block.marker_span.begin = static_cast<uint32_t>(m_doc.text.size());
            m_doc.text += marker;
            block.marker_span.end = static_cast<uint32_t>(m_doc.text.size());
            m_marker              = Marker::kNone;
        }
        return block;
    }

    void AddRow(block_t& table, std::string_view row)
    {
        split_row(row, m_cells);
        m_cells.resize(table.align.size());
        assert(m_cells.size() == table.align.size());
        if(!table.rows.empty())
        {
            m_doc.text += '\n';
        }
        std::vector<cell_t>& out = table.rows.emplace_back();
        out.resize(m_cells.size());
        for(size_t column = 0; column < m_cells.size(); ++column)
        {
            if(column > 0)
            {
                m_doc.text += '\t';
            }
            Inline(out[column].spans, m_cells[column], 0, -1, 0);
        }
    }

    void Append(std::vector<span_t>& spans, std::string_view text, uint32_t flags,
                int32_t link)
    {
        if(text.empty() || m_state.failed)
        {
            return;
        }
        assert(m_doc.text.size() + text.size() <= MAX_MARKDOWN_BYTES);
        if(m_doc.text.size() + text.size() > MAX_MARKDOWN_BYTES)
        {
            m_state.failed = true;
            return;
        }
        const uint32_t begin = static_cast<uint32_t>(m_doc.text.size());
        m_doc.text.append(text.data(), text.size());
        const uint32_t end = static_cast<uint32_t>(m_doc.text.size());
        if(!spans.empty() && spans.back().end == begin && spans.back().flags == flags &&
           spans.back().link == link)
        {
            spans.back().end = end;
        }
        else
        {
            span_t span;
            span.begin = begin;
            span.end   = end;
            span.flags = flags;
            span.link  = link;
            assert(span.begin <= span.end && span.end <= m_doc.text.size());
            spans.push_back(span);
        }
    }

    int32_t AddLink(std::string_view target)
    {
        m_doc.links.emplace_back(target);
        return static_cast<int32_t>(m_doc.links.size() - 1);
    }

    // Newlines in code spans read as spaces, and one space either side goes
    // when both are there.
    void AppendCode(std::vector<span_t>& spans, std::string_view code, uint32_t flags,
                    int32_t link)
    {
        std::string text(code);
        std::replace(text.begin(), text.end(), '\n', ' ');
        if(text.size() > 2 && text.front() == ' ' && text.back() == ' ' &&
           text.find_first_not_of(' ') != std::string::npos)
        {
            text = text.substr(1, text.size() - 2);
        }
        Append(spans, text, flags, link);
    }

    void Inline(std::vector<span_t>& spans, std::string_view s, uint32_t flags,
                int32_t link, int32_t depth)
    {
        if(m_state.failed)
        {
            return;
        }
        // Closer positions belong to this string. A nested call parses a
        // different slice, so the maps are rebuilt when this one resumes.
        m_state.closers.clear();
        m_state.tick_runs.clear();
        m_state.tick_data = nullptr;
        m_state.tick_size = 0;
        size_t plain      = 0;
        size_t i          = 0;
        while(i < s.size() && !m_state.failed)
        {
            const char   c        = s[i];
            const char   next     = i + 1 < s.size() ? s[i + 1] : '\0';
            const size_t consumed = Construct(spans, s, i, flags, link, depth, plain);
            if(consumed > 0)
            {
                i = consumed;
            }
            else if(c == '\\' && is_punct(next))
            {
                // The escaped character starts the next literal run.
                Append(spans, s.substr(plain, i - plain), flags, link);
                plain = i + 1;
                i += 2;
            }
            else
            {
                ++i;
            }
        }
        Append(spans, s.substr(std::min(plain, s.size())), flags, link);
    }

    // Handles the inline construct at `at`, if one starts there, and returns
    // where the text resumes; 0 when there is none. `plain` is where the
    // pending literal text starts, and moves only past what was consumed.
    size_t Construct(std::vector<span_t>& spans, std::string_view s, size_t at,
                     uint32_t flags, int32_t link, int32_t depth, size_t& plain)
    {
        const char c      = s[at];
        const char next   = at + 1 < s.size() ? s[at + 1] : '\0';
        size_t     resume = 0;
        bool       took   = false;  // [plain, resume) is done with
        if(c == '\n' || (c == '\\' && next == '\n'))
        {
            // Two trailing spaces or a backslash end the line; otherwise the
            // line break is just a space.
            size_t end = at;
            while(end > plain && s[end - 1] == ' ')
            {
                --end;
            }
            const bool hard = c == '\\' || at - end >= 2;
            Append(spans, s.substr(plain, end - plain), flags, link);
            Append(spans, hard ? "\n" : " ", flags, link);
            resume = at + (c == '\\' ? 2 : 1);
            resume += measure_indent(s.substr(resume)).bytes;
            took = true;
        }
        else if(c == '`')
        {
            // An unmatched run stays text, but whole, so its backticks never
            // pair up among themselves.
            const size_t ticks = run_length(s, at);
            const size_t close = find_ticks(m_state, s, at + ticks, ticks);
            resume             = close == NPOS ? at + ticks : close + ticks;
            took               = close != NPOS;
            if(took)
            {
                Append(spans, s.substr(plain, at - plain), flags, link);
                AppendCode(spans, s.substr(at + ticks, close - at - ticks), flags | kCode,
                           link);
            }
        }
        else if(c == '*' || c == '_' || c == '~')
        {
            resume = Emphasis(spans, s, at, flags, link, depth, plain, took);
        }
        else if((c == '[' || (c == '!' && next == '[')) && link < 0 &&
                depth < MAX_INLINE_DEPTH)
        {
            const size_t  label = c == '!' ? at + 1 : at;
            const size_t  close = find_bracket(m_state, s, label);
            destination_t target;
            if(close != NPOS && parse_destination(s, close + 1, target))
            {
                Append(spans, s.substr(plain, at - plain), flags, link);
                const std::string_view url =
                    s.substr(target.begin, target.end - target.begin);
                std::string_view text = s.substr(label + 1, close - label - 1);
                if(text.empty())
                {
                    text = url;
                }
                Inline(spans, text, flags, AddLink(url), depth + 1);
                resume = target.after;
                took   = true;
            }
        }
        else if(c == '<')
        {
            const size_t           close = s.find('>', at + 1);
            const std::string_view inner =
                close == NPOS ? std::string_view() : s.substr(at + 1, close - at - 1);
            if(close != NPOS && is_break_tag(inner))
            {
                Append(spans, s.substr(plain, at - plain), flags, link);
                Append(spans, "\n", flags, link);
                resume = close + 1;
                took   = true;
            }
            else if(close != NPOS && link < 0 && is_autolink(inner))
            {
                Append(spans, s.substr(plain, at - plain), flags, link);
                const bool email = inner.find(':') == NPOS;
                Append(
                    spans, inner, flags,
                    AddLink(email ? "mailto:" + std::string(inner) : std::string(inner)));
                resume = close + 1;
                took   = true;
            }
        }
        else if(c == '&')
        {
            for(const entity_t& entity : ENTITIES)
            {
                if(!took && starts_with(s.substr(at), entity.name))
                {
                    Append(spans, s.substr(plain, at - plain), flags, link);
                    Append(spans, entity.text, flags, link);
                    resume = at + entity.name.size();
                    took   = true;
                }
            }
        }
        else if((c == 'h' || c == 'w') && link < 0)
        {
            const size_t length = bare_url_length(s, at);
            if(length > 0)
            {
                Append(spans, s.substr(plain, at - plain), flags, link);
                const std::string_view url    = s.substr(at, length);
                const std::string      target = starts_with(url, "www.")
                                                    ? "http://" + std::string(url)
                                                    : std::string(url);
                Append(spans, url, flags, AddLink(target));
                resume = at + length;
                took   = true;
            }
        }
        if(took)
        {
            plain = resume;
        }
        return resume;
    }

    size_t Emphasis(std::vector<span_t>& spans, std::string_view s, size_t at,
                    uint32_t flags, int32_t link, int32_t depth, size_t plain, bool& took)
    {
        const char   marker = s[at];
        const size_t run    = run_length(s, at);
        const size_t width  = emphasis_width(marker, run);
        // The run's last `width` markers open; any before them are text.
        const size_t open  = at + run - width;
        size_t       close = NPOS;
        if((marker != '~' || run == 2) && depth < MAX_INLINE_DEPTH &&
           flank(s, at, run).open)
        {
            close = find_closer(m_state, s, at + run, marker, width, 0);
        }
        // With no partner the whole run stays text, left in the literal run.
        size_t resume = at + run;
        took          = close != NPOS;
        if(took)
        {
            uint32_t added = kStrike;
            if(marker != '~')
            {
                added = width == 1 ? kItalic : (width == 2 ? kBold : kBold | kItalic);
            }
            Append(spans, s.substr(plain, open - plain), flags, link);
            Inline(spans, s.substr(open + width, close - open - width), flags | added,
                   link, depth + 1);
            resume = close + width;
        }
        return resume;
    }

    document_t&            m_doc;
    std::string            m_paragraph;
    bool                   m_open            = false;  // m_paragraph is a paragraph
    bool                   m_paragraph_blank = false;  // a blank line came before it
    bool                   m_blank           = false;  // the last line was blank
    uint8_t                m_quote           = 0;
    std::vector<ListLevel> m_lists;
    // Owed to the next block opened.
    Marker        m_marker       = Marker::kNone;
    int32_t       m_number       = 0;
    int32_t       m_table        = -1;  // the open table, as a block index
    char          m_fence        = '\0';
    size_t        m_fence_length = 0;
    int32_t       m_fence_indent = 0;
    uint8_t       m_fence_quote  = 0;
    size_t        m_code_lines   = 0;
    bool          m_indented     = false;
    parse_state_t m_state;
    // Reused so a line does not allocate a vector to ask what it starts.
    mutable std::vector<Alignment>        m_align_scratch;
    mutable std::vector<std::string_view> m_cells;
    mutable std::vector<std::string_view> m_header_cells;
};

void
build_plain(document_t& document, std::string_view text)
{
    assert(text.size() <= MAX_MARKDOWN_BYTES);
    document.text.clear();
    document.text.reserve(std::min(text.size(), MAX_MARKDOWN_BYTES));
    for(char c : text)
    {
        if(c != '\r' && document.text.size() < MAX_MARKDOWN_BYTES)
        {
            document.text.push_back(c);
        }
    }
    block_t& block = document.blocks.emplace_back();
    if(!document.text.empty())
    {
        span_t span;
        span.end = static_cast<uint32_t>(document.text.size());
        assert(span.end <= document.text.size());
        block.spans.push_back(span);
    }
}

// ------------------------------------------------------------------ layout

enum class Role : uint8_t
{
    kText,
    kHeading,
    kQuote,
    kCodeBlock
};

enum class ShapeKind : uint8_t
{
    kRect,
    kDisc,
    kCircle,
    kBox,
    kCheckedBox
};

enum class Paint : uint8_t
{
    kText,
    kQuote,
    kLine,
    kCodeBackground,
    kHeaderBackground
};

struct fragment_t
{
    // Top left of the glyphs and the line box, relative to the view.
    float    x      = 0.0f;
    float    y      = 0.0f;
    float    top    = 0.0f;
    float    bottom = 0.0f;
    float    width  = 0.0f;
    float    ascent = 0.0f;
    float    size   = 0.0f;
    ImFont*  font   = nullptr;
    uint32_t begin  = 0;  // the range of document_t::text drawn
    uint32_t end    = 0;
    uint32_t flags  = 0;
    int32_t  link   = -1;
    Role     role   = Role::kText;
};

struct shape_t
{
    ShapeKind kind  = ShapeKind::kRect;
    Paint     paint = Paint::kLine;
    ImVec2    min;
    ImVec2    max;
};

struct layout_t
{
    std::vector<fragment_t> fragments;
    std::vector<shape_t>    shapes;
    float                   height = 0.0f;
};

struct fonts_t
{
    ImFont* regular                          = nullptr;
    ImFont* bold                             = nullptr;
    ImFont* italic                           = nullptr;
    ImFont* code                             = nullptr;
    float   size                             = 0.0f;
    float   heading_scale[MAX_HEADING_LEVEL] = {};
};

bool
same_fonts(const fonts_t& a, const fonts_t& b)
{
    bool same = a.regular == b.regular && a.bold == b.bold && a.italic == b.italic &&
                a.code == b.code && a.size == b.size;
    for(size_t level = 0; level < MAX_HEADING_LEVEL; ++level)
    {
        same = same && a.heading_scale[level] == b.heading_scale[level];
    }
    return same;
}

fonts_t
resolve_fonts(const markdown_style_t& style)
{
    fonts_t fonts;
    fonts.regular = style.font != nullptr ? style.font : ImGui::GetFont();
    fonts.bold    = style.bold_font;
    fonts.italic  = style.italic_font;
    fonts.code    = style.code_font != nullptr ? style.code_font : fonts.regular;
    fonts.size    = style.font_size > 0.0f ? style.font_size : ImGui::GetFontSize();
    for(size_t level = 0; level < MAX_HEADING_LEVEL; ++level)
    {
        fonts.heading_scale[level] = style.heading_scale[level];
    }
    return fonts;
}

class Layouter
{
public:
    Layouter(const document_t& document, const fonts_t& fonts, float width,
             layout_t& layout)
    : m_doc(document)
    , m_fonts(fonts)
    , m_width(width)
    , m_layout(layout)
    {}

    void Run()
    {
        float          y        = 0.0f;
        const block_t* previous = nullptr;
        for(const block_t& block : m_doc.blocks)
        {
            const float gap = previous != nullptr ? Em(Gap(block)) : 0.0f;
            y += gap;
            const float  x0  = Em(block.quote * QUOTE_INDENT + block.list * LIST_INDENT);
            const float  x1  = std::max(x0 + Em(MIN_COLUMN_WIDTH), m_width);
            const float  top = y;
            const size_t first = m_layout.fragments.size();
            y                  = Body(block, x0, x1, y);
            if(block.marker != Marker::kNone)
            {
                AddMarker(block, x0, top, first);
            }
            for(uint8_t level = 0; level < block.quote; ++level)
            {
                // Bars run through the gap when the block above is in the quote.
                const bool  joined = previous != nullptr && previous->quote > level;
                const float bar    = Em(level * QUOTE_INDENT);
                AddShape(ShapeKind::kRect, Paint::kLine,
                         ImVec2(bar, joined ? top - gap : top),
                         ImVec2(bar + Em(QUOTE_BAR_WIDTH), y));
            }
            previous = &block;
        }
        m_layout.height = y;
    }

private:
    // How a piece of text is drawn.
    struct ink_t
    {
        ImFont*  font   = nullptr;
        float    ascent = 0.0f;
        uint32_t flags  = 0;
        int32_t  link   = -1;
        Role     role   = Role::kText;
    };

    // Spaces held back until the next word lands on the same line, so a line
    // never ends in them.
    struct pending_t
    {
        ink_t    ink;
        uint32_t begin = 0;
        uint32_t end   = 0;
        float    width = 0.0f;
    };

    float Em(float value) const { return value * m_fonts.size; }

    static float Gap(const block_t& block)
    {
        float gap = BLOCK_GAP;
        if(block.kind == BlockKind::kHeading)
        {
            gap = HEADING_GAP;
        }
        else if(block.tight)
        {
            gap = LIST_GAP;
        }
        return gap;
    }

    static Role TextRole(const block_t& block)
    {
        return block.quote > 0 ? Role::kQuote : Role::kText;
    }

    float Body(const block_t& block, float x0, float x1, float y)
    {
        float bottom = y;
        if(block.kind == BlockKind::kParagraph)
        {
            bottom = Inline(block.spans, x0, x1, y, m_fonts.size, TextRole(block), 0,
                            Alignment::kLeft);
        }
        else if(block.kind == BlockKind::kHeading)
        {
            assert(block.level >= 1 && block.level <= MAX_HEADING_LEVEL);
            const float size = m_fonts.size * m_fonts.heading_scale[block.level - 1];
            bottom           = Inline(block.spans, x0, x1, y, size, Role::kHeading, kBold,
                                      Alignment::kLeft);
        }
        else if(block.kind == BlockKind::kCode)
        {
            const float pad = Em(CODE_PADDING);
            bottom = Inline(block.spans, x0 + pad, x1 - pad, y + pad, m_fonts.size,
                            Role::kCodeBlock, 0, Alignment::kLeft) +
                     pad;
            AddShape(ShapeKind::kRect, Paint::kCodeBackground, ImVec2(x0, y),
                     ImVec2(x1, bottom));
        }
        else if(block.kind == BlockKind::kRule)
        {
            const float middle = y + Em(RULE_HEIGHT) * HALF;
            AddShape(ShapeKind::kRect, Paint::kLine, ImVec2(x0, middle),
                     ImVec2(x1, middle + LINE_THICKNESS));
            bottom = y + Em(RULE_HEIGHT);
        }
        else
        {
            bottom = Table(block, x0, x1, y);
        }
        return bottom;
    }

    ink_t InkFor(uint32_t flags, int32_t link, Role role, float size) const
    {
        const bool bold   = (flags & kBold) != 0;
        const bool italic = (flags & kItalic) != 0;
        ink_t      ink;
        ink.font = m_fonts.regular;
        if((flags & kCode) != 0)
        {
            ink.font = m_fonts.code;
        }
        else if(bold && m_fonts.bold != nullptr)
        {
            ink.font = m_fonts.bold;
        }
        else if(italic && m_fonts.italic != nullptr)
        {
            ink.font = m_fonts.italic;
        }
        ink.flags = flags;
        if(bold && ink.font != m_fonts.bold)
        {
            ink.flags |= kFauxBold;
        }
        if(italic && ink.font != m_fonts.italic)
        {
            ink.flags |= kFauxItalic;
        }
        ink.link   = link;
        ink.role   = role;
        ink.ascent = ink.font->GetFontBaked(size)->Ascent;
        return ink;
    }

    float Measure(const ink_t& ink, uint32_t begin, uint32_t end) const
    {
        const char* text = m_doc.text.data();
        return ink.font->CalcTextSizeA(m_size, FLT_MAX, 0.0f, text + begin, text + end).x;
    }

    // Lays spans out from (x0, y), wrapping at x1, and returns the bottom.
    float Inline(const std::vector<span_t>& spans, float x0, float x1, float y,
                 float size, Role role, uint32_t extra, Alignment align)
    {
        m_x0             = x0;
        m_x1             = x1;
        m_x              = x0;
        m_y              = y;
        m_size           = size;
        m_align          = align;
        m_line_first     = m_layout.fragments.size();
        m_line_empty     = true;
        m_soft           = false;
        m_pending        = pending_t();
        const char* text = m_doc.text.data();
        for(const span_t& span : spans)
        {
            const ink_t ink = InkFor(span.flags | extra, span.link, role, size);
            uint32_t    at  = span.begin;
            while(at < span.end)
            {
                uint32_t stop = at;
                while(stop < span.end && text[stop] != ' ' && text[stop] != '\n')
                {
                    ++stop;
                }
                if(text[at] == '\n')
                {
                    m_pending = pending_t();
                    EndLine(false);
                    ++at;
                }
                else if(stop == at)
                {
                    while(stop < span.end && text[stop] == ' ')
                    {
                        ++stop;
                    }
                    Spaces(ink, at, stop);
                    at = stop;
                }
                else
                {
                    at = Word(ink, at, stop);
                }
            }
        }
        m_pending = pending_t();
        EndLine(false);
        return m_y;
    }

    void Spaces(const ink_t& ink, uint32_t begin, uint32_t end)
    {
        const float width = Measure(ink, begin, end);
        if(m_line_empty && !m_soft)
        {
            // Indentation at the start of a hard line, as in code.
            Piece(ink, begin, end, width);
        }
        else if(!m_line_empty)
        {
            FlushPending();
            m_pending.ink   = ink;
            m_pending.begin = begin;
            m_pending.end   = end;
            m_pending.width = width;
        }
    }

    // Places a word, wrapping first when it does not fit, and returns where
    // the text resumes: before the end when the word was cut.
    uint32_t Word(const ink_t& ink, uint32_t begin, uint32_t end)
    {
        const float width = Measure(ink, begin, end);
        if(!m_line_empty && m_x + m_pending.width + width > m_x1)
        {
            m_pending = pending_t();
            EndLine(true);
        }
        FlushPending();
        uint32_t stop = end;
        if(m_line_empty && width > m_x1 - m_x0)
        {
            // Wider than a whole line: cut it where the line runs out.
            const char* text = m_doc.text.data();
            const char* cut  = ink.font->CalcWordWrapPosition(m_size, text + begin,
                                                              text + end, m_x1 - m_x0);
            if(cut <= text + begin)
            {
                cut =
                    text + begin + ImTextCountUtf8BytesFromChar(text + begin, text + end);
            }
            stop = static_cast<uint32_t>(cut - text);
        }
        Piece(ink, begin, stop, stop == end ? width : Measure(ink, begin, stop));
        if(stop < end)
        {
            EndLine(true);
        }
        return stop;
    }

    void FlushPending()
    {
        if(m_pending.end > m_pending.begin)
        {
            Piece(m_pending.ink, m_pending.begin, m_pending.end, m_pending.width);
        }
        m_pending = pending_t();
    }

    void Piece(const ink_t& ink, uint32_t begin, uint32_t end, float width)
    {
        fragment_t* last = m_line_empty ? nullptr : &m_layout.fragments.back();
        if(last != nullptr && last->end == begin && last->font == ink.font &&
           last->flags == ink.flags && last->link == ink.link && last->role == ink.role)
        {
            last->end = end;
            last->width += width;
        }
        else if(begin < end)
        {
            fragment_t& fragment = m_layout.fragments.emplace_back();
            fragment.x           = m_x;
            fragment.width       = width;
            fragment.ascent      = ink.ascent;
            fragment.size        = m_size;
            fragment.font        = ink.font;
            fragment.begin       = begin;
            fragment.end         = end;
            fragment.flags       = ink.flags;
            fragment.link        = ink.link;
            fragment.role        = ink.role;
            m_line_empty         = false;
        }
        m_x += width;
    }

    void EndLine(bool soft)
    {
        float ascent = 0.0f;
        float height = m_line_empty ? m_size : 0.0f;
        for(size_t i = m_line_first; i < m_layout.fragments.size(); ++i)
        {
            ascent = std::max(ascent, m_layout.fragments[i].ascent);
            height = std::max(height, m_layout.fragments[i].size);
        }
        float shift = 0.0f;
        if(m_align != Alignment::kLeft)
        {
            const float room = std::max(m_x1 - m_x, 0.0f);
            shift            = m_align == Alignment::kCenter ? room * HALF : room;
        }
        for(size_t i = m_line_first; i < m_layout.fragments.size(); ++i)
        {
            fragment_t& fragment = m_layout.fragments[i];
            fragment.x += shift;
            fragment.y      = m_y + ascent - fragment.ascent;
            fragment.top    = m_y;
            fragment.bottom = m_y + height;
        }
        m_y += height;
        m_x          = m_x0;
        m_line_first = m_layout.fragments.size();
        m_line_empty = true;
        m_soft       = soft;
    }

    void AddShape(ShapeKind kind, Paint paint, ImVec2 min, ImVec2 max)
    {
        shape_t& shape = m_layout.shapes.emplace_back();
        shape.kind     = kind;
        shape.paint    = paint;
        shape.min      = min;
        shape.max      = max;
    }

    // The marker hangs in the indent, centered on the item's first line.
    void AddMarker(const block_t& block, float x0, float top, size_t first)
    {
        const bool  has_lead = first < m_layout.fragments.size();
        const float line_top = has_lead ? m_layout.fragments[first].top : top;
        const float line_bottom =
            has_lead ? m_layout.fragments[first].bottom : top + m_fonts.size;
        const float middle = (line_top + line_bottom) * HALF;
        const float right  = x0 - Em(MARKER_GAP);
        const Paint paint  = block.quote > 0 ? Paint::kQuote : Paint::kText;
        if(block.marker == Marker::kBullet)
        {
            const float radius = Em(BULLET_RADIUS);
            AddShape(block.list % 2 == 1 ? ShapeKind::kDisc : ShapeKind::kCircle, paint,
                     ImVec2(right - TWICE * radius, middle - radius),
                     ImVec2(right, middle + radius));
        }
        else if(block.marker == Marker::kNumber)
        {
            m_size              = m_fonts.size;
            const ink_t    ink  = InkFor(0, -1, TextRole(block), m_fonts.size);
            const uint32_t end  = block.marker_span.end - 1;  // without its space
            const float    wide = Measure(ink, block.marker_span.begin, end);
            fragment_t&    mark = m_layout.fragments.emplace_back();
            mark.x              = right - wide;
            mark.width          = wide;
            mark.top            = line_top;
            mark.bottom         = line_bottom;
            mark.ascent         = ink.ascent;
            mark.size           = m_fonts.size;
            mark.font           = ink.font;
            mark.begin          = block.marker_span.begin;
            mark.end            = end;
            mark.role           = ink.role;
            mark.y              = has_lead ? m_layout.fragments[first].y +
                                    m_layout.fragments[first].ascent - ink.ascent
                                           : line_top;
        }
        else
        {
            const float box = Em(TASK_BOX_SIZE);
            AddShape(block.marker == Marker::kTask ? ShapeKind::kBox
                                                   : ShapeKind::kCheckedBox,
                     paint, ImVec2(right - box, middle - box * HALF),
                     ImVec2(right, middle + box * HALF));
        }
    }

    // A cell's width on one line.
    float Natural(const std::vector<span_t>& spans, uint32_t extra)
    {
        float width = 0.0f;
        m_size      = m_fonts.size;
        for(const span_t& span : spans)
        {
            width += Measure(InkFor(span.flags | extra, span.link, Role::kText, m_size),
                             span.begin, span.end);
        }
        return width;
    }

    float Table(const block_t& table, float x0, float x1, float y)
    {
        const size_t       columns = table.align.size();
        const float        pad     = Em(CELL_PADDING);
        std::vector<float> widths(columns, 0.0f);
        float              total = 0.0f;
        for(size_t column = 0; column < columns; ++column)
        {
            for(size_t row = 0; row < table.rows.size(); ++row)
            {
                const float natural =
                    Natural(table.rows[row][column].spans, row == 0 ? kBold : 0) +
                    TWICE * pad;
                widths[column] = std::max(widths[column], natural);
            }
            total += widths[column];
        }
        if(total > x1 - x0)
        {
            // Too wide: share the room by natural width and wrap inside cells.
            const float scale = (x1 - x0) / total;
            for(float& width : widths)
            {
                width = std::max(Em(MIN_COLUMN_WIDTH), width * scale);
            }
        }

        const float top   = y;
        float       right = x0;
        for(size_t row = 0; row < table.rows.size(); ++row)
        {
            float x      = x0;
            float bottom = y;
            assert(table.rows[row].size() == columns);
            for(size_t column = 0; column < columns; ++column)
            {
                const float cell_bottom =
                    Inline(table.rows[row][column].spans, x + pad,
                           x + widths[column] - pad, y + pad, m_fonts.size,
                           TextRole(table), row == 0 ? kBold : 0, table.align[column]);
                bottom = std::max(bottom, cell_bottom + pad);
                x += widths[column];
            }
            if(row == 0)
            {
                AddShape(ShapeKind::kRect, Paint::kHeaderBackground, ImVec2(x0, y),
                         ImVec2(x, bottom));
            }
            AddShape(ShapeKind::kRect, Paint::kLine, ImVec2(x0, bottom - LINE_THICKNESS),
                     ImVec2(x, bottom));
            right = x;
            y     = bottom;
        }
        AddShape(ShapeKind::kRect, Paint::kLine, ImVec2(x0, top),
                 ImVec2(right, top + LINE_THICKNESS));
        float x = x0;
        for(size_t column = 0; column <= columns; ++column)
        {
            AddShape(ShapeKind::kRect, Paint::kLine, ImVec2(x, top),
                     ImVec2(x + LINE_THICKNESS, y));
            x += column < columns ? widths[column] : 0.0f;
        }
        return y;
    }

    const document_t& m_doc;
    const fonts_t&    m_fonts;
    const float       m_width;
    layout_t&         m_layout;

    // The line being built.
    float     m_x0         = 0.0f;
    float     m_x1         = 0.0f;
    float     m_x          = 0.0f;
    float     m_y          = 0.0f;
    float     m_size       = 0.0f;
    Alignment m_align      = Alignment::kLeft;
    size_t    m_line_first = 0;
    bool      m_line_empty = true;
    bool      m_soft       = false;  // the line began where the one above wrapped
    pending_t m_pending;
};

// ----------------------------------------------------------------- drawing

struct palette_t
{
    ImU32 text              = 0;
    ImU32 heading           = 0;
    ImU32 link              = 0;
    ImU32 code              = 0;
    ImU32 code_background   = 0;
    ImU32 quote             = 0;
    ImU32 line              = 0;
    ImU32 header_background = 0;
    ImU32 selection         = 0;
};

ImU32
pick_color(ImU32 color, ImU32 fallback)
{
    return color != 0 ? color : fallback;
}

palette_t
resolve_palette(const markdown_style_t& style)
{
    palette_t palette;
    palette.text    = pick_color(style.text_color, ImGui::GetColorU32(ImGuiCol_Text));
    palette.heading = pick_color(style.heading_color, palette.text);
    palette.link    = pick_color(style.link_color, ImGui::GetColorU32(ImGuiCol_TextLink));
    palette.code    = pick_color(style.code_color, palette.text);
    palette.code_background =
        pick_color(style.code_background, ImGui::GetColorU32(ImGuiCol_FrameBg));
    palette.quote =
        pick_color(style.quote_color, ImGui::GetColorU32(ImGuiCol_TextDisabled));
    palette.line = pick_color(style.line_color, ImGui::GetColorU32(ImGuiCol_Separator));
    palette.header_background =
        pick_color(style.header_background, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
    palette.selection =
        pick_color(style.selection_color, ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
    return palette;
}

ImU32
paint_color(const palette_t& palette, Paint paint)
{
    ImU32 color = palette.line;
    if(paint == Paint::kText)
    {
        color = palette.text;
    }
    else if(paint == Paint::kQuote)
    {
        color = palette.quote;
    }
    else if(paint == Paint::kCodeBackground)
    {
        color = palette.code_background;
    }
    else if(paint == Paint::kHeaderBackground)
    {
        color = palette.header_background;
    }
    return color;
}

// True when the callback will open this link. Asking does not open it.
bool
link_opens(const markdown_style_t& style, const document_t& document, int32_t link)
{
    if(link < 0 || !style.open_link)
    {
        return false;
    }
    return style.open_link(document.links[static_cast<size_t>(link)], false);
}

ImU32
fragment_color(const palette_t& palette, const fragment_t& fragment, bool linked)
{
    ImU32 color = palette.text;
    if(linked)
    {
        color = palette.link;
    }
    else if((fragment.flags & kCode) != 0)
    {
        color = palette.code;
    }
    else if(fragment.role == Role::kHeading)
    {
        color = palette.heading;
    }
    else if(fragment.role == Role::kQuote)
    {
        color = palette.quote;
    }
    return color;
}

bool
is_background(Paint paint)
{
    return paint == Paint::kCodeBackground || paint == Paint::kHeaderBackground;
}

struct cached_view_t
{
    std::unique_ptr<MarkdownView> view;
    int                           frame = 0;
};

std::unordered_map<ImGuiID, cached_view_t>&
view_cache()
{
    static std::unordered_map<ImGuiID, cached_view_t> cache;
    return cache;
}

int g_cache_frame = -1;

}  // namespace

bool     MarkdownView::s_selection_enabled = true;
uint64_t MarkdownView::s_active_view       = 0;
uint64_t MarkdownView::s_view_count        = 0;

// -------------------------------------------------------------------- view

struct MarkdownView::Impl
{
    std::string source;
    uint32_t    flags  = MARKDOWN_SELECTABLE;
    bool        parsed = false;
    document_t  document;
    layout_t    layout;
    fonts_t     layout_fonts;
    float       layout_width = -1.0f;
    bool        laid_out     = false;
    // The selection, as offsets into document.text; empty when they match.
    uint32_t anchor       = 0;
    uint32_t caret        = 0;
    bool     dragging     = false;
    int32_t  pressed_link = -1;
    uint64_t id           = 0;

    uint32_t Begin() const { return std::min(anchor, caret); }
    uint32_t End() const { return std::max(anchor, caret); }

    // The text offset nearest a point, relative to the view.
    uint32_t HitTest(const ImVec2& point) const
    {
        const fragment_t* best    = nullptr;
        float             best_dy = FLT_MAX;
        float             best_dx = FLT_MAX;
        for(const fragment_t& fragment : layout.fragments)
        {
            const float dy =
                std::max({ fragment.top - point.y, point.y - fragment.bottom, 0.0f });
            const float dx = std::max(
                { fragment.x - point.x, point.x - (fragment.x + fragment.width), 0.0f });
            if(dy < best_dy || (dy == best_dy && dx < best_dx))
            {
                best    = &fragment;
                best_dy = dy;
                best_dx = dx;
            }
        }
        uint32_t offset = 0;
        if(best != nullptr)
        {
            const char* text = document.text.data();
            const char* at   = text + best->begin;
            const char* end  = text + best->end;
            float       x    = best->x;
            bool        done = false;
            while(!done && at < end)
            {
                const char* next = at + ImTextCountUtf8BytesFromChar(at, end);
                const float advance =
                    best->font->CalcTextSizeA(best->size, FLT_MAX, 0.0f, at, next).x;
                done = point.x < x + advance * HALF;
                if(!done)
                {
                    x += advance;
                    at = next;
                }
            }
            offset = static_cast<uint32_t>(at - text);
        }
        return offset;
    }

    int32_t Linkoffset_pos(const ImVec2& point) const
    {
        int32_t link = -1;
        for(const fragment_t& fragment : layout.fragments)
        {
            if(fragment.link >= 0 && point.x >= fragment.x &&
               point.x < fragment.x + fragment.width && point.y >= fragment.top &&
               point.y < fragment.bottom)
            {
                link = fragment.link;
            }
        }
        return link;
    }

    void SelectWord(uint32_t at)
    {
        const std::string& text  = document.text;
        uint32_t           begin = std::min(at, static_cast<uint32_t>(text.size()));
        // Just past a word's end still means that word.
        if((begin == text.size() || !is_word_char(text[begin])) && begin > 0 &&
           is_word_char(text[begin - 1]))
        {
            --begin;
        }
        uint32_t end = begin;
        if(begin < text.size() && is_word_char(text[begin]))
        {
            while(begin > 0 && is_word_char(text[begin - 1]))
            {
                --begin;
            }
            while(end < text.size() && is_word_char(text[end]))
            {
                ++end;
            }
        }
        else if(begin < text.size())
        {
            end = begin + static_cast<uint32_t>(ImTextCountUtf8BytesFromChar(
                              text.data() + begin, text.data() + text.size()));
        }
        anchor = begin;
        caret  = end;
    }

    void SelectLine(uint32_t at)
    {
        const std::string& text   = document.text;
        const size_t       before = at > 0 ? text.rfind('\n', at - 1) : std::string::npos;
        const size_t       after  = text.find('\n', at);
        anchor = before == std::string::npos ? 0 : static_cast<uint32_t>(before + 1);
        caret  = static_cast<uint32_t>(after == std::string::npos ? text.size() : after);
    }

    bool Interact(const ImRect& bb, ImGuiID item, const markdown_style_t& style,
                  bool selectable)
    {
        bool hovered = false;
        bool held    = false;
        ImGui::ButtonBehavior(bb, item, &hovered, &held,
                              ImGuiButtonFlags_MouseButtonLeft |
                                  ImGuiButtonFlags_MouseButtonRight |
                                  ImGuiButtonFlags_PressedOnClick);
        const ImGuiIO& io = ImGui::GetIO();
        const ImVec2   point(io.MousePos.x - bb.Min.x, io.MousePos.y - bb.Min.y);
        const int32_t  link  = hovered ? Linkoffset_pos(point) : -1;
        const bool     opens = link_opens(style, document, link);
        if(hovered)
        {
            ImGui::SetMouseCursor(opens ? ImGuiMouseCursor_Hand
                                        : (selectable ? ImGuiMouseCursor_TextInput
                                                      : ImGuiMouseCursor_Arrow));
            if(opens && !dragging)
            {
                ImGui::SetTooltip("%s",
                                  document.links[static_cast<size_t>(link)].c_str());
            }
        }
        if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            const uint32_t at     = HitTest(point);
            const int32_t  clicks = ImGui::GetMouseClickedCount(ImGuiMouseButton_Left);
            dragging              = false;
            pressed_link          = clicks == 1 ? link : -1;
            if(selectable)
            {
                MarkdownView::s_active_view = id;
                if(clicks % CLICKS_PER_LINE == 0)
                {
                    SelectLine(at);
                }
                else if(clicks % CLICKS_PER_WORD == 0)
                {
                    SelectWord(at);
                }
                else
                {
                    if(!io.KeyShift)
                    {
                        anchor = at;
                    }
                    caret = at;
                }
            }
            dragging = clicks == 1;
        }
        if(selectable && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            MarkdownView::s_active_view = id;
        }
        if(dragging && held && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            pressed_link = -1;
            if(selectable)
            {
                caret = HitTest(point);
            }
        }
        if(dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if(pressed_link >= 0 && pressed_link == link && anchor == caret && opens)
            {
                style.open_link(document.links[static_cast<size_t>(pressed_link)], true);
            }
            dragging     = false;
            pressed_link = -1;
        }

        bool copied = false;
        if(selectable && MarkdownView::s_active_view == id)
        {
            if(ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A))
            {
                anchor = 0;
                caret  = static_cast<uint32_t>(document.text.size());
            }
            if(ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C) && anchor != caret)
            {
                ImGui::SetClipboardText(
                    document.text.substr(Begin(), End() - Begin()).c_str());
                copied = true;
            }
        }
        return copied;
    }

    void Draw(ImDrawList* draw, const ImVec2& origin, const palette_t& palette,
              const markdown_style_t& style) const
    {
        const char* text = document.text.data();
        for(const shape_t& shape : layout.shapes)
        {
            if(is_background(shape.paint))
            {
                draw->AddRectFilled(offset_pos(origin, shape.min.x, shape.min.y),
                                    offset_pos(origin, shape.max.x, shape.max.y),
                                    paint_color(palette, shape.paint), BOX_ROUNDING);
            }
        }
        for(const fragment_t& fragment : layout.fragments)
        {
            if((fragment.flags & kCode) != 0 && fragment.role != Role::kCodeBlock)
            {
                const float pad = fragment.size * INLINE_CODE_PADDING;
                draw->AddRectFilled(offset_pos(origin, fragment.x - pad, fragment.top),
                                    offset_pos(origin, fragment.x + fragment.width + pad,
                                               fragment.bottom),
                                    palette.code_background, BOX_ROUNDING);
            }
        }
        if(anchor != caret)
        {
            for(const fragment_t& fragment : layout.fragments)
            {
                const uint32_t begin = std::max(fragment.begin, Begin());
                const uint32_t end   = std::min(fragment.end, End());
                if(begin < end)
                {
                    const char* first = text + fragment.begin;
                    const float x0    = fragment.font
                                         ->CalcTextSizeA(fragment.size, FLT_MAX, 0.0f,
                                                         first, text + begin)
                                         .x;
                    const float x1 = fragment.font
                                         ->CalcTextSizeA(fragment.size, FLT_MAX, 0.0f,
                                                         first, text + end)
                                         .x;
                    draw->AddRectFilled(
                        offset_pos(origin, fragment.x + x0, fragment.top),
                        offset_pos(origin, fragment.x + x1, fragment.bottom),
                        palette.selection);
                }
            }
        }
        for(const fragment_t& fragment : layout.fragments)
        {
            const bool   linked   = link_opens(style, document, fragment.link);
            const ImU32  color    = fragment_color(palette, fragment, linked);
            const ImVec2 position = offset_pos(origin, fragment.x, fragment.y);
            const char*  begin    = text + fragment.begin;
            const char*  end      = text + fragment.end;
            const int    first    = draw->VtxBuffer.Size;
            draw->AddText(fragment.font, fragment.size, position, color, begin, end);
            if((fragment.flags & kFauxBold) != 0)
            {
                draw->AddText(fragment.font, fragment.size,
                              ImVec2(position.x + FAUX_BOLD_OFFSET, position.y), color,
                              begin, end);
            }
            if((fragment.flags & kFauxItalic) != 0)
            {
                const float baseline = position.y + fragment.ascent;
                for(int vertex = first; vertex < draw->VtxBuffer.Size; ++vertex)
                {
                    ImVec2& point = draw->VtxBuffer[vertex].pos;
                    point.x += (baseline - point.y) * ITALIC_SHEAR;
                }
            }
            if((fragment.flags & kStrike) != 0)
            {
                const float y = position.y + fragment.ascent * STRIKE_POSITION;
                draw->AddLine(ImVec2(position.x, y),
                              ImVec2(position.x + fragment.width, y), color,
                              LINE_THICKNESS);
            }
            if(linked)
            {
                const float y = position.y + fragment.ascent + LINE_THICKNESS;
                draw->AddLine(ImVec2(position.x, y),
                              ImVec2(position.x + fragment.width, y), color,
                              LINE_THICKNESS);
            }
        }
        for(const shape_t& shape : layout.shapes)
        {
            const ImVec2 min = offset_pos(origin, shape.min.x, shape.min.y);
            const ImVec2 max = offset_pos(origin, shape.max.x, shape.max.y);
            const ImVec2 center((min.x + max.x) * HALF, (min.y + max.y) * HALF);
            const ImU32  color = paint_color(palette, shape.paint);
            if(shape.kind == ShapeKind::kRect && !is_background(shape.paint))
            {
                draw->AddRectFilled(min, max, color);
            }
            else if(shape.kind == ShapeKind::kDisc)
            {
                draw->AddCircleFilled(center, (max.x - min.x) * HALF, color);
            }
            else if(shape.kind == ShapeKind::kCircle)
            {
                draw->AddCircle(center, (max.x - min.x) * HALF, color, 0, LINE_THICKNESS);
            }
            else if(shape.kind == ShapeKind::kBox || shape.kind == ShapeKind::kCheckedBox)
            {
                draw->AddRect(min, max, color, BOX_ROUNDING, 0, LINE_THICKNESS);
            }
            if(shape.kind == ShapeKind::kCheckedBox)
            {
                ImVec2 points[std::size(CHECK_MARK)];
                for(size_t i = 0; i < std::size(CHECK_MARK); ++i)
                {
                    points[i] = ImVec2(min.x + (max.x - min.x) * CHECK_MARK[i].x,
                                       min.y + (max.y - min.y) * CHECK_MARK[i].y);
                }
                draw->AddPolyline(points, static_cast<int>(std::size(CHECK_MARK)), color,
                                  ImDrawFlags_None, LINE_THICKNESS * TWICE);
            }
        }
    }
};

MarkdownView::MarkdownView()
: m_impl(std::make_unique<Impl>())
{
    m_impl->id = ++s_view_count;
}

MarkdownView::~MarkdownView() = default;

void
MarkdownView::SetText(std::string_view text, uint32_t flags)
{
    Impl& state = *m_impl;
    if(!state.parsed || state.flags != flags || state.source != text)
    {
        state.source.assign(text.data(), text.size());
        state.flags    = flags;
        state.document = document_t();
        if((flags & MARKDOWN_PLAIN) != 0)
        {
            build_plain(state.document, text);
        }
        else if(!Parser(state.document).Parse(text))
        {
            state.document = document_t();
            build_plain(state.document, text);
        }
        state.parsed   = true;
        state.laid_out = false;
        state.anchor   = 0;
        state.caret    = 0;
        state.dragging = false;
    }
}

bool
MarkdownView::Render(const markdown_style_t& style)
{
    Impl&        state  = *m_impl;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    bool         copied = false;
    if(!window->SkipItems)
    {
        const fonts_t fonts  = resolve_fonts(style);
        const ImVec2  origin = window->DC.CursorPos;
        const float   width  = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
        if(!state.laid_out || state.layout_width != width ||
           !same_fonts(state.layout_fonts, fonts))
        {
            state.layout = layout_t();
            Layouter(state.document, fonts, width, state.layout).Run();
            state.layout_fonts = fonts;
            state.layout_width = width;
            state.laid_out     = true;
        }

        const ImRect bb(origin, ImVec2(origin.x + width, origin.y + state.layout.height));
        const ImGuiID item = window->GetID(m_impl.get());
        ImGui::ItemSize(bb);
        if(ImGui::ItemAdd(bb, item))
        {
            const bool selectable = (state.flags & MARKDOWN_SELECTABLE) != 0 &&
                                    MarkdownView::s_selection_enabled;
            if(selectable || !state.document.links.empty())
            {
                copied = state.Interact(bb, item, style, selectable);
            }
            if((!selectable || MarkdownView::s_active_view != state.id) &&
               !state.dragging)
            {
                state.anchor = state.caret;
            }
            state.Draw(window->DrawList, origin, resolve_palette(style), style);
        }
    }
    return copied;
}

bool
MarkdownView::HasSelection() const
{
    return m_impl->anchor != m_impl->caret;
}

std::string
MarkdownView::GetSelectedText() const
{
    return m_impl->document.text.substr(m_impl->Begin(), m_impl->End() - m_impl->Begin());
}

const std::string&
MarkdownView::GetPlainText() const
{
    return m_impl->document.text;
}

void
MarkdownView::SelectAll()
{
    m_impl->anchor              = 0;
    m_impl->caret               = static_cast<uint32_t>(m_impl->document.text.size());
    MarkdownView::s_active_view = m_impl->id;
}

void
MarkdownView::ClearSelection()
{
    m_impl->anchor = m_impl->caret;
}

void
MarkdownView::Copy() const
{
    if(HasSelection())
    {
        ImGui::SetClipboardText(GetSelectedText().c_str());
    }
}

bool
render_markdown(const char* id, std::string_view text, uint32_t flags,
                const markdown_style_t& style)
{
    std::unordered_map<ImGuiID, cached_view_t>& cache = view_cache();
    const int                                   frame = ImGui::GetFrameCount();
    if(g_cache_frame != frame)
    {
        // Views not drawn last frame are gone from the screen.
        g_cache_frame = frame;
        for(std::unordered_map<ImGuiID, cached_view_t>::iterator it = cache.begin();
            it != cache.end();)
        {
            it = it->second.frame < frame - 1 ? cache.erase(it) : std::next(it);
        }
    }
    cached_view_t& entry = cache[ImGui::GetID(id)];
    if(entry.view == nullptr)
    {
        entry.view = std::make_unique<MarkdownView>();
    }
    entry.frame = frame;
    entry.view->SetText(text, flags);
    return entry.view->Render(style);
}

void
MarkdownView::SetSelectionEnabled(bool enabled)
{
    s_selection_enabled = enabled;
}

bool
MarkdownView::IsSelectionEnabled()
{
    return s_selection_enabled;
}

}  // namespace View
}  // namespace RocProfVis
