// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "immarkdown.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <iterator>
#include <unordered_map>
#include <vector>

#include "imgui_internal.h"

namespace ImMarkdown
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
constexpr int           MAX_INLINE_DEPTH = 16;
constexpr unsigned char FIRST_NON_ASCII  = 0x80;
constexpr int           DECIMAL_BASE     = 10;

constexpr size_t      NPOS        = std::string_view::npos;
constexpr const char* BULLET_TEXT = "\xE2\x80\xA2 ";  // U+2022 and a space

static_assert(sizeof(Style::heading_scale) / sizeof(Style::heading_scale[0]) ==
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

// A run of one style, as a range of Document::text.
struct Span
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

enum class Marker : uint8_t
{
    kNone,
    kBullet,
    kNumber,
    kTask,
    kTaskDone
};

struct Cell
{
    std::vector<Span> spans;
};

struct Block
{
    BlockKind kind   = BlockKind::kParagraph;
    uint8_t   level  = 0;  // heading level, 1 to 6
    uint8_t   quote  = 0;  // block quote depth
    uint8_t   list   = 0;  // list nesting depth
    Marker    marker = Marker::kNone;
    // The marker as it copies: "• ", "3. ", "[x] ".
    Span marker_span;
    // A list block straight after another, with no blank line between.
    bool              tight = false;
    std::vector<Span> spans;
    // Tables: the header row first, and each column's alignment, -1 left,
    // 0 center, 1 right.
    std::vector<std::vector<Cell>> rows;
    std::vector<int>               align;
};

struct Document
{
    // What selection copies: the text as drawn, blocks on their own lines and
    // table cells split by tabs.
    std::string              text;
    std::vector<Block>       blocks;
    std::vector<std::string> links;
};

// ----------------------------------------------------------------- helpers

bool
IsBlank(char c)
{
    return c == ' ' || c == '\t';
}

// The ends of the text count as whitespace.
bool
IsSpace(char c)
{
    return c == '\0' || c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool
IsPunct(char c)
{
    return std::ispunct(static_cast<unsigned char>(c)) != 0;
}

bool
IsWordChar(char c)
{
    const unsigned char byte = static_cast<unsigned char>(c);
    return std::isalnum(byte) != 0 || c == '_' || byte >= FIRST_NON_ASCII;
}

bool
StartsWith(std::string_view text, std::string_view prefix)
{
    return text.substr(0, prefix.size()) == prefix;
}

size_t
RunLength(std::string_view text, size_t at)
{
    size_t length = 0;
    while(at + length < text.size() && text[at + length] == text[at])
    {
        ++length;
    }
    return length;
}

size_t
LeadingSpaces(std::string_view text)
{
    return std::min(text.find_first_not_of(' '), text.size());
}

std::string_view
Trim(std::string_view text)
{
    const size_t first = std::min(text.find_first_not_of(" \t"), text.size());
    const size_t last  = text.find_last_not_of(" \t");
    return last == NPOS ? std::string_view() : text.substr(first, last + 1 - first);
}

ImVec2
At(const ImVec2& origin, float x, float y)
{
    return ImVec2(origin.x + x, origin.y + y);
}

// Strips up to max_depth block quote markers and reports how many there were.
std::string_view
StripQuotes(std::string_view line, uint8_t max_depth, uint8_t& depth)
{
    depth        = 0;
    size_t first = line.find_first_not_of(' ');
    while(depth < max_depth && first != NPOS && line[first] == '>')
    {
        ++depth;
        line.remove_prefix(first + 1);
        if(!line.empty() && line[0] == ' ')
        {
            line.remove_prefix(1);
        }
        first = line.find_first_not_of(' ');
    }
    return line;
}

// Three or more of one of - * _, with nothing else but spaces.
bool
IsRule(std::string_view text)
{
    const std::string_view trimmed   = Trim(text);
    const char             marker    = trimmed.empty() ? '\0' : trimmed[0];
    const char             allowed[] = { marker, ' ', '\0' };
    return (marker == '-' || marker == '*' || marker == '_') &&
           trimmed.find_first_not_of(allowed) == NPOS &&
           static_cast<size_t>(std::count(trimmed.begin(), trimmed.end(), marker)) >=
               MIN_RULE_LENGTH;
}

// The heading level an underline of = or - gives the paragraph above, or 0.
size_t
SetextLevel(std::string_view text)
{
    const std::string_view trimmed = Trim(text);
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
ListMarker(std::string_view text, bool& ordered, int& number)
{
    size_t digits = 0;
    number        = 0;
    while(digits < text.size() && digits <= MAX_ORDERED_DIGITS &&
          std::isdigit(static_cast<unsigned char>(text[digits])) != 0)
    {
        number = number * DECIMAL_BASE + (text[digits] - '0');
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
    const bool spaced = marker > 0 && (marker == text.size() || IsBlank(text[marker]));
    return spaced && !IsRule(text) ? marker : 0;
}

std::string_view
HeadingText(std::string_view text)
{
    text = Trim(text);
    // A closing run of # counts only after a space.
    const size_t last = text.find_last_not_of('#');
    if(last == NPOS)
    {
        text = std::string_view();
    }
    else if(last + 1 < text.size() && IsBlank(text[last]))
    {
        text = Trim(text.substr(0, last));
    }
    return text;
}

std::vector<std::string_view>
SplitRow(std::string_view row)
{
    row = Trim(row);
    if(!row.empty() && row.front() == '|')
    {
        row.remove_prefix(1);
    }
    if(!row.empty() && row.back() == '|' && (row.size() < 2 || row[row.size() - 2] != '\\'))
    {
        row.remove_suffix(1);
    }
    std::vector<std::string_view> cells;
    size_t                        start = 0;
    for(size_t at = 0; at <= row.size(); ++at)
    {
        if(at == row.size() || (row[at] == '|' && (at == 0 || row[at - 1] != '\\')))
        {
            cells.push_back(Trim(row.substr(start, at - start)));
            start = at + 1;
        }
    }
    return cells;
}

// A table's delimiter row, such as |---|:-:|--:|, and its alignments.
bool
DelimiterRow(std::string_view row, std::vector<int>& align)
{
    align.clear();
    bool valid = row.find_first_of("|-") != NPOS;
    for(std::string_view cell : SplitRow(row))
    {
        const bool left  = !cell.empty() && cell.front() == ':';
        const bool right = cell.size() > 1 && cell.back() == ':';
        const std::string_view dashes =
            cell.substr(left ? 1 : 0, cell.size() - (left ? 1 : 0) - (right ? 1 : 0));
        valid = valid && !dashes.empty() && dashes.find_first_not_of('-') == NPOS;
        if(left && right)
        {
            align.push_back(0);
        }
        else
        {
            align.push_back(right ? 1 : -1);
        }
    }
    return valid;
}

struct Flanking
{
    bool open  = false;
    bool close = false;
};

// CommonMark's rule for a run of * _ or ~: it opens emphasis when the text it
// wraps starts right after it, and closes when that text ends right before
// it, which keeps `a * b` and `float*, int*` as written. An underscore also
// refuses to open or close inside a word, so snake_case stays whole.
Flanking
Flank(std::string_view text, size_t at, size_t length)
{
    const char before = at > 0 ? text[at - 1] : '\0';
    const char after  = at + length < text.size() ? text[at + length] : '\0';
    const bool left =
        !IsSpace(after) && (!IsPunct(after) || IsSpace(before) || IsPunct(before));
    const bool right =
        !IsSpace(before) && (!IsPunct(before) || IsSpace(after) || IsPunct(after));
    Flanking result;
    result.open  = left;
    result.close = right;
    if(text[at] == '_')
    {
        result.open  = left && (!right || IsPunct(before));
        result.close = right && (!left || IsPunct(after));
    }
    return result;
}

// Where a run of exactly `ticks` backticks starts, at or after `from`.
size_t
FindTicks(std::string_view text, size_t from, size_t ticks)
{
    size_t at = text.find('`', from);
    while(at != NPOS && RunLength(text, at) != ticks)
    {
        at = text.find('`', at + RunLength(text, at));
    }
    return at;
}

// Past the code span starting at `at`, or past its backticks if it never
// closes.
size_t
SkipCodeSpan(std::string_view text, size_t at)
{
    const size_t ticks = RunLength(text, at);
    const size_t close = FindTicks(text, at + ticks, ticks);
    return close == NPOS ? at + ticks : close + ticks;
}

size_t
EmphasisWidth(char marker, size_t run)
{
    return marker == '~' ? run : std::min(run, MAX_EMPHASIS_RUN);
}

// Where the run closing `width` markers starts, scanning from `from`. Pairs
// nested inside are skipped whole, so the nearest opener gets the nearest
// closer.
size_t
FindCloser(std::string_view text, size_t from, char marker, size_t width, int depth)
{
    size_t at    = from;
    size_t close = NPOS;
    while(close == NPOS && at < text.size())
    {
        if(text[at] == '\\')
        {
            at += 2;
        }
        else if(text[at] == '`')
        {
            at = SkipCodeSpan(text, at);
        }
        else if(text[at] != marker)
        {
            ++at;
        }
        else
        {
            const size_t   run  = RunLength(text, at);
            const Flanking side = Flank(text, at, run);
            size_t         next = at + run;
            if(side.close && run >= width && (marker != '~' || run == width))
            {
                close = at;
            }
            else if(side.open && depth < MAX_INLINE_DEPTH)
            {
                const size_t nested_width = EmphasisWidth(marker, run);
                const size_t nested =
                    FindCloser(text, at + run, marker, nested_width, depth + 1);
                if(nested != NPOS)
                {
                    next = nested + nested_width;
                }
            }
            at = next;
        }
    }
    return close;
}

// The ']' closing the '[' at `open`, past nested brackets, escapes, and code.
size_t
FindBracket(std::string_view text, size_t open)
{
    int    level = 0;
    size_t at    = open;
    size_t close = NPOS;
    while(close == NPOS && at < text.size())
    {
        if(text[at] == '\\')
        {
            at += 2;
        }
        else if(text[at] == '`')
        {
            at = SkipCodeSpan(text, at);
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

struct Destination
{
    size_t begin = 0;
    size_t end   = 0;
    // Just past the closing parenthesis.
    size_t after = 0;
};

size_t
SkipLinkSpace(std::string_view text, size_t at)
{
    return std::min(text.find_first_not_of(" \t\n", at), text.size());
}

// The (destination "title") after a link label, starting at `at`.
bool
ParseDestination(std::string_view text, size_t at, Destination& out)
{
    bool valid = at < text.size() && text[at] == '(';
    size_t i   = valid ? SkipLinkSpace(text, at + 1) : text.size();
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
        out.begin  = i;
        int parens = 0;
        while(i < text.size() && !IsSpace(text[i]) && (text[i] != ')' || parens > 0))
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
    i = SkipLinkSpace(text, i);
    if(valid && i < text.size() && (text[i] == '"' || text[i] == '\''))
    {
        const size_t close = text.find(text[i], i + 1);
        valid              = close != NPOS;
        i                  = valid ? SkipLinkSpace(text, close + 1) : text.size();
    }
    valid     = valid && i < text.size() && text[i] == ')';
    out.after = i + 1;
    return valid;
}

// The length of a bare http://, https://, or www. URL at `at`, or 0. Trailing
// punctuation and an unmatched ')' belong to the sentence around it.
size_t
BareUrlLength(std::string_view text, size_t at)
{
    const std::string_view rest   = text.substr(at);
    const bool             starts = StartsWith(rest, "https://") ||
                        StartsWith(rest, "http://") || StartsWith(rest, "www.");
    const bool bounded = at == 0 || IsSpace(text[at - 1]) || text[at - 1] == '(' ||
                         text[at - 1] == '*' || text[at - 1] == '_';
    size_t end = 0;
    if(starts && bounded)
    {
        while(end < rest.size() && !IsSpace(rest[end]) && rest[end] != '<')
        {
            ++end;
        }
        size_t opened = static_cast<size_t>(std::count(rest.begin(), rest.begin() + end, '('));
        size_t closed = static_cast<size_t>(std::count(rest.begin(), rest.begin() + end, ')'));
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
IsAutolink(std::string_view inner)
{
    const bool spaced = inner.find_first_of(" \t\n<") != NPOS;
    const bool scheme = StartsWith(inner, "http://") || StartsWith(inner, "https://") ||
                        StartsWith(inner, "mailto:");
    const bool email = inner.find('@') != NPOS && inner.find('.') != NPOS;
    return !inner.empty() && !spaced && (scheme || email);
}

bool
IsBreakTag(std::string_view inner)
{
    std::string lower(Trim(inner));
    for(char& c : lower)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower == "br" || lower == "br/" || lower == "br /";
}

struct Entity
{
    std::string_view name;
    std::string_view text;
};

constexpr Entity ENTITIES[] = {
    { "&amp;", "&" },   { "&lt;", "<" },   { "&gt;", ">" },   { "&quot;", "\"" },
    { "&#39;", "'" },   { "&apos;", "'" }, { "&nbsp;", " " },
};

// ------------------------------------------------------------------ parser

class Parser
{
public:
    explicit Parser(Document& document)
    : m_doc(document)
    {}

    void Parse(std::string_view source)
    {
        size_t start = 0;
        while(start <= source.size())
        {
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
    }

private:
    struct ListLevel
    {
        size_t marker_indent  = 0;
        size_t content_indent = 0;
        bool   ordered        = false;
        int    number         = 0;
    };

    void Line(std::string_view line)
    {
        if(m_fence_length > 0 && !CodeLine(line))
        {
            return;
        }

        uint8_t          quote   = 0;
        std::string_view rest    = StripQuotes(line, MAX_NESTING, quote);
        const size_t     indent  = LeadingSpaces(rest);
        std::string_view content = rest.substr(indent);
        if(Trim(content).empty())
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

        const bool in_item = m_lists.empty() || indent >= m_lists.back().content_indent;
        if(m_open && in_item && SetextLevel(content) > 0)
        {
            Block& heading = Open(BlockKind::kHeading);
            heading.level  = static_cast<uint8_t>(SetextLevel(content));
            Inline(heading.spans, m_paragraph, 0, -1, 0);
            m_paragraph.clear();
            m_open  = false;
            m_blank = false;
            return;
        }

        size_t content_indent = indent;
        if(!ListItem(content, indent, content_indent))
        {
            if(m_open && !StartsBlock(content))
            {
                Continue(content);
                m_blank = false;
                return;
            }
            while(!m_lists.empty() && indent < m_lists.back().content_indent)
            {
                m_lists.pop_back();
            }
        }
        Content(content, content_indent);
        m_blank = false;
    }

    // Returns true when the line closed the fence and still needs reading.
    bool CodeLine(std::string_view line)
    {
        uint8_t          quote = 0;
        std::string_view rest  = StripQuotes(line, m_fence_quote, quote);
        bool             done  = quote < m_fence_quote;
        rest.remove_prefix(std::min(LeadingSpaces(rest), m_fence_indent));
        const std::string_view trimmed = Trim(rest);
        const size_t           run =
            !trimmed.empty() && trimmed[0] == m_fence ? RunLength(trimmed, 0) : 0;
        if(!done && run >= m_fence_length && run == trimmed.size())
        {
            m_fence_length = 0;
        }
        else if(!done)
        {
            Block& code = m_doc.blocks.back();
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
    bool ListItem(std::string_view& content, size_t indent, size_t& content_indent)
    {
        bool         ordered = false;
        int          number  = 0;
        const size_t marker  = ListMarker(content, ordered, number);
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
        const size_t spaces = LeadingSpaces(content.substr(marker));
        ListLevel    level;
        level.marker_indent = indent;
        level.content_indent =
            indent + marker + std::max<size_t>(std::min(spaces, MAX_LIST_SPACES), 1);
        level.ordered = ordered;
        level.number  = number;
        // A list counts on from its first number, whatever the next ones say.
        if(ordered && has_sibling && sibling.ordered && sibling.marker_indent == indent)
        {
            level.number = sibling.number + 1;
        }
        if(m_lists.size() < MAX_NESTING)
        {
            m_lists.push_back(level);
        }
        content        = content.substr(std::min(marker + spaces, content.size()));
        content_indent = level.content_indent;
        m_marker       = ordered ? Marker::kNumber : Marker::kBullet;
        m_number       = level.number;

        const bool task = content.size() >= TASK_MARKER_LENGTH && content[0] == '[' &&
                          content[2] == ']' &&
                          (content[1] == ' ' || content[1] == 'x' || content[1] == 'X') &&
                          (content.size() == TASK_MARKER_LENGTH ||
                           IsBlank(content[TASK_MARKER_LENGTH]));
        if(task)
        {
            m_marker = content[1] == ' ' ? Marker::kTask : Marker::kTaskDone;
            content  = Trim(content.substr(TASK_MARKER_LENGTH));
        }
        return true;
    }

    // Whether a line, quote markers and indent gone, interrupts a paragraph.
    bool StartsBlock(std::string_view content) const
    {
        bool         ordered = false;
        int          number  = 0;
        const size_t hashes  = !content.empty() && content[0] == '#' ? RunLength(content, 0) : 0;
        const bool   heading = hashes > 0 && hashes <= MAX_HEADING_LEVEL &&
                             (hashes == content.size() || IsBlank(content[hashes]));
        const bool fence = !content.empty() && (content[0] == '`' || content[0] == '~') &&
                           RunLength(content, 0) >= MIN_FENCE_LENGTH;
        std::vector<int> align;
        return heading || fence || IsRule(content) ||
               ListMarker(content, ordered, number) > 0 || TableStarts(content, align);
    }

    // Whether the open paragraph is a table header and this its delimiter row.
    bool TableStarts(std::string_view content, std::vector<int>& align) const
    {
        return m_open && m_paragraph.find('\n') == std::string::npos &&
               m_paragraph.find('|') != std::string::npos && DelimiterRow(content, align) &&
               align.size() == SplitRow(m_paragraph).size();
    }

    void Content(std::string_view content, size_t indent)
    {
        const char       first  = content.empty() ? '\0' : content[0];
        const size_t     fence  = (first == '`' || first == '~') ? RunLength(content, 0) : 0;
        const size_t     hashes = first == '#' ? RunLength(content, 0) : 0;
        std::vector<int> align;
        if(fence >= MIN_FENCE_LENGTH && (first == '~' || content.find('`', fence) == NPOS))
        {
            Flush();
            Open(BlockKind::kCode);
            m_fence        = first;
            m_fence_length = fence;
            m_fence_indent = indent;
            m_fence_quote  = m_quote;
            m_code_lines   = 0;
        }
        else if(IsRule(content))
        {
            Flush();
            Open(BlockKind::kRule);
        }
        else if(hashes > 0 && hashes <= MAX_HEADING_LEVEL &&
                (hashes == content.size() || IsBlank(content[hashes])))
        {
            Flush();
            Block& heading = Open(BlockKind::kHeading);
            heading.level  = static_cast<uint8_t>(hashes);
            Inline(heading.spans, HeadingText(content.substr(hashes)), 0, -1, 0);
        }
        else if(TableStarts(content, align))
        {
            const std::string header = m_paragraph;
            m_paragraph.clear();
            m_open       = false;
            Block& table = Open(BlockKind::kTable);
            table.align  = align;
            m_table      = static_cast<int32_t>(m_doc.blocks.size() - 1);
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
            Block& block = Open(BlockKind::kParagraph);
            Inline(block.spans, m_paragraph, 0, -1, 0);
            m_paragraph.clear();
            m_open = false;
        }
    }

    Block& Open(BlockKind kind)
    {
        const bool blank        = m_open ? m_paragraph_blank : m_blank;
        const bool follows_item = !m_doc.blocks.empty() && m_doc.blocks.back().list > 0;
        if(!m_doc.text.empty())
        {
            m_doc.text += '\n';
        }
        Block& block = m_doc.blocks.emplace_back();
        block.kind   = kind;
        block.quote  = m_quote;
        block.list   = static_cast<uint8_t>(m_lists.size());
        block.tight  = !blank && follows_item && block.list > 0;
        block.marker = m_marker;
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

    void AddRow(Block& table, std::string_view row)
    {
        std::vector<std::string_view> cells = SplitRow(row);
        cells.resize(table.align.size());
        if(!table.rows.empty())
        {
            m_doc.text += '\n';
        }
        std::vector<Cell>& out = table.rows.emplace_back();
        out.resize(cells.size());
        for(size_t column = 0; column < cells.size(); ++column)
        {
            if(column > 0)
            {
                m_doc.text += '\t';
            }
            Inline(out[column].spans, cells[column], 0, -1, 0);
        }
    }

    void Append(std::vector<Span>& spans, std::string_view text, uint32_t flags, int32_t link)
    {
        if(text.empty())
        {
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
            Span span;
            span.begin = begin;
            span.end   = end;
            span.flags = flags;
            span.link  = link;
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
    void AppendCode(std::vector<Span>& spans, std::string_view code, uint32_t flags,
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

    void Inline(std::vector<Span>& spans, std::string_view s, uint32_t flags, int32_t link,
                int depth)
    {
        size_t plain = 0;
        size_t i     = 0;
        while(i < s.size())
        {
            const char   c        = s[i];
            const char   next     = i + 1 < s.size() ? s[i + 1] : '\0';
            const size_t consumed = Construct(spans, s, i, flags, link, depth, plain);
            if(consumed > 0)
            {
                i = consumed;
            }
            else if(c == '\\' && IsPunct(next))
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
    size_t Construct(std::vector<Span>& spans, std::string_view s, size_t at,
                     uint32_t flags, int32_t link, int depth, size_t& plain)
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
            resume += LeadingSpaces(s.substr(resume));
            took = true;
        }
        else if(c == '`')
        {
            // An unmatched run stays text, but whole, so its backticks never
            // pair up among themselves.
            const size_t ticks = RunLength(s, at);
            const size_t close = FindTicks(s, at + ticks, ticks);
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
        else if((c == '[' || (c == '!' && next == '[')) && link < 0 && depth < MAX_INLINE_DEPTH)
        {
            const size_t label = c == '!' ? at + 1 : at;
            const size_t close = FindBracket(s, label);
            Destination  target;
            if(close != NPOS && ParseDestination(s, close + 1, target))
            {
                Append(spans, s.substr(plain, at - plain), flags, link);
                const std::string_view url = s.substr(target.begin, target.end - target.begin);
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
            if(close != NPOS && IsBreakTag(inner))
            {
                Append(spans, s.substr(plain, at - plain), flags, link);
                Append(spans, "\n", flags, link);
                resume = close + 1;
                took   = true;
            }
            else if(close != NPOS && link < 0 && IsAutolink(inner))
            {
                Append(spans, s.substr(plain, at - plain), flags, link);
                const bool email = inner.find(':') == NPOS;
                Append(spans, inner, flags,
                       AddLink(email ? "mailto:" + std::string(inner) : std::string(inner)));
                resume = close + 1;
                took   = true;
            }
        }
        else if(c == '&')
        {
            for(const Entity& entity : ENTITIES)
            {
                if(!took && StartsWith(s.substr(at), entity.name))
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
            const size_t length = BareUrlLength(s, at);
            if(length > 0)
            {
                Append(spans, s.substr(plain, at - plain), flags, link);
                const std::string_view url = s.substr(at, length);
                const std::string target =
                    StartsWith(url, "www.") ? "http://" + std::string(url) : std::string(url);
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

    size_t Emphasis(std::vector<Span>& spans, std::string_view s, size_t at, uint32_t flags,
                    int32_t link, int depth, size_t plain, bool& took)
    {
        const char   marker = s[at];
        const size_t run    = RunLength(s, at);
        const size_t width  = EmphasisWidth(marker, run);
        // The run's last `width` markers open; any before them are text.
        const size_t open  = at + run - width;
        size_t       close = NPOS;
        if((marker != '~' || run == 2) && depth < MAX_INLINE_DEPTH && Flank(s, at, run).open)
        {
            close = FindCloser(s, at + run, marker, width, 0);
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
            Inline(spans, s.substr(open + width, close - open - width), flags | added, link,
                   depth + 1);
            resume = close + width;
        }
        return resume;
    }

    Document&              m_doc;
    std::string            m_paragraph;
    bool                   m_open            = false;  // m_paragraph is a paragraph
    bool                   m_paragraph_blank = false;  // a blank line came before it
    bool                   m_blank           = false;  // the last line was blank
    uint8_t                m_quote           = 0;
    std::vector<ListLevel> m_lists;
    // Owed to the next block opened.
    Marker  m_marker = Marker::kNone;
    int     m_number = 0;
    int32_t m_table  = -1;  // the open table, as a block index
    char    m_fence  = '\0';
    size_t  m_fence_length = 0;
    size_t  m_fence_indent = 0;
    uint8_t m_fence_quote  = 0;
    size_t  m_code_lines   = 0;
};

void
BuildPlain(Document& document, std::string_view text)
{
    document.text.assign(text.data(), text.size());
    Block& block = document.blocks.emplace_back();
    if(!document.text.empty())
    {
        Span span;
        span.end = static_cast<uint32_t>(document.text.size());
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

struct Fragment
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
    uint32_t begin  = 0;  // the range of Document::text drawn
    uint32_t end    = 0;
    uint32_t flags  = 0;
    int32_t  link   = -1;
    Role     role   = Role::kText;
};

struct Shape
{
    ShapeKind kind  = ShapeKind::kRect;
    Paint     paint = Paint::kLine;
    ImVec2    min;
    ImVec2    max;
};

struct Layout
{
    std::vector<Fragment> fragments;
    std::vector<Shape>    shapes;
    float                 height = 0.0f;
};

struct Fonts
{
    ImFont* regular = nullptr;
    ImFont* bold    = nullptr;
    ImFont* italic  = nullptr;
    ImFont* code    = nullptr;
    float   size    = 0.0f;
    float   heading_scale[MAX_HEADING_LEVEL] = {};
};

bool
SameFonts(const Fonts& a, const Fonts& b)
{
    bool same = a.regular == b.regular && a.bold == b.bold && a.italic == b.italic &&
                a.code == b.code && a.size == b.size;
    for(size_t level = 0; level < MAX_HEADING_LEVEL; ++level)
    {
        same = same && a.heading_scale[level] == b.heading_scale[level];
    }
    return same;
}

Fonts
ResolveFonts(const Style& style)
{
    Fonts fonts;
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
    Layouter(const Document& document, const Fonts& fonts, float width, Layout& layout)
    : m_doc(document)
    , m_fonts(fonts)
    , m_width(width)
    , m_layout(layout)
    {}

    void Run()
    {
        float        y        = 0.0f;
        const Block* previous = nullptr;
        for(const Block& block : m_doc.blocks)
        {
            const float gap = previous != nullptr ? Em(Gap(block)) : 0.0f;
            y += gap;
            const float  x0    = Em(block.quote * QUOTE_INDENT + block.list * LIST_INDENT);
            const float  x1    = std::max(x0 + Em(MIN_COLUMN_WIDTH), m_width);
            const float  top   = y;
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
                AddShape(ShapeKind::kRect, Paint::kLine, ImVec2(bar, joined ? top - gap : top),
                         ImVec2(bar + Em(QUOTE_BAR_WIDTH), y));
            }
            previous = &block;
        }
        m_layout.height = y;
    }

private:
    // How a piece of text is drawn.
    struct Ink
    {
        ImFont*  font   = nullptr;
        float    ascent = 0.0f;
        uint32_t flags  = 0;
        int32_t  link   = -1;
        Role     role   = Role::kText;
    };

    // Spaces held back until the next word lands on the same line, so a line
    // never ends in them.
    struct Pending
    {
        Ink      ink;
        uint32_t begin = 0;
        uint32_t end   = 0;
        float    width = 0.0f;
    };

    float Em(float value) const { return value * m_fonts.size; }

    static float Gap(const Block& block)
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

    static Role TextRole(const Block& block)
    {
        return block.quote > 0 ? Role::kQuote : Role::kText;
    }

    float Body(const Block& block, float x0, float x1, float y)
    {
        float bottom = y;
        if(block.kind == BlockKind::kParagraph)
        {
            bottom = Inline(block.spans, x0, x1, y, m_fonts.size, TextRole(block), 0, -1);
        }
        else if(block.kind == BlockKind::kHeading)
        {
            const float size = m_fonts.size * m_fonts.heading_scale[block.level - 1];
            bottom           = Inline(block.spans, x0, x1, y, size, Role::kHeading, kBold, -1);
        }
        else if(block.kind == BlockKind::kCode)
        {
            const float pad = Em(CODE_PADDING);
            bottom = Inline(block.spans, x0 + pad, x1 - pad, y + pad, m_fonts.size,
                            Role::kCodeBlock, 0, -1) +
                     pad;
            AddShape(ShapeKind::kRect, Paint::kCodeBackground, ImVec2(x0, y),
                     ImVec2(x1, bottom));
        }
        else if(block.kind == BlockKind::kRule)
        {
            const float middle = y + Em(RULE_HEIGHT) * 0.5f;
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

    Ink InkFor(uint32_t flags, int32_t link, Role role, float size) const
    {
        const bool bold   = (flags & kBold) != 0;
        const bool italic = (flags & kItalic) != 0;
        Ink        ink;
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

    float Measure(const Ink& ink, uint32_t begin, uint32_t end) const
    {
        const char* text = m_doc.text.data();
        return ink.font->CalcTextSizeA(m_size, FLT_MAX, 0.0f, text + begin, text + end).x;
    }

    // Lays spans out from (x0, y), wrapping at x1, and returns the bottom.
    // align is -1 left, 0 center, 1 right.
    float Inline(const std::vector<Span>& spans, float x0, float x1, float y, float size,
                 Role role, uint32_t extra, int align)
    {
        m_x0         = x0;
        m_x1         = x1;
        m_x          = x0;
        m_y          = y;
        m_size       = size;
        m_align      = align;
        m_line_first = m_layout.fragments.size();
        m_line_empty = true;
        m_soft       = false;
        m_pending    = Pending();
        const char* text = m_doc.text.data();
        for(const Span& span : spans)
        {
            const Ink ink = InkFor(span.flags | extra, span.link, role, size);
            uint32_t  at  = span.begin;
            while(at < span.end)
            {
                uint32_t stop = at;
                while(stop < span.end && text[stop] != ' ' && text[stop] != '\n')
                {
                    ++stop;
                }
                if(text[at] == '\n')
                {
                    m_pending = Pending();
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
        m_pending = Pending();
        EndLine(false);
        return m_y;
    }

    void Spaces(const Ink& ink, uint32_t begin, uint32_t end)
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
    uint32_t Word(const Ink& ink, uint32_t begin, uint32_t end)
    {
        const float width = Measure(ink, begin, end);
        if(!m_line_empty && m_x + m_pending.width + width > m_x1)
        {
            m_pending = Pending();
            EndLine(true);
        }
        FlushPending();
        uint32_t stop = end;
        if(m_line_empty && width > m_x1 - m_x0)
        {
            // Wider than a whole line: cut it where the line runs out.
            const char* text = m_doc.text.data();
            const char* cut =
                ink.font->CalcWordWrapPosition(m_size, text + begin, text + end, m_x1 - m_x0);
            if(cut <= text + begin)
            {
                cut = text + begin + ImTextCountUtf8BytesFromChar(text + begin, text + end);
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
        m_pending = Pending();
    }

    void Piece(const Ink& ink, uint32_t begin, uint32_t end, float width)
    {
        Fragment* last = m_line_empty ? nullptr : &m_layout.fragments.back();
        if(last != nullptr && last->end == begin && last->font == ink.font &&
           last->flags == ink.flags && last->link == ink.link && last->role == ink.role)
        {
            last->end = end;
            last->width += width;
        }
        else if(begin < end)
        {
            Fragment& fragment = m_layout.fragments.emplace_back();
            fragment.x         = m_x;
            fragment.width     = width;
            fragment.ascent    = ink.ascent;
            fragment.size      = m_size;
            fragment.font      = ink.font;
            fragment.begin     = begin;
            fragment.end       = end;
            fragment.flags     = ink.flags;
            fragment.link      = ink.link;
            fragment.role      = ink.role;
            m_line_empty       = false;
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
        if(m_align >= 0)
        {
            const float room = std::max(m_x1 - m_x, 0.0f);
            shift            = m_align == 0 ? room * 0.5f : room;
        }
        for(size_t i = m_line_first; i < m_layout.fragments.size(); ++i)
        {
            Fragment& fragment = m_layout.fragments[i];
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
        Shape& shape = m_layout.shapes.emplace_back();
        shape.kind   = kind;
        shape.paint  = paint;
        shape.min    = min;
        shape.max    = max;
    }

    // The marker hangs in the indent, centered on the item's first line.
    void AddMarker(const Block& block, float x0, float top, size_t first)
    {
        const bool  has_lead = first < m_layout.fragments.size();
        const float line_top = has_lead ? m_layout.fragments[first].top : top;
        const float line_bottom =
            has_lead ? m_layout.fragments[first].bottom : top + m_fonts.size;
        const float middle = (line_top + line_bottom) * 0.5f;
        const float right  = x0 - Em(MARKER_GAP);
        const Paint paint  = block.quote > 0 ? Paint::kQuote : Paint::kText;
        if(block.marker == Marker::kBullet)
        {
            const float radius = Em(BULLET_RADIUS);
            AddShape(block.list % 2 == 1 ? ShapeKind::kDisc : ShapeKind::kCircle, paint,
                     ImVec2(right - 2.0f * radius, middle - radius),
                     ImVec2(right, middle + radius));
        }
        else if(block.marker == Marker::kNumber)
        {
            m_size              = m_fonts.size;
            const Ink      ink  = InkFor(0, -1, TextRole(block), m_fonts.size);
            const uint32_t end  = block.marker_span.end - 1;  // without its space
            const float    wide = Measure(ink, block.marker_span.begin, end);
            Fragment&      mark = m_layout.fragments.emplace_back();
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
            mark.y = has_lead ? m_layout.fragments[first].y + m_layout.fragments[first].ascent -
                                    ink.ascent
                              : line_top;
        }
        else
        {
            const float box = Em(TASK_BOX_SIZE);
            AddShape(block.marker == Marker::kTask ? ShapeKind::kBox : ShapeKind::kCheckedBox,
                     paint, ImVec2(right - box, middle - box * 0.5f),
                     ImVec2(right, middle + box * 0.5f));
        }
    }

    // A cell's width on one line.
    float Natural(const std::vector<Span>& spans, uint32_t extra)
    {
        float width = 0.0f;
        m_size      = m_fonts.size;
        for(const Span& span : spans)
        {
            width += Measure(InkFor(span.flags | extra, span.link, Role::kText, m_size),
                             span.begin, span.end);
        }
        return width;
    }

    float Table(const Block& table, float x0, float x1, float y)
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
                    Natural(table.rows[row][column].spans, row == 0 ? kBold : 0) + 2.0f * pad;
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
            for(size_t column = 0; column < columns; ++column)
            {
                const float cell_bottom =
                    Inline(table.rows[row][column].spans, x + pad, x + widths[column] - pad,
                           y + pad, m_fonts.size, TextRole(table), row == 0 ? kBold : 0,
                           table.align[column]);
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

    const Document& m_doc;
    const Fonts&    m_fonts;
    const float     m_width;
    Layout&         m_layout;

    // The line being built.
    float   m_x0         = 0.0f;
    float   m_x1         = 0.0f;
    float   m_x          = 0.0f;
    float   m_y          = 0.0f;
    float   m_size       = 0.0f;
    int     m_align      = -1;
    size_t  m_line_first = 0;
    bool    m_line_empty = true;
    bool    m_soft       = false;  // the line began where the one above wrapped
    Pending m_pending;
};

// ----------------------------------------------------------------- drawing

struct Palette
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
Pick(ImU32 color, ImU32 fallback)
{
    return color != 0 ? color : fallback;
}

Palette
ResolvePalette(const Style& style)
{
    Palette palette;
    palette.text    = Pick(style.text_color, ImGui::GetColorU32(ImGuiCol_Text));
    palette.heading = Pick(style.heading_color, palette.text);
    palette.link    = Pick(style.link_color, ImGui::GetColorU32(ImGuiCol_TextLink));
    palette.code    = Pick(style.code_color, palette.text);
    palette.code_background =
        Pick(style.code_background, ImGui::GetColorU32(ImGuiCol_FrameBg));
    palette.quote = Pick(style.quote_color, ImGui::GetColorU32(ImGuiCol_TextDisabled));
    palette.line  = Pick(style.line_color, ImGui::GetColorU32(ImGuiCol_Separator));
    palette.header_background =
        Pick(style.header_background, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
    palette.selection =
        Pick(style.selection_color, ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
    return palette;
}

ImU32
PaintColor(const Palette& palette, Paint paint)
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

ImU32
FragmentColor(const Palette& palette, const Fragment& fragment)
{
    ImU32 color = palette.text;
    if(fragment.link >= 0)
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
IsBackground(Paint paint)
{
    return paint == Paint::kCodeBackground || paint == Paint::kHeaderBackground;
}

bool g_selection_enabled = true;
// The view that holds the one selection; any other clears its own.
uint64_t g_active_view = 0;
uint64_t g_view_count  = 0;

struct CachedView
{
    std::unique_ptr<View> view;
    int                   frame = 0;
};

std::unordered_map<ImGuiID, CachedView>&
ViewCache()
{
    static std::unordered_map<ImGuiID, CachedView> cache;
    return cache;
}

int g_cache_frame = -1;

}  // namespace

// -------------------------------------------------------------------- view

struct View::Impl
{
    std::string source;
    uint32_t    flags  = kSelectable;
    bool        parsed = false;
    Document    document;
    Layout      layout;
    Fonts       layout_fonts;
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
        const Fragment* best    = nullptr;
        float           best_dy = FLT_MAX;
        float           best_dx = FLT_MAX;
        for(const Fragment& fragment : layout.fragments)
        {
            const float dy = std::max({ fragment.top - point.y, point.y - fragment.bottom, 0.0f });
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
                done = point.x < x + advance * 0.5f;
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

    int32_t LinkAt(const ImVec2& point) const
    {
        int32_t link = -1;
        for(const Fragment& fragment : layout.fragments)
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
        if((begin == text.size() || !IsWordChar(text[begin])) && begin > 0 &&
           IsWordChar(text[begin - 1]))
        {
            --begin;
        }
        uint32_t end = begin;
        if(begin < text.size() && IsWordChar(text[begin]))
        {
            while(begin > 0 && IsWordChar(text[begin - 1]))
            {
                --begin;
            }
            while(end < text.size() && IsWordChar(text[end]))
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
        const std::string& text    = document.text;
        const size_t       before  = at > 0 ? text.rfind('\n', at - 1) : std::string::npos;
        const size_t       after   = text.find('\n', at);
        anchor = before == std::string::npos ? 0 : static_cast<uint32_t>(before + 1);
        caret  = static_cast<uint32_t>(after == std::string::npos ? text.size() : after);
    }

    bool Interact(const ImRect& bb, ImGuiID item, const Style& style, bool selectable)
    {
        bool hovered = false;
        bool held    = false;
        ImGui::ButtonBehavior(bb, item, &hovered, &held,
                              ImGuiButtonFlags_MouseButtonLeft |
                                  ImGuiButtonFlags_MouseButtonRight |
                                  ImGuiButtonFlags_PressedOnClick);
        const ImGuiIO& io = ImGui::GetIO();
        const ImVec2   point(io.MousePos.x - bb.Min.x, io.MousePos.y - bb.Min.y);
        const int32_t  link  = hovered ? LinkAt(point) : -1;
        const bool     opens = link >= 0 && static_cast<bool>(style.open_link);
        if(hovered)
        {
            ImGui::SetMouseCursor(opens ? ImGuiMouseCursor_Hand
                                        : (selectable ? ImGuiMouseCursor_TextInput
                                                      : ImGuiMouseCursor_Arrow));
            if(link >= 0 && !dragging)
            {
                ImGui::SetTooltip("%s", document.links[static_cast<size_t>(link)].c_str());
            }
        }
        if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            const uint32_t at     = HitTest(point);
            const int      clicks = ImGui::GetMouseClickedCount(ImGuiMouseButton_Left);
            dragging              = false;
            pressed_link          = clicks == 1 ? link : -1;
            if(selectable)
            {
                g_active_view = id;
                if(clicks % 3 == 0)
                {
                    SelectLine(at);
                }
                else if(clicks % 2 == 0)
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
            g_active_view = id;
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
                style.open_link(document.links[static_cast<size_t>(pressed_link)]);
            }
            dragging     = false;
            pressed_link = -1;
        }

        bool copied = false;
        if(selectable && g_active_view == id)
        {
            if(ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_A))
            {
                anchor = 0;
                caret  = static_cast<uint32_t>(document.text.size());
            }
            if(ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_C) && anchor != caret)
            {
                ImGui::SetClipboardText(document.text.substr(Begin(), End() - Begin()).c_str());
                copied = true;
            }
        }
        return copied;
    }

    void Draw(ImDrawList* draw, const ImVec2& origin, const Palette& palette) const
    {
        const char* text = document.text.data();
        for(const Shape& shape : layout.shapes)
        {
            if(IsBackground(shape.paint))
            {
                draw->AddRectFilled(At(origin, shape.min.x, shape.min.y),
                                    At(origin, shape.max.x, shape.max.y),
                                    PaintColor(palette, shape.paint), BOX_ROUNDING);
            }
        }
        for(const Fragment& fragment : layout.fragments)
        {
            if((fragment.flags & kCode) != 0 && fragment.role != Role::kCodeBlock)
            {
                const float pad = fragment.size * INLINE_CODE_PADDING;
                draw->AddRectFilled(At(origin, fragment.x - pad, fragment.top),
                                    At(origin, fragment.x + fragment.width + pad, fragment.bottom),
                                    palette.code_background, BOX_ROUNDING);
            }
        }
        if(anchor != caret)
        {
            for(const Fragment& fragment : layout.fragments)
            {
                const uint32_t begin = std::max(fragment.begin, Begin());
                const uint32_t end   = std::min(fragment.end, End());
                if(begin < end)
                {
                    const char* first = text + fragment.begin;
                    const float x0 =
                        fragment.font->CalcTextSizeA(fragment.size, FLT_MAX, 0.0f, first, text + begin).x;
                    const float x1 =
                        fragment.font->CalcTextSizeA(fragment.size, FLT_MAX, 0.0f, first, text + end).x;
                    draw->AddRectFilled(At(origin, fragment.x + x0, fragment.top),
                                        At(origin, fragment.x + x1, fragment.bottom),
                                        palette.selection);
                }
            }
        }
        for(const Fragment& fragment : layout.fragments)
        {
            const ImU32  color    = FragmentColor(palette, fragment);
            const ImVec2 position = At(origin, fragment.x, fragment.y);
            const char*  begin    = text + fragment.begin;
            const char*  end      = text + fragment.end;
            const int    first    = draw->VtxBuffer.Size;
            draw->AddText(fragment.font, fragment.size, position, color, begin, end);
            if((fragment.flags & kFauxBold) != 0)
            {
                draw->AddText(fragment.font, fragment.size,
                              ImVec2(position.x + FAUX_BOLD_OFFSET, position.y), color, begin,
                              end);
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
                draw->AddLine(ImVec2(position.x, y), ImVec2(position.x + fragment.width, y),
                              color, LINE_THICKNESS);
            }
            if(fragment.link >= 0)
            {
                const float y = position.y + fragment.ascent + LINE_THICKNESS;
                draw->AddLine(ImVec2(position.x, y), ImVec2(position.x + fragment.width, y),
                              color, LINE_THICKNESS);
            }
        }
        for(const Shape& shape : layout.shapes)
        {
            const ImVec2 min = At(origin, shape.min.x, shape.min.y);
            const ImVec2 max = At(origin, shape.max.x, shape.max.y);
            const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
            const ImU32  color = PaintColor(palette, shape.paint);
            if(shape.kind == ShapeKind::kRect && !IsBackground(shape.paint))
            {
                draw->AddRectFilled(min, max, color);
            }
            else if(shape.kind == ShapeKind::kDisc)
            {
                draw->AddCircleFilled(center, (max.x - min.x) * 0.5f, color);
            }
            else if(shape.kind == ShapeKind::kCircle)
            {
                draw->AddCircle(center, (max.x - min.x) * 0.5f, color, 0, LINE_THICKNESS);
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
                                  ImDrawFlags_None, LINE_THICKNESS * 2.0f);
            }
        }
    }
};

View::View()
: m_impl(std::make_unique<Impl>())
{
    m_impl->id = ++g_view_count;
}

View::~View() = default;

void
View::SetText(std::string_view text, uint32_t flags)
{
    Impl& state = *m_impl;
    if(!state.parsed || state.flags != flags || state.source != text)
    {
        state.source.assign(text.data(), text.size());
        state.flags    = flags;
        state.document = Document();
        if((flags & kPlain) != 0)
        {
            BuildPlain(state.document, text);
        }
        else
        {
            Parser(state.document).Parse(text);
        }
        state.parsed   = true;
        state.laid_out = false;
        state.anchor   = 0;
        state.caret    = 0;
        state.dragging = false;
    }
}

bool
View::Render(const Style& style)
{
    Impl&        state  = *m_impl;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    bool         copied = false;
    if(!window->SkipItems)
    {
        const Fonts  fonts  = ResolveFonts(style);
        const ImVec2 origin = window->DC.CursorPos;
        const float  width  = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
        if(!state.laid_out || state.layout_width != width ||
           !SameFonts(state.layout_fonts, fonts))
        {
            state.layout = Layout();
            Layouter(state.document, fonts, width, state.layout).Run();
            state.layout_fonts = fonts;
            state.layout_width = width;
            state.laid_out     = true;
        }

        const ImRect  bb(origin, ImVec2(origin.x + width, origin.y + state.layout.height));
        const ImGuiID item = window->GetID(m_impl.get());
        ImGui::ItemSize(bb);
        if(ImGui::ItemAdd(bb, item))
        {
            const bool selectable = (state.flags & kSelectable) != 0 && g_selection_enabled;
            if(selectable || !state.document.links.empty())
            {
                copied = state.Interact(bb, item, style, selectable);
            }
            if((!selectable || g_active_view != state.id) && !state.dragging)
            {
                state.anchor = state.caret;
            }
            state.Draw(window->DrawList, origin, ResolvePalette(style));
        }
    }
    return copied;
}

bool
View::HasSelection() const
{
    return m_impl->anchor != m_impl->caret;
}

std::string
View::GetSelectedText() const
{
    return m_impl->document.text.substr(m_impl->Begin(), m_impl->End() - m_impl->Begin());
}

const std::string&
View::GetPlainText() const
{
    return m_impl->document.text;
}

void
View::SelectAll()
{
    m_impl->anchor = 0;
    m_impl->caret  = static_cast<uint32_t>(m_impl->document.text.size());
    g_active_view  = m_impl->id;
}

void
View::ClearSelection()
{
    m_impl->anchor = m_impl->caret;
}

void
View::Copy() const
{
    if(HasSelection())
    {
        ImGui::SetClipboardText(GetSelectedText().c_str());
    }
}

bool
Text(const char* id, std::string_view text, uint32_t flags, const Style& style)
{
    std::unordered_map<ImGuiID, CachedView>& cache = ViewCache();
    const int                                frame = ImGui::GetFrameCount();
    if(g_cache_frame != frame)
    {
        // Views not drawn last frame are gone from the screen.
        g_cache_frame = frame;
        for(std::unordered_map<ImGuiID, CachedView>::iterator it = cache.begin();
            it != cache.end();)
        {
            it = it->second.frame < frame - 1 ? cache.erase(it) : std::next(it);
        }
    }
    CachedView& entry = cache[ImGui::GetID(id)];
    if(entry.view == nullptr)
    {
        entry.view = std::make_unique<View>();
    }
    entry.frame = frame;
    entry.view->SetText(text, flags);
    return entry.view->Render(style);
}

void
SetSelectionEnabled(bool enabled)
{
    g_selection_enabled = enabled;
}

bool
IsSelectionEnabled()
{
    return g_selection_enabled;
}

}  // namespace ImMarkdown
