#pragma once

#ifndef GLIMMER_DISABLE_RICHTEXT

#include <string_view>
#include <optional>
#include <stdint.h>

#ifdef IM_RICHTEXT_TARGET_IMGUI
#include "imgui.h"
#include "imgui_internal.h"
#endif

#include "im_font_manager.h"
#include "renderer.h"
#include "style.h"

namespace ImRichText
{
	using FontType = glimmer::FontType;
	using TextContentCharset = glimmer::TextContentCharset;
}

#ifndef IM_RICHTEXT_MAX_COLORSTOPS
#define IM_RICHTEXT_MAX_COLORSTOPS 4
#endif

#ifndef IM_RICHTEXT_TARGET_IMGUI
#define IM_COL32_BLACK       ImRichText::ToRGBA(0, 0, 0, 255)
#define IM_COL32_BLACK_TRANS ImRichText::ToRGBA(0, 0, 0, 0)

struct ImVec2
{
    float x = 0.f, y = 0.f;
};

struct ImRect
{
    ImVec2 min, max;

    bool Contains(const ImRect& other) const
    {
        return min.x <= other.min.x && min.y <= other.min.y &&
            max.x >= other.max.x && max.y >= other.max.y;
    }

    void Expand(const float amount) 
    { 
        Min.x -= amount;   Min.y -= amount;   Max.x += amount;   Max.y += amount; 
    }

    void Translate(const ImVec2& d) 
    { 
        Min.x += d.x; Min.y += d.y; Max.x += d.x; Max.y += d.y; 
    }
};

enum ImGuiDir : int
{
    ImGuiDir_None = -1,
    ImGuiDir_Left = 0,
    ImGuiDir_Right = 1,
    ImGuiDir_Up = 2,
    ImGuiDir_Down = 3,
    ImGuiDir_COUNT
};

#endif

#if !defined(IMGUI_DEFINE_MATH_OPERATORS)
[[nodiscard]] ImVec2 operator+(ImVec2 lhs, ImVec2 rhs) { return ImVec2{ lhs.x + rhs.x, lhs.y + rhs.y }; }
[[nodiscard]] ImVec2 operator*(ImVec2 lhs, float rhs) { return ImVec2{ lhs.x * rhs, lhs.y * rhs }; }
[[nodiscard]] ImVec2 operator-(ImVec2 lhs, ImVec2 rhs) { return ImVec2{ lhs.x - rhs.x, lhs.y - rhs.y }; }
#endif

namespace ImRichText
{
    enum class BulletType
    {
        Circle,
        FilledCircle,
        Disk = FilledCircle,
        Square,
        Triangle,
        Arrow,
        CheckMark,
        CheckBox, // Needs fixing
        Concentric,
        Custom
    };

    template <typename T>
    struct Span
    {
        T const* source = nullptr;
        int sz = 0;

        template <typename ItrT>
        Span(ItrT start, ItrT end)
            : source{ &(*start) }, sz{ (int)(end - start) }
        {}

        Span(T const* from, int size) : source{ from }, sz{ size } {}

        explicit Span(T const & el) : source{ &el }, sz{ 1 } {}

        T const* begin() const { return source; }
        T const* end() const { return source + sz; }

        const T& front() const { return *source; }
        const T& back() const { return *(source + sz - 1); }

        const T& operator[](int idx) const { return source[idx]; }
    };

    template <typename T> Span(T* from, int size) -> Span<T>;
    template <typename T> Span(T&) -> Span<T>;

    struct BoundedBox
    {
        float top = 0.f, left = 0.f;
        float width = 0.f, height = 0.f;

        ImVec2 start(ImVec2 origin) const { return ImVec2{ left, top } + origin; }
        ImVec2 end(ImVec2 origin) const { return ImVec2{ left + width, top + height } + origin; }
        ImVec2 center(ImVec2 origin) const { return ImVec2{ left + (0.5f * width), top + (0.5f * height) } + origin; }
    };

	struct IRenderer
    {
        void* UserData = nullptr;

        virtual float EllipsisWidth(void* fontptr, float sz);

        virtual void DrawBullet(ImVec2 startpos, ImVec2 endpos, uint32_t color, int index, int depth);
        void DrawDefaultBullet(BulletType type, ImVec2 initpos, const BoundedBox& bounds, uint32_t color, float bulletsz);
    };

    enum class WhitespaceCollapseBehavior
    {
        Collapse, Preserve, PreserveBreaks, PreserveSpaces, BreakSpaces
    };

    enum class WordBreakBehavior
    {
        Normal, BreakAll, KeepAll, AutoPhrase, BreakWord
    };

    struct ITextShaper
    {
        struct WordProperty
        {
            void* font;
            float sz;
            WordBreakBehavior wb;
        };

        using StyleAccessor = WordProperty (*)(int wordIdx, void* userdata);
        using LineRecorder = void (*)(int wordIdx, void* userdata);
        using WordRecorder = void (*)(int wordIdx, std::string_view, ImVec2 dim, void* userdata);

        virtual void ShapeText(float availwidth, const Span<std::string_view>& words,
            StyleAccessor accessor, LineRecorder lineRecorder, WordRecorder wordRecorder, 
            const RenderConfig& config, void* userdata) = 0;
        virtual void SegmentText(std::string_view content, WhitespaceCollapseBehavior wsbhv, 
            LineRecorder lineRecorder, WordRecorder wordRecorder, const RenderConfig& config, 
            bool ignoreLineBreaks, bool ignoreEscapeCodes, void* userdata) = 0;

        virtual int NextGraphemeCluster(const char* from, const char* end) const = 0;
        virtual int NextWordBreak(const char* from, const char* end) const = 0;
        virtual int NextLineBreak(const char* from, const char* end) const = 0;

        // TODO: Implement these...
        int SkipSuccessiveWordBreaks(const char* from, const char* end) const;
        int SkipSuccessiveLineBreaks(const char* from, const char* end) const;
    };

    // Implement this interface to handle parsed rich text
    // NOTE: If you are questioning why this interface if there is
    // only one implementation in the codebase, you are right!
    // However, this interface along with ImRichText::ParseRichText
    // function can be used for parsing HTML correctly for any purpose
    // you see fit, hence the code can be copied and used easily!
    // ADDENDUM: There are some larger goals surrounding this...
    struct ITagVisitor
    {
        virtual bool TagStart(std::string_view tag) = 0;
        virtual bool Attribute(std::string_view name, std::optional<std::string_view> value) = 0;
        virtual bool TagStartDone() = 0;
        virtual bool Content(std::string_view content) = 0;
        virtual bool TagEnd(std::string_view tag, bool selfTerminating) = 0;
        virtual void Finalize() = 0;

        virtual void Error(std::string_view tag) = 0;

        virtual bool IsSelfTerminating(std::string_view tag) const = 0;
        virtual bool IsPreformattedContent(std::string_view tag) const = 0;
    };

    // Implement this to handle platform interactions
    struct IPlatform
    {
        virtual ImVec2 GetCurrentMousePos() = 0;
        virtual bool IsMouseClicked() = 0;

        virtual void HandleHyperlink(std::string_view) = 0;
        virtual void RequestFrame() = 0;
        virtual void HandleHover(bool) = 0;
    };

    struct IntOrFloat
    {
        float value = 0.f;
        bool isFloat = false;
    };

	using ColorStop = glimmer::ColorStop;
	using ColorGradient = glimmer::ColorGradient;
	using FourSidedMeasure = glimmer::FourSidedMeasure;
	using FourSidedBorder = glimmer::FourSidedBorder;
	using Border = glimmer::Border;
    using BoxCorner = glimmer::BoxCorner;
    using LineType = glimmer::LineType;

    // Generic string helpers, case-insensitive matches
    using glimmer::AreSame;
    using glimmer::StartsWith;
    using glimmer::SkipDigits;
    using glimmer::SkipFDigits;
    using glimmer::SkipSpace;
    using glimmer::GetQuotedString;

    // String to number conversion functions
    using glimmer::ExtractInt;
    using glimmer::ExtractIntFromHex;
    using glimmer::ExtractNumber;
    using glimmer::ExtractFloatWithUnit;

    // Color related functions
    using glimmer::ToRGBA;
    using glimmer::IsColorVisible;
 
    // Parsing functions
    using glimmer::ExtractColor;
    using glimmer::ExtractLinearGradient;
    using glimmer::ExtractBorder;

    // Parse rich text and invoke appropriate visitor methods
    void ParseRichText(const char* text, const char* textend, char TagStart, char TagEnd, ITagVisitor& visitor);
}

#endif