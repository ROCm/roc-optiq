#pragma once

#include "types.h"

namespace glimmer
{
    struct LayoutBuilder;

    [[nodiscard]] uint32_t GetColor(const char* name, void*);

    // Internal API, should not be used
    void _IgnoreStyleStackInternal(int32_t wtypes);

    struct ColorStop
    {
        uint32_t from, to;
        float pos = -1.f;
    };

    struct ColorGradient
    {
        ColorStop colorStops[GLIMMER_MAX_COLORSTOPS];
        int totalStops = 0;
        float angleDegrees = 0.f;
        ImGuiDir dir = ImGuiDir::ImGuiDir_Down;
    };

    struct FourSidedMeasure
    {
        float top = 0.f, left = 0.f, right = 0.f, bottom = 0.f;

        float h() const { return left + right; }
        float v() const { return top + bottom; }
    };

    enum BoxCorner
    {
        TopLeftCorner,
        TopRightCorner,
        BottomRightCorner,
        BottomLeftCorner
    };

    struct FourSidedBorder
    {
        Border top, left, bottom, right;
        float cornerRadius[4];
        bool isUniform = false;

        float h() const { return left.thickness + right.thickness; }
        float v() const { return top.thickness + bottom.thickness; }
        bool isRounded() const;
        bool exists() const;

        FourSidedBorder& setColor(uint32_t color);
        FourSidedBorder& setThickness(float thickness);
        FourSidedBorder& setRadius(float radius);
        //FourSidedBorder& setLineType(uint32_t color);
    };

    struct BoxShadow
    {
        ImVec2 offset{ 0.f, 0.f };
        float spread = 0.f;
        float blur = 0.f;
        uint32_t color = IM_COL32_BLACK_TRANS;
    };

    struct FontStyle
    {
        void* font = nullptr; // Pointer to font object
        std::string_view family = GLIMMER_DEFAULT_FONTFAMILY;
        float size = 16.f;
        int32_t flags = TextIsPlainText;
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
        StyleBorder = 1 << 13,
        StyleOverflow = 1 << 14,
        StyleBorderRadius = 1 << 16,
        StyleCellSpacing = 1 << 17, // Unused
        StyleBlink = 1 << 18,
        StyleTextWrap = 1 << 19,
        StyleBoxShadow = 1 << 20,
        StyleWordBreak = 1 << 21,
        StyleWhitespaceCollapse = 1 << 22,
        StyleWhitespace = 1 << 23,
        StyleTextOverflow = 1 << 24,
        StyleMinWidth = 1 << 25,
        StyleMaxWidth = 1 << 26,
        StyleMinHeight = 1 << 27,
        StyleMaxHeight = 1 << 28,
        StyleThumbColor = 1 << 29,
        StyleTrackColor = 1 << 30,
        StyleTrackOutlineColor = 1ll << 31,
        StyleThumbOffset = 1ll << 32,

        StyleUpdatedFromBase = 1ll << 62,
        StyleTotal = 33 // Total number of style properties
    };

    enum RelativeStyleProperty : uint32_t
    {
        None = 0,
        RSP_Width = 1,
        RSP_Height = 1 << 1,
        RSP_MinHeight = 1 << 2,
        RSP_MaxHeight = 1 << 3,
        RSP_MinWidth = 1 << 4,
        RSP_MaxWidth = 1 << 5,
        RSP_BorderTopRightRadius = 1 << 6,
        RSP_BorderTopLeftRadius = 1 << 7,
        RSP_BorderBottomLeftRadius = 1 << 8,
        RSP_BorderBottomRightRadius = 1 << 9,
    };

    enum AnimationType
    {
        AT_None = 0,
        AT_Marquee = 1,
        AT_Blink = 1 << 1,
        AT_Shimmer = 1 << 2,
        AT_Cycle = 1 << 3,
        AT_Pulsate = 1 << 4,
        AT_Linear = 1 << 5,
        AT_Bounce = 1 << 6,
        AT_ExpDecay = 1 << 7,
        AT_Switch = 1 << 8
    };

    enum AnimationElement
    {
        AE_None = 0,
        AE_BgColor = 1,
        AE_FgColor = 1 << 1,
        AE_Padding = 1 << 2,
        AE_BorderColor = 1 << 3,
        AE_BorderWidth = 1 << 4,
        AE_FontSize = 1 << 5,
        AE_FontWeight = 1 << 6
    };

    struct CustomStyleDataIndices
    {
        unsigned animation : 8;
        unsigned custom : 16;
    };

    struct GlobalWidgetTheme;

    struct StyleDescriptor
    {
        uint64_t specified = 0;
        uint32_t bgcolor = IM_COL32_BLACK_TRANS;
        uint32_t fgcolor = IM_COL32_BLACK;
        ImVec2 dimension{ -1.f, -1.f };
        ImVec2 mindim{ 0.f, 0.f };
        ImVec2 maxdim{ FLT_MAX, FLT_MAX };
        int32_t alignment = TextAlignLeading;
        uint32_t relativeProps = 0;
        CustomStyleDataIndices index;
        FourSidedMeasure padding;
        FourSidedMeasure margin;
        FourSidedBorder border;
        FontStyle font;
        BoxShadow shadow;
        ColorGradient gradient;

        StyleDescriptor();
        StyleDescriptor(std::string_view css);

        StyleDescriptor& BgColor(int r, int g, int b, int a = 255);
        StyleDescriptor& FgColor(int r, int g, int b, int a = 255);
        StyleDescriptor& Size(float w, float h);
        StyleDescriptor& Align(int32_t align);
        StyleDescriptor& Padding(float p);
        StyleDescriptor& Margin(float p);
        StyleDescriptor& Border(float thick, std::tuple<int, int, int, int> color);
        StyleDescriptor& Raised(float amount);

        StyleDescriptor& From(std::string_view css, bool checkForDuplicate = true);
        StyleDescriptor& From(const StyleDescriptor& style, bool overwrite = true);

        static void(*GlobalThemeProvider)(GlobalWidgetTheme*);
    };

    struct ToggleButtonStyleDescriptor
    {
        uint32_t trackColor;
        uint32_t trackBorderColor;
        uint32_t thumbColor;
        int32_t trackGradient = -1;
        int32_t indicatorTextColor; // Text color of ON/OFF text
        float trackBorderThickness = 0.f;
        float thumbOffset = -2.f;
        float thumbExpand = 0.f;
        float animate = 0.3f;
        bool showText = true; // Show ON/OFF text inside button
        void* fontptr = nullptr;
        float fontsz = 12.f;

        static ToggleButtonStyleDescriptor ParseFrom(std::string_view css);
    };

    struct SliderStyleDescriptor
    {
        uint32_t trackColor;
        uint32_t thumbColor;
        uint32_t fillColor;
        int32_t trackGradient = -1;
        float trackBorderThickness = 1.f;
        float thumbOffset = 2.f;
        float thumbExpand = 0.f;

        static SliderStyleDescriptor ParseFrom(std::string_view css);
    };

    struct RangeSliderStyleDescriptor
    {
        struct ThumbStyle
        {
            uint32_t color;
            float offset = 2.f;
            float expand = 0.f;
        };

        uint32_t trackColor;
        uint32_t fillColor;
        int32_t trackGradient = -1;
        float trackBorderThickness = 1.f;
        ThumbStyle minThumb, maxThumb;

		float offset() const { return (minThumb.offset + maxThumb.offset); }

        static RangeSliderStyleDescriptor ParseFrom(std::string_view css);
    };

    struct SpinnerStyleDescriptor
    {
        uint32_t upColor, downColor;
        uint32_t upbtnColor, downbtnColor;
        float btnBorderThickness = 2.f;
        bool upDownArrows = true;

        static SpinnerStyleDescriptor ParseFrom(std::string_view css);
    };

    struct RadioButtonStyleDescriptor
    {
        uint32_t checkedColor = ToRGBA(0, 0, 0);
        uint32_t outlineColor = ToRGBA(0, 0, 0);
        float outlineThickness = 2.f;
        float checkedRadius = 0.6f; // relative to total
        float animate = 0.3f;

        static RadioButtonStyleDescriptor ParseFrom(std::string_view css);
    };

    struct TabBarStyleDescriptor
    {
        uint32_t pincolor = 0;
        uint32_t pinbgcolor = 0;
        uint32_t closecolor = 0;
        uint32_t closebgcolor = 0;
        float pinPadding = 2.f;
        float closePadding = 2.f;

        static TabBarStyleDescriptor ParseFrom(std::string_view css);
    };

    struct GlobalWidgetTheme
    {
        ToggleButtonStyleDescriptor toggle;
        SliderStyleDescriptor slider;
        RangeSliderStyleDescriptor rangeSlider;
        SpinnerStyleDescriptor spinner;
        RadioButtonStyleDescriptor radio;
        TabBarStyleDescriptor tabbar;
    };

	// TODO: Add other widget style descriptors here and populate in style.cpp
    union CommonWidgetStyleDescriptor
    {
        ToggleButtonStyleDescriptor toggle;

        CommonWidgetStyleDescriptor() {}
    };

    // Set all styles for ids/classes as a stylesheet (This should be done before event loop, or at the start
    // of a frame ideally)
    void SetStyle(std::string_view id, const std::initializer_list<std::pair<int32_t, std::string_view>>& css);
    void SetStyle(std::string_view id, int32_t state, std::string_view fmt, ...);
    StyleDescriptor& GetStyle(std::string_view id, WidgetStateIndex index);
    StyleDescriptor& GetWidgetStyle(WidgetType type, WidgetStateIndex index);

    // Push/Pop styles for a widget for temporary style changes
    void PushStyle(std::string_view defcss, std::string_view hovercss = "", std::string_view pressedcss = "",
        std::string_view focusedcss = "", std::string_view checkedcss = "", std::string_view disblcss = "");
    void PushStyleFmt(int32_t state, std::string_view fmt, ...);
    void PushStyleFmt(std::string_view fmt, ...);
    void PushStyle(int32_t state, std::string_view css);
    void PopStyle(int depth = 1, int32_t state = WS_Default);

#ifndef GLIMMER_DISABLE_RICHTEXT
    void PushTextType(TextType type);
	void PopTextType();
#endif

    template <typename... ArgsT>
    void IgnoreStyleStack(ArgsT... args)
    {
        int32_t wtypes = ((1 << args) | ...);
        return _IgnoreStyleStackInternal(wtypes);
    }
    void RestoreStyleStack();

    std::pair<Sizing, bool> ParseLayoutStyle(LayoutBuilder& layout, std::string_view css, float pwidth, float pheight);

#ifndef GLIMMER_DISABLE_RICHTEXT

    [[nodiscard]] int SkipSpace(const char* text, int idx, int end);
    [[nodiscard]] int SkipSpace(const std::string_view text, int from = 0);
    [[nodiscard]] int WholeWord(const std::string_view text, int from = 0);
    [[nodiscard]] int SkipDigits(const std::string_view text, int from = 0);
    [[nodiscard]] int SkipFDigits(const std::string_view text, int from = 0);

    [[nodiscard]] bool AreSame(const std::string_view lhs, const char* rhs);
    [[nodiscard]] bool StartsWith(const std::string_view lhs, const char* rhs);
    [[nodiscard]] bool AreSame(const std::string_view lhs, const std::string_view rhs);

    [[nodiscard]] int ExtractInt(std::string_view input, int defaultVal);
    [[nodiscard]] int ExtractIntFromHex(std::string_view input, int defaultVal);
    [[nodiscard]] IntOrFloat ExtractNumber(std::string_view input, float defaultVal);
    [[nodiscard]] float ExtractFloatWithUnit(std::string_view input, float defaultVal, float ems, float parent, float scale);
    [[nodiscard]] FourSidedMeasure ExtractWithUnit(std::string_view input, float defaultVal, float ems, float parent, float scale);
    
    [[nodiscard]] uint32_t ExtractColor(std::string_view stylePropVal, uint32_t(*NamedColor)(const char*, void*), void* userData);
    std::pair<uint32_t, float> ExtractColorStop(std::string_view input, uint32_t(*NamedColor)(const char*, void*), void* userData);
    ColorGradient ExtractLinearGradient(std::string_view input, uint32_t(*NamedColor)(const char*, void*), void* userData);
    
    Border ExtractBorder(std::string_view input, float ems, float percent, uint32_t(*NamedColor)(const char*, void*), void* userData);
    BoxShadow ExtractBoxShadow(std::string_view input, float ems, float percent, uint32_t(*NamedColor)(const char*, void*), void* userData);
    std::pair<std::string_view, bool> ExtractTag(const char* text, int end, char tagEnd, int& idx, bool& tagStart);
    [[nodiscard]] std::optional<std::string_view> GetQuotedString(const char* text, int& idx, int end);

#endif

#define RECT_OUT(X) X.Min.x, X.Min.y, X.Max.x, X.Max.y
#define RECT_FMT "(%f, %f) x (%f, %f)"
}
