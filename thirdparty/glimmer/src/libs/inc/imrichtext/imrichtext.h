#pragma once

#ifndef GLIMMER_DISABLE_RICHTEXT

#ifdef IM_RICHTEXT_TARGET_IMGUI
#include "imgui.h"
#endif
#include "imrichtextutils.h"
#include "imrichtextimpl.h"

#include <string_view>
#include <vector>
#include <stdint.h>

#include "im_font_manager.h"

#ifndef IM_RICHTEXT_MAXDEPTH
#define IM_RICHTEXT_MAXDEPTH 32
#endif

#ifndef IM_RICHTEXT_MAX_LISTDEPTH
#define IM_RICHTEXT_MAX_LISTDEPTH 16
#endif

#ifndef IM_RICHTEXT_MAX_LISTITEM
#define IM_RICHTEXT_MAX_LISTITEM 128
#endif

#ifndef IM_RICHTEXT_MAXTABSTOP
#define IM_RICHTEXT_MAXTABSTOP 32
#endif

#ifndef IM_RICHTEXT_BLINK_ANIMATION_INTERVAL
#define IM_RICHTEXT_BLINK_ANIMATION_INTERVAL 500
#endif

#ifndef IM_RICHTEXT_MARQUEE_ANIMATION_INTERVAL
#define IM_RICHTEXT_MARQUEE_ANIMATION_INTERVAL 18
#endif

#define IM_RICHTEXT_NESTED_ITEMCOUNT_STRSZ 64

namespace ImRichText
{
	using TextContentCharset = glimmer::TextContentCharset;

    enum class TokenType
    {
        Text,
        ElidedText,
        ListItemBullet,
        ListItemNumbered,
        HorizontalRule,
        Meter
    };

    struct ListItemTokenDescriptor
    {
        char NestedListItemIndex[IM_RICHTEXT_NESTED_ITEMCOUNT_STRSZ];
        int16_t ListDepth = -1;
        int16_t ListItemIndex = -1;
    };

    struct TagPropertyDescriptor
    {
        std::string_view tooltip = "";
        std::string_view link = "";
        float value = 0.f;
        std::pair<float, float> range = { 0.f, 0.f };
    };

    struct Token
    {
        TokenType Type = TokenType::Text;
        std::string_view Content = "";
        BoundedBox Bounds; // Absolute coordinates
        FourSidedMeasure Offset; // Local coordinates
        int16_t ListPropsIdx = -1;
        int16_t PropertiesIdx = -1;
        int16_t VisibleTextSize = -1;
    };

    enum FontStyleFlag : int32_t
    {
        FontStyleNone = 0,
        FontStyleNormal = 1,
        FontStyleBold = 1 << 1,
        FontStyleItalics = 1 << 2,
        FontStyleLight = 1 << 3,
        FontStyleStrikethrough = 1 << 4,
        FontStyleUnderline = 1 << 5,
        FontStyleOverflowEllipsis = 1 << 6,
        FontStyleNoWrap = 1 << 7
    };

    struct FontStyle
    {
        void* font = nullptr; // Pointer to font object
        std::string_view family = GLIMMER_DEFAULT_FONTFAMILY;
        float size = 24.f;
        int32_t flags = FontStyleNone;
    };

    struct ListStyle
    {
        uint32_t itemColor = IM_COL32_BLACK;
        BulletType itemStyle = BulletType::FilledCircle;
    };

    enum StyleProperty : int64_t
    {
        StyleError = -1,
        NoStyleChange = 0,
        StyleBackground = 1,
        StyleFgColor = 1 << 1,
        StyleFontSize = 1 << 2,
        StyleFontFamily = 1 << 3,
        StyleFontWeight = 1 << 4,
        StyleFontStyle = 1 << 5,
        StyleHeight = 1 << 6,
        StyleWidth = 1 << 7,
        StyleListBulletType = 1 << 8,
        StyleHAlignment = 1 << 9,
        StyleVAlignment = 1 << 10,
        StylePadding = 1 << 11,
        StyleMargin = 1 << 12,
        StyleBorder = 1 << 15,
        StyleBorderRadius = 1 << 16,
        StyleCellSpacing = 1 << 17,
        StyleBlink = 1 << 18,
        StyleTextWrap = 1 << 19,
        StyleBoxShadow = 1 << 20,
        StyleWordBreak = 1 << 21,
        StyleWhitespaceCollapse = 1 << 22,
        StyleWhitespace = 1 << 23,
        StyleTextOverflow = 1 << 24,
    };

    enum TextAlignment
    {
        TextAlignLeft = 1,
        TextAlignRight = 1 << 1,
        TextAlignHCenter = 1 << 2,
        TextAlignTop = 1 << 3,
        TextAlignBottom = 1 << 4,
        TextAlignVCenter = 1 << 5,
        TextAlignJustify = 1 << 6,
        TextAlignCenter = TextAlignHCenter | TextAlignVCenter,
        TextAlignLeading = TextAlignLeft | TextAlignVCenter
    };

    struct StyleDescriptor
    {
        int64_t propsSpecified = NoStyleChange;
        uint32_t fgcolor = IM_COL32_BLACK;
        float height = 0;
        float width = 0;
        FontStyle font;
        WordBreakBehavior wbbhv = WordBreakBehavior::Normal;
        WhitespaceCollapseBehavior wscbhv = WhitespaceCollapseBehavior::Collapse;
        ListStyle list;
        int alignment = TextAlignment::TextAlignLeading;
        float superscriptOffset = 0.f; // TODO: Move to DrawableLine
        float subscriptOffset = 0.f; // TODO: Move to DrawableLine
        bool blink = false;
    };

    struct SegmentData
    {
        std::vector<Token> Tokens;
        BoundedBox Bounds; // Absolute coordinates
        int StyleIdx = -1;

        int SubscriptDepth = 0;
        int SuperscriptDepth = 0;
        bool HasText = false;

        float width() const { return Bounds.width; }
        float height() const { return Bounds.height; }
    };

    struct DrawableLine
    {
        std::vector<SegmentData> Segments;
        BoundedBox Content; // Absolute coordinates
        FourSidedMeasure Offset; // Local coordinates

        int  BlockquoteDepth = 0;
        bool HasText = false;
        bool HasSuperscript = false;
        bool HasSubscript = false;
        bool Marquee = false;

        float width() const { return Content.width + Offset.left + Offset.right; }
        float height() const { return Content.height + Offset.top + Offset.bottom; }
    };

    struct DrawableBlock
    {
        ImVec2 Start{ -1.f, -1.f }, End{ -1.f, -1.f }, ViewportPos{ 0.f, 0.f };
        uint32_t Color = IM_COL32_BLACK_TRANS;
        FourSidedMeasure padding;
        FourSidedMeasure margin;
        FourSidedBorder Border;
        int  BorderCornerRel = 0;
        ColorGradient Gradient;
    };

#ifdef _DEBUG
    enum DebugContentType
    {
        ContentTypeToken,
        ContentTypeSegment,
        ContentTypeLine,
        ContentTypeBg,
        ContentTypeTotal
    };
#endif

    struct RenderConfig
    {
        // The members at the beginning are the ones that will
        // be used in drawing loop
        IPlatform* Platform = nullptr;
        IRenderer* RTRenderer = nullptr;
		glimmer::IRenderer* Renderer = nullptr;

#ifdef _DEBUG
        glimmer::IRenderer* OverlayRenderer = nullptr;
#endif

        // All other members are used in parsing phase
        uint32_t(*NamedColor)(const char*, void*) = nullptr;
        ITextShaper* TextShaper = nullptr;

        float Scale = 1.0f;
        float FontScale = 1.f;

        char TagStart = '<';
        char TagEnd = '>';
        char EscapeSeqStart = '&';
        char EscapeSeqEnd = ';';
        std::string_view CommentStart = "!--"; // UNUSED
        std::string_view CommentEnd = "--"; // UNUSED

        float  LineGap = 5;
        bool   WordWrap = true;

        int   ParagraphStop = 4;
        int   TabStop = 4;
        float ListItemIndent = 15.f;
        float ListItemOffset = 15.f;
        BulletType ListItemBullet = BulletType::FilledCircle;

        std::string_view DefaultFontFamily = GLIMMER_DEFAULT_FONTFAMILY;
        float    DefaultFontSize = 24.f;
        uint32_t DefaultFgColor = IM_COL32_BLACK;
        uint32_t DefaultBgColor = IM_COL32_WHITE;
        uint32_t MarkHighlight = ToRGBA(255, 255, 0);
        uint32_t HyperlinkColor = ToRGBA(0, 50, 255);
        
        float    HFontSizes[6] = { 48.f, 36.f, 24.f * 1.17f, 24.f, 0.83f * 24.f, 0.67f * 24.f };
        uint32_t HeaderLineColor = ToRGBA(128, 128, 128, 255);

        uint32_t BlockquoteBar = ToRGBA(0.25f, 0.25f, 0.25f);
        uint32_t BlockquoteBg = ToRGBA(0.5f, 0.5f, 0.5f);
        float    BlockquotePadding = 5.f;
        float    BlockquoteOffset = 15.f;
        float    BlockquoteBarWidth = 5.f;

        uint32_t MeterBorderColor = ToRGBA(100, 100, 100);
        uint32_t MeterBgColor = ToRGBA(200, 200, 200);
        uint32_t MeterFgColor = ToRGBA(0, 200, 25);
        ImVec2   MeterDefaultSize = { 80.f, 16.f };
        uint32_t CodeBlockBg = IM_COL32_BLACK_TRANS;
        float    CodeBlockPadding = 5.f;

        float BulletSizeScale = 2.f;
        float ScaleSuperscript = 0.62f;
        float ScaleSubscript = 0.62f;
        float HrVerticalMargins = 5.f;
        void* UserData = nullptr;

        bool IsStrictHTML5 = false;

#ifdef _DEBUG
        uint32_t DebugContents[ContentTypeTotal] = {
            IM_COL32_BLACK_TRANS, IM_COL32_BLACK_TRANS, 
            IM_COL32_BLACK_TRANS, IM_COL32_BLACK_TRANS
        };
#endif
    };

    struct Drawables
    {
        std::vector<DrawableLine>  ForegroundLines;
        std::vector<DrawableBlock> BackgroundBlocks[IM_RICHTEXT_MAXDEPTH];
        std::vector<StyleDescriptor> StyleDescriptors;
        std::vector<TagPropertyDescriptor>   TagDescriptors;
        std::vector<ListItemTokenDescriptor> ListItemTokens;
        bool BoundsComputed = false;
    };

    struct DefaultConfigParams
    {
        float DefaultFontSize = 24.f;
        float FontScale = 1.f;
        uint64_t FontLoadFlags = glimmer::FLT_Proportional;
        TextContentCharset Charset = TextContentCharset::ASCII;
    };

    struct ITextShaper;

    // RenderConfig related functions. In order to render rich text, such configs should be pushed/popped as desired 
    [[nodiscard]] RenderConfig* GetDefaultConfig(const DefaultConfigParams& params);

    // Create the correct text shaper engine based on charset support
    [[nodiscard]] ITextShaper* CreateTextShaper(TextContentCharset charset);

#ifdef IM_RICHTEXT_TARGET_IMGUI
    [[nodiscard]] RenderConfig* GetCurrentConfig();
    void PushConfig(RenderConfig& config);
    void PopConfig();
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
    [[nodiscard]] RenderConfig* GetCurrentConfig(BLContext& context);
    void PushConfig(RenderConfig& config, BLContext& context);
    void PopConfig(BLContext& context);
#endif

    // Create cacheable rich text content
    [[nodiscard]] std::size_t CreateRichText(const char* text, const char* end = nullptr);
    bool UpdateRichText(std::size_t id, const char* text, const char* end = nullptr);
    bool RemoveRichText(std::size_t id);
    void ClearAllRichTexts();

#ifdef IM_RICHTEXT_TARGET_IMGUI
    [[nodiscard]] ImVec2 GetBounds(std::size_t richTextId, std::optional<ImVec2> sz = std::nullopt);
    bool Show(ImVec2 pos, std::size_t richTextId, std::optional<ImVec2> sz = std::nullopt);
    bool Show(std::size_t richTextId, std::optional<ImVec2> sz = std::nullopt);
    bool ToggleOverlay();
#endif
#ifdef IM_RICHTEXT_TARGET_BLEND2D
    [[nodiscard]] ImVec2 GetBounds(BLContext& context, std::size_t richTextId);
    bool Show(BLContext& context, ImVec2 pos, std::size_t richTextId, std::optional<ImVec2> sz = std::nullopt);
#endif
}

#endif