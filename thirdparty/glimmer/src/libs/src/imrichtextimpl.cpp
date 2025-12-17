#define _USE_MATH_DEFINES
#include <math.h>

#ifndef GLIMMER_DISABLE_RICHTEXT
#include "imrichtextimpl.h"
#include "imrichtext.h"

#include <cctype>

namespace ImRichText
{
    // This only returns the bytes size of "codepoints" as per UTF-8 encoding
    // This does not return the grapheme cluster size w.r.t bytes
    static int UTF8CharSize(unsigned char leading)
    {
        constexpr unsigned char FourByteChar = 0b11110000;
        constexpr unsigned char ThreeByteChar = 0b11100000;
        constexpr unsigned char TwoByteChar = 0b11000000;
        constexpr unsigned char OneByteChar = 0b01111111;
        return (leading & OneByteChar) == 0 ? 1 :
            (leading & FourByteChar) == FourByteChar ? 4 :
            (leading & ThreeByteChar) == ThreeByteChar ? 3 : 2;
    }

    std::tuple<bool, bool, std::string_view> AddEscapeSequences(const std::string_view content,
        Span<std::pair<std::string_view, std::string_view>> escapeCodes, int& curridx, 
        char escapeStart, char escapeEnd)
    {
        auto hasEscape = false;
        auto isNewLine = false;
        std::string_view value;

        for (const auto& pair : escapeCodes)
        {
            auto sz = (int)pair.first.size();
            if ((curridx + sz) < (int)content.size() &&
                AreSame(content.substr(curridx, sz), pair.first) &&
                content[curridx + sz] == escapeEnd)
            {
                if (pair.second == "\n") isNewLine = true;
                else
                {
                    value = pair.second;
                }

                curridx += sz + 1;
                hasEscape = true;
                break;
            }
        }

        return { hasEscape, isNewLine, value };
    }

    ASCIITextShaper* ASCIITextShaper::Instance()
    {
        static ASCIITextShaper shaper;
        return &shaper;
    }

    void ASCIITextShaper::ShapeText(float availwidth, const Span<std::string_view>& words, 
        StyleAccessor accessor, LineRecorder lineRecorder, WordRecorder wordRecorder, const RenderConfig& config, 
        void* userdata)
    {
        auto currentx = 0.f;

        for (auto idx = 0; idx < words.sz; ++idx)
        {
            auto font = accessor(idx, userdata);
            auto sz = config.Renderer->GetTextSize(words[idx], font.font, font.sz);

            if ((sz.x > availwidth) && (font.wb == WordBreakBehavior::BreakWord || font.wb == WordBreakBehavior::BreakAll))
            {
                lineRecorder(idx, userdata);
                currentx = 0.f;

                ImVec2 currsz{ 0.f, 0.f };
                auto lastidx = 0, chidx = 1;

                for (; chidx < (int)words[idx].size(); chidx++)
                {
                    auto chsz = config.Renderer->GetTextSize(words[idx].substr(chidx, 1), font.font, font.sz);

                    if (currsz.x + chsz.x > availwidth)
                    {
                        wordRecorder(idx, words[idx].substr(lastidx, (std::size_t)(chidx - lastidx)), currsz, userdata);
                        lineRecorder(idx, userdata);
                        currsz.x = 0.f;
                        lastidx = chidx - 1;
                    }
                }

                wordRecorder(idx, words[idx].substr(lastidx, (std::size_t)(chidx - lastidx)), currsz, userdata);
                currentx += currsz.x;
            }
            else
            {
                if (currentx + sz.x > availwidth)
                {
                    lineRecorder(idx, userdata);
                    currentx = 0.f;
                }

                wordRecorder(idx, words[idx], sz, userdata);
                currentx += sz.x;
            }
        }
    }

    void ASCIITextShaper::SegmentText(std::string_view content, WhitespaceCollapseBehavior wsbhv, 
        LineRecorder lineRecorder, WordRecorder wordRecorder, const RenderConfig& config, 
        bool ignoreLineBreaks, bool ignoreEscapeCodes, void* userdata)
    {
        auto to = 0;

        while (to < (int)content.size())
        {
            // If this assert hits, then text is not ASCII!
            assert(content[to] > 0);

            if (content[to] == '\n')
            {
                if (ignoreLineBreaks) { to++; continue; }

                switch (wsbhv)
                {
                case ImRichText::WhitespaceCollapseBehavior::PreserveSpaces: [[fallthrough]];
                case ImRichText::WhitespaceCollapseBehavior::BreakSpaces: [[fallthrough]];
                case ImRichText::WhitespaceCollapseBehavior::Collapse:
                {
                    auto from = to;
                    while ((to < (int)content.size()) && (content[to] == '\n')) to++;
                    to--;
                    break;
                } 
                case ImRichText::WhitespaceCollapseBehavior::PreserveBreaks: [[fallthrough]];
                case ImRichText::WhitespaceCollapseBehavior::Preserve:
                {
                    while ((to < (int)content.size()) && (content[to] == '\n'))
                    {
                        lineRecorder(-1, userdata);
                        to++;
                    }

                    to--;
                    break;
                }
                default: break;
                }
                
            }
            else if (std::isspace(content[to]))
            {
                switch (wsbhv)
                {
                case ImRichText::WhitespaceCollapseBehavior::PreserveBreaks: [[fallthrough]];
                case ImRichText::WhitespaceCollapseBehavior::Collapse:
                {
                    auto from = to;
                    to = SkipSpace(content, to);
                    to--;
                    break;
                }
                case ImRichText::WhitespaceCollapseBehavior::Preserve: [[fallthrough]];
                case ImRichText::WhitespaceCollapseBehavior::PreserveSpaces: [[fallthrough]];
                case ImRichText::WhitespaceCollapseBehavior::BreakSpaces:
                {
                    auto from = to;
                    to = SkipSpace(content, to);
                    wordRecorder(-1, content.substr(from, (std::size_t)(to - from)), {}, userdata);
                    to--;
                    break;
                }
                default:
                    break;
                }
            }
            else if (!ignoreEscapeCodes && (content[to] == config.EscapeSeqStart))
            {
                to++;
                auto [hasEscape, isNewLine, substitute] = AddEscapeSequences(content, 
                    Span{ EscapeCodes, 6 }, to, config.EscapeSeqStart, config.EscapeSeqEnd);

                if (isNewLine && !ignoreLineBreaks)
                {
                    lineRecorder(-1, userdata);
                }
                else if (hasEscape)
                {
                    wordRecorder(-1, substitute, {}, userdata);
                }
                else
                {
                    auto from = to;
                    while ((to < (int)content.size()) && std::isgraph(content[to]) &&
                        (content[to] != config.EscapeSeqStart)) to++;
                    if ((to < (int)content.size()) && !std::isspace(content[to])) to--;

                    wordRecorder(-1, content.substr(from, (std::size_t)(to - from + 1)), {}, userdata);
                }
            }
            else
            {
                auto from = to;
                while ((to < (int)content.size()) && std::isgraph(content[to]) &&
                    (ignoreEscapeCodes || (!ignoreEscapeCodes  && (content[to] != config.EscapeSeqStart)))) to++;
                if ((to < (int)content.size()) && !std::isspace(content[to])) to--;

                wordRecorder(-1, content.substr(from, (std::size_t)(to - from + 1)), {}, userdata);
            }

            to++;
        }
    }

    // Dummy impl for ASCII, unused
    int ASCIITextShaper::NextGraphemeCluster(const char* from, const char* end) const
    {
        return 1;
    }

    // Dummy impl for ASCII, unused
    int ASCIITextShaper::NextWordBreak(const char* from, const char* end) const
    {
        return 1;
    }

    // Dummy impl for ASCII, unused
    int ASCIITextShaper::NextLineBreak(const char* from, const char* end) const
    {
        return 1;
    }

    const std::pair<std::string_view, std::string_view> ASCIITextShaper::EscapeCodes[6]{
        { "Tab", "\t" }, { "NewLine", "\n" }, { "nbsp", " " },
        { "gt", ">" }, { "lt", "<" }, { "amp", "&" }
    };

    ASCIISymbolTextShaper* ASCIISymbolTextShaper::Instance()
    {
        static ASCIISymbolTextShaper shaper;
        return &shaper;
    }

    void ASCIISymbolTextShaper::ShapeText(float availwidth, const Span<std::string_view>& words, 
        StyleAccessor accessor, LineRecorder lineRecorder, WordRecorder wordRecorder, 
        const RenderConfig& config, void* userdata)
    {
        auto currentx = 0.f;

        for (auto idx = 0; idx < words.sz; ++idx)
        {
            auto font = accessor(idx, userdata);
            auto sz = config.Renderer->GetTextSize(words[idx], font.font, font.sz);

            if ((sz.x > availwidth) && (font.wb == WordBreakBehavior::BreakWord || font.wb == WordBreakBehavior::BreakAll))
            {
                lineRecorder(idx, userdata);
                currentx = 0.f;

                ImVec2 currsz{ 0.f, 0.f };
                auto lastidx = 0, chidx = 1;

                for (; chidx < (int)words[idx].size();)
                {
                    auto charsz = NextGraphemeCluster(words[idx].data() + lastidx, words[idx].data() + words[idx].size());
                    auto chsz = config.Renderer->GetTextSize(words[idx].substr(chidx, charsz), font.font, font.sz);

                    if (currsz.x + chsz.x > availwidth)
                    {
                        wordRecorder(idx, words[idx].substr(lastidx, (std::size_t)(chidx - lastidx)), currsz, userdata);
                        lineRecorder(idx, userdata);
                        currsz.x = 0.f;
                        lastidx = chidx;
                    }

                    chidx += charsz;
                }

                wordRecorder(idx, words[idx].substr(lastidx, (std::size_t)(chidx - lastidx)), currsz, userdata);
                currentx += currsz.x;
            }
            else
            {
                if (currentx + sz.x > availwidth)
                {
                    lineRecorder(idx, userdata);
                    currentx = 0.f;
                }

                wordRecorder(idx, words[idx], sz, userdata);
                currentx += sz.x;
            }
        }
    }

    void ASCIISymbolTextShaper::SegmentText(std::string_view content, WhitespaceCollapseBehavior wsbhv, 
        LineRecorder lineRecorder, WordRecorder wordRecorder, const RenderConfig& config, bool ignoreLineBreaks, 
        bool ignoreEscapeCodes, void* userdata)
    {
        auto to = 0;

        while (to < (int)content.size())
        {
            if (content[to] == '\n')
            {
                if (ignoreLineBreaks) { to++; continue; }

                switch (wsbhv)
                {
                case ImRichText::WhitespaceCollapseBehavior::PreserveSpaces: [[fallthrough]];
                case ImRichText::WhitespaceCollapseBehavior::BreakSpaces: [[fallthrough]];
                case ImRichText::WhitespaceCollapseBehavior::Collapse:
                {
                    auto from = to;
                    while ((to < (int)content.size()) && (content[to] == '\n')) to++;
                    to--;
                    break;
                }
                case ImRichText::WhitespaceCollapseBehavior::PreserveBreaks: [[fallthrough]];
                case ImRichText::WhitespaceCollapseBehavior::Preserve:
                {
                    while ((to < (int)content.size()) && (content[to] == '\n'))
                    {
                        lineRecorder(-1, userdata);
                        to++;
                    }

                    to--;
                    break;
                }
                default: break;
                }

            }
            else if (std::isspace(content[to]))
            {
                switch (wsbhv)
                {
                case ImRichText::WhitespaceCollapseBehavior::PreserveBreaks: [[fallthrough]];
                case ImRichText::WhitespaceCollapseBehavior::Collapse:
                {
                    auto from = to;
                    to = SkipSpace(content, to);
                    to--;
                    break;
                }
                case ImRichText::WhitespaceCollapseBehavior::Preserve: [[fallthrough]];
                case ImRichText::WhitespaceCollapseBehavior::PreserveSpaces: [[fallthrough]];
                case ImRichText::WhitespaceCollapseBehavior::BreakSpaces:
                {
                    auto from = to;
                    to = SkipSpace(content, to);
                    wordRecorder(-1, content.substr(from, (std::size_t)(to - from)), {}, userdata);
                    to--;
                    break;
                }
                default:
                    break;
                }
            }
            else if (!ignoreEscapeCodes && (content[to] == config.EscapeSeqStart))
            {
                to++;
                auto [hasEscape, isNewLine, substitute] = AddEscapeSequences(content,
                    Span{ EscapeCodes, 6 }, to, config.EscapeSeqStart, config.EscapeSeqEnd);

                if (isNewLine && !ignoreLineBreaks)
                {
                    lineRecorder(-1, userdata);
                }
                else if (hasEscape)
                {
                    wordRecorder(-1, substitute, {}, userdata);
                }
                else
                {
                    auto from = to;
                    while ((to < (int)content.size()) && std::isgraph(content[to]) &&
                        (content[to] != config.EscapeSeqStart)) to++;
                    if ((to < (int)content.size()) && !std::isspace(content[to])) to--;

                    wordRecorder(-1, content.substr(from, (std::size_t)(to - from + 1)), {}, userdata);
                }
            }
            else
            {
                auto from = to;
                while ((to < (int)content.size()) && std::isgraph(content[to]) &&
                    (ignoreEscapeCodes || (!ignoreEscapeCodes && (content[to] != config.EscapeSeqStart)))) to++;
                if ((to < (int)content.size()) && !std::isspace(content[to])) to--;

                wordRecorder(-1, content.substr(from, (std::size_t)(to - from + 1)), {}, userdata);
            }

            to++;
        }
    }

    int ASCIISymbolTextShaper::NextGraphemeCluster(const char* from, const char* end) const
    {
        auto sz = UTF8CharSize(*from);
        sz = std::min(sz, (int)(end - from));
		return sz;
    }

    int ASCIISymbolTextShaper::NextWordBreak(const char* from, const char* end) const
    {
        auto initial = from;
        while ((from < end) && (*from != ' ' && *from != '\t')) from += UTF8CharSize(*from);
        return from - initial;
    }

    int ASCIISymbolTextShaper::NextLineBreak(const char* from, const char* end) const
    {
        auto initial = from;
        while ((from < end) && (*from != '\n')) from += UTF8CharSize(*from);
        return from - initial;
    }

    const std::pair<std::string_view, std::string_view> ASCIISymbolTextShaper::EscapeCodes[11] = {
        { "Tab", "\t" }, { "NewLine", "\n" }, { "nbsp", " " },
        { "gt", ">" }, { "lt", "<" },
        { "amp", "&" }, { "copy", "©" }, { "reg", "®" },
        { "deg", "°" }, { "micro", "μ" }, { "trade", "™" }
    };
}
#endif