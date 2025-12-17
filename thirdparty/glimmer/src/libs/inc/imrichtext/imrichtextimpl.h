#pragma once
#ifndef GLIMMER_DISABLE_RICHTEXT
#include "imrichtextutils.h"

namespace ImRichText
{
    struct RenderConfig;

    struct ASCIITextShaper final : public ITextShaper
    {
        [[nodiscard]] static ASCIITextShaper* Instance();

        void ShapeText(float availwidth, const Span<std::string_view>& words,
            StyleAccessor accessor, LineRecorder lineRecorder, WordRecorder wordRecorder,
            const RenderConfig& config, void* userdata);
        void SegmentText(std::string_view content, WhitespaceCollapseBehavior wsbhv,
            LineRecorder lineRecorder, WordRecorder wordRecorder, const RenderConfig& config,
            bool ignoreLineBreaks, bool ignoreEscapeCodes, void* userdata);
        
        [[nodiscard]] int NextGraphemeCluster(const char* from, const char* end) const; // Dummy impl for ASCII, unused
        [[nodiscard]] int NextWordBreak(const char* from, const char* end) const; // Dummy impl for ASCII, unused
        [[nodiscard]] int NextLineBreak(const char* from, const char* end) const; // Dummy impl for ASCII, unused

        static const std::pair<std::string_view, std::string_view> EscapeCodes[6];
    };

    struct ASCIISymbolTextShaper final : public ITextShaper
    {
        [[nodiscard]] static ASCIISymbolTextShaper* Instance();

        void ShapeText(float availwidth, const Span<std::string_view>& words,
            StyleAccessor accessor, LineRecorder lineRecorder, WordRecorder wordRecorder,
            const RenderConfig& config, void* userdata);
        void SegmentText(std::string_view content, WhitespaceCollapseBehavior wsbhv,
            LineRecorder lineRecorder, WordRecorder wordRecorder, const RenderConfig& config,
            bool ignoreLineBreaks, bool ignoreEscapeCodes, void* userdata);

        [[nodiscard]] int NextGraphemeCluster(const char* from, const char* end) const;
        [[nodiscard]] int NextWordBreak(const char* from, const char* end) const;
        [[nodiscard]] int NextLineBreak(const char* from, const char* end) const;

        static const std::pair<std::string_view, std::string_view> EscapeCodes[11];
    };
}
#endif
