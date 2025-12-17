#include "types.h"
#include "context.h"

#include <cctype>
#include <unordered_map>
#include <variant>
#include "style.h"

#ifndef GLIMMER_STYLE_BUFSZ 
#define GLIMMER_STYLE_BUFSZ 4096
#endif

namespace glimmer
{
    // This maps the styles to classes/ids which do not follow the style stack.
    // When applying a style to a widget, any class/id specified with the widget will be used to lookup
    // corresponding styles, merged (with the current stack as well), and then applied.
    static std::unordered_map<std::string_view, StyleDescriptor[WSI_Total]> StyleSheet;

#pragma optimize( "", on )

    [[nodiscard]] int SkipSpace(const char* text, int idx, int end)
    {
        while ((idx < end) && std::isspace(text[idx])) idx++;
        return idx;
    }

#ifdef GLIMMER_DISABLE_RICHTEXT
    [[nodiscard]] int SkipSpace(const std::string_view text, int from = 0)
#else
    int SkipSpace(const std::string_view text, int from)
#endif
    {
        auto end = (int)text.size();
        while ((from < end) && (std::isspace(text[from]))) from++;
        return from;
    }

#ifdef GLIMMER_DISABLE_RICHTEXT
    [[nodiscard]] int WholeWord(const std::string_view text, int from = 0)
#else
    int WholeWord(const std::string_view text, int from)
#endif
    {
        auto end = (int)text.size();
        while ((from < end) && (!std::isspace(text[from]))) from++;
        return from;
    }

#ifdef GLIMMER_DISABLE_RICHTEXT
    [[nodiscard]] int SkipDigits(const std::string_view text, int from = 0)
#else
    int SkipDigits(const std::string_view text, int from)
#endif
    {
        auto end = (int)text.size();
        while ((from < end) && (std::isdigit(text[from]))) from++;
        return from;
    }

#ifdef GLIMMER_DISABLE_RICHTEXT
    [[nodiscard]] int SkipFDigits(const std::string_view text, int from = 0)
#else
    int SkipFDigits(const std::string_view text, int from)
#endif
    {
        auto end = (int)text.size();
        while ((from < end) && ((std::isdigit(text[from])) || (text[from] == '.'))) from++;
        return from;
    }
#pragma optimize( "", off )

    [[nodiscard]] bool AreSame(const std::string_view lhs, const char* rhs)
    {
        auto rlimit = (int)std::strlen(rhs);
        auto llimit = (int)lhs.size();
        if (rlimit != llimit)
            return false;

        for (auto idx = 0; idx < rlimit; ++idx)
            if (std::tolower(lhs[idx]) != std::tolower(rhs[idx]))
                return false;

        return true;
    }

    [[nodiscard]] bool StartsWith(const std::string_view lhs, const char* rhs)
    {
        auto rlimit = (int)std::strlen(rhs);
        auto llimit = (int)lhs.size();
        if (rlimit > llimit)
            return false;

        for (auto idx = 0; idx < rlimit; ++idx)
            if (std::tolower(lhs[idx]) != std::tolower(rhs[idx]))
                return false;

        return true;
    }

    [[nodiscard]] bool AreSame(const std::string_view lhs, const std::string_view rhs)
    {
        auto rlimit = (int)rhs.size();
        auto llimit = (int)lhs.size();
        if (rlimit != llimit)
            return false;

        for (auto idx = 0; idx < rlimit; ++idx)
            if (std::tolower(lhs[idx]) != std::tolower(rhs[idx]))
                return false;

        return true;
    }

    [[nodiscard]] int ExtractInt(std::string_view input, int defaultVal)
    {
        int result = defaultVal;
        int base = 1;
        auto idx = (int)input.size() - 1;
        while (!std::isdigit(input[idx]) && idx >= 0) idx--;

        for (; idx >= 0; --idx)
        {
            result += (input[idx] - '0') * base;
            base *= 10;
        }

        return result;
    }

    [[nodiscard]] int ExtractIntFromHex(std::string_view input, int defaultVal)
    {
        int result = defaultVal;
        int base = 1;
        auto idx = (int)input.size() - 1;
        while (!std::isdigit(input[idx]) && !std::isalpha(input[idx]) && idx >= 0) idx--;

        for (; idx >= 0; --idx)
        {
            result += std::isdigit(input[idx]) ? (input[idx] - '0') * base :
                std::islower(input[idx]) ? ((input[idx] - 'a') + 10) * base :
                ((input[idx] - 'A') + 10) * base;
            base *= 16;
        }

        return result;
    }

    [[nodiscard]] IntOrFloat ExtractNumber(std::string_view input, float defaultVal)
    {
        float result = 0.f, base = 1.f;
        bool isInt = false;
        auto idx = (int)input.size() - 1;

        while (idx >= 0 && input[idx] != '.') idx--;
        auto decimal = idx;

        if (decimal != -1)
        {
            for (auto midx = decimal - 1; midx >= 0; --midx)
            {
                result += (input[midx] - '0') * base;
                base *= 10.f;
            }

            base = 0.1f;
            for (auto midx = decimal + 1; midx < (int)input.size(); ++midx)
            {
                result += (input[midx] - '0') * base;
                base *= 0.1f;
            }
        }
        else
        {
            for (auto midx = (int)input.size() - 1; midx >= 0; --midx)
            {
                result += (input[midx] - '0') * base;
                base *= 10.f;
            }

            isInt = true;
        }

        return { result, !isInt };
    }

    [[nodiscard]] float ExtractFloatWithUnit(std::string_view input, float defaultVal, float ems, float parent, float scale)
    {
        float result = defaultVal, base = 1.f;
        auto idx = (int)input.size() - 1;
        while (!std::isdigit(input[idx]) && idx >= 0) idx--;

        if (AreSame(input.substr(idx + 1), "pt")) scale = 1.3333f;
        else if (AreSame(input.substr(idx + 1), "em")) scale = ems;
        else if (input[idx + 1] == '%') scale = parent * 0.01f;

        auto num = ExtractNumber(input.substr(0, idx + 1), defaultVal);
        result = num.value;

        return result * scale;
    }

    [[nodiscard]] FourSidedMeasure ExtractWithUnit(std::string_view input, float defaultVal, float ems, float parent, float scale)
    {
        FourSidedMeasure result;
        auto idx = SkipSpace(input, 0);
        auto end = (int)input.size();
        while ((idx < end) && !std::isspace(input[idx])) idx++;
        result.top = result.bottom = result.left = result.right = ExtractFloatWithUnit(input.substr(0, idx), defaultVal, ems, parent, scale);
        idx = SkipSpace(input, idx);

        if (idx < ((int)input.size() - 1))
        {
            auto start = idx;
            while ((idx < end) && !std::isspace(input[idx])) idx++;
            result.right = ExtractFloatWithUnit(input.substr(start, idx - start), defaultVal, ems, parent, scale);
            idx = SkipSpace(input, idx);

            start = idx;
            while ((idx < end) && !std::isspace(input[idx])) idx++;
            result.bottom = ExtractFloatWithUnit(input.substr(start, idx - start), defaultVal, ems, parent, scale);
            idx = SkipSpace(input, idx);

            start = idx;
            while ((idx < end) && !std::isspace(input[idx])) idx++;
            result.left = ExtractFloatWithUnit(input.substr(start, idx - start), defaultVal, ems, parent, scale);
        }

        return result;
    }

    [[nodiscard]] std::tuple<IntOrFloat, IntOrFloat, IntOrFloat, IntOrFloat> GetCommaSeparatedNumbers(std::string_view stylePropVal, int& curr)
    {
        std::tuple<IntOrFloat, IntOrFloat, IntOrFloat, IntOrFloat> res;
        auto hasFourth = curr == 4;
        curr = SkipSpace(stylePropVal, curr);
        assert(stylePropVal[curr] == '(');
        curr++;

        auto valstart = curr;
        curr = SkipFDigits(stylePropVal, curr);
        std::get<0>(res) = ExtractNumber(stylePropVal.substr(valstart, curr - valstart), 0);
        curr = SkipSpace(stylePropVal, curr);
        assert(stylePropVal[curr] == ',');
        curr++;
        curr = SkipSpace(stylePropVal, curr);

        valstart = curr;
        curr = SkipFDigits(stylePropVal, curr);
        std::get<1>(res) = ExtractNumber(stylePropVal.substr(valstart, curr - valstart), 0);
        curr = SkipSpace(stylePropVal, curr);
        assert(stylePropVal[curr] == ',');
        curr++;
        curr = SkipSpace(stylePropVal, curr);

        valstart = curr;
        curr = SkipFDigits(stylePropVal, curr);
        std::get<2>(res) = ExtractNumber(stylePropVal.substr(valstart, curr - valstart), 0);
        curr = SkipSpace(stylePropVal, curr);

        if (hasFourth)
        {
            assert(stylePropVal[curr] == ',');
            curr++;
            curr = SkipSpace(stylePropVal, curr);

            valstart = curr;
            curr = SkipFDigits(stylePropVal, curr);
            std::get<3>(res) = ExtractNumber(stylePropVal.substr(valstart, curr - valstart), 0);
        }

        return res;
    }

    [[nodiscard]] uint32_t ExtractColor(std::string_view stylePropVal, uint32_t(*NamedColor)(const char*, void*), void* userData)
    {
        if (stylePropVal.size() >= 3u && AreSame(stylePropVal.substr(0, 3), "rgb"))
        {
            IntOrFloat r, g, b, a;
            auto hasAlpha = stylePropVal[3] == 'a' || stylePropVal[3] == 'A';
            int curr = hasAlpha ? 4 : 3;
            std::tie(r, g, b, a) = GetCommaSeparatedNumbers(stylePropVal, curr);
            auto isRelative = r.isFloat && g.isFloat && b.isFloat;
            a.value = isRelative ? hasAlpha ? a.value : 1.f :
                hasAlpha ? a.value : 255;

            assert(stylePropVal[curr] == ')');
            return isRelative ? ToRGBA(r.value, g.value, b.value, a.value) :
                ToRGBA((int)r.value, (int)g.value, (int)b.value, (int)a.value);
        }
        else if (AreSame(stylePropVal, "transparent"))
        {
            return ToRGBA(0, 0, 0, 0);
        }
        else if (NamedColor != nullptr)
        {
            static char buffer[32] = { 0 };
            memset(buffer, 0, 32);
            memcpy(buffer, stylePropVal.data(), std::min((int)stylePropVal.size(), 31));
            return NamedColor(buffer, userData);
        }
        else if (stylePropVal.size() >= 3u && AreSame(stylePropVal.substr(0, 3), "hsv"))
        {
            IntOrFloat h, s, v;
            auto curr = 3;
            std::tie(h, s, v, std::ignore) = GetCommaSeparatedNumbers(stylePropVal, curr);

            assert(stylePropVal[curr] == ')');
            return ImColor::HSV(h.value, s.value, v.value);
        }
        else if (stylePropVal.size() >= 3u && AreSame(stylePropVal.substr(0, 3), "hsl"))
        {
            IntOrFloat h, s, l;
            auto curr = 3;
            std::tie(h, s, l, std::ignore) = GetCommaSeparatedNumbers(stylePropVal, curr);
            auto v = l.value + s.value * std::min(l.value, 1.f - l.value);
            s.value = v == 0.f ? 0.f : 2.f * (1.f - (l.value / v));

            assert(stylePropVal[curr] == ')');
            return ImColor::HSV(h.value, s.value, v);
        }
        else if (stylePropVal.size() >= 1u && stylePropVal[0] == '#')
        {
            return (uint32_t)ExtractIntFromHex(stylePropVal.substr(1), 0);
        }
        else
        {
            return IM_COL32_BLACK;
        }
    }

    std::pair<uint32_t, float> ExtractColorStop(std::string_view input, uint32_t(*NamedColor)(const char*, void*), void* userData)
    {
        auto idx = 0;
        std::pair<uint32_t, float> colorstop;

        idx = WholeWord(input, idx);
        colorstop.first = ExtractColor(input.substr(0, idx), NamedColor, userData);
        idx = SkipSpace(input, idx);

        if ((idx < (int)input.size()) && std::isdigit(input[idx]))
        {
            auto start = idx;
            idx = SkipDigits(input, start);
            colorstop.second = ExtractNumber(input.substr(start, idx - start), -1.f).value;
        }
        else colorstop.second = -1.f;

        return colorstop;
    }

    ColorGradient ExtractLinearGradient(std::string_view input, uint32_t(*NamedColor)(const char*, void*), void* userData)
    {
        ColorGradient gradient;
        auto idx = 15; // size of "linear-gradient" string

        if (idx < (int)input.size())
        {
            idx = SkipSpace(input, idx);
            assert(input[idx] == '(');
            idx++;
            idx = SkipSpace(input, idx);

            std::optional<std::pair<uint32_t, float>> lastStop = std::nullopt;
            auto firstPart = true;
            auto start = idx;
            auto total = 0.f, unspecified = 0.f;

            do
            {
                idx = SkipSpace(input, idx);

                auto start = idx;
                while ((idx < (int)input.size()) && (input[idx] != ',') && (input[idx] != ')')
                    && !std::isspace(input[idx])) idx++;
                auto part = input.substr(start, idx - start);
                idx = SkipSpace(input, idx);
                auto isEnd = input[idx] == ')';

                if ((idx < (int)input.size()) && (input[idx] == ',' || isEnd)) {
                    if (firstPart)
                    {
                        if (AreSame(input, "to right")) {
                            gradient.dir = ImGuiDir::ImGuiDir_Right;
                        }
                        else if (AreSame(input, "to left")) {
                            gradient.dir = ImGuiDir::ImGuiDir_Left;
                        }
                        else {
                            auto colorstop = ExtractColorStop(part, NamedColor, userData);
                            if (colorstop.second != -1.f) total += colorstop.second;
                            else unspecified += 1.f;
                            lastStop = colorstop;
                        }
                        firstPart = false;
                    }
                    else {
                        auto colorstop = ExtractColorStop(part, NamedColor, userData);
                        if (colorstop.second != -1.f) total += colorstop.second;
                        else unspecified += 1.f;

                        if (lastStop.has_value())
                        {
                            gradient.colorStops[gradient.totalStops] =
                                ColorStop{ lastStop.value().first, colorstop.first, colorstop.second };
                            gradient.totalStops = std::min(gradient.totalStops + 1, GLIMMER_MAX_COLORSTOPS - 1);
                        }

                        lastStop = colorstop;
                    }
                }
                else break;

                if (isEnd) break;
                else if (input[idx] == ',') idx++;
            } while ((idx < (int)input.size()) && (input[idx] != ')'));

            unspecified -= 1.f;
            for (auto& colorstop : gradient.colorStops)
                if (colorstop.pos == -1.f) colorstop.pos = (100.f - total) / (100.f * unspecified);
                else colorstop.pos /= 100.f;
        }

        return gradient;
    }

    template <int maxsz>
    struct CaseInsensitiveHasher
    {
        std::size_t operator()(std::string_view key) const
        {
            thread_local static char buffer[maxsz] = { 0 };
            memset(buffer, 0, maxsz);
            auto limit = std::min((int)key.size(), maxsz - 1);

            for (auto idx = 0; idx < limit; ++idx)
                buffer[idx] = std::tolower(key[idx]);

            return std::hash<std::string_view>()(buffer);
        }
    };

    uint32_t GetColor(const char* name, void*)
    {
        const static std::unordered_map<std::string_view, uint32_t, CaseInsensitiveHasher<32>> Colors{
            { "black", ToRGBA(0, 0, 0) },
            { "silver", ToRGBA(192, 192, 192) },
            { "gray", ToRGBA(128, 128, 128) },
            { "white", ToRGBA(255, 255, 255) },
            { "maroon", ToRGBA(128, 0, 0) },
            { "red", ToRGBA(255, 0, 0) },
            { "purple", ToRGBA(128, 0, 128) },
            { "fuchsia", ToRGBA(255, 0, 255) },
            { "green", ToRGBA(0, 128, 0) },
            { "lime", ToRGBA(0, 255, 0) },
            { "olive", ToRGBA(128, 128, 0) },
            { "yellow", ToRGBA(255, 255, 0) },
            { "navy", ToRGBA(0, 0, 128) },
            { "blue", ToRGBA(0, 0, 255) },
            { "teal", ToRGBA(0, 128, 128) },
            { "aqua", ToRGBA(0, 255, 255) },
            { "aliceblue", ToRGBA(240, 248, 255) },
            { "antiquewhite", ToRGBA(250, 235, 215) },
            { "aquamarine", ToRGBA(127, 255, 212) },
            { "azure", ToRGBA(240, 255, 255) },
            { "beige", ToRGBA(245, 245, 220) },
            { "bisque", ToRGBA(255, 228, 196) },
            { "blanchedalmond", ToRGBA(255, 235, 205) },
            { "blueviolet", ToRGBA(138, 43, 226) },
            { "brown", ToRGBA(165, 42, 42) },
            { "burlywood", ToRGBA(222, 184, 135) },
            { "cadetblue", ToRGBA(95, 158, 160) },
            { "chartreuse", ToRGBA(127, 255, 0) },
            { "chocolate", ToRGBA(210, 105, 30) },
            { "coral", ToRGBA(255, 127, 80) },
            { "cornflowerblue", ToRGBA(100, 149, 237) },
            { "cornsilk", ToRGBA(255, 248, 220) },
            { "crimson", ToRGBA(220, 20, 60) },
            { "darkblue", ToRGBA(0, 0, 139) },
            { "darkcyan", ToRGBA(0, 139, 139) },
            { "darkgoldenrod", ToRGBA(184, 134, 11) },
            { "darkgray", ToRGBA(169, 169, 169) },
            { "darkgreen", ToRGBA(0, 100, 0) },
            { "darkgrey", ToRGBA(169, 169, 169) },
            { "darkkhaki", ToRGBA(189, 183, 107) },
            { "darkmagenta", ToRGBA(139, 0, 139) },
            { "darkolivegreen", ToRGBA(85, 107, 47) },
            { "darkorange", ToRGBA(255, 140, 0) },
            { "darkorchid", ToRGBA(153, 50, 204) },
            { "darkred", ToRGBA(139, 0, 0) },
            { "darksalmon", ToRGBA(233, 150, 122) },
            { "darkseagreen", ToRGBA(143, 188, 143) },
            { "darkslateblue", ToRGBA(72, 61, 139) },
            { "darkslategray", ToRGBA(47, 79, 79) },
            { "darkslategray", ToRGBA(47, 79, 79) },
            { "darkturquoise", ToRGBA(0, 206, 209) },
            { "darkviolet", ToRGBA(148, 0, 211) },
            { "deeppink", ToRGBA(255, 20, 147) },
            { "deepskyblue", ToRGBA(0, 191, 255) },
            { "dimgray", ToRGBA(105, 105, 105) },
            { "dimgrey", ToRGBA(105, 105, 105) },
            { "dodgerblue", ToRGBA(30, 144, 255) },
            { "firebrick", ToRGBA(178, 34, 34) },
            { "floralwhite", ToRGBA(255, 250, 240) },
            { "forestgreen", ToRGBA(34, 139, 34) },
            { "gainsboro", ToRGBA(220, 220, 220) },
            { "ghoshtwhite", ToRGBA(248, 248, 255) },
            { "gold", ToRGBA(255, 215, 0) },
            { "goldenrod", ToRGBA(218, 165, 32) },
            { "greenyellow", ToRGBA(173, 255, 47) },
            { "honeydew", ToRGBA(240, 255, 240) },
            { "hotpink", ToRGBA(255, 105, 180) },
            { "indianred", ToRGBA(205, 92, 92) },
            { "indigo", ToRGBA(75, 0, 130) },
            { "ivory", ToRGBA(255, 255, 240) },
            { "khaki", ToRGBA(240, 230, 140) },
            { "lavender", ToRGBA(230, 230, 250) },
            { "lavenderblush", ToRGBA(255, 240, 245) },
            { "lawngreen", ToRGBA(124, 252, 0) },
            { "lemonchiffon", ToRGBA(255, 250, 205) },
            { "lightblue", ToRGBA(173, 216, 230) },
            { "lightcoral", ToRGBA(240, 128, 128) },
            { "lightcyan", ToRGBA(224, 255, 255) },
            { "lightgoldenrodyellow", ToRGBA(250, 250, 210) },
            { "lightgray", ToRGBA(211, 211, 211) },
            { "lightgreen", ToRGBA(144, 238, 144) },
            { "lightgrey", ToRGBA(211, 211, 211) },
            { "lightpink", ToRGBA(255, 182, 193) },
            { "lightsalmon", ToRGBA(255, 160, 122) },
            { "lightseagreen", ToRGBA(32, 178, 170) },
            { "lightskyblue", ToRGBA(135, 206, 250) },
            { "lightslategray", ToRGBA(119, 136, 153) },
            { "lightslategrey", ToRGBA(119, 136, 153) },
            { "lightsteelblue", ToRGBA(176, 196, 222) },
            { "lightyellow", ToRGBA(255, 255, 224) },
            { "lilac", ToRGBA(200, 162, 200) },
            { "limegreen", ToRGBA(50, 255, 50) },
            { "linen", ToRGBA(250, 240, 230) },
            { "mediumaquamarine", ToRGBA(102, 205, 170) },
            { "mediumblue", ToRGBA(0, 0, 205) },
            { "mediumorchid", ToRGBA(186, 85, 211) },
            { "mediumpurple", ToRGBA(147, 112, 219) },
            { "mediumseagreen", ToRGBA(60, 179, 113) },
            { "mediumslateblue", ToRGBA(123, 104, 238) },
            { "mediumspringgreen", ToRGBA(0, 250, 154) },
            { "mediumturquoise", ToRGBA(72, 209, 204) },
            { "mediumvioletred", ToRGBA(199, 21, 133) },
            { "midnightblue", ToRGBA(25, 25, 112) },
            { "mintcream", ToRGBA(245, 255, 250) },
            { "mistyrose", ToRGBA(255, 228, 225) },
            { "moccasin", ToRGBA(255, 228, 181) },
            { "navajowhite", ToRGBA(255, 222, 173) },
            { "oldlace", ToRGBA(253, 245, 230) },
            { "olivedrab", ToRGBA(107, 142, 35) },
            { "orange", ToRGBA(255, 165, 0) },
            { "orangered", ToRGBA(255, 69, 0) },
            { "orchid", ToRGBA(218, 112, 214) },
            { "palegoldenrod", ToRGBA(238, 232, 170) },
            { "palegreen", ToRGBA(152, 251, 152) },
            { "paleturquoise", ToRGBA(175, 238, 238) },
            { "palevioletred", ToRGBA(219, 112, 147) },
            { "papayawhip", ToRGBA(255, 239, 213) },
            { "peachpuff", ToRGBA(255, 218, 185) },
            { "peru", ToRGBA(205, 133, 63) },
            { "pink", ToRGBA(255, 192, 203) },
            { "plum", ToRGBA(221, 160, 221) },
            { "powderblue", ToRGBA(176, 224, 230) },
            { "rosybrown", ToRGBA(188, 143, 143) },
            { "royalblue", ToRGBA(65, 105, 225) },
            { "saddlebrown", ToRGBA(139, 69, 19) },
            { "salmon", ToRGBA(250, 128, 114) },
            { "sandybrown", ToRGBA(244, 164, 96) },
            { "seagreen", ToRGBA(46, 139, 87) },
            { "seashell", ToRGBA(255, 245, 238) },
            { "sienna", ToRGBA(160, 82, 45) },
            { "skyblue", ToRGBA(135, 206, 235) },
            { "slateblue", ToRGBA(106, 90, 205) },
            { "slategray", ToRGBA(112, 128, 144) },
            { "slategrey", ToRGBA(112, 128, 144) },
            { "snow", ToRGBA(255, 250, 250) },
            { "springgreen", ToRGBA(0, 255, 127) },
            { "steelblue", ToRGBA(70, 130, 180) },
            { "tan", ToRGBA(210, 180, 140) },
            { "thistle", ToRGBA(216, 191, 216) },
            { "tomato", ToRGBA(255, 99, 71) },
            { "violet", ToRGBA(238, 130, 238) },
            { "wheat", ToRGBA(245, 222, 179) },
            { "whitesmoke", ToRGBA(245, 245, 245) },
            { "yellowgreen", ToRGBA(154, 205, 50) }
        };

        auto it = Colors.find(name);
        return it != Colors.end() ? it->second : uint32_t{ 0 };
    }

    Border ExtractBorder(std::string_view input, float ems, float percent,
        uint32_t(*NamedColor)(const char*, void*), void* userData)
    {
        Border result;
        auto idx = WholeWord(input);

        if (AreSame(input.substr(0, idx), "none")) return result;
        result.thickness = ExtractFloatWithUnit(input.substr(0, idx), 1.f, ems, percent, 1.f);
        idx = SkipSpace(input, idx);

        auto idx2 = WholeWord(input, idx + 1);
        auto type = input.substr(idx + 1, idx2);
        if (AreSame(type, "solid")) result.lineType = LineType::Solid;
        else if (AreSame(type, "dashed")) result.lineType = LineType::Dashed;
        else if (AreSame(type, "dotted")) result.lineType = LineType::Dotted;

        idx2 = SkipSpace(input, idx2);
        auto idx3 = WholeWord(input, idx2 + 1);
        auto color = input.substr(idx2, idx3);
        result.color = ExtractColor(color, NamedColor, userData);

        return result;
    }

    bool IsColor(std::string_view input, int from)
    {
        return input[from] != '-' && !std::isdigit(input[from]);
    }

    BoxShadow ExtractBoxShadow(std::string_view input, float ems, float percent,
        uint32_t(*NamedColor)(const char*, void*), void* userData)
    {
        BoxShadow result;
        auto idx = WholeWord(input);

        if (AreSame(input.substr(0, idx), "none")) return result;
        result.offset.x = ExtractFloatWithUnit(input.substr(0, idx), 0.f, ems, percent, 1.f);
        idx = SkipSpace(input, idx);

        auto prev = idx;
        idx = WholeWord(input, idx);

        if (!IsColor(input, prev))
        {
            result.offset.y = ExtractFloatWithUnit(input.substr(prev, (idx - prev)), 0.f, ems, percent, 1.f);
            idx = SkipSpace(input, idx);

            prev = idx;
            idx = WholeWord(input, idx);

            if (!IsColor(input, prev))
            {
                result.blur = ExtractFloatWithUnit(input.substr(prev, (idx - prev)), 0.f, ems, percent, 1.f);
                idx = SkipSpace(input, idx);

                prev = idx;
                idx = WholeWord(input, idx);

                if (!IsColor(input, prev))
                {
                    result.spread = ExtractFloatWithUnit(input.substr(prev, (idx - prev)), 0.f, ems, percent, 1.f);
                    idx = SkipSpace(input, idx);

                    prev = idx;
                    idx = WholeWord(input, idx);
                    result.color = ExtractColor(input.substr(prev, (idx - prev)), NamedColor, userData);
                }
                else
                    result.color = ExtractColor(input.substr(prev, (idx - prev)), NamedColor, userData);
            }
            else
                result.color = ExtractColor(input.substr(prev, (idx - prev)), NamedColor, userData);
        }
        else
            result.color = ExtractColor(input.substr(prev, (idx - prev)), NamedColor, userData);
        return result;
    }

    std::pair<std::string_view, bool> ExtractTag(const char* text, int end, char TagEnd,
        int& idx, bool& tagStart)
    {
        std::pair<std::string_view, bool> result;
        result.second = true;

        if (text[idx] == '/')
        {
            tagStart = false;
            idx++;
        }
        else if (!std::isalnum(text[idx]))
        {
            result.second = false;
            return result;
        }

        auto begin = idx;
        while ((idx < end) && !std::isspace(text[idx]) && (text[idx] != TagEnd)) idx++;

        if (idx - begin == 0)
        {
            result.second = false;
            return result;
        }

        result.first = std::string_view{ text + begin, (std::size_t)(idx - begin) };
        if (result.first.back() == '/') result.first = result.first.substr(0, result.first.size() - 1u);

        if (!tagStart)
        {
            if (text[idx] == TagEnd) idx++;

            if (result.first.empty())
            {
                result.second = false;
                return result;
            }
        }

        idx = SkipSpace(text, idx, end);
        return result;
    }

    [[nodiscard]] std::optional<std::string_view> GetQuotedString(const char* text, int& idx, int end)
    {
        auto insideQuotedString = false;
        auto begin = idx;

        if ((idx < end) && text[idx] == '\'')
        {
            idx++;

            while (idx < end)
            {
                if (text[idx] == '\\' && ((idx + 1 < end) && text[idx + 1] == '\''))
                {
                    insideQuotedString = !insideQuotedString;
                    idx++;
                }
                else if (!insideQuotedString && text[idx] == '\'') break;
                idx++;
            }
        }
        else if ((idx < end) && text[idx] == '"')
        {
            idx++;

            while (idx < end)
            {
                if (text[idx] == '\\' && ((idx + 1 < end) && text[idx + 1] == '"'))
                {
                    insideQuotedString = !insideQuotedString;
                    idx++;
                }
                else if (!insideQuotedString && text[idx] == '"') break;
                idx++;
            }
        }

        if ((idx < end) && (text[idx] == '"' || text[idx] == '\''))
        {
            std::string_view res{ text + begin + 1, (std::size_t)(idx - begin - 1) };
            idx++;
            return res;
        }

        return std::nullopt;
    }

    bool FourSidedBorder::isRounded() const
    {
        return cornerRadius[TopLeftCorner] > 0.f ||
            cornerRadius[TopRightCorner] > 0.f ||
            cornerRadius[BottomRightCorner] > 0.f ||
            cornerRadius[BottomLeftCorner] > 0.f;
    }

    bool FourSidedBorder::exists() const
    {
        return top.thickness > 0.f || bottom.thickness > 0.f ||
            left.thickness > 0.f || right.thickness > 0.f;
    }

    FourSidedBorder& FourSidedBorder::setColor(uint32_t color)
    {
        left.color = right.color = top.color = bottom.color = color;
        return *this;
    }

    FourSidedBorder& FourSidedBorder::setThickness(float thickness)
    {
        left.thickness = right.thickness = top.thickness = bottom.thickness = thickness;
        return *this;
    }

    FourSidedBorder& FourSidedBorder::setRadius(float radius)
    {
        cornerRadius[0] = cornerRadius[1] = cornerRadius[2] = cornerRadius[3] = radius;
        return *this;
    }

    static int PopulateSegmentStyle(StyleDescriptor& style, CommonWidgetStyleDescriptor& specific,
        std::string_view stylePropName, std::string_view stylePropVal, UIConfig& Config)
    {
        int prop = NoStyleChange;

        if (AreSame(stylePropName, "font-size"))
        {
            if (AreSame(stylePropVal, "xx-small")) style.font.size = Config.defaultFontSz * 0.6f * Config.fontScaling;
            else if (AreSame(stylePropVal, "x-small")) style.font.size = Config.defaultFontSz * 0.75f * Config.fontScaling;
            else if (AreSame(stylePropVal, "small")) style.font.size = Config.defaultFontSz * 0.89f * Config.fontScaling;
            else if (AreSame(stylePropVal, "medium")) style.font.size = Config.defaultFontSz * Config.fontScaling;
            else if (AreSame(stylePropVal, "large")) style.font.size = Config.defaultFontSz * 1.2f * Config.fontScaling;
            else if (AreSame(stylePropVal, "x-large")) style.font.size = Config.defaultFontSz * 1.5f * Config.fontScaling;
            else if (AreSame(stylePropVal, "xx-large")) style.font.size = Config.defaultFontSz * 2.f * Config.fontScaling;
            else if (AreSame(stylePropVal, "xxx-large")) style.font.size = Config.defaultFontSz * 3.f * Config.fontScaling;
            else
                style.font.size = ExtractFloatWithUnit(stylePropVal, Config.defaultFontSz * Config.fontScaling,
                    Config.defaultFontSz * Config.fontScaling, 1.f, Config.fontScaling);
            prop = StyleFontSize;
        }
        else if (AreSame(stylePropName, "font-weight"))
        {
            auto idx = SkipDigits(stylePropVal);

            if (idx == 0)
            {
                if (AreSame(stylePropVal, "bold")) style.font.flags |= FontStyleBold;
                else if (AreSame(stylePropVal, "light")) style.font.flags |= FontStyleLight;
                else LOGERROR("Invalid font-weight property value... [%.*s]\n",
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
                style.gradient = ExtractLinearGradient(stylePropVal, GetColor, Config.userData);
            else style.bgcolor = ExtractColor(stylePropVal, GetColor, Config.userData);
            prop = StyleBackground;
        }
        else if (AreSame(stylePropName, "color"))
        {
            style.fgcolor = ExtractColor(stylePropVal, GetColor, Config.userData);
            prop = StyleFgColor;
        }
        else if (AreSame(stylePropName, "width"))
        {
            style.dimension.x = ExtractFloatWithUnit(stylePropVal, 0, Config.defaultFontSz * Config.fontScaling, 1.f, Config.scaling);
            prop = StyleWidth;
        }
        else if (AreSame(stylePropName, "height"))
        {
            style.dimension.y = ExtractFloatWithUnit(stylePropVal, 0, Config.defaultFontSz * Config.fontScaling, 1.f, Config.scaling);
            prop = StyleHeight;
        }
        else if (AreSame(stylePropName, "min-width"))
        {
            style.mindim.x = ExtractFloatWithUnit(stylePropVal, 0, Config.defaultFontSz * Config.fontScaling, 1.f, Config.scaling);
            prop = StyleWidth;
        }
        else if (AreSame(stylePropName, "min-height"))
        {
            style.mindim.y = ExtractFloatWithUnit(stylePropVal, 0, Config.defaultFontSz * Config.fontScaling, 1.f, Config.scaling);
            prop = StyleHeight;
        }
        else if (AreSame(stylePropName, "max-width"))
        {
            style.maxdim.x = ExtractFloatWithUnit(stylePropVal, 0, Config.defaultFontSz * Config.fontScaling, 1.f, Config.scaling);
            prop = StyleWidth;
        }
        else if (AreSame(stylePropName, "max-height"))
        {
            style.maxdim.y = ExtractFloatWithUnit(stylePropVal, 0, Config.defaultFontSz * Config.fontScaling, 1.f, Config.scaling);
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
            style.padding = ExtractWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling, 1.f, Config.scaling);
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "padding-top"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling, 1.f, Config.scaling);
            style.padding.top = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "padding-bottom"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling, 1.f, Config.scaling);
            style.padding.bottom = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "padding-left"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling, 1.f, Config.scaling);
            style.padding.left = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "padding-right"))
        {
            auto val = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling, 1.f, Config.scaling);
            style.padding.right = val;
            prop = StylePadding;
        }
        else if (AreSame(stylePropName, "text-overflow"))
        {
            if (AreSame(stylePropVal, "ellipsis"))
            {
                style.font.flags |= FontStyleOverflowEllipsis;
                prop = StyleTextOverflow;
            }
        }
        else if (AreSame(stylePropName, "border"))
        {
            style.border.top = style.border.bottom = style.border.left = style.border.right = ExtractBorder(stylePropVal,
                Config.defaultFontSz * Config.fontScaling, 1.f, GetColor, Config.userData);
            style.border.isUniform = true;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-top"))
        {
            style.border.top = ExtractBorder(stylePropVal, Config.defaultFontSz * Config.fontScaling,
                1.f, GetColor, Config.userData);
            style.border.isUniform = false;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-left"))
        {
            style.border.left = ExtractBorder(stylePropVal, Config.defaultFontSz * Config.fontScaling,
                1.f, GetColor, Config.userData);
            style.border.isUniform = false;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-right"))
        {
            style.border.right = ExtractBorder(stylePropVal, Config.defaultFontSz * Config.fontScaling,
                1.f, GetColor, Config.userData);
            style.border.isUniform = false;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-bottom"))
        {
            style.border.bottom = ExtractBorder(stylePropVal, Config.defaultFontSz * Config.fontScaling,
                1.f, GetColor, Config.userData);
            prop = StyleBorder;
            style.border.isUniform = false;
        }
        else if (AreSame(stylePropName, "border-radius"))
        {
            auto radius = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling,
                1.f, 1.f);
            if (stylePropVal.back() == '%') style.relativeProps |= (RSP_BorderTopLeftRadius | RSP_BorderTopRightRadius | RSP_BorderBottomLeftRadius |
                RSP_BorderBottomRightRadius);
            style.border.setRadius(radius);
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-width"))
        {
            auto width = ExtractWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling, 1.f, 1.f);
            style.border.top.thickness = width.top;
            style.border.left.thickness = width.left;
            style.border.bottom.thickness = width.bottom;
            style.border.right.thickness = width.right;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-color"))
        {
            auto color = ExtractColor(stylePropVal, GetColor, Config.userData);
            style.border.setColor(color);
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-top-left-radius"))
        {
            style.border.cornerRadius[TopLeftCorner] = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling,
                1.f, 1.f);
            if (stylePropVal.back() == '%') style.relativeProps |= RSP_BorderTopLeftRadius;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-top-right-radius"))
        {
            style.border.cornerRadius[TopRightCorner] = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling,
                1.f, 1.f);
            if (stylePropVal.back() == '%') style.relativeProps |= RSP_BorderTopRightRadius;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-bottom-right-radius"))
        {
            style.border.cornerRadius[BottomRightCorner] = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling,
                1.f, 1.f);
            if (stylePropVal.back() == '%') style.relativeProps |= RSP_BorderBottomRightRadius;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "border-bottom-left-radius"))
        {
            style.border.cornerRadius[BottomLeftCorner] = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling,
                1.f, 1.f);
            if (stylePropVal.back() == '%') style.relativeProps |= RSP_BorderBottomLeftRadius;
            prop = StyleBorder;
        }
        else if (AreSame(stylePropName, "margin"))
        {
            style.margin = ExtractWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling, 1.f, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "margin-top"))
        {
            style.margin.top = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling, 1.f, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "margin-left"))
        {
            style.margin.left = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling, 1.f, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "margin-right"))
        {
            style.margin.right = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling, 1.f, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "margin-bottom"))
        {
            style.margin.bottom = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling, 1.f, 1.f);
            prop = StyleMargin;
        }
        else if (AreSame(stylePropName, "font-style"))
        {
            if (AreSame(stylePropVal, "normal")) style.font.flags |= FontStyleNormal;
            else if (AreSame(stylePropVal, "italic") || AreSame(stylePropVal, "oblique"))
                style.font.flags |= FontStyleItalics;
            else LOGERROR("Invalid font-style property value [%.*s]\n",
                (int)stylePropVal.size(), stylePropVal.data());
            prop = StyleFontStyle;
        }
        else if (AreSame(stylePropName, "box-shadow"))
        {
            style.shadow = ExtractBoxShadow(stylePropVal, Config.defaultFontSz, 1.f, GetColor, Config.userData);
            prop = StyleBoxShadow;
        }
        else if (AreSame(stylePropName, "thumb-color"))
        {
            if (StartsWith(stylePropVal, "linear-gradient"))
            {
                style.gradient = ExtractLinearGradient(stylePropVal, GetColor, Config.userData);
            }
            else specific.toggle.thumbColor = ExtractColor(stylePropVal, GetColor, Config.userData);
            prop = StyleThumbColor;
        }
        else if (AreSame(stylePropName, "track-color"))
        {
            if (StartsWith(stylePropVal, "linear-gradient"))
            {
                style.gradient = ExtractLinearGradient(stylePropVal, GetColor, Config.userData);
            }
            else specific.toggle.trackColor = ExtractColor(stylePropVal, GetColor, Config.userData);
            prop = StyleTrackColor;
        }
        else if (AreSame(stylePropName, "track-outline"))
        {
            auto brd = ExtractBorder(stylePropVal, Config.defaultFontSz, 1.f, GetColor, Config.userData);
            specific.toggle.trackBorderColor = brd.color;
            specific.toggle.trackBorderThickness = brd.thickness;
            prop = StyleTrackOutlineColor;
        }
        else if (AreSame(stylePropName, "thumb-offset"))
        {
            specific.toggle.thumbOffset = ExtractFloatWithUnit(stylePropVal, 0.f, Config.defaultFontSz * Config.fontScaling, 1.f, 1.f);
            prop = StyleThumbOffset;
        }
        else
        {
            LOGERROR("Invalid style property... [%.*s]\n", (int)stylePropName.size(), stylePropName.data());
        }

        return prop;
    }

#pragma region Style stack

    void CopyStyle(const StyleDescriptor& src, StyleDescriptor& dest)
    {
        if (&src == &dest || dest.specified & StyleUpdatedFromBase) return;

        for (int64_t idx = 0; idx <= StyleTotal; ++idx)
        {
            auto prop = (StyleProperty)((1ll << idx));
            if ((dest.specified & prop) == 0)
            {
                switch (prop)
                {
                case glimmer::StyleBackground:
                    dest.bgcolor = src.bgcolor;
                    dest.gradient = src.gradient;
                    break;
                case glimmer::StyleFgColor:
                    dest.fgcolor = src.fgcolor;
                    break;
                case glimmer::StyleFontSize:
                    dest.font.size = src.font.size;
                    break;
                case glimmer::StyleFontFamily:
                    dest.font.family = src.font.family;
					break;
                case glimmer::StyleFontWeight:
                    dest.font.flags = src.font.flags;
					break;
                case glimmer::StyleFontStyle:
                    dest.font.flags = src.font.flags;
                    break;
                case glimmer::StyleHeight:
                    dest.dimension.y = src.dimension.y;
                    break;
                case glimmer::StyleWidth:
                    dest.dimension.x = src.dimension.x;
                    break;
                case glimmer::StyleHAlignment:
                    (src.alignment & TextAlignLeft) ? dest.alignment |= TextAlignLeft : dest.alignment &= ~TextAlignLeft;
                    (src.alignment & TextAlignRight) ? dest.alignment |= TextAlignRight : dest.alignment &= ~TextAlignRight;
                    (src.alignment & TextAlignHCenter) ? dest.alignment |= TextAlignHCenter : dest.alignment &= ~TextAlignHCenter;
                    break;
                case glimmer::StyleVAlignment:
                    (src.alignment & TextAlignTop) ? dest.alignment |= TextAlignTop : dest.alignment &= ~TextAlignTop;
                    (src.alignment & TextAlignBottom) ? dest.alignment |= TextAlignBottom : dest.alignment &= ~TextAlignBottom;
                    (src.alignment & TextAlignVCenter) ? dest.alignment |= TextAlignVCenter : dest.alignment &= ~TextAlignVCenter;
                    break;
                case glimmer::StylePadding:
                    dest.padding = src.padding;
                    break;
                case glimmer::StyleMargin:
                    dest.margin = src.margin;
                    break;
                case glimmer::StyleBorder:
                    dest.border = src.border;
                    break;
                case glimmer::StyleOverflow:
                    break;
                case glimmer::StyleBorderRadius:
                {
                    const auto& srcborder = src.border;
                    auto& dstborder = dest.border;
                    dstborder.cornerRadius[0] = srcborder.cornerRadius[0];
                    dstborder.cornerRadius[1] = srcborder.cornerRadius[1];
                    dstborder.cornerRadius[2] = srcborder.cornerRadius[2];
                    dstborder.cornerRadius[3] = srcborder.cornerRadius[3];
                    break;
                }
                case glimmer::StyleCellSpacing:
                    break;
                case glimmer::StyleTextWrap:
                    break;
                case glimmer::StyleBoxShadow:
                    dest.shadow = src.shadow;
                    break;
                case glimmer::StyleTextOverflow:
                    break;
                case glimmer::StyleMinWidth:
                    dest.mindim.x = src.mindim.x;
                    break;
                case glimmer::StyleMaxWidth:
                    dest.maxdim.x = src.maxdim.x;
                    break;
                case glimmer::StyleMinHeight:
                    dest.mindim.y = src.mindim.y;
                    break;
                case glimmer::StyleMaxHeight:
                    dest.maxdim.y = src.maxdim.y;
                    break;
                default:
                    break;
                }
            }
        }
    }

    static void ResetNonInheritableProps(StyleDescriptor& style)
    {
        for (int64_t idx = 0; idx <= StyleTotal; ++idx)
        {
            auto prop = (StyleProperty)((1ll << idx));
			if (Config.implicitInheritedProps & prop) continue;

            switch (prop)
            {
            case glimmer::StyleBackground:
                style.bgcolor = IM_COL32_BLACK_TRANS;
                style.gradient = ColorGradient{};
                break;
            case glimmer::StyleFgColor:
                style.fgcolor = ToRGBA(0, 0, 0);
                break;
            case glimmer::StyleFontSize:
                style.font.size = Config.defaultFontSz * Config.fontScaling;
                break;
            case glimmer::StyleFontFamily:
                style.font.family = GLIMMER_DEFAULT_FONTFAMILY;
                break;
            case glimmer::StyleFontWeight:
                style.font.flags &= ~(FontStyleBold | FontStyleLight);
                style.font.flags |= FontStyleNormal;
                break;
            case glimmer::StyleFontStyle:
                style.font.flags &= ~FontStyleItalics;
                break;
            case glimmer::StyleHeight:
                style.dimension.y = -1.f;
                break;
            case glimmer::StyleWidth:
                style.dimension.x = -1.f;
                break;
            case glimmer::StyleHAlignment:
                style.alignment &= ~(TextAlignRight | TextAlignHCenter);
                style.alignment |= TextAlignLeft;
                break;
            case glimmer::StyleVAlignment:
                style.alignment &= ~(TextAlignBottom | TextAlignVCenter);
                style.alignment |= TextAlignTop;
                break;
            case glimmer::StylePadding:
                style.padding = FourSidedMeasure{};
                break;
            case glimmer::StyleMargin:
                style.margin = FourSidedMeasure{};
                break;
            case glimmer::StyleBorder:
				style.border = FourSidedBorder{};
                break;
            case glimmer::StyleBorderRadius:
            {
				style.border.cornerRadius[0] = 0.f;
				style.border.cornerRadius[1] = 0.f;
				style.border.cornerRadius[2] = 0.f;
				style.border.cornerRadius[3] = 0.f;
                break;
            }
            case glimmer::StyleTextWrap:
				style.font.flags &= ~StyleTextWrap;
                break;
            case glimmer::StyleBoxShadow:
                style.shadow = BoxShadow{};
                break;
            case glimmer::StyleTextOverflow:
                break;
            case glimmer::StyleMinWidth:
                style.mindim.x = 0.f;
                break;
            case glimmer::StyleMaxWidth:
                style.maxdim.x = FLT_MAX;
                break;
            case glimmer::StyleMinHeight:
                style.mindim.y = 0.f;
                break;
            case glimmer::StyleMaxHeight:
                style.maxdim.y = FLT_MAX;
                break;
            default:
                break;
            }

            style.specified &= ~prop;
        }
    }

    template <typename StackT>
    static int32_t PushStyle(std::string_view* css, StackT* stack)
    {
        int32_t res = 0;

        // When pushing style, the default style behaves slightly differently then rest
        // The default style inherits from parent in the stack if present, parse the CSS and gets pushed
        // The other styles, inherit from default and then parse the CSS and get pushed
        for (auto style = 0; style < WSI_Total; ++style)
        {
            if (!css[style].empty())
            {
                if (style == WSI_Default)
                {
                    auto parent = stack[WSI_Default].empty() ? 
                        GetContext().StyleStack[WSI_Default].top() : stack[WSI_Default].top();
                    auto& pushed = stack[style].push();
                    pushed = parent;
                    ResetNonInheritableProps(pushed);
                    pushed.From(css[style]);
                }
                else
                {
                    stack[style].push().From(css[style]);
                }

                res |= (1 << style);
            }
        }

        return res;
    }

    template <typename StackT>
    static void PushStyle(WidgetState state, std::string_view css, StackT* stack)
    {
        auto idx = log2((unsigned)state);

        if (idx == WSI_Default)
        {
            if (!stack[idx].empty())
            {
                auto parent = stack[idx].top();
                auto& style = stack[idx].push();
                style = parent;
                ResetNonInheritableProps(style);
                style.From(css);
            }
            else
                stack[idx].push().From(css);
        }
        else
        {
            stack[idx].push().From(css);
        }
    }

    void PushStyle(std::string_view defcss, std::string_view hovercss, std::string_view pressedcss,
        std::string_view focusedcss, std::string_view checkedcss, std::string_view disblcss)
    {
        std::string_view css[WSI_Total] = { defcss, focusedcss, hovercss, pressedcss, checkedcss, "", "", "", disblcss };
        auto& context = GetContext();
        
        if (!context.layoutStack.empty())
        {
            auto& layout = context.layoutStack[0];
            auto state = PushStyle(css, context.layoutStyles);

            // Enqueue multiple layout ops, to capture indexes of each widget state specific style stack
            for (auto idx = 0; idx < WSI_Total; ++idx)
            {
                if (state & (1 << idx))
                {
                    auto sz = (int64_t)(context.layoutStyles[idx].size() - 1);
                    context.RecordForReplay((sz << 32) | (int64_t)idx, LayoutOps::PushStyle);
                }
            }
        }
       
        PushStyle(css, context.StyleStack);
    }

    void PushStyleFmt(int32_t state, std::string_view fmt, ...)
    {
        static char buffer[GLIMMER_STYLE_BUFSZ] = { 0 };

        std::memset(buffer, 0, GLIMMER_STYLE_BUFSZ);
        va_list args;
        va_start(args, fmt);
        auto sz = std::vsnprintf(buffer, GLIMMER_STYLE_BUFSZ - 1, fmt.data(), args);
        buffer[std::min(sz, GLIMMER_STYLE_BUFSZ - 1)] = 0;
        va_end(args);

        PushStyle(state, buffer);
    }

    void PushStyleFmt(std::string_view fmt, ...)
    {
        static char buffer[GLIMMER_STYLE_BUFSZ] = { 0 };

        std::memset(buffer, 0, GLIMMER_STYLE_BUFSZ);
        va_list args;
        va_start(args, fmt);
        auto sz = std::vsnprintf(buffer, GLIMMER_STYLE_BUFSZ - 1, fmt.data(), args);
        buffer[std::min(sz, GLIMMER_STYLE_BUFSZ - 1)] = 0;
        va_end(args);

        PushStyle(buffer);
    }

    void PushStyle(int32_t state, std::string_view css)
    {
        auto& context = GetContext();

        for (auto style = 0; style < WSI_Total; ++style)
        {
            if ((1 << style) & state)
            {
                if (!context.layoutStack.empty())
                {
                    PushStyle((WidgetState)(1 << style), css, context.layoutStyles);

                    if (!css.empty())
                    {
                        auto idx = style;
                        auto sz = (int64_t)(context.layoutStyles[idx].size() - 1);
                        context.RecordForReplay((sz << 32) | (int64_t)idx, LayoutOps::PushStyle);
                    }
                }

                PushStyle((WidgetState)(1 << style), css, context.StyleStack);
            }
        }
    }

    void SetStyle(std::string_view id, const std::initializer_list<std::pair<int32_t, std::string_view>>& css)
    {
        auto& dest = StyleSheet[id];

        for (const auto& [state, style] : css)
        {
            if (!style.empty())
            {
                for (auto idx = 0; idx < WSI_Total; ++idx)
                {
                    auto ws = 1 << idx;
                    if (ws & state)
                        dest[idx].From(style);
                }
            }
        }
    }

    void SetStyle(std::string_view id, int32_t state, std::string_view fmt, ...)
    {
        static char buffer[GLIMMER_STYLE_BUFSZ] = { 0 };

        std::memset(buffer, 0, GLIMMER_STYLE_BUFSZ);
        va_list args;
        va_start(args, fmt);
        auto sz = std::vsnprintf(buffer, GLIMMER_STYLE_BUFSZ - 1, fmt.data(), args);
        buffer[std::min(sz, GLIMMER_STYLE_BUFSZ - 1)] = 0;
        va_end(args);

        auto& dest = StyleSheet[id];
        for (auto idx = 0; idx < WSI_Total; ++idx)
        {
            auto ws = 1 << idx;
            if (ws & state)
                dest[idx].From(buffer);
        }
    }

    StyleDescriptor& GetStyle(std::string_view id, WidgetStateIndex index)
    {
        static StyleDescriptor invalid{};
        auto it = StyleSheet.find(id);
        return it == StyleSheet.end() ? invalid : it->second[index];
    }

    StyleDescriptor& GetWidgetStyle(WidgetType type, WidgetStateIndex index)
    {
        auto name = Config.widgetNames[type];
        return GetStyle(name, index);
    }

    void PopStyle(int depth, int32_t state)
    {
        auto& context = GetContext();

        if (!context.layoutStack.empty())
        {
            auto dd = (int64_t)depth;
            context.RecordForReplay((dd << 32) | (int64_t)state, LayoutOps::PopStyle);
        }
        
        for (auto style = 0; style < WSI_Total; ++style)
        {
            if ((1 << style) & state)
            {
                auto popsz = std::min(context.StyleStack[style].size() - 1, depth);
                context.StyleStack[style].pop(popsz, true);
            }
        }
    }

#ifndef GLIMMER_DISABLE_RICHTEXT
    void PushTextType(TextType type)
    {
        auto& context = GetContext();

        if (!context.layoutStack.empty())
        {
            context.RecordForReplay((int64_t)type, LayoutOps::PushTextType);
        }

        for (auto style = 0; style < WSI_Total; ++style)
        {
            auto desc = context.StyleStack[style].top();
			desc.font.flags = type == TextType::RichText ? (desc.font.flags | TextIsRichText) : 
                (desc.font.flags & ~TextIsRichText);
			context.StyleStack[style].push() = desc;
        }
    }

    void PopTextType()
    {
		PopStyle(1, 0b111111111);
    }
#endif

    void _IgnoreStyleStackInternal(int32_t wtypes)
    {
        if (!GetContext().layoutStack.empty())
        {
            auto& op = GetContext().replayContent.emplace_back();
            op.first = wtypes;
            op.second = LayoutOps::IgnoreStyleStack;
        }

        WidgetContextData::IgnoreStyleStack(wtypes);
    }

    void RestoreStyleStack()
    {
        if (!GetContext().layoutStack.empty())
        {
            auto& op = GetContext().replayContent.emplace_back();
            op.second = LayoutOps::RestoreStyleStack;
        }

        WidgetContextData::RestoreStyleStack();
    }

    // TODO: Fix layout generation from stylesheet
    /*std::pair<Sizing, bool> ParseLayoutStyle(LayoutBuilder& layout, std::string_view css, float pwidth, float pheight)
    {
        auto sidx = 0;
        Sizing sizing;
        auto hasSizing = false;

        while (sidx < (int)css.size())
        {
            sidx = SkipSpace(css, sidx);
            auto stbegin = sidx;
            while ((sidx < (int)css.size()) && (css[sidx] != ':') &&
                !std::isspace(css[sidx])) sidx++;
            auto stylePropName = css.substr(stbegin, sidx - stbegin);

            sidx = SkipSpace(css, sidx);
            if (css[sidx] == ':') sidx++;
            sidx = SkipSpace(css, sidx);

            auto stylePropVal = GetQuotedString(css.data(), sidx, (int)css.size());
            if (!stylePropVal.has_value() || stylePropVal.value().empty())
            {
                stbegin = sidx;
                while ((sidx < (int)css.size()) && css[sidx] != ';') sidx++;
                stylePropVal = css.substr(stbegin, sidx - stbegin);

                if ((sidx < (int)css.size()) && css[sidx] == ';') sidx++;
            }

            if (stylePropVal.has_value())
            {
                if (AreSame(stylePropName, "width"))
                {
                    sizing.horizontal = ExtractFloatWithUnit(stylePropVal.value(), 0.f,
                        Config.defaultFontSz, pwidth, 1.f);
                    sizing.relativeh = stylePropVal.value().back() == '%';
                    hasSizing = true;
                }
                else if (AreSame(stylePropName, "height"))
                {
                    sizing.vertical = ExtractFloatWithUnit(stylePropVal.value(), 0.f,
                        Config.defaultFontSz, pheight, 1.f);
                    sizing.relativev = stylePropVal.value().back() == '%';
                    hasSizing = true;
                }
                else if (AreSame(stylePropName, "spacing-x")) layout.spacing.x =
                    ExtractFloatWithUnit(stylePropVal.value(), 0.f, Config.defaultFontSz, 1.f, 1.f);
                else if (AreSame(stylePropName, "spacing-y")) layout.spacing.x =
                    ExtractFloatWithUnit(stylePropVal.value(), 0.f, Config.defaultFontSz, 1.f, 1.f);
                else if (AreSame(stylePropName, "spacing")) layout.spacing.x = layout.spacing.y =
                    ExtractFloatWithUnit(stylePropVal.value(), 0.f, Config.defaultFontSz, 1.f, 1.f);
                else if (AreSame(stylePropName, "overflow-x")) layout.hofmode = AreSame(stylePropVal.value(), "clip") ?
                    OverflowMode::Clip : AreSame(stylePropVal.value(), "scroll") ? OverflowMode::Scroll : OverflowMode::Wrap;
                else if (AreSame(stylePropName, "overflow-y")) layout.vofmode = AreSame(stylePropVal.value(), "clip") ?
                    OverflowMode::Clip : AreSame(stylePropVal.value(), "scroll") ? OverflowMode::Scroll : OverflowMode::Wrap;
                else if (AreSame(stylePropName, "overflow")) layout.vofmode = layout.hofmode = AreSame(stylePropVal.value(), "clip") ?
                    OverflowMode::Clip : AreSame(stylePropVal.value(), "scroll") ? OverflowMode::Scroll : OverflowMode::Wrap;
                else if (AreSame(stylePropName, "halign") || AreSame(stylePropName, "horizontal-align"))
                    layout.alignment |= AreSame(stylePropVal.value(), "right") ? TextAlignRight :
                    AreSame(stylePropVal.value(), "center") ? TextAlignHCenter : TextAlignLeft;
                else if (AreSame(stylePropName, "valign") || AreSame(stylePropName, "vertical-align"))
                    layout.alignment |= AreSame(stylePropVal.value(), "bottom") ? TextAlignBottom :
                    AreSame(stylePropVal.value(), "center") ? TextAlignVCenter : TextAlignTop;
                else if (AreSame(stylePropName, "align") && AreSame(stylePropVal.value(), "center"))
                    layout.alignment = TextAlignCenter;
                else if (AreSame(stylePropName, "fill")) layout.fill = AreSame(stylePropVal.value(), "all") ?
                    FD_Horizontal | FD_Vertical : AreSame(stylePropVal.value(), "horizontal") ?
                    FD_Horizontal : AreSame(stylePropVal.value(), "vertical") ?
                    FD_Vertical : FD_None;
            }
        }

        return { sizing, hasSizing };
    }*/

    StyleDescriptor::StyleDescriptor()
    {
        font.size = Config.defaultFontSz * Config.fontScaling;
        index.animation = index.custom = 0;
        border.cornerRadius[0] = border.cornerRadius[1] = border.cornerRadius[2] = border.cornerRadius[3] = 0.f;
    }

    StyleDescriptor::StyleDescriptor(std::string_view css)
        : StyleDescriptor{}
    {
        From(css);
    }

    StyleDescriptor& StyleDescriptor::BgColor(int r, int g, int b, int a) { bgcolor = ToRGBA(r, g, b, a); return *this; }
    StyleDescriptor& StyleDescriptor::FgColor(int r, int g, int b, int a) { fgcolor = ToRGBA(r, g, b, a); return *this; }
    StyleDescriptor& StyleDescriptor::Size(float w, float h) { dimension = ImVec2{ w, h }; return *this; }
    StyleDescriptor& StyleDescriptor::Align(int32_t align) { alignment = align; return *this; }
    StyleDescriptor& StyleDescriptor::Padding(float p) { padding.left = padding.top = padding.bottom = padding.right = p; return *this; }
    StyleDescriptor& StyleDescriptor::Margin(float p) { margin.left = margin.top = margin.bottom = margin.right = p; return *this; }

    StyleDescriptor& StyleDescriptor::Border(float thick, std::tuple<int, int, int, int> color)
    {
        border.setThickness(thick); border.setColor(ToRGBA(color));
        return *this;
    }

    StyleDescriptor& StyleDescriptor::Raised(float amount)
    {
        return *this;
    }

    StyleDescriptor& StyleDescriptor::From(std::string_view css, bool checkForDuplicate)
    {
        if (css.empty()) return *this;

        auto sidx = 0;
        int prop = 0;
        CommonWidgetStyleDescriptor desc{};

        while (sidx < (int)css.size())
        {
            sidx = SkipSpace(css, sidx);
            auto stbegin = sidx;
            while ((sidx < (int)css.size()) && (css[sidx] != ':') &&
                !std::isspace(css[sidx])) sidx++;
            auto stylePropName = css.substr(stbegin, sidx - stbegin);

            sidx = SkipSpace(css, sidx);
            if (css[sidx] == ':') sidx++;
            sidx = SkipSpace(css, sidx);

            auto stylePropVal = GetQuotedString(css.data(), sidx, (int)css.size());
            if (!stylePropVal.has_value() || stylePropVal.value().empty())
            {
                stbegin = sidx;
                while ((sidx < (int)css.size()) && css[sidx] != ';') sidx++;
                stylePropVal = css.substr(stbegin, sidx - stbegin);

                if ((sidx < (int)css.size()) && css[sidx] == ';') sidx++;
            }

            if (stylePropVal.has_value())
                prop |= PopulateSegmentStyle(*this, desc, stylePropName, stylePropVal.value(), Config);
        }

        if ((prop & StyleFontFamily) || (prop & StyleFontSize) || (prop & StyleFontWeight))
            font.font = nullptr;

        AddFontPtr(font);
        specified |= prop;
        return *this;
    }

    StyleDescriptor& StyleDescriptor::From(const StyleDescriptor& style, bool overwrite)
    {
        for (auto idx = 0; idx < StyleTotal; ++idx)
        {
            auto styleprop = 1 << idx;
            if ((overwrite || !(styleprop & specified)) && (styleprop & style.specified))
            {
                switch (styleprop)
                {
                case StyleBackground:
                    bgcolor = style.bgcolor;
                    gradient = style.gradient;
                    break;
                case StyleFgColor:
                    fgcolor = style.fgcolor;
                    break;
                case StyleFontSize:
                    font.size = style.font.size;
                    break;
                case StyleFontFamily:
                    font.family = style.font.family;
                    break;
                case StyleFontWeight:
                    font.flags |= style.font.flags & FontStyleBold ? FontStyleBold : FontStyleNormal;
                    break;
                case StyleHeight:
                    dimension.y = style.dimension.y;
                    break;
                case StyleWidth:
                    dimension.x = style.dimension.x;
                    break;
                case StyleHAlignment:
                    alignment |= (style.alignment & TextAlignLeft) ? TextAlignLeft : 0;
                    alignment |= (style.alignment & TextAlignRight) ? TextAlignRight : 0;
                    alignment |= (style.alignment & TextAlignHCenter) ? TextAlignHCenter : 0;
                    break;
                case StyleVAlignment:
                    alignment |= (style.alignment & TextAlignTop) ? TextAlignTop : 0;
                    alignment |= (style.alignment & TextAlignBottom) ? TextAlignBottom : 0;
                    alignment |= (style.alignment & TextAlignVCenter) ? TextAlignVCenter : 0;
                    break;
                case StylePadding:
                    padding = style.padding;
                    break;
                case StyleMargin:
                    margin = style.margin;
                    break;
                case StyleBorder:
                    border = style.border;
                    break;
                case StyleBoxShadow:
                    shadow = style.shadow;
                    break;
                case StyleBorderRadius:
                {
                    const auto& srcborder = style.border;
                    auto& dstborder = border;
                    dstborder.cornerRadius[0] = srcborder.cornerRadius[0];
                    dstborder.cornerRadius[1] = srcborder.cornerRadius[1];
                    dstborder.cornerRadius[2] = srcborder.cornerRadius[2];
                    dstborder.cornerRadius[3] = srcborder.cornerRadius[3];
                }
                break;
                default: break;
                }
				specified |= styleprop;
            }
        }

        return *this;
    }

#pragma endregion

    void (*StyleDescriptor::GlobalThemeProvider)(GlobalWidgetTheme*) = nullptr;
}