#include "imrichtext.h"

#include "imrichtextutils.h"
#include <unordered_map>
#include <cstring>
#include <optional>
#include <string>
#include <chrono>
#include <deque>

#include "style.h"
#include "draw.h"

#ifdef _WIN32
#pragma warning( push )
#pragma warning( disable : 4244)
#endif

#ifdef _DEBUG
#include <cstdio>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#undef ERROR
#define ERROR(FMT, ...) { \
    CONSOLE_SCREEN_BUFFER_INFO cbinfo; \
    auto h = GetStdHandle(STD_ERROR_HANDLE); \
    GetConsoleScreenBufferInfo(h, &cbinfo); \
    SetConsoleTextAttribute(h, FOREGROUND_RED | FOREGROUND_INTENSITY); \
    std::fprintf(stderr, FMT, __VA_ARGS__); \
    SetConsoleTextAttribute(h, cbinfo.wAttributes); }
#undef DrawText
#else
#define ERROR(FMT, ...) std::fprintf(stderr, "\x1B[31m" FMT "\x1B[0m", __VA_ARGS__)
#endif

static const char* GetTokenTypeString(const ImRichText::Token& token)
{
    switch (token.Type)
    {
    case ImRichText::TokenType::ElidedText:
    case ImRichText::TokenType::Text: return "Text";
    case ImRichText::TokenType::HorizontalRule: return "HorizontalRule";
    case ImRichText::TokenType::ListItemBullet: return "ListItemBullet";
    case ImRichText::TokenType::ListItemNumbered: return "ListItemNumbered";
    default: return "InvalidToken";
    }
}

#ifdef IM_RICHTEXT_ENABLE_PARSER_LOGS
#define DashedLine "-----------------------------------------"
const char* TabLine = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
#define LOG(FMT, ...)  std::fprintf(stdout, "%.*s" FMT, _currentStackPos+1, TabLine, __VA_ARGS__)
#define HIGHLIGHT(FMT, ...) std::fprintf(stdout, DashedLine FMT "\n" DashedLine "\n", __VA_ARGS__)
#else
#define LOG(FMT, ...)
#define HIGHLIGHT(FMT, ...)
#endif
#else
#define ERROR(FMT, ...)
#define LOG(FMT, ...)
#define HIGHLIGHT(FMT, ...)
#endif

namespace ImRichText
{
    struct BlockquoteDrawData
    {
        std::vector<std::pair<ImVec2, ImVec2>> bounds;
    };

    struct AnimationData
    {
        std::vector<float> xoffsets;
        long long lastBlinkTime;
        long long lastMarqueeTime;
        bool isVisible = true;
    };

    struct RichTextData
    {
        ImVec2 specifiedBounds;
        ImVec2 computedBounds;
        RenderConfig* config = nullptr;
        std::string_view richText;
        float scale = 1.f;
        float fontScale = 1.f;
        uint32_t bgcolor;
        bool contentChanged = false;

        Drawables drawables;
        AnimationData animationData;
    };

    struct TooltipData
    {
        ImVec2 pos;
        std::string_view content;
    };

    struct BlockSpanData
    {
        std::pair<int, int> start{ -1, -1 };
        std::pair<int, int> end{ -1, -1 };
    };

    enum class TagType
    {
        Unknown,
        Bold, Italics, Underline, Strikethrough, Mark, Small, Font, Center,
        Span, List, ListItem, Paragraph, Header, RawText, Blockquote, Quotation, Abbr, CodeBlock, Hyperlink,
        Subscript, Superscript,
        Hr, LineBreak,
        Blink, Marquee,
        Meter
    };

    struct StackData
    {
        std::string_view tag;
        TagType tagType = TagType::Unknown;
        int styleIdx = -1;
        bool hasBackground = false;
    };

    struct BackgroundBlockData
    {
        BlockSpanData span;
        DrawableBlock shape;
        int styleIdx = -1;
        bool isMultilineCapable = true;
    };

    static std::unordered_map<std::size_t, RichTextData> RichTextMap;

    // Using std::deque as a stable vector, could be replaced
#ifdef IM_RICHTEXT_TARGET_IMGUI
    static std::unordered_map<ImGuiContext*, std::deque<RenderConfig>> ImRenderConfigs;
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
    static std::unordered_map<BLContext*, std::deque<RenderConfig>> BLRenderConfigs;
#endif

    static const ListItemTokenDescriptor InvalidListItemToken{};
    static const TagPropertyDescriptor InvalidTagPropDesc{};
    static const DrawableBlock InvalidBgBlock{};

    // String representation of numbers, std::string_view is constructed from
    // these strings and used for <li> in <ol> lists
    static std::vector<std::string> NumbersAsStr;

#ifdef IM_RICHTEXT_TARGET_IMGUI
#ifdef _DEBUG
    static bool ShowOverlay = false;
    static bool ShowBoundingBox = false;
#else
    static const bool ShowOverlay = false;
    static const bool ShowBoundingBox = false;
#endif
#endif

    static const char* LineSpaces = "                                ";

    class DefaultTagVisitor final : public ITagVisitor
    {
        enum class Operation
        {
            None, TagStart, TagStartDone, Content, TagEnd
        };

        std::string_view _currTag;
        TagType _currTagType = TagType::Unknown;
        TagType _prevTagType = TagType::Unknown;
        bool _currHasBgBlock = false;
        bool _pendingBgBlockCreation = false;
        // This is an index into _result.StyleDescriptors shifted by -1
        int _currStyleIdx = -1, _prevStyleIdx = -1;
        int _currentStackPos = -1, _maxDepth = 0;
        int _currListDepth = -1, _currBlockquoteDepth = -1;
        int _currSubscriptLevel = 0, _currSuperscriptLevel = 0;
        float _maxWidth = 0.f;
        Operation _lastOp = Operation::None;
        ImVec2 _bounds;

        const RenderConfig& _config;
        Drawables& _result;

        DrawableLine _currLine;
        StyleDescriptor _currStyle;
        TagPropertyDescriptor _currTagProps;
        DrawableBlock _currBgBlock;

        StackData _tagStack[IM_RICHTEXT_MAXDEPTH];
        int _styleIndexStack[IM_RICHTEXT_MAXDEPTH] = { 0 };
        std::vector<BackgroundBlockData> _backgroundBlocks[IM_RICHTEXT_MAXDEPTH];

        int _listItemCountByDepths[IM_RICHTEXT_MAX_LISTDEPTH];
        BlockquoteDrawData _blockquoteStack[IM_RICHTEXT_MAXDEPTH];

        struct TokenPosition
        {
            int lineIdx = 0;
            int segmentIdx = 0;
            int tokenIdx = 0;
        };

        struct TokenPositionRemapping
        {
            TokenPosition oldIdx;
            TokenPosition newIdx;
        };

        void PushTag(std::string_view currTag, TagType tagType)
        {
            _currentStackPos++;
            _tagStack[_currentStackPos].tag = currTag;
            _tagStack[_currentStackPos].tagType = tagType;
        }

        void PopTag(bool reset)
        {
            if (reset) _tagStack[_currentStackPos] = StackData{};
            --_currentStackPos;
        }

        void AddToken(Token token, int propsChanged);
        SegmentData& AddSegment();
        SegmentData& AddSegment(DrawableLine& line, int styleIdx);
        void GenerateTextToken(std::string_view content);
        std::vector<TokenPositionRemapping> PerformWordWrap(int index);
        void UpdateBackgroundSpan(int startDepth, int lineIdx, const std::vector<TokenPositionRemapping>& remapping);
        void ComputeSuperSubscriptOffsets(const std::pair<int, int>& indexes);
        void UpdateLineGeometry(const std::pair<int, int>& linesModified, int depth);
        void RecordBackgroundSpanStart();
        void RecordBackgroundSpanEnd(bool isTagStart, bool segmentAdded, int depth, bool includeChildren);
        DrawableLine MoveToNextLine(bool isTagStart, int depth);

        float GetMaxSuperscriptOffset(const DrawableLine& line, float scale) const;
        float GetMaxSubscriptOffset(const DrawableLine& line, float scale) const;
        std::tuple<int, int, bool, bool, bool> GetBlockSpanIndex(int lineIdx, int segmentIdx) const;

        StyleDescriptor& Style(int stackpos);
        bool CreateNewStyle();
        void PopCurrentStyle();

    public:

        DefaultTagVisitor(const RenderConfig& cfg, Drawables& res, ImVec2 bounds);

        bool TagStart(std::string_view tag);
        bool Attribute(std::string_view name, std::optional<std::string_view> value);
        bool TagStartDone();
        bool Content(std::string_view content);
        bool TagEnd(std::string_view tag, bool selfTerminatingTag);
        void Finalize();

        void Error(std::string_view tag);
        bool IsSelfTerminating(std::string_view tag) const;
        bool IsPreformattedContent(std::string_view tag) const;
    };

    // ===============================================================
    // Section #1 : Implementation of style-related functions
    // ===============================================================

    template <typename T>
    static T Clamp(T val, T min, T max)
    {
        return val < min ? min : val > max ? max : val;
    }

    static int PopulateSegmentStyle(StyleDescriptor& style,
        const StyleDescriptor& parentStyle,
        DrawableBlock& block,
        std::string_view stylePropName,
        std::string_view stylePropVal,
        const RenderConfig& config)
    {
        int prop = NoStyleChange;

        if (AreSame(stylePropName, "font-size"))
        {
            if (AreSame(stylePropVal, "xx-small")) style.font.size = config.DefaultFontSize * 0.6f * config.FontScale;
            else if (AreSame(stylePropVal, "x-small")) style.font.size = config.DefaultFontSize * 0.75f * config.FontScale;
            else if (AreSame(stylePropVal, "small")) style.font.size = config.DefaultFontSize * 0.89f * config.FontScale;
            else if (AreSame(stylePropVal, "medium")) style.font.size = config.DefaultFontSize * config.FontScale;
            else if (AreSame(stylePropVal, "large")) style.font.size = config.DefaultFontSize * 1.2f * config.FontScale;
            else if (AreSame(stylePropVal, "x-large")) style.font.size = config.DefaultFontSize * 1.5f * config.FontScale;
            else if (AreSame(stylePropVal, "xx-large")) style.font.size = config.DefaultFontSize * 2.f * config.FontScale;
            else if (AreSame(stylePropVal, "xxx-large")) style.font.size = config.DefaultFontSize * 3.f * config.FontScale;
            else
                style.font.size = ExtractFloatWithUnit(stylePropVal, config.DefaultFontSize * config.FontScale,
                    config.DefaultFontSize * config.FontScale, parentStyle.font.size, config.FontScale);
            prop = StyleFontSize;
        }
        else if (AreSame(stylePropName, "font-weight"))
        {
            auto idx = SkipDigits(stylePropVal);

            if (idx == 0)
            {
                if (AreSame(stylePropVal, "bold")) style.font.flags |= FontStyleBold;
                else if (AreSame(stylePropVal, "light")) style.font.flags |= FontStyleLight;
                else ERROR("Invalid font-weight property value... [%.*s]\n",
                    (int)stylePropVal.size(), stylePropVal.data());
            }
            else
            {
                int weight = ExtractInt(stylePropVal.substr(0u, idx), 400);
                if (weight >= 600) style.font.flags |= FontStyleBold;
                if (weight < 400) style.font.flags |= FontStyleLight;
            }

            prop = StyleFontWeight;
        }
        else if (AreSame(stylePropName, "text-wrap"))
        {
            if (AreSame(stylePropVal, "nowrap")) style.font.flags |= FontStyleNoWrap;
            prop = StyleTextWrap;
        }
        else if (AreSame(stylePropName, "background-color") || AreSame(stylePropName, "background"))
        {
            if (StartsWith(stylePropVal, "linear-gradient")) 
                block.Gradient = ExtractLinearGradient(stylePropVal, config.NamedColor, config.UserData);
            else block.Color = ExtractColor(stylePropVal, config.NamedColor, config.UserData);
            prop = StyleBackground;
        }
        else if (AreSame(stylePropName, "color"))
        {
            style.fgcolor = ExtractColor(stylePropVal, config.NamedColor, config.UserData);
            prop = StyleFgColor;
        }
        else if (AreSame(stylePropName, "width"))
        {
            style.width = ExtractFloatWithUnit(stylePropVal, 0, config.DefaultFontSize * config.FontScale, parentStyle.width, config.Scale);
            prop = StyleWidth;
        }
        else if (AreSame(stylePropName, "height"))
        {
            style.height = ExtractFloatWithUnit(stylePropVal, 0, config.DefaultFontSize * config.FontScale, parentStyle.height, config.Scale);
            prop = StyleHeight;
        }
        else if (AreSame(stylePropName, "alignment") || AreSame(stylePropName, "text-align"))
        {
            style.alignment |= AreSame(stylePropVal, "justify") ? TextAlignJustify :
                AreSame(stylePropVal, "right") ? TextAlignRight :
                AreSame(stylePropVal, "center") ? TextAlignHCenter :
                TextAlignLeft;
            prop = StyleHAlignment;
        }
        else if (AreSame(stylePropName, "vertical-align"))
        {
            style.alignment |= AreSame(stylePropVal, "top") ? TextAlignTop :
                AreSame(stylePropVal, "bottom") ? TextAlignBottom :
                TextAlignVCenter;
            prop = StyleVAlignment;
        }
        else if (AreSame(stylePropName, "font-family"))
        {
            style.font.family = stylePropVal;
            prop = StyleFontFamily;
        }
        else if (AreSame(stylePropName, "padding"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale, parentStyle.height, config.Scale);
            block.padding.top = block.padding.right = block.padding.left = block.padding.bottom = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "padding-top"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale, parentStyle.height, config.Scale);
            block.padding.top = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "padding-bottom"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale, parentStyle.height, config.Scale);
            block.padding.bottom = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "padding-left"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale, parentStyle.height, config.Scale);
            block.padding.left = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "padding-right"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale, parentStyle.height, config.Scale);
            block.padding.right = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "white-space"))
        {
            if (AreSame(stylePropVal, "normal")) 
            { 
                style.wbbhv = WordBreakBehavior::Normal; 
                style.wscbhv = WhitespaceCollapseBehavior::Collapse;
            }
            else if (AreSame(stylePropVal, "pre"))
            {
                style.wbbhv = WordBreakBehavior::Normal; 
                style.wscbhv = WhitespaceCollapseBehavior::Preserve;
                style.font.flags |= FontStyleNoWrap;
            }
            else if (AreSame(stylePropVal, "pre-wrap"))
            {
                style.wbbhv = WordBreakBehavior::Normal; 
                style.wscbhv = WhitespaceCollapseBehavior::Preserve;
                style.font.flags &= ~FontStyleNoWrap;
            }
            else if (AreSame(stylePropVal, "pre-line"))
            {
                style.wbbhv = WordBreakBehavior::Normal;
                style.wscbhv = WhitespaceCollapseBehavior::PreserveBreaks;
                style.font.flags &= ~FontStyleNoWrap;
            }

            prop = StyleWhitespace;
        }
        else if (AreSame(stylePropName, "text-overflow"))
        {
            if (AreSame(stylePropVal, "ellipsis"))
            {
                style.font.flags |= FontStyleOverflowEllipsis;
                prop = StyleTextOverflow;
            }
        }
        else if (AreSame(stylePropName, "word-break"))
        {
            if (AreSame(stylePropVal, "normal")) style.wbbhv = WordBreakBehavior::Normal;
            if (AreSame(stylePropVal, "break-all")) style.wbbhv = WordBreakBehavior::BreakAll;
            if (AreSame(stylePropVal, "keep-all")) style.wbbhv = WordBreakBehavior::KeepAll;
            if (AreSame(stylePropVal, "break-word")) style.wbbhv = WordBreakBehavior::BreakWord;
            prop = StyleWordBreak;
        }
        else if (AreSame(stylePropName, "white-space-collapse"))
        {
            if (AreSame(stylePropVal, "collapse")) style.wscbhv = WhitespaceCollapseBehavior::Collapse;
            if (AreSame(stylePropVal, "preserve")) style.wscbhv = WhitespaceCollapseBehavior::Preserve;
            if (AreSame(stylePropVal, "preserve-breaks")) style.wscbhv = WhitespaceCollapseBehavior::PreserveBreaks;
            if (AreSame(stylePropVal, "preserve-spaces")) style.wscbhv = WhitespaceCollapseBehavior::PreserveSpaces;
            if (AreSame(stylePropVal, "break-spaces")) style.wscbhv = WhitespaceCollapseBehavior::BreakSpaces;
            prop = StyleWhitespaceCollapse;
        }
        else if (AreSame(stylePropName, "border"))
        {
            block.Border.top = block.Border.bottom = block.Border.left = block.Border.right = ExtractBorder(stylePropVal,
                config.DefaultFontSize * config.FontScale, parentStyle.height, config.NamedColor, config.UserData);
            block.Border.isUniform = true;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-top"))
        {
            block.Border.top = ExtractBorder(stylePropVal, config.DefaultFontSize * config.FontScale,
                parentStyle.height, config.NamedColor, config.UserData);
            block.Border.isUniform = false;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-left"))
        {
            block.Border.left = ExtractBorder(stylePropVal, config.DefaultFontSize * config.FontScale,
                parentStyle.height, config.NamedColor, config.UserData);
            block.Border.isUniform = false;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-right"))
        {
            block.Border.right = ExtractBorder(stylePropVal, config.DefaultFontSize * config.FontScale,
                parentStyle.height, config.NamedColor, config.UserData);
            block.Border.isUniform = false;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-bottom"))
        {
            block.Border.bottom = ExtractBorder(stylePropVal, config.DefaultFontSize * config.FontScale,
                parentStyle.height, config.NamedColor, config.UserData);
            prop = StyleBorder;
            block.Border.isUniform = false;
        }
        else if (AreSame(stylePropName, "border-radius"))
        {
            auto radius = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale,
                1.f, 1.f);
            block.BorderCornerRel = stylePropVal.back() == '%' ? (1 << glimmer::TopLeftCorner) | (1 << glimmer::TopRightCorner) |
                (1 << glimmer::BottomRightCorner)  | (1 << glimmer::BottomLeftCorner) : 0;
            block.Border.setRadius(radius);
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-width"))
        {
            auto width = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale,
                1.f, 1.f);
            block.Border.setThickness(width);
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-color"))
        {
            auto color = ExtractColor(stylePropVal, config.NamedColor, config.UserData);
            block.Border.setColor(color);
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-top-left-radius"))
        {
            block.Border.cornerRadius[glimmer::TopLeftCorner] = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale,
                1.f, 1.f);
            if (stylePropVal.back() == '%') block.BorderCornerRel = block.BorderCornerRel | (1 << glimmer::TopLeftCorner);
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-top-right-radius"))
        {
            block.Border.cornerRadius[glimmer::TopRightCorner] = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale,
                1.f, 1.f);
            if (stylePropVal.back() == '%') block.BorderCornerRel = block.BorderCornerRel | (1 << glimmer::TopRightCorner);
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-bottom-right-radius"))
        {
            block.Border.cornerRadius[glimmer::BottomRightCorner] = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale,
                1.f, 1.f);
            if (stylePropVal.back() == '%') block.BorderCornerRel = block.BorderCornerRel | (1 << glimmer::BottomRightCorner);
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-bottom-left-radius"))
        {
            block.Border.cornerRadius[glimmer::BottomLeftCorner] = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale,
                1.f, 1.f);
            if (stylePropVal.back() == '%') block.BorderCornerRel = block.BorderCornerRel | (1 << glimmer::BottomLeftCorner);
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "margin"))
        {
            block.margin.left = block.margin.right = block.margin.top = block.margin.bottom =
                ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale, style.height, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "margin-top"))
        {
            block.margin.top = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale, style.height, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "margin-left"))
        {
            block.margin.left = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale, style.height, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "margin-right"))
        {
            block.margin.right = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale, style.height, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "margin-bottom"))
        {
            block.margin.bottom = ExtractFloatWithUnit(stylePropVal, 0.f, config.DefaultFontSize * config.FontScale, style.height, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "font-style"))
        {
            if (AreSame(stylePropVal, "normal")) style.font.flags |= FontStyleNormal;
            else if (AreSame(stylePropVal, "italic") || AreSame(stylePropVal, "oblique"))
                style.font.flags |= FontStyleItalics;
            else ERROR("Invalid font-style property value [%.*s]\n",
                (int)stylePropVal.size(), stylePropVal.data());
            prop = StyleFontStyle;
        }
        else if (AreSame(stylePropName, "list-style-type"))
        {
            if (AreSame(stylePropVal, "circle")) style.list.itemStyle = BulletType::Circle;
            else if (AreSame(stylePropVal, "disk")) style.list.itemStyle = BulletType::Disk;
            else if (AreSame(stylePropVal, "square")) style.list.itemStyle = BulletType::Square;
            else if (AreSame(stylePropVal, "tickmark")) style.list.itemStyle = BulletType::CheckMark;
            else if (AreSame(stylePropVal, "checkbox")) style.list.itemStyle = BulletType::CheckBox;
            else if (AreSame(stylePropVal, "arrow")) style.list.itemStyle = BulletType::Arrow;
            else if (AreSame(stylePropVal, "triangle")) style.list.itemStyle = BulletType::Triangle;
            prop = StyleListBulletType;
        }
        else
        {
            ERROR("Invalid style property... [%.*s]\n", (int)stylePropName.size(), stylePropName.data());
        }

        return prop;
    }

    static StyleDescriptor CreateDefaultStyle(const RenderConfig& config)
    {
        StyleDescriptor result;
        result.font.family = config.DefaultFontFamily;
        result.font.size = config.DefaultFontSize * config.FontScale;
        result.font.font = glimmer::GetFont(result.font.family, result.font.size, glimmer::FT_Normal);
        result.fgcolor = config.DefaultFgColor;
        result.list.itemStyle = config.ListItemBullet;
        return result;
    }

    static DrawableLine CreateNewLine(int)
    {
        DrawableLine line;
        line.BlockquoteDepth = -1;
        return line;
    }

    static float CalcVerticalOffset(int maxSuperscriptDepth, float baseFontSz, float scale)
    {
        float sum = 0.f, multiplier = scale;
        for (auto idx = 1; idx <= maxSuperscriptDepth; ++idx)
        {
            sum += multiplier;
            multiplier *= multiplier;
        }
        return sum * (baseFontSz * 0.5f);
    }

    static bool IsLineEmpty(const DrawableLine& line)
    {
        bool isEmpty = true;

        for (const auto& segment : line.Segments)
            isEmpty = isEmpty && segment.Tokens.empty();

        return isEmpty;
    }

    static void CreateElidedTextToken(DrawableLine& line, const StyleDescriptor& style, const RenderConfig& config, ImVec2 bounds)
    {
        auto width = bounds.x;
        width = (style.propsSpecified & StyleWidth) != 0 ? std::min(width, style.width) : width;
        auto sz = config.Renderer->EllipsisWidth(style.font.font, style.font.size);
        width -= sz;

        if ((style.font.flags & FontStyleOverflowEllipsis) != 0 && width > 0.f)
        {
            auto startx = line.Content.left;

            for (auto& segment : line.Segments)
            {
                for (auto& token : segment.Tokens)
                {
                    startx += token.Bounds.width + token.Offset.h();

                    if (startx > width)
                    {
                        if (token.Type == TokenType::Text)
                        {
                            auto revidx = (int)token.Content.size() - 1;

                            while (startx > width)
                            {
                                auto partial = token.Content.substr(revidx, 1);
                                startx -= config.Renderer->GetTextSize(partial, style.font.font, style.font.size).x;
                                token.VisibleTextSize -= (int16_t)1;
                                --revidx;
                            }

                            token.Type = TokenType::ElidedText;
                        }

                        break;
                    }
                }
            }
        }
    }

    static bool IsStyleSupported(TagType type)
    {
        switch (type)
        {
        case TagType::Unknown:
        case TagType::Bold:
        case TagType::Italics:
        case TagType::Underline:
        case TagType::Strikethrough:
        case TagType::Small:
        case TagType::LineBreak:
        case TagType::Center:
            return false;
        default: return true;
        }
    }

    static std::pair<int, bool> RecordTagProperties(TagType tagType, std::string_view attribName, std::optional<std::string_view> attribValue,
        StyleDescriptor& style, DrawableBlock& block, TagPropertyDescriptor& tagprops, const StyleDescriptor& parentStyle,
        const RenderConfig& config)
    {
        int result = 0;
        bool nonStyleAttribute = false;

        if (AreSame(attribName, "style") && IsStyleSupported(tagType))
        {
            if (!attribValue.has_value())
            {
                ERROR("Style attribute value not specified...");
                return { 0, false };
            }
            else
            {
                auto sidx = 0;
                auto styleProps = attribValue.value();

                while (sidx < (int)styleProps.size())
                {
                    sidx = SkipSpace(styleProps, sidx);
                    auto stbegin = sidx;
                    while ((sidx < (int)styleProps.size()) && (styleProps[sidx] != ':') &&
                        !std::isspace(styleProps[sidx])) sidx++;
                    auto stylePropName = styleProps.substr(stbegin, sidx - stbegin);

                    sidx = SkipSpace(styleProps, sidx);
                    if (styleProps[sidx] == ':') sidx++;
                    sidx = SkipSpace(styleProps, sidx);

                    auto stylePropVal = GetQuotedString(styleProps.data(), sidx, (int)styleProps.size());
                    if (!stylePropVal.has_value() || stylePropVal.value().empty())
                    {
                        stbegin = sidx;
                        while ((sidx < (int)styleProps.size()) && styleProps[sidx] != ';') sidx++;
                        stylePropVal = styleProps.substr(stbegin, sidx - stbegin);

                        if (styleProps[sidx] == ';') sidx++;
                    }

                    if (stylePropVal.has_value())
                    {
                        auto prop = PopulateSegmentStyle(style, parentStyle, block, stylePropName,
                            stylePropVal.value(), config);
                        result = result | prop;
                    }
                }
            }
        }
        else if (tagType == TagType::Abbr && AreSame(attribName, "title") && attribValue.has_value())
        {
            tagprops.tooltip = attribValue.value();
            nonStyleAttribute = true;
        }
        else if (tagType == TagType::Hyperlink && AreSame(attribName, "href") && attribValue.has_value())
        {
            tagprops.link = attribValue.value();
            nonStyleAttribute = true;
        }
        else if (tagType == TagType::Font)
        {
            if (AreSame(attribName, "color") && attribValue.has_value())
            {
                style.fgcolor = ExtractColor(attribValue.value(), config.NamedColor, config.UserData);
                result = result | StyleFgColor;
            }
            else if (AreSame(attribName, "size") && attribValue.has_value())
            {
                style.font.size = ExtractFloatWithUnit(attribValue.value(), config.DefaultFontSize * config.FontScale,
                    config.DefaultFontSize * config.FontScale, parentStyle.height, config.Scale);
                result = result | StyleFontSize;
            }
            else if (AreSame(attribName, "face") && attribValue.has_value())
            {
                style.font.family = attribValue.value();
                result = result | StyleFontFamily;
            }
        }
        else if (tagType == TagType::Meter)
        {
            if (AreSame(attribName, "value") && attribValue.has_value()) tagprops.value = ExtractInt(attribValue.value(), 0);
            if (AreSame(attribName, "min") && attribValue.has_value()) tagprops.range.first = ExtractInt(attribValue.value(), 0);
            if (AreSame(attribName, "max") && attribValue.has_value()) tagprops.range.second = ExtractInt(attribValue.value(), 0);
            nonStyleAttribute = true;
        }

        return { result, nonStyleAttribute };
    }

    static int CreateNextStyle(std::vector<StyleDescriptor>& styles)
    {
        auto& newstyle = styles.emplace_back(styles.back());
        return (int)styles.size() - 1;
    }

    static TagType GetTagType(std::string_view currTag, bool isStrictHTML5)
    {
        if (AreSame(currTag, "b") || AreSame(currTag, "strong")) return TagType::Bold;
        else if (AreSame(currTag, "i") || AreSame(currTag, "em") || AreSame(currTag, "cite") || AreSame(currTag, "var"))
            return TagType::Italics;
        else if (!isStrictHTML5 && AreSame(currTag, "font")) return TagType::Font;
        else if (AreSame(currTag, "hr")) return TagType::Hr;
        else if (AreSame(currTag, "br")) return TagType::LineBreak;
        else if (AreSame(currTag, "span")) return TagType::Span;
        else if (!isStrictHTML5 && AreSame(currTag, "center")) return TagType::Center;
        else if (AreSame(currTag, "a")) return TagType::Hyperlink;
        else if (AreSame(currTag, "sub")) return TagType::Subscript;
        else if (AreSame(currTag, "sup")) return TagType::Superscript;
        else if (AreSame(currTag, "mark")) return TagType::Mark;
        else if (AreSame(currTag, "small")) return TagType::Small;
        else if (AreSame(currTag, "ul") || AreSame(currTag, "ol")) return TagType::List;
        else if (AreSame(currTag, "p")) return TagType::Paragraph;
        else if (currTag.size() == 2u && (currTag[0] == 'h' || currTag[0] == 'H') && std::isdigit(currTag[1])) return TagType::Header;
        else if (AreSame(currTag, "li")) return TagType::ListItem;
        else if (AreSame(currTag, "q")) return TagType::Quotation;
        else if (AreSame(currTag, "pre") || AreSame(currTag, "samp")) return TagType::RawText;
        else if (AreSame(currTag, "u")) return TagType::Underline;
        else if (AreSame(currTag, "s") || AreSame(currTag, "del")) return TagType::Strikethrough;
        else if (AreSame(currTag, "blockquote")) return TagType::Blockquote;
        else if (AreSame(currTag, "code")) return TagType::CodeBlock;
        else if (AreSame(currTag, "abbr")) return TagType::Abbr;
        else if (!isStrictHTML5 && AreSame(currTag, "blink")) return TagType::Blink;
        else if (AreSame(currTag, "marquee")) return TagType::Marquee;
        else if (AreSame(currTag, "meter")) return TagType::Meter;
        return TagType::Unknown;
    }

    static void SetImplicitStyleProps(TagType tagType, std::string_view currTag,
        StyleDescriptor& style, const StyleDescriptor& parentStyle,
        DrawableBlock& block, DrawableLine& line, const RenderConfig& config)
    {
        if (tagType == TagType::Header)
        {
            style.font.size = config.HFontSizes[currTag[1] - '1'] * config.FontScale;
            style.font.flags |= FontStyleBold;
            style.propsSpecified = style.propsSpecified | StyleFontStyle | StyleFontSize;
        }
        else if (tagType == TagType::RawText || tagType == TagType::CodeBlock)
        {
            style.font.family = GLIMMER_MONOSPACE_FONTFAMILY;
            style.propsSpecified = style.propsSpecified | StyleFontFamily;
            if (((style.propsSpecified & StyleWhitespace) == 0) && ((style.propsSpecified & StyleTextWrap) == 0))
                style.font.flags |= FontStyleNoWrap;
            if (((style.propsSpecified & StyleWhitespace) == 0) && ((style.propsSpecified & StyleWhitespaceCollapse) == 0))
                style.wscbhv = WhitespaceCollapseBehavior::Preserve;

            if (tagType == TagType::CodeBlock)
            {
                if ((style.propsSpecified & StyleBackground) == 0)
                    block.Color = config.CodeBlockBg;
            }
        }
        else if (tagType == TagType::Italics)
        {
            style.font.flags |= FontStyleItalics;
            style.propsSpecified = style.propsSpecified | StyleFontStyle;
        }
        else if (tagType == TagType::Bold)
        {
            style.font.flags |= FontStyleBold;
            style.propsSpecified = style.propsSpecified | StyleFontStyle;
        }
        else if (tagType == TagType::Mark)
        {
            if ((style.propsSpecified & StyleBackground) == 0)
                block.Color = config.MarkHighlight;
            style.propsSpecified = style.propsSpecified | StyleBackground;
        }
        else if (tagType == TagType::Small)
        {
            style.font.size = parentStyle.font.size * 0.8f;
            style.propsSpecified = style.propsSpecified | StyleFontSize;
        }
        else if (tagType == TagType::Superscript)
        {
            style.font.size *= config.ScaleSuperscript;
            style.propsSpecified = style.propsSpecified | StyleFontSize;
        }
        else if (tagType == TagType::Subscript)
        {
            style.font.size *= config.ScaleSubscript;
            style.propsSpecified = style.propsSpecified | StyleFontSize;
        }
        else if (tagType == TagType::Underline)
        {
            style.font.flags |= FontStyleUnderline;
            style.propsSpecified = style.propsSpecified | StyleFontStyle;
        }
        else if (tagType == TagType::Strikethrough)
        {
            style.font.flags |= FontStyleStrikethrough;
            style.propsSpecified = style.propsSpecified | StyleFontStyle;
        }
        else if (tagType == TagType::Hyperlink)
        {
            if ((style.propsSpecified & StyleFontStyle) == 0) style.font.flags |= FontStyleUnderline;
            if ((style.propsSpecified & StyleFgColor) == 0) style.fgcolor = config.HyperlinkColor;
            style.propsSpecified = style.propsSpecified | StyleFontStyle | StyleFgColor;
        }
        else if (tagType == TagType::Blink)
        {
            style.blink = true;
            style.propsSpecified = style.propsSpecified | StyleBlink;
        }
        else if (tagType == TagType::Center)
        {
            style.alignment = TextAlignCenter;
            style.propsSpecified = StyleHAlignment | StyleVAlignment;
        }
        else if (tagType == TagType::Hr)
        {
            block.margin.top = block.margin.bottom = config.HrVerticalMargins;
            style.propsSpecified = style.propsSpecified | StyleMargin;
        }

        if (style.propsSpecified != NoStyleChange)
        {
            FontType fstyle = glimmer::FT_Normal;
            if ((style.font.flags & FontStyleBold) != 0 &&
                (style.font.flags & FontStyleItalics) != 0) fstyle = glimmer::FT_BoldItalics;
            else if ((style.font.flags & FontStyleBold) != 0) fstyle = glimmer::FT_Bold;
            else if ((style.font.flags & FontStyleItalics) != 0) fstyle = glimmer::FT_Italics;
            else if ((style.font.flags & FontStyleLight) != 0) fstyle = glimmer::FT_Light;
            style.font.font = GetFont(style.font.family, style.font.size, fstyle);
        }
    }

    static bool CanContentBeMultiline(TagType type)
    {
        switch (type)
        {
        case ImRichText::TagType::Span: [[fallthrough]];
        case ImRichText::TagType::Subscript: [[fallthrough]];
        case ImRichText::TagType::Superscript: [[fallthrough]];
        case ImRichText::TagType::Hyperlink: [[fallthrough]];
        case ImRichText::TagType::Meter: [[fallthrough]];
        case ImRichText::TagType::Marquee: return false;
        default: return true;
        }
    }

    // ===============================================================
    // Section #2 : Implementation of drawing routines for drawables
    // ===============================================================

#if defined(_DEBUG) && defined(IM_RICHTEXT_TARGET_IMGUI)
    inline void DrawBoundingBox(DebugContentType type, ImVec2 startpos, ImVec2 endpos, const RenderConfig& config)
    {
        if (config.DebugContents[type] != IM_COL32_BLACK_TRANS && ShowBoundingBox)
            config.OverlayRenderer->DrawRect(startpos, endpos, config.DebugContents[type], false);
    }
#else
#define DrawBoundingBox(...)
#endif

#ifdef IM_RICHTEXT_TARGET_IMGUI

    std::tuple<int, int, int> DecomposeToRGBChannels(uint32_t color)
    {
        auto mask = (uint32_t)-1;
        return std::make_tuple((int)(color & (mask >> 24)),
            (int)(color & ((mask >> 16) & (mask << 8))) >> 8,
            (int)(color & ((mask >> 8) & (mask << 16))) >> 16);
    }

    bool DrawOverlay(ImVec2 startpos, ImVec2 endpos, const Token& token, 
        const StyleDescriptor& style, const DrawableBlock& block, 
        const TagPropertyDescriptor& tagprops, const RenderConfig& config)
    {
        const auto& io = ImGui::GetCurrentContext()->IO;
        if (ImRect{ startpos, endpos }.Contains(io.MousePos) && ShowOverlay)
        {
            auto overlay = ImGui::GetForegroundDrawList();
            startpos.y = 0.f;

            char props[2048] = { 0 };
            auto currpos = 0;
            for (auto exp = 0; exp <= 21; ++exp)
            {
                auto prop = 1 << exp;
                if ((style.propsSpecified & prop) != 0)
                {
                    switch (prop)
                    {
                    case NoStyleChange: currpos += std::snprintf(props + currpos, 2047 - currpos, "NoStyleChange,"); break;
                    case StyleBackground: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleBackground,"); break;
                    case StyleFgColor: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleFgColor,"); break;
                    case StyleFontSize: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleFontSize,"); break;
                    case StyleFontFamily: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleFontFamily,"); break;
                    case StyleFontWeight: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleFontWeight,"); break;
                    case StyleFontStyle: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleFontStyle,"); break;
                    case StyleHeight: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleHeight,"); break;
                    case StyleWidth: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleWidth,"); break;
                    case StyleListBulletType: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleListBulletType,"); break;
                    case StylePadding: currpos += std::snprintf(props + currpos, 2047 - currpos, "StylePadding,"); break;
                    case StyleBorder: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleBorder,"); break;
                    case StyleBorderRadius: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleBorderRadius,"); break;
                    case StyleBlink: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleBlink,"); break;
                    case StyleTextWrap: currpos += std::snprintf(props + currpos, 2047 - currpos, "StyleTextWrap,"); break;
                    default: break;
                    }
                }
            }

            constexpr int bufsz = 4096;
            char buffer[bufsz] = { 0 };
            auto yesorno = [](bool val) { return val ? "Yes" : "No"; };
            auto [fr, fg, fb] = DecomposeToRGBChannels(style.fgcolor);
            auto [br, bg, bb] = DecomposeToRGBChannels(block.Color);

            currpos = std::snprintf(buffer, bufsz - 1, "Position            : (%.2f, %.2f)\n"
                "Bounds              : (%.2f, %.2f)\n",
                startpos.x, startpos.y, token.Bounds.width, token.Bounds.height);

            currpos += std::snprintf(buffer + currpos, bufsz - currpos,
                "\nProperties Specified: %s\nForeground Color    : (%d, %d, %d)\n",
                props, fr, fg, fb);

            if (block.Start != ImVec2{ -1.f, -1.f } &&
                block.End != ImVec2{ -1.f, -1.f })
            {
                if (block.Gradient.totalStops == 0)
                    if (block.Color != IM_COL32_BLACK_TRANS)
                        currpos += std::snprintf(buffer + currpos, bufsz - currpos, "Background Color    : (%d, %d, %d)\n", br, bg, bb);
                    else
                        currpos += std::snprintf(buffer + currpos, bufsz - currpos, "Background Color    : Transparent\n");
                else
                {
                    currpos += std::snprintf(buffer + currpos, bufsz - currpos, "Linear Gradient     :");

                    for (auto idx = 0; idx < block.Gradient.totalStops; ++idx)
                    {
                        auto [r1, g1, b1] = DecomposeToRGBChannels(block.Gradient.colorStops[idx].from);
                        auto [r2, g2, b2] = DecomposeToRGBChannels(block.Gradient.colorStops[idx].to);
                        currpos += std::snprintf(buffer + currpos, bufsz - currpos, "From (%d, %d, %d) To (%d, %d, %d) at %.2f\n",
                            r1, g1, b1, r2, g2, b2, block.Gradient.colorStops[idx].pos);
                    }
                }

                int br = 0, bg = 0, bb = 0;
                std::tie(br, bg, bb) = DecomposeToRGBChannels(block.Border.top.color);
                currpos += std::snprintf(buffer + currpos, bufsz - currpos,
                    "Border.top          : (%.2fpx, rgb(%d, %d, %d))\n",
                    block.Border.top.thickness, br, bg, bb);

                std::tie(br, bg, bb) = DecomposeToRGBChannels(block.Border.right.color);
                currpos += std::snprintf(buffer + currpos, bufsz - currpos,
                    "Border.right        : (%.2fpx, rgb(%d, %d, %d))\n",
                    block.Border.right.thickness, br, bg, bb);

                std::tie(br, bg, bb) = DecomposeToRGBChannels(block.Border.bottom.color);
                currpos += std::snprintf(buffer + currpos, bufsz - currpos,
                    "Border.bottom       : (%.2fpx, rgb(%d, %d, %d))\n",
                    block.Border.bottom.thickness, br, bg, bb);

                std::tie(br, bg, bb) = DecomposeToRGBChannels(block.Border.left.color);
                currpos += std::snprintf(buffer + currpos, bufsz - currpos,
                    "Border.left         : (%.2fpx, rgb(%d, %d, %d))\n",
                    block.Border.left.thickness, br, bg, bb);

                currpos += std::snprintf(buffer + currpos, bufsz - currpos,
                    "Padding             : (%.2fpx, %.2fpx, %.2fpx, %.2fpx)\n",
                    block.padding.top, block.padding.right, block.padding.bottom, block.padding.left);
            }

            currpos += std::snprintf(buffer + currpos, bufsz - currpos,
                "\nHeight              : %.2fpx\nWidth               : %.2fpx\n"
                "Tooltip               : %.*s\nLink                : %.*s\n"
                "Blink                 : %s\n",
                style.width, style.height, (int)tagprops.tooltip.size(), tagprops.tooltip.data(),
                (int)tagprops.link.size(), tagprops.link.data(), yesorno(style.blink));

            if (token.Type == TokenType::Text || token.Type == TokenType::ElidedText)
            {
                currpos += std::snprintf(buffer + currpos, bufsz - currpos, "\n\nFont.family         : %.*s\n"
                    "Font.size           : %.2fpx\nFont.bold           : %s\nFont.italics        : %s\n"
                    "Font.underline      : %s\n"
                    "Font.strike         : %s\n"
                    "Font.wrap           : %s", (int)style.font.family.size(), style.font.family.data(), style.font.size,
                    yesorno(style.font.flags & FontStyleBold), yesorno(style.font.flags & FontStyleItalics), 
                    yesorno(style.font.flags & FontStyleUnderline), yesorno(style.font.flags & FontStyleStrikethrough),
                    yesorno(!(style.font.flags & FontStyleNoWrap)));
            }
            else if (token.Type == TokenType::Meter)
            {
                currpos += std::snprintf(buffer + currpos, bufsz - currpos, "\n\nRange               : "
                    "(%.2f, %.2f)\nValue          : %.2f",
                    tagprops.range.first, tagprops.range.second, tagprops.value);
            }

            auto font = (ImFont*)glimmer::CreateImGuiRenderer();
            ImGui::PushFont(font);
            auto sz = ImGui::CalcTextSize(buffer, buffer + currpos, false, 300.f);
            sz.x += 20.f;

            startpos.x = ImGui::GetCurrentWindow()->Size.x - sz.x;
            overlay->AddRectFilled(startpos, startpos + ImVec2{ ImGui::GetCurrentWindow()->Size.x, sz.y }, IM_COL32_WHITE);
            overlay->AddText(font, font->FontSize, startpos, IM_COL32_BLACK, buffer, NULL, 300.f);
            ImGui::PopFont();
            return true;
        }

        return false;
    }
#endif

    static bool DrawToken(const Token& token, ImVec2 initpos,
        ImVec2 bounds, const StyleDescriptor& style, const TagPropertyDescriptor& tagprops, 
        const DrawableBlock& block, const ListItemTokenDescriptor& listItem, 
        const RenderConfig& config, TooltipData& tooltip, AnimationData& animation)
    {
        auto startpos = token.Bounds.start(initpos) + ImVec2{ token.Offset.left, token.Offset.top };
        auto endpos = token.Bounds.end(initpos);

        if ((style.blink && animation.isVisible) || !style.blink)
        {
            if (token.Type == TokenType::HorizontalRule)
            {
                config.Renderer->DrawRect(startpos, endpos, style.fgcolor, true);
            }
            else if (token.Type == TokenType::ListItemBullet)
            {
                auto bulletscale = Clamp(config.BulletSizeScale, 1.f, 4.f);
                auto bulletsz = (style.font.size) / bulletscale;

                if (style.list.itemStyle == BulletType::Custom)
                    config.RTRenderer->DrawBullet(startpos, endpos, style.fgcolor, listItem.ListItemIndex, listItem.ListDepth);
                else config.RTRenderer->DrawDefaultBullet(style.list.itemStyle, initpos, token.Bounds, style.fgcolor, bulletsz);
            }
            else if (token.Type == TokenType::ListItemNumbered)
            {
                config.Renderer->DrawText(listItem.NestedListItemIndex, startpos, style.fgcolor);
            }
            else if (token.Type == TokenType::Meter)
            {
                auto border = ImVec2{ 1.f, 1.f };
                auto borderRadius = (endpos.y - startpos.y) * 0.5f;
                auto diff = tagprops.range.second - tagprops.range.first;
                auto progress = (tagprops.value / diff) * token.Bounds.width;

                config.Renderer->DrawRoundedRect(startpos, endpos, config.MeterBgColor, true, borderRadius, borderRadius, borderRadius, borderRadius);
                config.Renderer->DrawRoundedRect(startpos, endpos, config.MeterBorderColor, false, borderRadius, borderRadius, borderRadius, borderRadius);
                config.Renderer->DrawRoundedRect(startpos + border, startpos - border + ImVec2{ progress, token.Bounds.height },
                    config.MeterFgColor, true, borderRadius, 0.f, 0.f, borderRadius);
            }
            else
            {
                auto textend = token.Content.data() + token.VisibleTextSize;
                auto halfh = token.Bounds.height * 0.5f;
                config.Renderer->DrawText(token.Content, startpos, style.fgcolor);

                if (token.Type == TokenType::ElidedText)
                {
                    auto ewidth = config.Renderer->EllipsisWidth(style.font.font, style.font.size);
                    config.Renderer->DrawText("...", ImVec2{ startpos.x + token.Bounds.width - ewidth, startpos.y }, style.fgcolor);
                }

                if (style.font.flags & FontStyleStrikethrough) config.Renderer->DrawLine(startpos + ImVec2{ 0.f, halfh }, endpos + ImVec2{ 0.f, -halfh }, style.fgcolor);
                if (style.font.flags & FontStyleUnderline) config.Renderer->DrawLine(startpos + ImVec2{ 0.f, token.Bounds.height }, endpos, style.fgcolor);

                if (!tagprops.tooltip.empty())
                {
                    if (!(style.font.flags & FontStyleUnderline))
                    {
                        // TODO: Refactor this out
                        auto posx = startpos.x;
                        while (posx < endpos.x)
                        {
                            config.Renderer->DrawCircle(ImVec2{ posx, endpos.y }, 1.f, style.fgcolor, true);
                            posx += 3.f;
                        }
                    }

                    auto mousepos = config.Platform->GetCurrentMousePos();
                    if (ImRect{ startpos, endpos }.Contains(mousepos))
                    {
                        tooltip.pos = mousepos;
                        tooltip.content = tagprops.tooltip;
                    }
                }
                else if (!tagprops.link.empty() && (config.Platform != nullptr))
                { 
                    auto pos = config.Platform->GetCurrentMousePos();
                    if (ImRect{ startpos, endpos }.Contains(pos))
                    {
                        config.Platform->HandleHover(true);
                        if (config.Platform->IsMouseClicked())
                            config.Platform->HandleHyperlink(tagprops.link);
                    }
                    else
                        config.Platform->HandleHover(false);
                }
            }
        }

#ifdef IM_RICHTEXT_TARGET_IMGUI
        if (DrawOverlay(startpos, endpos, token, style, block, tagprops, config))
#endif
            DrawBoundingBox(ContentTypeToken, startpos, endpos, config);
        if ((token.Bounds.left + token.Bounds.width) > (bounds.x + initpos.x)) return false;
        return true;
    }

    static bool DrawSegment(const SegmentData& segment, const DrawableBlock& block, 
        ImVec2 initpos, ImVec2 bounds, const Drawables& result, const RenderConfig& config, 
        TooltipData& tooltip, AnimationData& animation)
    {
        if (segment.Tokens.empty()) return true;
        const auto& style = result.StyleDescriptors[segment.StyleIdx + 1];
        auto popFont = false;

        if (style.font.font != nullptr)
        {
            popFont = config.Renderer->SetCurrentFont(style.font.font, style.font.size);
        }

        auto drawTokens = true;
        auto startpos = segment.Bounds.start(initpos), endpos = segment.Bounds.end(initpos);
        auto isMeter = (segment.Tokens.size() == 1u &&
            (segment.Tokens.front().Type == TokenType::Meter));

        for (const auto& token : segment.Tokens)
        {
            const auto& listItem = token.ListPropsIdx == -1 ? InvalidListItemToken :
                result.ListItemTokens[token.ListPropsIdx];
            const auto& tagprops = token.PropertiesIdx == -1 ? InvalidTagPropDesc :
                result.TagDescriptors[token.PropertiesIdx];
            if (drawTokens && !DrawToken(token, initpos, bounds, style,
                tagprops, block, listItem, config, tooltip, animation))
            {
                drawTokens = false; 
                break;
            }
        }

        DrawBoundingBox(ContentTypeSegment, startpos, endpos, config);
        if (popFont) config.Renderer->ResetFont();
        return drawTokens;
    }

    static std::optional<std::pair<int, int>> GetBlockIndex(const Drawables& result, ImVec2 pos)
    {
        for (auto depth = 0; depth < IM_RICHTEXT_MAXDEPTH; ++depth)
        {
            auto blockidx = 0;
            for (const auto& block : result.BackgroundBlocks[depth])
            {
                if (ImRect{ block.Start, block.End }.Contains(pos))
                    return std::make_pair(depth, blockidx);
                blockidx++;
            }
        }

        return std::nullopt;
    }

    static void DrawForegroundLayer(ImVec2 initpos, ImVec2 bounds,
        const Drawables& result, const RenderConfig& config, TooltipData& tooltip, 
        AnimationData& animation)
    {
        std::optional<std::pair<int, int>> bidx = std::nullopt;
        if (config.Platform) bidx = GetBlockIndex(result, config.Platform->GetCurrentMousePos());
        const auto& block = bidx != std::nullopt ? result.BackgroundBlocks[bidx.value().first][bidx.value().second] :
            InvalidBgBlock;
        const auto& lines = result.ForegroundLines;

        for (auto lineidx = 0; lineidx < (int)lines.size(); ++lineidx)
        {
            auto segmentidx = 0;
            if (lines[lineidx].Segments.empty()) continue;

            for (const auto& segment : lines[lineidx].Segments)
            {
                auto linestart = initpos;
                if (lines[lineidx].Marquee) linestart.x += animation.xoffsets[lineidx];
                if (!DrawSegment(segment, block, linestart, bounds, result, config, tooltip, animation))
                    break;
                ++segmentidx;
            }
            
#ifdef _DEBUG
            auto linestart = lines[lineidx].Content.start(initpos) + ImVec2{ lines[lineidx].Offset.left, lines[lineidx].Offset.top };
            auto lineend = lines[lineidx].Content.end(initpos);
            DrawBoundingBox(ContentTypeLine, linestart, lineend, config);
#endif
            if ((lines[lineidx].Content.top + lines[lineidx].height()) > (bounds.y + initpos.y)) break;
        }
    }

    static void DrawBackgroundLayer(ImVec2 initpos, ImVec2 bounds,
        const std::vector<DrawableBlock>* blocks, const RenderConfig& config)
    {
        // Draw backgrounds on top of shadows
        for (auto depth = 0; depth < IM_RICHTEXT_MAXDEPTH; ++depth)
        {
            for (const auto& block : blocks[depth])
            {
                auto startpos = block.Start + initpos;
                auto endpos = block.End + initpos;
                DrawBackground(startpos, endpos, block.Color, block.Gradient, block.Border, *config.Renderer);
                DrawBoundingBox(ContentTypeBg, startpos, endpos, config);
                DrawBorderRect(startpos, endpos, block.Border, block.Color, *config.Renderer);
                if (block.End.y > (bounds.y + initpos.y)) break;
            }
        }
    }

    static void DrawImpl(AnimationData& animation, const Drawables& drawables, ImVec2 pos, ImVec2 bounds, RenderConfig* config)
    {
        using namespace std::chrono;

#if defined(_DEBUG) && defined(IM_RICHTEXT_TARGET_IMGUI)
        config->OverlayRenderer = glimmer::CreateImGuiRenderer();
        config->OverlayRenderer->UserData = ImGui::GetForegroundDrawList();
#endif

        auto endpos = pos + bounds;
        TooltipData tooltip;

        if (animation.xoffsets.empty())
        {
            animation.xoffsets.resize(drawables.ForegroundLines.size());
            std::fill(animation.xoffsets.begin(), animation.xoffsets.end(), 0.f);
        }

        auto currFrameTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

        config->Renderer->SetClipRect(pos, endpos);
        config->Renderer->DrawRect(pos, endpos, config->DefaultBgColor, true);

        DrawBackgroundLayer(pos, bounds, drawables.BackgroundBlocks, *config);
        DrawForegroundLayer(pos, bounds, drawables, *config, tooltip, animation);
        config->Renderer->DrawTooltip(tooltip.pos, tooltip.content);

        if (config->Platform != nullptr)
        {
            if (!config->IsStrictHTML5 && (currFrameTime - animation.lastBlinkTime > IM_RICHTEXT_BLINK_ANIMATION_INTERVAL))
            {
                animation.isVisible = !animation.isVisible;
                animation.lastBlinkTime = currFrameTime;
                config->Platform->RequestFrame();
            }

            if (currFrameTime - animation.lastMarqueeTime > IM_RICHTEXT_MARQUEE_ANIMATION_INTERVAL)
            {
                for (auto lineidx = 0; lineidx < (int)animation.xoffsets.size(); ++lineidx)
                {
                    animation.xoffsets[lineidx] += 1.f;
                    auto linewidth = drawables.ForegroundLines[lineidx].Content.width;

                    if (animation.xoffsets[lineidx] >= linewidth)
                        animation.xoffsets[lineidx] = -linewidth;
                }

                config->Platform->RequestFrame();
                animation.lastMarqueeTime = currFrameTime;
            }
        }

        config->Renderer->ResetClipRect();
    }

    // ===============================================================
    // Section #3 : Implementation of `DefaultTagVisitor` member functions
    // ===============================================================

    static bool operator!=(const TagPropertyDescriptor& lhs, const TagPropertyDescriptor& rhs)
    {
        return lhs.tooltip != rhs.tooltip || lhs.link != rhs.link || lhs.value != rhs.value ||
            lhs.range != rhs.range;
    }

    DefaultTagVisitor::DefaultTagVisitor(const RenderConfig& cfg, Drawables& res, ImVec2 bounds)
        : _bounds{ bounds }, _config{ cfg }, _result{ res }
    {
        std::memset(_listItemCountByDepths, 0, IM_RICHTEXT_MAX_LISTDEPTH);
        for (auto idx = 0; idx < IM_RICHTEXT_MAXDEPTH; ++idx) _styleIndexStack[idx] = -2;
        _result.StyleDescriptors.emplace_back(CreateDefaultStyle(_config));
        _currStyle = _result.StyleDescriptors.front();
        _maxWidth = _bounds.x;
    }

    void DefaultTagVisitor::AddToken(Token token, int propsChanged)
    {
        auto& segment = _currLine.Segments.back();
        const auto& style = _result.StyleDescriptors[segment.StyleIdx + 1];

        if (token.Type == TokenType::Text)
        {
            auto sz = _config.Renderer->GetTextSize(token.Content, style.font.font, style.font.size);
            token.VisibleTextSize = (int16_t)token.Content.size();
            token.Bounds.width = sz.x;
            token.Bounds.height = sz.y;
        }
        else if (token.Type == TokenType::HorizontalRule)
        {
            if ((propsChanged & StyleWidth) == 0) token.Bounds.width = style.width;
            if ((propsChanged & StyleHeight) == 0) token.Bounds.height = style.height;
        }
        else if (token.Type == TokenType::ListItemBullet)
        {
            auto bulletscale = Clamp(_config.BulletSizeScale, 1.f, 4.f);
            auto bulletsz = (style.font.size) / bulletscale;
            token.Bounds.width = token.Bounds.height = bulletsz;
            token.Offset.right = _config.ListItemOffset;
        }
        else if (token.Type == TokenType::ListItemNumbered)
        {
            if (NumbersAsStr.empty())
            {
                NumbersAsStr.reserve(IM_RICHTEXT_MAX_LISTITEM);

                for (auto num = 1; num <= IM_RICHTEXT_MAX_LISTITEM; ++num)
                    NumbersAsStr.emplace_back(std::to_string(num));
            }

            auto& listItem = _result.ListItemTokens[token.ListPropsIdx];
            std::memset(listItem.NestedListItemIndex, 0, IM_RICHTEXT_NESTED_ITEMCOUNT_STRSZ);
            auto currbuf = 0;

            for (auto depth = 0; depth <= listItem.ListDepth && currbuf < IM_RICHTEXT_NESTED_ITEMCOUNT_STRSZ; ++depth)
            {
                auto itemcount = _listItemCountByDepths[depth] - 1;
                auto itemlen = itemcount > 99 ? 3 : itemcount > 9 ? 2 : 1;
                std::memcpy(listItem.NestedListItemIndex + currbuf, NumbersAsStr[itemcount].data(), itemlen);
                currbuf += itemlen;

                listItem.NestedListItemIndex[currbuf] = '.';
                currbuf += 1;
            }

            std::string_view input{ listItem.NestedListItemIndex, (size_t)currbuf };
            auto sz = _config.Renderer->GetTextSize(input, style.font.font, style.font.size);
            token.Bounds.width = sz.x;
            token.Bounds.height = sz.y;
        }
        else if (token.Type == TokenType::Meter)
        {
            if ((propsChanged & StyleWidth) == 0) token.Bounds.width = _config.MeterDefaultSize.x;
            if ((propsChanged & StyleHeight) == 0) token.Bounds.height = _config.MeterDefaultSize.y;
        }

        segment.Tokens.emplace_back(token);

        segment.HasText = segment.HasText || (!token.Content.empty());
        segment.Bounds.width += token.Bounds.width;
        segment.Bounds.height = std::max(token.Bounds.height, segment.Bounds.height);
        _currLine.HasText = _currLine.HasText || segment.HasText;
        _currLine.HasSubscript = _currLine.HasSubscript || segment.SubscriptDepth > 0;
        _currLine.HasSuperscript = _currLine.HasSuperscript || segment.SuperscriptDepth > 0;

        LOG("Added token: %.*s [itemtype: %s][font-size: %f][size: (%f, %f)]\n",
            (int)token.Content.size(), token.Content.data(),
            GetTokenTypeString(token), style.font.size,
            token.Bounds.width, token.Bounds.height);
    }

    SegmentData& DefaultTagVisitor::AddSegment()
    {
        auto& segment = _currLine.Segments.emplace_back();
        segment.StyleIdx = _currStyleIdx;
        segment.SubscriptDepth = _currSubscriptLevel;
        segment.SuperscriptDepth = _currSuperscriptLevel;
        return segment;
    }

    SegmentData& DefaultTagVisitor::AddSegment(DrawableLine& line, int styleIdx)
    {
        auto& segment = line.Segments.emplace_back();
        segment.StyleIdx = styleIdx;
        segment.SubscriptDepth = _currSubscriptLevel;
        segment.SuperscriptDepth = _currSuperscriptLevel;
        return segment;
    }

    void DefaultTagVisitor::GenerateTextToken(std::string_view content)
    {
        Token token;
        token.Content = content;
        AddToken(token, NoStyleChange);
    }

    std::vector<DefaultTagVisitor::TokenPositionRemapping> DefaultTagVisitor::PerformWordWrap(int index)
    {
        // Word wrapping happens through the registered text shaper in _config member
        // Since a single line can now map to multiple lines, we record the mappings 
        // of original (line, segment, token) triplet to newer triplets in the broken
        // up lines. This information is crucial to re-layout backgrounds.
        LOG("Performing word wrap on line #%d", index);

        // TODO: Reduce memory allocations in this function, the result could be reused.
        std::vector<DefaultTagVisitor::TokenPositionRemapping> result;
        auto& lines = _result.ForegroundLines;

        if (!lines[index].HasText || !_config.WordWrap || (_bounds.x <= 0.f))
        {
            return result;
        }

        struct TokenInfo
        {
            int styleIdx, segmentIdx, tokenIdx;
        };

        std::vector<DrawableLine> newlines;
        std::vector<std::string_view> words;
        std::vector<TokenInfo> tokenIndexes;

        auto currline = CreateNewLine(-1);
        AddSegment(currline, -1);

        auto currentx = 0.f;
        const auto& currStyle = _result.StyleDescriptors[_currStyleIdx + 1];
        auto availwidth = currStyle.propsSpecified & StyleWidth ? std::min(_bounds.x, currStyle.width) : _bounds.x;
        auto segmentIdx = 0;

        // In order to preserve the styleIdx and depth information of original segments, 
        // create a vector of (segment, token, style, depth) from original line.
        // This information is then used to create the new segments in the new lines
        // created as a result of word wrapping
        for (auto& segment : lines[index].Segments)
        {
            auto tokenIdx = 0;

            for (auto& token : segment.Tokens)
            {
                if (token.Type == TokenType::Text)
                {
                    tokenIndexes.emplace_back(TokenInfo{ segment.StyleIdx, segmentIdx, tokenIdx });
                    words.push_back(token.Content);
                    ++tokenIdx;
                }
            }

            ++segmentIdx;
        }

        struct UserData
        {
            const std::vector<StyleDescriptor>& styles;
            const std::vector<TokenInfo>& tokenIndexes;
            std::vector<DrawableLine>& newlines;
            std::vector<DefaultTagVisitor::TokenPositionRemapping>& result;
            DrawableLine& currline;
            DrawableLine& targetline;
            DefaultTagVisitor* self;
            int index;
        };

        UserData data{ _result.StyleDescriptors, tokenIndexes, newlines, 
            result, currline, lines[index], this, index };

        _config.TextShaper->ShapeText(availwidth, { words.begin(), words.end() },
            [](int wordIdx, void* userdata) {
                const auto& data = *reinterpret_cast<UserData*>(userdata);
                const auto& style = data.styles[data.tokenIndexes[wordIdx].styleIdx + 1];
                return ITextShaper::WordProperty{ style.font.font, style.font.size, style.wbbhv };
            },
            [](int wordIdx, void* userdata) {
                const auto& data = *reinterpret_cast<UserData*>(userdata);
                data.newlines.push_back(data.currline);

                data.currline = CreateNewLine(-1);
                data.self->AddSegment(data.currline, data.tokenIndexes[wordIdx].styleIdx);
            },
            [](int wordIdx, std::string_view word, ImVec2 dim, void* userdata) {
                const auto& data = *reinterpret_cast<UserData*>(userdata);
                const auto& tidx = data.tokenIndexes[wordIdx];

                if ((wordIdx > 0) && (data.tokenIndexes[wordIdx - 1].styleIdx != tidx.styleIdx))
                    data.self->AddSegment(data.currline, tidx.styleIdx);
                else 
                {
                    auto& segment = data.currline.Segments.back();
                    segment.StyleIdx = tidx.styleIdx;
                }

                const auto& token = data.targetline.Segments[tidx.segmentIdx].Tokens[tidx.tokenIdx];
                auto& segment = data.currline.Segments.back();
                auto& ntk = segment.Tokens.emplace_back(token);

                ntk.VisibleTextSize = (int16_t)(word.size());
                ntk.Content = word;
                ntk.Bounds.width = dim.x;
                ntk.Bounds.height = dim.y;

                auto& remap = data.result.emplace_back();
                remap.oldIdx.lineIdx = data.index;
                remap.oldIdx.segmentIdx = tidx.segmentIdx;
                remap.oldIdx.tokenIdx = tidx.tokenIdx;
                remap.newIdx.lineIdx = (int)data.newlines.size() + data.index;
                remap.newIdx.segmentIdx = (int)data.currline.Segments.size() - 1;
                remap.newIdx.tokenIdx = (int)segment.Tokens.size() - 1;
            },
            _config, &data);

        newlines.push_back(currline);
        auto it = lines.erase(lines.begin() + index);
        auto sz = (int)lines.size();
        lines.insert(it, newlines.begin(), newlines.end());
        return result;
    }

    void DefaultTagVisitor::UpdateBackgroundSpan(int startDepth, int lineIdx, const std::vector<TokenPositionRemapping>& remapping)
    {
        // The background spans that are recorded in TagStart/TagEnd are invalid in case of
        // word wrapping since a single line now maps to multiple lines.
        // Hence, from the (line, segment, token) remapping b/w original line and broken up
        // lines, we find out which segments from the original line now span to what extent
        // in the new lines. Since a single segment from original can be broken into multiple
        // lines, hence, one segment now maps to (line, segment) from start to end.
        struct SegmentRemap
        {
            int segmentIdx;
            std::pair<int, int> from;
            std::pair<int, int> to;
        };

        std::vector<SegmentRemap> segmentMappings;

        for (auto idx = 0; idx < (int)remapping.size(); ++idx)
        {
            auto& entry = segmentMappings.emplace_back();
            entry.segmentIdx = remapping[idx].oldIdx.segmentIdx;
            entry.from = { remapping[idx].newIdx.lineIdx, remapping[idx].newIdx.segmentIdx };
            
            while ((idx < (int)remapping.size()) &&
                (entry.segmentIdx == remapping[idx].oldIdx.segmentIdx)) idx++;
            idx--;
            entry.to = { remapping[idx].newIdx.lineIdx, remapping[idx].newIdx.segmentIdx };
        }

        for (auto depth = startDepth; depth <= _maxDepth; ++depth)
        {
            for (auto bidx = 0; bidx < (int)_backgroundBlocks[depth].size(); ++bidx)
            {
                auto& block = _backgroundBlocks[depth][bidx];
                if (block.span.end.first == -1) continue;
                if (block.span.start.first == lineIdx)
                    for (const auto& segment : segmentMappings)
                        if (segment.segmentIdx == block.span.start.second)
                        {
                            block.span.start = segment.from;
                            block.span.end = segment.to;
                            break;
                        }
            }
        }
    }

    void DefaultTagVisitor::ComputeSuperSubscriptOffsets(const std::pair<int, int>& indexes)
    {
        auto& lines = _result.ForegroundLines;

        for (auto idx = indexes.first; idx < (indexes.first + indexes.second); ++idx)
        {
            auto& line = lines[idx];
            if (!line.HasSubscript && !line.HasSuperscript) continue;

            auto maxTopOffset = GetMaxSuperscriptOffset(line, _config.ScaleSuperscript);
            auto maxBottomOffset = GetMaxSubscriptOffset(line, _config.ScaleSubscript);
            auto lastFontSz = _config.DefaultFontSize * _config.FontScale;
            auto lastSuperscriptDepth = 0, lastSubscriptDepth = 0;

            for (const auto& segment : line.Segments)
            {
                auto& style = _result.StyleDescriptors[segment.StyleIdx + 1];

                if (segment.SuperscriptDepth > lastSuperscriptDepth)
                {
                    style.font.size = lastFontSz * _config.ScaleSuperscript;
                    maxTopOffset -= style.font.size * 0.5f;
                }
                else if (segment.SuperscriptDepth < lastSuperscriptDepth)
                {
                    maxTopOffset += lastFontSz * 0.5f;
                    style.font.size = lastFontSz / _config.ScaleSuperscript;
                }

                if (segment.SubscriptDepth > lastSubscriptDepth)
                {
                    style.font.size = lastFontSz * _config.ScaleSubscript;
                    maxBottomOffset += (lastFontSz - style.font.size * 0.5f);
                }
                else if (segment.SubscriptDepth < lastSubscriptDepth)
                {
                    style.font.size = lastFontSz / _config.ScaleSubscript;
                    maxBottomOffset -= style.font.size * 0.5f;
                }

                style.superscriptOffset = maxTopOffset;
                style.subscriptOffset = maxBottomOffset;
                lastSuperscriptDepth = segment.SuperscriptDepth;
                lastSubscriptDepth = segment.SubscriptDepth;
                lastFontSz = style.font.size;
            }
        }
    }

    void DefaultTagVisitor::UpdateLineGeometry(const std::pair<int, int>& linesModified, int depth)
    {
        auto& result = _result.ForegroundLines;

        if (_currHasBgBlock)
            RecordBackgroundSpanEnd(true, false, depth, true);

        for (auto lineIdx = 0; lineIdx < (linesModified.first + linesModified.second); ++lineIdx)
        {
            auto segmentIdx = 0;
            auto& line = result[lineIdx];
            line.Content.width = line.Content.height = 0.f;
            auto currx = line.Content.left + line.Offset.left;

            if (lineIdx > 0) line.Content.top = result[lineIdx - 1].Content.top + result[lineIdx - 1].height() + _config.LineGap;

            for (auto& segment : line.Segments)
            {
                if (segment.Tokens.empty()) continue;
                
                segment.Bounds.top = line.Content.top + line.Offset.top;
                segment.Bounds.left = currx;
                segment.Bounds.width = segment.Bounds.height = 0.f;
                const auto& style = _result.StyleDescriptors[segment.StyleIdx + 1];
                auto [depth, bgidx, found, considerTop, considerBottom] = GetBlockSpanIndex(lineIdx, segmentIdx);

                if (found)
                {
                    auto& block = _backgroundBlocks[depth][bgidx].shape;
                    segment.Bounds.left += block.margin.left;
                    currx += block.padding.left + block.Border.left.thickness + block.margin.left;
                    if (considerTop) segment.Bounds.top += block.margin.top;
                }
                
                auto height = 0.f;

                for (auto tokidx = 0; tokidx < (int)segment.Tokens.size(); ++tokidx)
                {
                    auto& token = segment.Tokens[tokidx];
                    token.Bounds.top = segment.Bounds.top + style.superscriptOffset + style.subscriptOffset;
                    if (considerTop) token.Bounds.top += _backgroundBlocks[depth][bgidx].shape.padding.top + 
                        _backgroundBlocks[depth][bgidx].shape.Border.top.thickness;

                    // TODO: Fix bullet positioning w.r.t. first text block (baseline aligned?)
                    /*if ((token.Type == TokenType::ListItemBullet) && ((tokidx + 1) < (int)segment.Tokens.size()))
                         segment.Tokens[tokidx + 1]*/
                    token.Bounds.left = currx + token.Offset.left;
                    currx += token.Bounds.width + token.Offset.h();
                    height = std::max(height, token.Bounds.height);
                }

                if (found)
                {
                    auto& block = _backgroundBlocks[depth][bgidx].shape;
                    currx += block.padding.right + block.Border.right.thickness;
                    segment.Bounds.width = currx - segment.Bounds.left;
                    currx += block.margin.right;
                    line.Content.width += segment.Bounds.width + block.margin.right;
                    if (considerBottom) segment.Bounds.height = block.padding.v() + block.Border.v();
                }
                else
                {
                    segment.Bounds.width = currx - segment.Bounds.left;
                    line.Content.width += segment.Bounds.width;
                }
                    
                segment.Bounds.height += height;
                line.Content.height = std::max(segment.Bounds.top + segment.Bounds.height - line.Content.top , 
                    line.Content.height);
                segmentIdx++;
            }

            HIGHLIGHT("\nCreated line #%d at (%f, %f) of size (%f, %f) with %d segments", index,
                line.Content.left, line.Content.top, line.Content.width, line.Content.height,
                (int)line.Segments.size());
        }
    }

    void DefaultTagVisitor::RecordBackgroundSpanStart()
    {
        auto& block = _backgroundBlocks[_currentStackPos].emplace_back();
        block.span.start.first = (int)_result.ForegroundLines.size();
        block.span.start.second = (int)_currLine.Segments.size() - 1;
        block.styleIdx = _currStyleIdx;
        block.shape = _currBgBlock;
        block.isMultilineCapable = CanContentBeMultiline(_currTagType);
        _currBgBlock = DrawableBlock{};
        _pendingBgBlockCreation = false;
    }

    void DefaultTagVisitor::RecordBackgroundSpanEnd(bool lineAdded, bool segmentAdded, int depth, bool includeChildren)
    {
        if (includeChildren)
        {
            for (auto childDepth = depth; childDepth < IM_RICHTEXT_MAXDEPTH; ++childDepth)
            {
                for (auto& block : _backgroundBlocks[childDepth])
                {
                    auto currLineIdx = (int)_result.ForegroundLines.size() - (lineAdded ? 1 : 0);

                    if (block.span.end.first == -1)
                    {
                        block.span.end.first = std::max(currLineIdx, block.span.start.first);
                        block.span.end.second = lineAdded ?
                            std::max(0, (int)_result.ForegroundLines.back().Segments.size() - (segmentAdded ? 2 : 1)) :
                            std::max(0, (int)_currLine.Segments.size() - (segmentAdded ? 2 : 1));
                    }
                }
            }
        }
        else if (!_backgroundBlocks[depth].empty())
        {
            auto currLineIdx = (int)_result.ForegroundLines.size() - (lineAdded ? 1 : 0);
            auto& block = _backgroundBlocks[depth].back();

            if (block.span.end.first == -1)
            {
                block.span.end.first = std::max(currLineIdx, block.span.start.first);
                block.span.end.second = lineAdded ?
                    std::max(0, (int)_result.ForegroundLines.back().Segments.size() - (segmentAdded ? 2 : 1)) :
                    std::max(0, (int)_currLine.Segments.size() - (segmentAdded ? 2 : 1));
            }
        }
    }

    DrawableLine DefaultTagVisitor::MoveToNextLine(bool isTagStart, int depth)
    {
        auto isEmpty = IsLineEmpty(_currLine);
        std::pair<int, int> linesModified;
        _result.ForegroundLines.emplace_back(_currLine);
        auto lineIdx = (int)_result.ForegroundLines.size() - 1;
        const auto& style = _result.StyleDescriptors[_currStyleIdx + 1];

        if (_currLine.Segments.size() == 1u && _currLine.Segments.front().Tokens.size() == 1u &&
            _currLine.Segments.front().Tokens.front().Type == TokenType::HorizontalRule)
        {
            linesModified = std::make_pair(lineIdx, 1);
        }
        else
        {
            linesModified = std::make_pair(lineIdx, 1);
            UpdateLineGeometry(linesModified, depth);
            auto xwidth = _currStyle.propsSpecified & StyleWidth ? _currStyle.width : _bounds.x;

            if (!_currLine.Marquee && xwidth > 0.f && (style.font.flags & FontStyleNoWrap) == 0 &&
                _result.ForegroundLines.back().width() > xwidth)
            {
                auto remapping = PerformWordWrap(lineIdx);
                UpdateBackgroundSpan(depth, lineIdx, remapping);
            }

            linesModified = std::make_pair(linesModified.first, (int)_result.ForegroundLines.size() - linesModified.first);
        }

        ComputeSuperSubscriptOffsets(linesModified);
        _maxDepth = 0;

        auto& lastline = _result.ForegroundLines.back();
        auto newline = CreateNewLine(_currStyleIdx);
        newline.BlockquoteDepth = _currBlockquoteDepth;
        if (isTagStart) newline.Marquee = _currTagType == TagType::Marquee;

        if (_currBlockquoteDepth > 0) newline.Offset.left = newline.Offset.right = _config.BlockquotePadding;
        if (_currBlockquoteDepth > lastline.BlockquoteDepth) newline.Offset.top = _config.BlockquotePadding;
        else if (_currBlockquoteDepth < lastline.BlockquoteDepth) lastline.Offset.bottom = _config.BlockquotePadding;

        UpdateLineGeometry(linesModified, depth);
        CreateElidedTextToken(_result.ForegroundLines.back(), style, _config, _bounds);

        newline.Content.left = ((float)(_currListDepth + 1) * _config.ListItemIndent) +
            ((float)(_currBlockquoteDepth + 1) * _config.BlockquoteOffset);
        newline.Content.top = lastline.Content.top + lastline.height() + (isEmpty ? 0.f : _config.LineGap);
        return newline;
    }

    float DefaultTagVisitor::GetMaxSuperscriptOffset(const DrawableLine& line, float scale) const
    {
        auto topOffset = 0.f;
        auto baseFontSz = 0.f;

        for (auto idx = 0; idx < (int)line.Segments.size();)
        {
            const auto& segment = line.Segments[idx];
            baseFontSz = _result.StyleDescriptors[segment.StyleIdx + 1].font.size;
            auto depth = 0, begin = idx;

            while ((idx < (int)line.Segments.size()) && (line.Segments[idx].SuperscriptDepth > 0))
            {
                depth = std::max(depth, segment.SuperscriptDepth);
                idx++;
            }

            topOffset = std::max(topOffset, CalcVerticalOffset(depth, baseFontSz, scale));
            if (idx == begin) idx++;
        }

        return topOffset;
    }

    float DefaultTagVisitor::GetMaxSubscriptOffset(const DrawableLine& line, float scale) const
    {
        auto topOffset = 0.f;
        auto baseFontSz = 0.f;

        for (auto idx = 0; idx < (int)line.Segments.size();)
        {
            const auto& segment = line.Segments[idx];
            baseFontSz = _result.StyleDescriptors[segment.StyleIdx + 1].font.size;
            auto depth = 0, begin = idx;

            while ((idx < (int)line.Segments.size()) && (line.Segments[idx].SubscriptDepth > 0))
            {
                depth = std::max(depth, segment.SubscriptDepth);
                idx++;
            }

            topOffset = std::max(topOffset, CalcVerticalOffset(depth, baseFontSz, scale));
            if (idx == begin) idx++;
        }

        return topOffset;
    }

    std::tuple<int, int, bool, bool, bool> DefaultTagVisitor::GetBlockSpanIndex(int lineIdx, int segmentIdx) const
    {
        auto depth = 0, bgidx = 0;
        auto found = false, considerTop = false, considerBottom = false;

        for (; depth < IM_RICHTEXT_MAXDEPTH; ++depth)
        {
            bgidx = 0;

            for (const auto& block : _backgroundBlocks[depth])
            {
                const auto& [from, to] = block.span;

                if (from.first <= lineIdx && to.first >= lineIdx &&
                    from.second <= segmentIdx && to.second >= segmentIdx)
                {
                    considerTop = from.first == lineIdx;
                    considerBottom = to.first == lineIdx;
                    found = true;
                    break;
                }

                bgidx++;
            }

            if (found) break;
        }

        return std::make_tuple(depth, bgidx, found, considerTop, considerBottom);
    }

    StyleDescriptor& DefaultTagVisitor::Style(int stackpos)
    {
        return stackpos < 0 ? _result.StyleDescriptors.front() : 
            _result.StyleDescriptors[_tagStack[stackpos].styleIdx + 1];
    }

    bool DefaultTagVisitor::CreateNewStyle()
    {
        auto parentIdx = _currentStackPos <= 0 ? -1 : _styleIndexStack[_currentStackPos - 1];
        const auto& parentStyle = _result.StyleDescriptors[parentIdx + 1];
        SetImplicitStyleProps(_currTagType, _currTag, _currStyle, parentStyle, _currBgBlock, 
            _currLine, _config);
        auto hasUniqueStyle = _currStyle.propsSpecified != 0;

        if (hasUniqueStyle)
        {
            // Since any of these style attributes applies to an entire block, 
            // minus the text content, presence of such properties imply creation
            // of background block
            if ((_currStyle.propsSpecified & StyleBackground) ||
                (_currStyle.propsSpecified & StyleBorder) ||
                (_currStyle.propsSpecified & StyleBoxShadow) ||
                (_currStyle.propsSpecified & StylePadding) ||
                (_currStyle.propsSpecified & StyleMargin))
            {
                _currHasBgBlock = _tagStack[_currentStackPos].hasBackground = true;
            }

            _result.StyleDescriptors.emplace_back(_currStyle);
            _currStyleIdx = ((int)_result.StyleDescriptors.size() - 2);
        }

        _styleIndexStack[_currentStackPos] = _currStyleIdx;
        _tagStack[_currentStackPos].styleIdx = _currStyleIdx;
        return hasUniqueStyle;
    }

    void DefaultTagVisitor::PopCurrentStyle()
    {
        if (_currStyleIdx == -1) return;

        // Save previous style index, as this will be used to figure out style mismatch
        // and hence the need to create more segments when processing content...
        _prevStyleIdx = _currStyleIdx;
        _prevTagType = _currTagType;

        // Make _currStyle refer to parent style, if there are no parents,
        // reference the default style at index 0 i.e. -1 (since 1 is added during access)
        _currStyleIdx = _currentStackPos >= 0 ? _styleIndexStack[_currentStackPos] : -1;
        _currStyle = _result.StyleDescriptors[_currStyleIdx + 1];
        if ((_currentStackPos - 1) < IM_RICHTEXT_MAXDEPTH) _styleIndexStack[_currentStackPos + 1] = -2;

        //if (_currTagType != TagType::LineBreak)
        //{
        //    // This reset is necessary as we are popping the style, and CreateNewStyle will create
        //    // a new style instead if not unset
        //    // TODO: Restore this value once popping is done
        //    _currStyle.propsSpecified = 0;
        //    _currStyle.superscriptOffset = _currStyle.subscriptOffset = 0.f;
        //}
    }

    bool DefaultTagVisitor::TagStart(std::string_view tag)
    {
        if (!CanContentBeMultiline(_currTagType) && AreSame(tag, "br")) return true;
        if (_pendingBgBlockCreation) RecordBackgroundSpanStart();
        _prevTagType = _currTagType;

        LOG("Entering Tag: <%.*s>\n", (int)tag.size(), tag.data());
        _currTag = tag;
        _currTagType = GetTagType(tag, _config.IsStrictHTML5);
        _currHasBgBlock = false;
        //PopCurrentStyle();
            
        PushTag(_currTag, _currTagType);
        if (_currTagType == TagType::Superscript) _currSuperscriptLevel++;
        else if (_currTagType == TagType::Subscript) _currSubscriptLevel++;

        if (_currentStackPos >= 0 && _tagStack[_currentStackPos].tag != _currTag)
            ERROR("Tag mismatch...");
        _lastOp = Operation::TagStart;
        _maxDepth++;
        return true;
    }
        
    bool DefaultTagVisitor::Attribute(std::string_view name, std::optional<std::string_view> value)
    {
        LOG("Reading attribute: %.*s\n", (int)name.size(), name.data());
        auto propsSpecified = 0;
        auto nonStyleAttribute = false;
        const auto& parentStyle = Style(_currentStackPos - 1);
        std::tie(propsSpecified, nonStyleAttribute) = RecordTagProperties(
            _currTagType, name, value, _currStyle, _currBgBlock, _currTagProps, parentStyle, _config);

        if (!nonStyleAttribute)
            _currStyle.propsSpecified |= propsSpecified;

        return true;
    }

    bool DefaultTagVisitor::TagStartDone()
    {
        auto hasSegments = !_currLine.Segments.empty();
        auto hasUniqueStyle = CreateNewStyle();
        auto& currentStyle = Style(_currentStackPos);
        int16_t tagPropIdx = -1;
        auto currListIsNumbered = false;

        if (_currTagProps != TagPropertyDescriptor{})
        {
            tagPropIdx = (int16_t)_result.TagDescriptors.size();
            _result.TagDescriptors.emplace_back(_currTagProps);
        }

        if (_currTagType == TagType::List)
        {
            _currListDepth++;
            currListIsNumbered = AreSame(_currTag, "ol");
        }
        else if (_currTagType == TagType::Paragraph || _currTagType == TagType::Header ||
            _currTagType == TagType::RawText || _currTagType == TagType::ListItem ||
            _currTagType == TagType::CodeBlock || _currTagType == TagType::Marquee)
        {
            if (hasSegments)
                _currLine = MoveToNextLine(true, _currentStackPos);
            _maxWidth = std::max(_maxWidth, _result.ForegroundLines.empty() ? 0.f : 
                _result.ForegroundLines.back().Content.width);

            if (_currTagType == TagType::Paragraph && _config.ParagraphStop > 0)
                _currLine.Offset.left += _config.Renderer->GetTextSize(std::string_view{ LineSpaces,
                    (std::size_t)std::min(_config.ParagraphStop, IM_RICHTEXT_MAXTABSTOP) }, 
                    currentStyle.font.font, currentStyle.font.size).x;
            else if (_currTagType == TagType::ListItem)
            {
                _listItemCountByDepths[_currListDepth]++;

                Token token;
                auto& listItem = _result.ListItemTokens.emplace_back();
                token.Type = !currListIsNumbered ? TokenType::ListItemBullet :
                    TokenType::ListItemNumbered;
                listItem.ListDepth = _currListDepth;
                listItem.ListItemIndex = _listItemCountByDepths[_currListDepth];
                token.ListPropsIdx = (int16_t)(_result.ListItemTokens.size() - 1u);

                AddSegment();
                AddToken(token, currentStyle.propsSpecified);
            }
        }
        else if (_currTagType == TagType::Blockquote)
        {
            _currBlockquoteDepth++;
            if (!_currLine.Segments.empty())
                _currLine = MoveToNextLine(true, _currentStackPos);
            _maxWidth = std::max(_maxWidth, _result.ForegroundLines.empty() ? 0.f : 
                _result.ForegroundLines.back().Content.width);
            auto& start = _blockquoteStack[_currBlockquoteDepth].bounds.emplace_back();
            start.first = ImVec2{ _currLine.Content.left, _currLine.Content.top };
        }
        else if (_currTagType == TagType::Quotation)
        {
            Token token;
            token.Type = TokenType::Text;
            token.Content = "\"";
            if (!hasSegments || (_prevStyleIdx != _currStyleIdx))
            {
                _prevStyleIdx = _currStyleIdx;
                AddSegment();
            }
            AddToken(token, currentStyle.propsSpecified);
        }
        else if (_currTagType == TagType::Meter)
        {
            Token token;
            token.Type = TokenType::Meter;
            token.PropertiesIdx = tagPropIdx;
            if (!hasSegments || (_prevStyleIdx != _currStyleIdx))
            {
                _prevStyleIdx = _currStyleIdx;
                AddSegment();
            }
            AddToken(token, currentStyle.propsSpecified);
        }

        if (_currHasBgBlock) _pendingBgBlockCreation = true;

        _lastOp = Operation::TagStartDone;
        return true;
    }

    bool DefaultTagVisitor::Content(std::string_view content)
    {
        struct UserData
        {
            StyleDescriptor& currentStyle;
            DrawableLine& currline;
            std::vector<DrawableLine>& newlines;
            std::string_view content;
            int styleIdx;
            DefaultTagVisitor* self;
        };

        // Ignore newlines, tabs & consecutive spaces
        auto to = 0, from = 0;
        auto& currentStyle = Style(_currentStackPos);
        LOG("Processing content [%.*s]\n", (int)content.size(), content.data());

        // If last processed entry was tag end, then this is a continuation of
        // text content of the parent tag of last tag. Hence, if style differs,
        // create a new segment (or if current line is empty)
        auto isSegmentCreatingOp = _lastOp == Operation::TagEnd || _lastOp == Operation::None ||
            _lastOp == Operation::TagStartDone;
        if ((isSegmentCreatingOp && _currStyleIdx != _prevStyleIdx) || _currLine.Segments.empty())
            AddSegment();

        if (_pendingBgBlockCreation) RecordBackgroundSpanStart();

        auto curridx = 0, start = 0;
        auto ignoreLineBreaks = _currSuperscriptLevel > 0 || _currSubscriptLevel > 0;
        auto isPreformatted = IsPreformattedContent(_currTag);
        UserData userdata{ currentStyle, _currLine, _result.ForegroundLines, content, _currStyleIdx, this };

        _config.TextShaper->SegmentText(content, _currStyle.wscbhv, 
            [](int, void* userdata)
            {
                const auto& data = *reinterpret_cast<UserData*>(userdata);
                data.newlines.push_back(data.currline);

                data.currline = CreateNewLine(-1);
                data.self->AddSegment();
                data.currline.Segments.back().StyleIdx = data.styleIdx;
            }, 
            [](int, std::string_view word, ImVec2 dim, void* userdata)
            {
                const auto& data = *reinterpret_cast<UserData*>(userdata);
                data.self->GenerateTextToken(word);
            }, 
            _config, ignoreLineBreaks, isPreformatted, &userdata);
        _lastOp = Operation::Content;
        return true;
    }

    bool DefaultTagVisitor::TagEnd(std::string_view tag, bool selfTerminatingTag)
    {
        if (!CanContentBeMultiline(_currTagType) && AreSame(tag, "br")) return true;

        // pop style properties and reset
        PopTag(!selfTerminatingTag);
        PopCurrentStyle();

        auto segmentAdded = false, lineAdded = false;
        LOG("Exited Tag: <%.*s>\n", (int)_currTag.size(), _currTag.data());

        if (_currTagType == TagType::List || _currTagType == TagType::Paragraph || 
            _currTagType == TagType::Header ||
            _currTagType == TagType::RawText || _currTagType == TagType::Blockquote || 
            _currTagType == TagType::LineBreak ||
            _currTagType == TagType::CodeBlock || _currTagType == TagType::Marquee)
        {
            if (_currTagType == TagType::List)
            {
                _listItemCountByDepths[_currListDepth] = 0;
                _currListDepth--;
            }

            _currLine.Marquee = _currTagType == TagType::Marquee;
            _currLine = MoveToNextLine(false, _currentStackPos + 1);
            _maxWidth = std::max(_maxWidth, _result.ForegroundLines.back().Content.width);
            lineAdded = true;

            if (_currTagType == TagType::Blockquote)
            {
                assert(!_blockquoteStack[_currBlockquoteDepth].bounds.empty());
                auto& bounds = _blockquoteStack[_currBlockquoteDepth].bounds.back();
                const auto& lastLine = _result.ForegroundLines[_result.ForegroundLines.size() - 2u];
                bounds.second = ImVec2{ lastLine.width() + bounds.first.x, lastLine.Content.top + lastLine.height() };
                _currBlockquoteDepth--;
            }
            else if (_currTagType == TagType::Header)
            {
                // Add properties for horizontal line below header
                StyleDescriptor style = _currStyle;
                style.height = 1.f;
                style.fgcolor = _config.HeaderLineColor;
                _result.StyleDescriptors.emplace_back(style);
                AddSegment().StyleIdx = (int)_result.StyleDescriptors.size() - 2;

                Token token;
                token.Type = TokenType::HorizontalRule;
                AddToken(token, NoStyleChange);

                // Move to next line for other content
                _currLine = MoveToNextLine(false, _currentStackPos + 1);
                _maxWidth = std::max(_maxWidth, _result.ForegroundLines.back().Content.width);
                segmentAdded = false;
            }
        }
        else if (_currTagType == TagType::Hr)
        {
            if (!_currLine.Segments.empty())
            {
                _currLine = MoveToNextLine(false, _currentStackPos + 1);
            }

            _maxWidth = std::max(_maxWidth, _result.ForegroundLines.empty() ? 0.f : 
                _result.ForegroundLines.back().Content.width);

            Token token;
            token.Type = TokenType::HorizontalRule;
            AddSegment();
            AddToken(token, NoStyleChange);

            _currLine = MoveToNextLine(true, _currentStackPos + 1);
            _maxWidth = std::max(_maxWidth, _result.ForegroundLines.back().Content.width);
            lineAdded = true;
        }
        else if (_currTagType == TagType::Quotation)
        {
            Token token;
            token.Type = TokenType::Text;
            token.Content = "\"";
            AddToken(token, NoStyleChange);
        }
        else if (_currTagType != TagType::Unknown)
        {
            if (_currTagType == TagType::Superscript)
            {
                _currSuperscriptLevel--;
                AddSegment();
                segmentAdded = true;
            }
            else if (_currTagType == TagType::Subscript)
            {
                _currSubscriptLevel--;
                AddSegment();
                segmentAdded = true;
            }
        }

        if (!selfTerminatingTag && _currHasBgBlock)
            RecordBackgroundSpanEnd(lineAdded, segmentAdded, _currentStackPos + 1, false);

        // Update all members for next tag in stack
        if (selfTerminatingTag) _tagStack[_currentStackPos + 1] = StackData{};
        _currTag = _currentStackPos == -1 ? "" : _tagStack[_currentStackPos].tag;
        _currTagType = _currentStackPos == -1 ? TagType::Unknown : _tagStack[_currentStackPos].tagType;
        _currHasBgBlock = _currentStackPos == -1 ? false : _tagStack[_currentStackPos].hasBackground;
        _currTagProps = TagPropertyDescriptor{};
        _lastOp = Operation::TagEnd;
        return true;
    }

    static void UpdateRelativeToAbs(DrawableBlock& block)
    {
        auto width = block.End.x - block.Start.x;
        auto height = block.End.y - block.End.x;
        auto length = std::min(width, height);
        if (block.BorderCornerRel & (1 << glimmer::TopLeftCorner)) block.Border.cornerRadius[glimmer::TopLeftCorner] *= length;
        if (block.BorderCornerRel & (1 << glimmer::TopRightCorner)) block.Border.cornerRadius[glimmer::TopRightCorner] *= length;
        if (block.BorderCornerRel & (1 << glimmer::BottomRightCorner)) block.Border.cornerRadius[glimmer::BottomRightCorner] *= length;
        if (block.BorderCornerRel & (1 << glimmer::BottomLeftCorner)) block.Border.cornerRadius[glimmer::BottomLeftCorner] *= length;
    }

    void DefaultTagVisitor::Finalize()
    {
        MoveToNextLine(false, 0);
        _maxWidth = std::max(_maxWidth, _result.ForegroundLines.back().Content.width);

        // Default aligment of segments is left horizontally and centered vertically in the current line
        for (auto index = 0; index < (int)_result.ForegroundLines.size(); ++index)
        {
            auto& line = _result.ForegroundLines[index];

            for (auto& segment : line.Segments)
            {
                for (auto& token : segment.Tokens)
                    token.Bounds.top += (line.height() - token.Bounds.height) * 0.5f;
                segment.Bounds.top += (line.height() - segment.Bounds.height) * 0.5f;
            }
        }

        // Apply alignment to geometry
        for (auto& line : _result.ForegroundLines)
        {
            if (line.Marquee) line.Content.width = _maxWidth;

            for (auto& segment : line.Segments)
            {
                auto& segment = line.Segments.front();
                auto& style = _result.StyleDescriptors[segment.StyleIdx + 1];

                // If complete text is already clipped, do not apply alignment
                if (segment.Tokens.size() == 1u && (segment.Tokens.front().Type == TokenType::Text ||
                    segment.Tokens.front().Type == TokenType::ElidedText) &&
                    segment.Tokens.front().VisibleTextSize < (int16_t)segment.Tokens.front().Content.size())
                    continue;

                if ((style.alignment & TextAlignHCenter) || (style.alignment & TextAlignRight)
                    || (style.alignment & TextAlignJustify))
                {
                    float occupiedWidth = line.width();
                    auto leftover = _maxWidth - occupiedWidth;

                    for (auto tidx = 0; tidx < (int)segment.Tokens.size(); ++tidx)
                    {
                        auto& token = segment.Tokens[tidx];
                        
                        if (style.alignment & TextAlignHCenter)
                            token.Offset.left += leftover * 0.5f;
                        else if (style.alignment & TextAlignRight)
                            token.Offset.left += leftover;
                        else if (style.alignment & TextAlignJustify)
                        {
                            if (tidx == (int)(segment.Tokens.size() - 1u)) break;
                            token.Offset.right += (leftover / (float)(segment.Tokens.size() - 1u));
                        }
                    }

                    // Update segment's bounding box
                    if (style.alignment & TextAlignHCenter)
                        segment.Bounds.left += leftover * 0.5f;
                    else if (style.alignment & TextAlignRight)
                        segment.Bounds.left += leftover;
                    else if (style.alignment & TextAlignJustify)
                    {
                        segment.Bounds.left = 0.f;
                        segment.Bounds.width = _maxWidth;
                    }   
                }

                // TODO: If entire content is inside <center> tag, perform global vertical centering
                //       of those lines
                if ((style.alignment & TextAlignVCenter) || (style.alignment & TextAlignBottom))
                {
                    float occupiedHeight = segment.height();

                    for (auto& token : segment.Tokens)
                    {
                        if (style.alignment & TextAlignTop)
                            token.Offset.top = 0.f;
                        else if (style.alignment & TextAlignBottom)
                            token.Offset.top = line.height() - token.Bounds.height;
                    }

                    if (style.alignment & TextAlignTop)
                        segment.Bounds.top = line.Content.top;
                    else if (style.alignment & TextAlignBottom)
                        segment.Bounds.top += line.height() - occupiedHeight;
                }
            }
        }

        // Process backgrounds in increasing depth order i.e. Painter's algorithm
        for (auto depth = 0; depth < IM_RICHTEXT_MAXDEPTH; ++depth)
        {
            /*for (const auto& bound : _blockquoteStack[depth].bounds)
            {
                if (config.BlockquoteBarWidth > 1.f && config.DefaultBgColor != config.BlockquoteBar)
                    result.BackgroundBlocks.emplace_back(DrawableBlock{ bound.first, ImVec2{ config.BlockquoteBarWidth, bound.second.y },
                        config.BlockquoteBar });

                if (config.DefaultBgColor != config.BlockquoteBg)
                    result.BackgroundBlocks.emplace_back(DrawableBlock{ ImVec2{ bound.first.x + config.BlockquoteBarWidth, bound.first.y },
                        bound.second, config.BlockquoteBg });
            }*/

            // Create background blocks for each depth and reset original specifications
            // There are three kinds of background geometries:
            // 1. Backgrounds that can span multiple lines i.e. <p> tag, for such case,
            //    the background spans the entire region across lines.
            // 2. Backgrounds that are limited to one line but got split due to text layout,
            //    such backgrounds should be generated as separate blocks across the lines
            //    (The behavior for such backgrounds is copied from web-browsers, see: 
            //    https://jsfiddle.net/9zrLyo6s/ for the reference behavior)
            // 3. Backgrounds that are limited to one line and did not split after text layout,
            //    This is the simplest case, generate simple geometry.
            for (const auto& block : _backgroundBlocks[depth])
            {
                if (block.span.end.first == -1) continue;

                auto startBounds = block.span.start.second == -1 ? _result.ForegroundLines[block.span.start.first].Content :
                    _result.ForegroundLines[block.span.start.first].Segments[block.span.start.second].Bounds;
                auto endBounds = block.span.end.second == -1 ? _result.ForegroundLines[block.span.end.first].Content :
                    _result.ForegroundLines[block.span.end.first].Segments[block.span.end.second].Bounds;

                auto& background = _result.BackgroundBlocks[depth].emplace_back();
                auto bgidx = (int)_result.BackgroundBlocks[depth].size() - 1;
                background = block.shape;
                background.Start = { startBounds.left, startBounds.top };

                if (block.isMultilineCapable)
                {
                    background.End = { endBounds.left + endBounds.width,
                        endBounds.top + _result.ForegroundLines[block.span.end.first].height() };
                    background.Start.x = std::min(background.Start.x, endBounds.left);
                    background.End.x = std::max(background.End.x, startBounds.left + startBounds.width);
                    UpdateRelativeToAbs(background);
                }
                else if (block.span.end.first > block.span.start.first)
                {
                    auto segmentIdx = block.span.start.second;
                    auto startLine = block.span.start.first;
                    auto bgheight = 0.f;
                    
                    if (segmentIdx == -1)
                    {
                        [this, &block, &startLine, &segmentIdx, &bgheight, depth]() mutable {
                            for (auto line = block.span.start.first; line <= block.span.end.first; ++line)
                            {
                                if (!_result.ForegroundLines[line].Segments.empty())
                                {
                                    segmentIdx = 0;
                                    startLine = line;
                                    bgheight = _result.ForegroundLines[line].Segments.front().height();
                                    return;
                                }
                            }
                        }();
                    }
                    else
                        bgheight = _result.ForegroundLines[startLine].Segments[segmentIdx].height();

                    auto& firstLine = _result.ForegroundLines[startLine];
                    auto& firstSegment = firstLine.Segments[segmentIdx];
                    background.End = { firstSegment.Bounds.left + firstSegment.Bounds.width,
                         firstSegment.Bounds.top + firstSegment.Bounds.height };
                    UpdateRelativeToAbs(background);

                    for (auto line = startLine + 1; line < block.span.end.first; ++line)
                    {
                        const auto& segments = _result.ForegroundLines[line].Segments;

                        if (!segments.empty())
                        {
                            auto& extendedBlock = _result.BackgroundBlocks[depth].emplace_back();
                            extendedBlock = block.shape;
                            extendedBlock.Start = { segments.front().Bounds.left, segments.front().Bounds.top };
                            extendedBlock.End = { segments.back().Bounds.left + segments.back().Bounds.width, 
                                segments.back().Bounds.top + bgheight };
                            UpdateRelativeToAbs(extendedBlock);
                        }
                    }

                    const auto& segments = _result.ForegroundLines[block.span.end.first].Segments;

                    if (!segments.empty())
                    {
                        auto& lastBlock = _result.BackgroundBlocks[depth].emplace_back();
                        lastBlock = block.shape;
                        lastBlock.Start = { segments.front().Bounds.left, segments.front().Bounds.top };
                        lastBlock.End = { endBounds.left + endBounds.width, endBounds.top + bgheight };
                        UpdateRelativeToAbs(lastBlock);
                    }
                }
                else
                {
                    background.End = { endBounds.left + endBounds.width,
                        endBounds.top + _result.ForegroundLines[block.span.end.first].height() };
                    UpdateRelativeToAbs(background);
                }
            }
        }
    }

    void DefaultTagVisitor::Error(std::string_view tag)
    {
        // TODO
    }

    bool DefaultTagVisitor::IsSelfTerminating(std::string_view tag) const
    {
        return AreSame(tag, "br") || AreSame(tag, "hr");
    }

    bool DefaultTagVisitor::IsPreformattedContent(std::string_view tag) const
    {
        return AreSame(tag, "code") || AreSame(tag, "pre");
    }

    // ===============================================================
    // Section #4. Implementation of public API
    // ===============================================================

#ifdef IM_RICHTEXT_TARGET_IMGUI
    static RenderConfig* GetRenderConfig(RenderConfig* config = nullptr)
    {
        if (config == nullptr)
        {
            auto ctx = ImGui::GetCurrentContext();
            auto it = ImRenderConfigs.find(ctx);
            assert(it != ImRenderConfigs.end());
            config = &(it->second.back());
        }

        return config;
    }
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
    static RenderConfig* GetRenderConfig(BLContext& ctx, RenderConfig* config = nullptr)
    {
        if (config != nullptr) return config;
        auto it = BLRenderConfigs.find(&ctx);
        assert(it != BLRenderConfigs.end());
        return &(it->second.back());
    }
#endif

#ifdef IM_RICHTEXT_TARGET_IMGUI
    static void Draw(std::size_t richTextId, const Drawables& drawables, ImVec2 pos, ImVec2 bounds, RenderConfig* config)
    {
        config = GetRenderConfig(config);
        auto& animation = RichTextMap.at(richTextId).animationData;
        DrawImpl(animation, drawables, pos, bounds, config);
    }

    static bool ShowDrawables(ImVec2 pos, std::string_view content, std::size_t richTextId, Drawables& drawables,
        ImVec2 bounds, RenderConfig* config)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        const auto& style = ImGui::GetCurrentContext()->Style;
        Draw(richTextId, drawables, pos + style.FramePadding, bounds, config);
        return true;
    }

    RenderConfig* GetCurrentConfig()
    {
        auto ctx = ImGui::GetCurrentContext();
        auto it = ImRenderConfigs.find(ctx);
        return it != ImRenderConfigs.end() ? &(it->second.back()) : &(ImRenderConfigs.at(nullptr).front());
    }

    void PushConfig(RenderConfig& config)
    {
        config.HFontSizes[0] = config.DefaultFontSize * 2.f;
        config.HFontSizes[1] = config.DefaultFontSize * 1.5f;
        config.HFontSizes[2] = config.DefaultFontSize * 1.17f;
        config.HFontSizes[3] = config.DefaultFontSize;
        config.HFontSizes[4] = config.DefaultFontSize * 0.83f;
        config.HFontSizes[5] = config.DefaultFontSize * 0.67f;

        auto ctx = ImGui::GetCurrentContext();
        ImRenderConfigs[ctx].push_back(config);
    }

    void PopConfig()
    {
        auto ctx = ImGui::GetCurrentContext();
        auto it = ImRenderConfigs.find(ctx);
        if (it != ImRenderConfigs.end()) it->second.pop_back();
    }

#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
    void Draw(BLContext& context, std::size_t richTextId, const Drawables& drawables,
        ImVec2 pos, ImVec2 bounds, RenderConfig* config)
    {
        config = GetRenderConfig(context, config);
        auto& animation = RichTextMap.at(richTextId).animationData;
        DrawImpl(animation, drawables, pos, bounds, config);
    }

    bool ShowDrawables(BLContext& context, ImVec2 pos, std::size_t richTextId, Drawables& drawables,
        ImVec2 bounds, RenderConfig* config)
    {
        Draw(context, richTextId, drawables, pos + style.FramePadding, bounds, config);
        return true;
    }

    RenderConfig* GetCurrentConfig(BLContext& context)
    {
        auto it = BLRenderConfigs.find(&context);
        return it != BLRenderConfigs.end() ? &(it->second.back()) : &(BLRenderConfigs.at(nullptr).front());
    }

    void PushConfig(RenderConfig& config, BLContext& context)
    {
        config.HFontSizes[0] = config.DefaultFontSize * 2.f;
        config.HFontSizes[1] = config.DefaultFontSize * 1.5f;
        config.HFontSizes[2] = config.DefaultFontSize * 1.17f;
        config.HFontSizes[3] = config.DefaultFontSize;
        config.HFontSizes[4] = config.DefaultFontSize * 0.83f;
        config.HFontSizes[5] = config.DefaultFontSize * 0.67f;

        BLRenderConfigs[&context].push_back(config);
    }

    void PopConfig(BLContext& context)
    {
        auto it = BLRenderConfigs.find(&context);
        if (it != BLRenderConfigs.end()) it->second.pop_back();
    }
#endif

    RenderConfig* GetDefaultConfig(const DefaultConfigParams& params)
    {
        auto config = &(ImRenderConfigs[nullptr].emplace_back());
        config->NamedColor = &glimmer::GetColor;
        config->FontScale = params.FontScale;
        config->DefaultFontSize = params.DefaultFontSize;
        config->MeterDefaultSize = { params.DefaultFontSize * 5.0f, params.DefaultFontSize };
        config->TextShaper = CreateTextShaper(params.Charset);
        config->HFontSizes[0] = params.DefaultFontSize * 2.f;
        config->HFontSizes[1] = params.DefaultFontSize * 1.5f;
        config->HFontSizes[2] = params.DefaultFontSize * 1.17f;
        config->HFontSizes[3] = params.DefaultFontSize;
        config->HFontSizes[4] = params.DefaultFontSize * 0.83f;
        config->HFontSizes[5] = params.DefaultFontSize * 0.67f;

#ifdef IM_RICHTEXT_BUNDLED_FONTLOADER
        if (params.FontLoadFlags != 0) LoadDefaultFonts(*config, params.FontLoadFlags, params.Charset);
#endif
        return config;
    }

    ITextShaper* CreateTextShaper(TextContentCharset charset)
    {
        switch (charset)
        {
        case TextContentCharset::ASCII: return ASCIITextShaper::Instance();
        default: break;
        }

        return nullptr;
    }

    static Drawables GetDrawables(const char* text, const char* textend, const RenderConfig& config, ImVec2 bounds)
    {
        Drawables result;
        DefaultTagVisitor visitor{ config, result, bounds };
        ParseRichText(text, textend, config.TagStart, config.TagEnd, visitor);
        return result;
    }

    static ImVec2 GetBounds(const Drawables& drawables, ImVec2 bounds)
    {
        ImVec2 result = bounds;
        const auto& style = ImGui::GetCurrentContext()->Style;

        if (bounds.x == FLT_MAX || bounds.x <= 0.f)
        {
            float width = 0.f;
            for (const auto& line : drawables.ForegroundLines)
                width = std::max(width, line.width() + line.Content.left);
            for (auto depth = 0; depth < IM_RICHTEXT_MAXDEPTH; ++depth)
                for (const auto& bg : drawables.BackgroundBlocks[depth])
                    width = std::max(width, bg.End.x);
            result.x = width + (2.f * style.FramePadding.x);
        }

        if (bounds.y == FLT_MAX || bounds.y <= 1.f)
        {
            auto fgheight = 0.f, bgheight = 0.f;

            if (!drawables.ForegroundLines.empty())
            {
                const auto& lastFg = drawables.ForegroundLines.back();
                fgheight = lastFg.height() + lastFg.Content.top;
            }
            
            for (auto depth = 0; depth < IM_RICHTEXT_MAXDEPTH; ++depth)
                if (!drawables.BackgroundBlocks[depth].empty())
                    bgheight = std::max(bgheight, drawables.BackgroundBlocks[depth].back().End.y);
           
            result.y = std::max(fgheight, bgheight) + (2.f * style.FramePadding.y);
        }

        return result;
    }

    static ImVec2 ComputeBounds(Drawables& drawables, RenderConfig* config, ImVec2 bounds)
    {
        auto computed = GetBounds(drawables, bounds);

        // <hr> elements may not have width unless pre-specified, hence update them
        for (auto& line : drawables.ForegroundLines)
            for (auto& segment : line.Segments)
                for (auto& token : segment.Tokens)
                    if ((token.Type == TokenType::HorizontalRule) && ((drawables.StyleDescriptors[segment.StyleIdx + 1].propsSpecified & StyleWidth) == 0)
                        && token.Bounds.width == -1.f)
                        token.Bounds.width = segment.Bounds.width = line.Content.width = computed.x;
        return computed;
    }

    std::size_t CreateRichText(const char* text, const char* end)
    {
        if (end == nullptr) end = text + std::strlen(text);

        std::string_view key{ text, (size_t)(end - text) };
        auto hash = std::hash<std::string_view>()(key);

        if (auto it = RichTextMap.find(hash); 
            it == RichTextMap.end() || it->second.richText != text)
        {
            RichTextMap[hash].richText = key;
            RichTextMap[hash].contentChanged = true;
        }
        
        return hash;
    }

    bool UpdateRichText(std::size_t id, const char* text, const char* end)
    {
        auto rit = RichTextMap.find(id);

        if (rit != RichTextMap.end())
        {
            if (end == nullptr) end = text + std::strlen(text);

            std::string_view existingKey{ rit->second.richText };
            std::string_view key{ text, (size_t)(end - text) };

            if (key != existingKey)
            {
                RichTextMap[id].richText = key;
                RichTextMap[id].contentChanged = true;
                return true;
            }
        }

        return false;
    }

    bool RemoveRichText(std::size_t id)
    {
        auto it = RichTextMap.find(id);

        if (it != RichTextMap.end())
        {
            RichTextMap.erase(it);
            return true;
        }

        return false;
    }

    void ClearAllRichTexts()
    {
        RichTextMap.clear();
    }

#ifdef IM_RICHTEXT_TARGET_IMGUI

    static bool Render(ImVec2 pos, std::size_t richTextId, std::optional<ImVec2> sz, bool show)
    {
        auto it = RichTextMap.find(richTextId);

        if (it != RichTextMap.end())
        {
            auto& drawdata = it->second;
            auto config = GetRenderConfig();

            if (config != drawdata.config || config->Scale != drawdata.scale ||
                config->FontScale != drawdata.fontScale || config->DefaultBgColor != drawdata.bgcolor
                || (sz.has_value() && sz.value() != drawdata.specifiedBounds) || drawdata.contentChanged)
            {
                drawdata.contentChanged = false;
                drawdata.config = config;
                drawdata.bgcolor = config->DefaultBgColor;
                drawdata.scale = config->Scale;
                drawdata.fontScale = config->FontScale;
                drawdata.specifiedBounds = sz.has_value() ? sz.value() : drawdata.specifiedBounds;

#ifdef _DEBUG
                auto ts = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock().now().time_since_epoch());

                drawdata.drawables = GetDrawables(drawdata.richText.data(),
                    drawdata.richText.data() + drawdata.richText.size(), *config,
                    drawdata.specifiedBounds);

                ts = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock().now().time_since_epoch()) - ts;
                HIGHLIGHT("\nParsing [#%d] took %lldus", (int)richTextId, ts.count());
#else
                drawdata.drawables = GetDrawables(drawdata.richText.data(),
                    drawdata.richText.data() + drawdata.richText.size(), *config,
                    drawdata.specifiedBounds);
#endif

                drawdata.computedBounds = ComputeBounds(drawdata.drawables, config, drawdata.specifiedBounds);
            }

            if (show) ShowDrawables(pos, drawdata.richText, richTextId, drawdata.drawables, drawdata.computedBounds, config);
            return true;
        }

        return false;
    }

    ImVec2 GetBounds(std::size_t richTextId, std::optional<ImVec2> sz)
    {
        if (Render({}, richTextId, sz, false))
            return RichTextMap.at(richTextId).computedBounds;
        return ImVec2{ 0.f, 0.f };
    }

    bool Show(std::size_t richTextId, std::optional<ImVec2> sz)
    {
        return Show(ImGui::GetCurrentWindow()->DC.CursorPos, richTextId, sz);
    }

    bool Show(ImVec2 pos, std::size_t richTextId, std::optional<ImVec2> sz)
    {
        return Render(pos, richTextId, sz, true);
    }

    bool ToggleOverlay()
    {
#ifdef _DEBUG
        ShowOverlay = !ShowOverlay;
        ShowBoundingBox = !ShowBoundingBox;
#endif
        return ShowOverlay;
    }
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D

    static bool Render(BLContext& context, ImVec2 pos, std::size_t richTextId, std::optional<ImVec2> sz, bool show)
    {
        auto it = RichTextMap.find(richTextId);

        if (it != RichTextMap.end())
        {
            auto& drawdata = RichTextMap[richTextId];
            auto config = GetRenderConfig(context);

            if (config != drawdata.config || config->Scale != drawdata.scale ||
                config->FontScale != drawdata.fontScale || config->DefaultBgColor != drawdata.bgcolor
                || (sz.has_value() && sz.value() != drawdata.specifiedBounds) || drawdata.contentChanged)
            {
                drawdata.contentChanged = false;
                drawdata.config = config;
                drawdata.bgcolor = config->DefaultBgColor;
                drawdata.scale = config->Scale;
                drawdata.fontScale = config->FontScale;
                drawdata.specifiedBounds = sz.has_value() ? sz.value() : drawdata.specifiedBounds;

#ifdef _DEBUG
                auto ts = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock().now().time_since_epoch());

                drawdata.drawables = GetDrawables(drawdata.richText.data(),
                    drawdata.richText.data() + drawdata.richText.size(), *config,
                    drawdata.specifiedBounds);

                ts = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock().now().time_since_epoch()) - ts;
                HIGHLIGHT("\nParsing [#%d] took %lldus", (int)richTextId, ts.count());
#else
                drawdata.drawables = GetDrawables(drawdata.richText.data(),
                    drawdata.richText.data() + drawdata.richText.size(), *config,
                    drawdata.specifiedBounds);
#endif
            }

            drawdata.computedBounds = ComputeBounds(drawdata.drawables, config, drawdata.specifiedBounds);
            ShowDrawables(context, pos, richTextId, drawdata.drawables, drawdata.computedBounds, config);
            return true;
        }

        return false;
    }

    ImVec2 GetBounds(BLContext& context, std::size_t richTextId)
    {
        if (Render(context, {}, richTextId, std::nullopt, false))
            return RichTextMap.at(richTextId).computedBounds;
        return ImVec2{ 0.f, 0.f };
    }

    bool Show(BLContext& context, ImVec2 pos, std::size_t richTextId, std::optional<ImVec2> sz)
    {
        return Render(context, pos, richTextId, sz, true);
    }
    
#endif
}