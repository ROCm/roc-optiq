#include "im_font_manager.h"

#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
#include "libs/inc/imgui/imgui.h"
#ifdef IMGUI_ENABLE_FREETYPE
#include "libs/inc/imgui/misc/freetype/imgui_freetype.h"
#endif
#endif
#ifndef GLIMMER_DISABLE_BLEND2D_RENDERER
#include "libs/inc/blend2d/blend2d.h"
#endif

#include <string>
#include <cstring>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <filesystem>
#include <fstream>
#include <deque>
#include <cassert>

#include "imrichtext.h"
#include "context.h"

#ifdef _DEBUG
#include <iostream>
#endif

#ifdef _WIN32
#define WINDOWS_DEFAULT_FONT \
    "c:\\Windows\\Fonts\\segoeui.ttf", \
    "c:\\Windows\\Fonts\\segoeuil.ttf",\
    "c:\\Windows\\Fonts\\segoeuib.ttf",\
    "c:\\Windows\\Fonts\\segoeuii.ttf",\
    "c:\\Windows\\Fonts\\segoeuiz.ttf"

#define WINDOWS_DEFAULT_MONOFONT \
    "c:\\Windows\\Fonts\\consola.ttf",\
    "",\
    "c:\\Windows\\Fonts\\consolab.ttf",\
    "c:\\Windows\\Fonts\\consolai.ttf",\
    "c:\\Windows\\Fonts\\consolaz.ttf"

#elif __linux__
#define FEDORA_DEFAULT_FONT \
    "/usr/share/fonts/open-sans/OpenSans-Regular.ttf",\
    "/usr/share/fonts/open-sans/OpenSans-Light.ttf",\
    "/usr/share/fonts/open-sans/OpenSans-Bold.ttf",\
    "/usr/share/fonts/open-sans/OpenSans-Italic.ttf",\
    "/usr/share/fonts/open-sans/OpenSans-BoldItalic.ttf"

#define FEDORA_DEFAULT_MONOFONT \
    "/usr/share/fonts/liberation-mono/LiberationMono-Regular.ttf",\
    "",\
    "/usr/share/fonts/liberation-mono/LiberationMono-Bold.ttf",\
    "/usr/share/fonts/liberation-mono/LiberationMono-Italic.ttf",\
    "/usr/share/fonts/liberation-mono/LiberationMono-BoldItalic.ttf"

#define POPOS_DEFAULT_FONT \
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",\
    "",\
    "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",\
    "/usr/share/fonts/truetype/freefont/FreeSansOblique.ttf",\
    "/usr/share/fonts/truetype/freefont/FreeSansBoldOblique.ttf"

#define POPOS_DEFAULT_MONOFONT \
    "/usr/share/fonts/truetype/freefont/FreeMono.ttf",\
    "",\
    "/usr/share/fonts/truetype/freefont/FreeMonoBold.ttf",\
    "/usr/share/fonts/truetype/freefont/FreeMonoOblique.ttf",\
    "/usr/share/fonts/truetype/freefont/FreeMonoBoldOblique.ttf"

#define MANJARO_DEFAULT_FONT \
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",\
    "/usr/share/fonts/noto/NotoSans-Light.ttf",\
    "/usr/share/fonts/noto/NotoSans-Bold.ttf",\
    "/usr/share/fonts/noto/NotoSans-Italic.ttf",\
    "/usr/share/fonts/noto/NotoSans-BoldItalic.ttf"

#define MANJARO_DEFAULT_MONOFONT \
    "/usr/share/fonts/TTF/Hack-Regular.ttf",\
    "",\
    "/usr/share/fonts/TTF/Hack-Bold.ttf",\
    "/usr/share/fonts/TTF/Hack-Italic.ttf",\
    "/usr/share/fonts/TTF/Hack-BoldItalic.ttf",

#include <sstream>
#include <cstdio>
#include <memory>
#include <array>

#endif

namespace glimmer
{
    struct FontFamily
    {
#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
        std::map<float, ImFont*> FontPtrs[FT_Total];
#endif
#ifndef GLIMMER_DISABLE_BLEND2D_RENDERER
        std::map<float, BLFont> Fonts[FT_Total];
        BLFontFace FontFace[FT_Total];
#endif
        FontCollectionFile Files;
        bool AutoScale = false;
    };

    struct FontMatchInfo
    {
        std::string files[FT_Total];
        std::string family;
        bool serif = false;
    };

    struct FontLookupInfo
    {
        std::deque<FontMatchInfo> info;
        std::unordered_map<std::string_view, int> ProportionalFontFamilies;
        std::unordered_map<std::string_view, int> MonospaceFontFamilies;
        std::unordered_set<void*> MonospaceFonts;
        std::unordered_set<std::string_view> LookupPaths;

        void Register(const std::string& family, const std::string& filepath, FontType ft, bool isMono, bool serif)
        {
            auto& lookup = info.emplace_back();
            lookup.files[ft] = filepath;
            lookup.serif = serif;
            lookup.family = family;
            auto& index = !isMono ? ProportionalFontFamilies[lookup.family] :
                MonospaceFontFamilies[lookup.family];
            index = (int)info.size() - 1;
        }
    };

    static std::unordered_map<std::string_view, FontFamily> FontStore;
    static FontLookupInfo FontLookup;

#ifdef GLIMMER_ENABLE_ICON_FONT
    struct GlyphRangeMap
    {
        ImWchar start, end;
        std::string_view path;
        uint64_t flags = 0;
    };

    static std::map<float, std::vector<GlyphRangeMap>> IconFontAttachRange;

    static void AddIconFont(const std::vector<GlyphRangeMap>& ranges, float size, bool mergeWithPrevious)
    {
        auto isFirst = true;

        for (const auto& range : ranges)
        {
#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
            ImWchar glyphs[3] = { range.start, range.end, 0 };

            ImFontConfig fconfig;
            fconfig.OversampleH = 2;
            fconfig.OversampleV = 1;
            fconfig.GlyphRanges = glyphs;
            fconfig.MergeMode = mergeWithPrevious || !isFirst;
            fconfig.RasterizerMultiply = size <= 16.f ? 2.f : 1.f;
            auto hinting = range.flags & FLT_Hinting;
            auto antialias = range.flags & FLT_Antialias;

#ifdef IMGUI_ENABLE_FREETYPE
            int32_t flags = !hinting ? ImGuiFreeTypeBuilderFlags_NoHinting :
                !antialias ? ImGuiFreeTypeBuilderFlags_MonoHinting : ImGuiFreeTypeBuilderFlags_LightHinting;
            flags = flags | (!antialias ? ImGuiFreeTypeBuilderFlags_Monochrome : 0);
#endif

            ImGuiIO& io = ImGui::GetIO();
            fconfig.FontBuilderFlags = fconfig.FontBuilderFlags | flags;
            auto path = range.path.data();
            auto fontptr = io.Fonts->AddFontFromFileTTF(path, size, &fconfig, glyphs);
            if (!mergeWithPrevious) Config.iconFont = fontptr;
#endif

            isFirst = false;
        }
    }
#endif

#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
    static void LoadFont(ImGuiIO& io, FontFamily& family, FontType ft, float size, ImFontConfig config, int flag, bool isMonospace)
    {
        config.FontBuilderFlags = config.FontBuilderFlags | flag;

        if (ft == FT_Normal)
        {
            auto font = family.Files.Files[FT_Normal].empty() ? nullptr :
                io.Fonts->AddFontFromFileTTF(family.Files.Files[FT_Normal].data(), size, &config);
            assert(font != nullptr);
            family.FontPtrs[FT_Normal][size] = font;
            if (isMonospace) FontLookup.MonospaceFonts.insert(font);
        }
        else
        {
            auto fallback = family.FontPtrs[FT_Normal][size];

#ifdef IMGUI_ENABLE_FREETYPE

            if (family.Files.Files[ft].empty()) {
                family.FontPtrs[ft][size] = io.Fonts->AddFontFromFileTTF(
                    family.Files.Files[FT_Normal].data(), size, &config);
            }
            else family.FontPtrs[ft][size] = io.Fonts->AddFontFromFileTTF(
                family.Files.Files[ft].data(), size, &config);

            if (isMonospace) FontLookup.MonospaceFonts.insert(family.FontPtrs[ft][size]);
#else
            fonts[ft][size] = files.Files[ft].empty() ? fallback :
                io.Fonts->AddFontFromFileTTF(files.Files[ft].data(), size, &config);
            if (isMonospace) FontLookup.MonospaceFonts.insert(fonts[ft][size]);
#endif

#ifdef GLIMMER_ENABLE_ICON_FONT
            if (IconFontAttachRange.count(size) != 0)
                AddIconFont(IconFontAttachRange.at(size), size, true);
#endif
        }
    }

    bool LoadFonts(std::string_view family, const FontCollectionFile& files, float size, ImFontConfig config, 
        bool autoScale, bool isMonospace, bool hinting, bool antialias)
    {
        int32_t flags = !hinting ? ImGuiFreeTypeBuilderFlags_NoHinting : 
            !antialias ? ImGuiFreeTypeBuilderFlags_MonoHinting : ImGuiFreeTypeBuilderFlags_LightHinting;
        flags = flags | (!antialias ? ImGuiFreeTypeBuilderFlags_Monochrome : 0);

        ImGuiIO& io = ImGui::GetIO();
        FontStore[family].Files = files;

        auto& ffamily = FontStore[family];
        ffamily.AutoScale = autoScale;
        LoadFont(io, ffamily, FT_Normal, size, config, flags, isMonospace);

#ifdef IMGUI_ENABLE_FREETYPE
        LoadFont(io, ffamily, FT_Bold, size, config, ImGuiFreeTypeBuilderFlags_Bold, isMonospace);
        LoadFont(io, ffamily, FT_Italics, size, config, ImGuiFreeTypeBuilderFlags_Oblique, isMonospace);
        LoadFont(io, ffamily, FT_BoldItalics, size, config, ImGuiFreeTypeBuilderFlags_Bold | 
            ImGuiFreeTypeBuilderFlags_Oblique, isMonospace);
#else
        LoadFont(io, ffamily, FT_Bold, size, config, 0, isMonospace);
        LoadFont(io, ffamily, FT_Italics, size, config, 0, isMonospace);
        LoadFont(io, ffamily, FT_BoldItalics, size, config, 0, isMonospace);
#endif
        LoadFont(io, ffamily, FT_Light, size, config, 0, isMonospace);
        return true;
    }

#endif
#ifndef GLIMMER_DISABLE_BLEND2D_RENDERER
    static void CreateFont(FontFamily& family, FontType ft, float size)
    {
        auto& face = family.FontFace[ft];

        if (ft == FT_Normal)
        {
            auto& font = family.Fonts[FT_Normal][size];
            auto res = face.createFromFile(family.Files.Files[FT_Normal].c_str());
            res = res == BL_SUCCESS ? font.createFromFace(face, size) : res;
            assert(res == BL_SUCCESS);
        }
        else
        {
            const auto& fallback = family.Fonts[FT_Normal][size];

            if (!files.Files[ft].empty())
            {
                auto res = face.createFromFile(family.Files.Files[ft].c_str());
                if (res == BL_SUCCESS) res = family.Fonts[ft][size].createFromFace(face, size);
                else family.Fonts[ft][size] = fallback;

                if (res != BL_SUCCESS) family.Fonts[ft][size] = fallback;
            }
            else
                family.Fonts[ft][size] = fallback;
        }
    }

    bool LoadFonts(std::string_view family, const FontCollectionFile& files, float size)
    {
        auto& ffamily = FontStore[family];
        ffamily.Files = files;
        CreateFont(ffamily, FT_Normal, size);
        CreateFont(ffamily, FT_Light, size);
        CreateFont(ffamily, FT_Bold, size);
        CreateFont(ffamily, FT_Italics, size);
        CreateFont(ffamily, FT_BoldItalics, size);
    }
#endif

#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
    static void LoadDefaultProportionalFont(float sz, const ImFontConfig& fconfig, bool autoScale, bool hinting, bool antialias)
    {
#ifdef _WIN32
        LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { WINDOWS_DEFAULT_FONT }, sz, fconfig, autoScale, false, hinting, antialias);
#elif __linux__
        std::filesystem::path fedoradir = "/usr/share/fonts/open-sans";
        std::filesystem::path ubuntudir = "/usr/share/fonts/truetype/freefont";
        std::filesystem::exists(fedoradir) ?
            LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { FEDORA_DEFAULT_FONT }, sz, fconfig, autoScale, false, hinting, antialias) :
            std::filesystem::exists(ubuntudir) ?
            LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { POPOS_DEFAULT_FONT }, sz, fconfig, autoScale, false, hinting, antialias) :
            LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { MANJARO_DEFAULT_FONT }, sz, fconfig, autoScale, false, hinting, antialias);
#endif
        // TODO: Add default fonts for other platforms
    }

    static void LoadDefaultMonospaceFont(float sz, const ImFontConfig& fconfig, bool autoScale, bool hinting, bool antialias)
    {
#ifdef _WIN32
        LoadFonts(GLIMMER_MONOSPACE_FONTFAMILY, { WINDOWS_DEFAULT_MONOFONT }, sz, fconfig, autoScale, true, hinting, antialias);
#elif __linux__
        std::filesystem::path fedoradir = "/usr/share/fonts/liberation-mono";
        std::filesystem::path ubuntudir = "/usr/share/fonts/truetype/freefont";
        std::filesystem::exists(fedoradir) ?
            LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { FEDORA_DEFAULT_MONOFONT }, sz, fconfig, autoScale, false, hinting, antialias) :
            std::filesystem::exists(ubuntudir) ?
            LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { POPOS_DEFAULT_MONOFONT }, sz, fconfig, autoScale, false, hinting, antialias) :
            LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { MANJARO_DEFAULT_MONOFONT }, sz, fconfig, autoScale, false, hinting, antialias);
#endif
        // TODO: Add default fonts for other platforms
    }
#endif

#ifndef GLIMMER_DISABLE_BLEND2D_RENDERER
    static void LoadDefaultProportionalFont(float sz)
    {
#ifdef _WIN32
        LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { WINDOWS_DEFAULT_FONT }, sz);
#elif __linux__
        std::filesystem::path fedoradir = "/usr/share/fonts/open-sans";
        std::filesystem::path ubuntudir = "/usr/share/fonts/truetype/freefont";
        std::filesystem::exists(fedoradir) ?
            LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { FEDORA_DEFAULT_FONT }, sz) :
            std::filesystem::exists(ubuntudir) ?
            LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { POPOS_DEFAULT_FONT }, sz) :
            LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { MANJARO_DEFAULT_FONT }, sz);
#endif
        // TODO: Add default fonts for other platforms
    }

    static void LoadDefaultMonospaceFont(float sz)
    {
#ifdef _WIN32
        LoadFonts(GLIMMER_MONOSPACE_FONTFAMILY, { WINDOWS_DEFAULT_MONOFONT }, sz);
#elif __linux__
        std::filesystem::path fedoradir = "/usr/share/fonts/liberation-mono";
        std::filesystem::path ubuntudir = "/usr/share/fonts/truetype/freefont";
        std::filesystem::exists(fedoradir) ?
            LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { FEDORA_DEFAULT_MONOFONT }, sz) :
            std::filesystem::exists(ubuntudir) ?
            LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { POPOS_DEFAULT_MONOFONT }, sz) :
            LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, { MANJARO_DEFAULT_MONOFONT }, sz);
#endif
        // TODO: Add default fonts for other platforms
    }
#endif

#ifndef IM_RICHTEXT_TARGET_IMGUI
    using ImWchar = uint32_t;
#endif

    static bool LoadDefaultFonts(float sz, const FontFileNames* names, bool skipProportional, bool skipMonospace,
        bool autoScale, bool hinting, bool antialias, const ImWchar* glyphs)
    {
#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
        ImFontConfig fconfig;
        fconfig.OversampleH = 2;
        fconfig.OversampleV = 1;
        fconfig.RasterizerMultiply = sz <= 16.f ? 2.f : 1.f;
        fconfig.GlyphRanges = glyphs;
#endif

        auto copyFileName = [](const std::string_view fontname, char* fontpath, int startidx) {
            auto sz = std::min((int)fontname.size(), _MAX_PATH - startidx);

            if (sz == 0) memset(fontpath, 0, _MAX_PATH);
            else
            {
#ifdef _WIN32
                if (fontpath[startidx - 1] != '\\') { fontpath[startidx] = '\\'; startidx++; }
#else
                if (fontpath[startidx - 1] != '/') { fontpath[startidx] = '/'; startidx++; }
#endif
                memcpy(fontpath + startidx, fontname.data(), sz);
                fontpath[startidx + sz] = 0;
            }

            return fontpath;
        };

        if (names == nullptr)
        {
#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
            if (!skipProportional) LoadDefaultProportionalFont(sz, fconfig, autoScale, hinting, antialias);
            if (!skipMonospace) LoadDefaultMonospaceFont(sz, fconfig, autoScale, hinting, antialias);
#endif
#ifndef GLIMMER_DISABLE_BLEND2D_RENDERER
            if (!skipProportional) LoadDefaultProportionalFont(sz);
            if (!skipMonospace) LoadDefaultMonospaceFont(sz);
#endif
        }
        else
        {
#if defined(_WIN32)
            static char BaseFontPaths[FT_Total][_MAX_PATH] = { "c:\\Windows\\Fonts\\", "c:\\Windows\\Fonts\\",
                "c:\\Windows\\Fonts\\","c:\\Windows\\Fonts\\", "c:\\Windows\\Fonts\\" };
#elif __APPLE__
            static char BaseFontPaths[FT_Total][_MAX_PATH] = { "/Library/Fonts/", "/Library/Fonts/",
                "/Library/Fonts/", "/Library/Fonts/", "/Library/Fonts/" };
#elif __linux__
            static char BaseFontPaths[FT_Total][_MAX_PATH] = { "/usr/share/fonts/", "/usr/share/fonts/",
                "/usr/share/fonts/", "/usr/share/fonts/", "/usr/share/fonts/" };
#else
#error "Platform unspported..."
#endif

            if (!names->BasePath.empty())
            {
                for (auto idx = 0; idx < FT_Total; ++idx)
                {
                    auto baseFontPath = BaseFontPaths[idx];
                    memset(baseFontPath, 0, _MAX_PATH);
                    auto sz = std::min((int)names->BasePath.size(), _MAX_PATH);
                    strncpy_s(baseFontPath, _MAX_PATH - 1, names->BasePath.data(), sz);
                    baseFontPath[sz] = '\0';
                }
            }

            const int startidx = (int)std::strlen(BaseFontPaths[0]);
            FontCollectionFile files;

            if (!skipProportional && !names->Proportional.Files[FT_Normal].empty())
            {
                files.Files[FT_Normal] = copyFileName(names->Proportional.Files[FT_Normal], BaseFontPaths[FT_Normal], startidx);
                files.Files[FT_Light] = copyFileName(names->Proportional.Files[FT_Light], BaseFontPaths[FT_Light], startidx);
                files.Files[FT_Bold] = copyFileName(names->Proportional.Files[FT_Bold], BaseFontPaths[FT_Bold], startidx);
                files.Files[FT_Italics] = copyFileName(names->Proportional.Files[FT_Italics], BaseFontPaths[FT_Italics], startidx);
                files.Files[FT_BoldItalics] = copyFileName(names->Proportional.Files[FT_BoldItalics], BaseFontPaths[FT_BoldItalics], startidx);
#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
                LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, files, sz, fconfig, autoScale, false, hinting, antialias);
#endif
#ifndef GLIMMER_DISABLE_BLEND2D_RENDERER
                LoadFonts(GLIMMER_DEFAULT_FONTFAMILY, files, sz);
#endif
            }
            else
            {
#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
                if (!skipProportional) LoadDefaultProportionalFont(sz, fconfig, autoScale, hinting, antialias);
#endif
#ifndef GLIMMER_DISABLE_BLEND2D_RENDERER
                if (!skipProportional) LoadDefaultProportionalFont(sz);
#endif
            }

            if (!skipMonospace && !names->Monospace.Files[FT_Normal].empty())
            {
                files.Files[FT_Normal] = copyFileName(names->Monospace.Files[FT_Normal], BaseFontPaths[FT_Normal], startidx);
                files.Files[FT_Bold] = copyFileName(names->Monospace.Files[FT_Bold], BaseFontPaths[FT_Bold], startidx);
                files.Files[FT_Italics] = copyFileName(names->Monospace.Files[FT_Italics], BaseFontPaths[FT_Italics], startidx);
                files.Files[FT_BoldItalics] = copyFileName(names->Monospace.Files[FT_BoldItalics], BaseFontPaths[FT_BoldItalics], startidx);
#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
                LoadFonts(GLIMMER_MONOSPACE_FONTFAMILY, files, sz, fconfig, autoScale, true, hinting, antialias);
#endif
#ifndef GLIMMER_DISABLE_BLEND2D_RENDERER
                LoadFonts(GLIMMER_MONOSPACE_FONTFAMILY, files, sz);
#endif
            }
            else
            {
#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
                if (!skipMonospace) LoadDefaultMonospaceFont(sz, fconfig, autoScale, hinting, antialias);
#endif
#ifndef GLIMMER_DISABLE_BLEND2D_RENDERER
                if (!skipMonospace) LoadDefaultMonospaceFont(sz);
#endif
            }
        }

        return true;
    }

    const static std::unordered_map<TextContentCharset, std::vector<ImWchar>> GlyphRanges
    {
        { TextContentCharset::ASCII, { 1, 127, 0 } },
        { TextContentCharset::ASCIISymbols, { 1, 127, 0x20A0, 0x20CF, 0x2122, 0x2122,
            0x2190, 0x21FF, 0x2200, 0x22FF, 0x2573, 0x2573, 0x25A0, 0x25FF, 0x2705, 0x2705,
            0x2713, 0x2716, 0x274E, 0x274E, 0x2794, 0x2794, 0x27A4, 0x27A4, 0x27F2, 0x27F3,
            0x2921, 0x2922, 0X2A7D, 0X2A7E, 0x2AF6, 0x2AF6, 0x2B04, 0x2B0D, 0x2B60, 0x2BD1,
            0 } },
        { TextContentCharset::UTF8Simple, { 1, 256, 0x100, 0x17F, 0x180, 0x24F,
            0x370, 0x3FF, 0x400, 0x4FF, 0x500, 0x52F, 0x1E00, 0x1EFF, 0x1F00, 0x1FFF,
            0x20A0, 0x20CF, 0x2122, 0x2122,
            0x2190, 0x21FF, 0x2200, 0x22FF, 0x2573, 0x2573, 0x25A0, 0x25FF, 0x2705, 0x2705,
            0x2713, 0x2716, 0x274E, 0x274E, 0x2794, 0x2794, 0x27A4, 0x27A4, 0x27F2, 0x27F3,
            0x2921, 0x2922, 0x2980, 0x29FF, 0x2A00, 0x2AFF, 0X2A7D, 0X2A7E, 0x2AF6, 0x2AF6,
            0x2B04, 0x2B0D, 0x2B60, 0x2BD1, 0x1F600, 0x1F64F, 0x1F800, 0x1F8FF,
            0 } },
        { TextContentCharset::UnicodeBidir, {} }, // All glyphs supported by font
    };

    static bool LoadDefaultFonts(const std::vector<float>& sizes, uint64_t flt, TextContentCharset charset,
        const FontFileNames* names)
    {
        assert((names != nullptr) || (flt & FLT_Proportional) || (flt & FLT_Monospace));

        auto it = GlyphRanges.find(charset);
        auto glyphrange = it->second.empty() ? nullptr : it->second.data();

        for (auto sz : sizes)
        {
            LoadDefaultFonts(sz, names, !(flt & FLT_Proportional), !(flt & FLT_Monospace),
                flt & FLT_AutoScale, flt & FLT_Hinting, flt & FLT_Antialias, glyphrange);
        }

#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
        ImGui::GetIO().Fonts->Build();
#endif
        return true;
    }

#ifndef GLIMMER_DISABLE_RICHTEXT
    std::vector<float> GetFontSizes(const ImRichText::RenderConfig& config, uint64_t flt)
    {
        std::vector<float> sizes;
        sizes.push_back(config.DefaultFontSize * config.FontScale);

        if (flt & FLT_HasSubscript) sizes.push_back(config.DefaultFontSize * config.ScaleSubscript * config.FontScale);
        if (flt & FLT_HasSuperscript) sizes.push_back(config.DefaultFontSize * config.ScaleSuperscript * config.FontScale);
        if (flt & FLT_HasSmall) sizes.push_back(config.DefaultFontSize * 0.8f * config.FontScale);
        if (flt & FLT_HasH1) sizes.push_back(config.HFontSizes[0] * config.FontScale);
        if (flt & FLT_HasH2) sizes.push_back(config.HFontSizes[1] * config.FontScale);
        if (flt & FLT_HasH3) sizes.push_back(config.HFontSizes[2] * config.FontScale);
        if (flt & FLT_HasH4) sizes.push_back(config.HFontSizes[3] * config.FontScale);
        if (flt & FLT_HasH5) sizes.push_back(config.HFontSizes[4] * config.FontScale);
        if (flt & FLT_HasH6) sizes.push_back(config.HFontSizes[5] * config.FontScale);
        if (flt & FLT_HasHeaders) for (auto sz : config.HFontSizes) sizes.push_back(sz * config.FontScale);
        std::sort(sizes.begin(), sizes.end());

        return (flt & FLT_AutoScale) ? std::vector<float>{ *(--sizes.end()) } : sizes;
    }
#endif

    bool LoadDefaultFonts(const FontDescriptor* descriptors, int total, bool needRichText)
    {
        assert(descriptors != nullptr);

        std::vector<bool> iconFontIndices;

#ifdef GLIMMER_ENABLE_ICON_FONT
        std::map<float, std::vector<GlyphRangeMap>> exclusiveRange;

        for (auto idx = 0; idx < total; ++idx)
        {
            auto isIconFont = descriptors[idx].flags & FLT_IsIconFont;
            auto isExclusive = descriptors[idx].flags & FLT_IconFontExclusive;

            if (isExclusive)
            {
                for (auto sz : descriptors[idx].sizes)
                {
                    auto& range = exclusiveRange[sz];
                    range.emplace_back(
                        descriptors[idx].customCharRange.first,
                        descriptors[idx].customCharRange.second,
                        descriptors[idx].path, descriptors[idx].flags
                    );
                }
            }
            else if (isIconFont)
            {
                for (auto sz : descriptors[idx].sizes)
                {
                    auto& range = IconFontAttachRange[sz];
                    range.emplace_back(
                        descriptors[idx].customCharRange.first,
                        descriptors[idx].customCharRange.second,
                        descriptors[idx].path, descriptors[idx].flags
                    );
                }
            }

            iconFontIndices.emplace_back(isIconFont);
        }

        for (const auto& [size, range] : exclusiveRange)
            AddIconFont(range, size, false);

#else
        iconFontIndices.resize(total, false);
#endif

        for (auto idx = 0; idx < total; ++idx)
        {
            if (!iconFontIndices[idx])
            {
                auto names = descriptors[idx].names.has_value() ? &(descriptors[idx].names.value()) : nullptr;

#ifndef GLIMMER_DISABLE_RICHTEXT
                if (needRichText)
                {
                    auto sizes = GetFontSizes(*Config.richTextConfig, descriptors[idx].flags);
                    sizes.insert(sizes.begin(), descriptors[idx].sizes.begin(), descriptors[idx].sizes.end());
                    LoadDefaultFonts(sizes, descriptors[idx].flags, descriptors[idx].charset, names);
                }
                else
#endif
                    LoadDefaultFonts(descriptors[idx].sizes, descriptors[idx].flags, descriptors[idx].charset, names);
            }
        }

        return true;
    }

    // Structure to hold font data
    struct FontInfo
    {
        std::string fontFamily;
        int weight = 400;          // Default to normal (400)
        bool isItalic = false;
        bool isBold = false;
        bool isMono = false;
        bool isLight = false;
        bool isSerif = true;
    };

    // Function to read a big-endian 16-bit unsigned integer
    uint16_t ReadUInt16(const unsigned char* data, size_t offset)
    {
        return (data[offset] << 8) | data[offset + 1];
    }

    // Function to read a big-endian 32-bit unsigned integer
    uint32_t ReadUInt32(const unsigned char* data, size_t offset)
    {
        return (static_cast<uint32_t>(data[offset]) << 24) |
            (static_cast<uint32_t>(data[offset + 1]) << 16) |
            (static_cast<uint32_t>(data[offset + 2]) << 8) |
            static_cast<uint32_t>(data[offset + 3]);
    }

    // Read a Pascal string (length byte followed by string data)
    std::string ReadPascalString(const unsigned char* data, size_t offset, size_t length)
    {
        std::string result;
        for (size_t i = 0; i < length; i++)
            if (data[offset + i] != 0)
                result.push_back(static_cast<char>(data[offset + i]));
        return result;
    }

    // Extract font information from a TTF file
    FontInfo ExtractFontInfo(const std::string& filename)
    {
        FontInfo info;
        std::ifstream file(filename, std::ios::binary);

        if (!file.is_open())
        {
#ifdef _DEBUG
            std::cerr << "Error: Could not open file " << filename << std::endl;
#endif
            return info;
        }

        // Get file size
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // Read the entire file into memory
        std::vector<unsigned char> buffer(fileSize);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        // Check if this is a valid TTF file (signature should be 0x00010000 for TTF)
        uint32_t sfntVersion = ReadUInt32(buffer.data(), 0);
        if (sfntVersion != 0x00010000 && sfntVersion != 0x4F54544F)
        { // TTF or OTF
#ifdef _DEBUG
            std::cerr << "Error: Not a valid TTF/OTF file" << std::endl;
#endif
            return info;
        }

        // Parse the table directory
        uint16_t numTables = ReadUInt16(buffer.data(), 4);
        bool foundName = false;
        bool foundOS2 = false;
        uint32_t nameTableOffset = 0;
        uint32_t os2TableOffset = 0;

        // Table directory starts at offset 12
        for (int i = 0; i < numTables; i++)
        {
            size_t entryOffset = 12 + i * 16;
            char tag[5] = { 0 };
            memcpy(tag, buffer.data() + entryOffset, 4);

            if (strcmp(tag, "name") == 0)
            {
                nameTableOffset = ReadUInt32(buffer.data(), entryOffset + 8);
                foundName = true;
            }
            else if (strcmp(tag, "OS/2") == 0)
            {
                os2TableOffset = ReadUInt32(buffer.data(), entryOffset + 8);
                foundOS2 = true;
            }

            if (foundName && foundOS2) break;
        }

        // Process the 'name' table if found
        // Docs: https://learn.microsoft.com/en-us/typography/opentype/spec/name
        if (foundName)
        {
            uint16_t nameCount = ReadUInt16(buffer.data(), nameTableOffset + 2);
            uint16_t storageOffset = ReadUInt16(buffer.data(), nameTableOffset + 4);
            uint16_t familyNameID = 1;  // Font Family name
            uint16_t subfamilyNameID = 2;  // Font Subfamily name

            for (int i = 0; i < nameCount; i++)
            {
                size_t recordOffset = nameTableOffset + 6 + i * 12;
                uint16_t platformID = ReadUInt16(buffer.data(), recordOffset);
                uint16_t encodingID = ReadUInt16(buffer.data(), recordOffset + 2);
                uint16_t languageID = ReadUInt16(buffer.data(), recordOffset + 4);
                uint16_t nameID = ReadUInt16(buffer.data(), recordOffset + 6);
                uint16_t length = ReadUInt16(buffer.data(), recordOffset + 8);
                uint16_t stringOffset = ReadUInt16(buffer.data(), recordOffset + 10);

                // We prefer English Windows (platformID=3, encodingID=1, languageID=0x0409)
                bool isEnglish = (platformID == 3 && encodingID == 1 && (languageID == 0x0409 || languageID == 0));

                // If not English Windows, try platform-independent entries as fallback
                if (!isEnglish && platformID == 0) isEnglish = true;

                if (isEnglish)
                {
                    if (nameID == familyNameID && info.fontFamily.empty())
                    {
                        // Convert UTF-16BE to ASCII for simplicity
                        std::string name;
                        for (int j = 0; j < length; j += 2)
                        {
                            char c = buffer[nameTableOffset + storageOffset + stringOffset + j + 1];
                            if (c) name.push_back(c);
                        }
                        info.fontFamily = name;
                    }
                    else if (nameID == subfamilyNameID)
                    {
                        // Convert UTF-16BE to ASCII for simplicity
                        std::string name;
                        for (int j = 0; j < length; j += 2)
                        {
                            char c = buffer[nameTableOffset + storageOffset + stringOffset + j + 1];
                            if (c) name.push_back(c);
                        }

                        // Check if it contains "Italic" or "Oblique"
                        if (name.find("Italic") != std::string::npos ||
                            name.find("italic") != std::string::npos ||
                            name.find("Oblique") != std::string::npos ||
                            name.find("oblique") != std::string::npos)
                            info.isItalic = true;
                    }
                }
            }
        }

        // Process the 'OS/2' table if found
        // Docs: https://learn.microsoft.com/en-us/typography/opentype/spec/os2
        if (foundOS2)
        {
            // Weight is at offset 4 in the OS/2 table
            info.weight = ReadUInt16(buffer.data(), os2TableOffset + 4);

            // Check fsSelection bit field for italic flag (bit 0)
            uint16_t fsSelection = ReadUInt16(buffer.data(), os2TableOffset + 62);
            if ((fsSelection & 0x01) || (fsSelection & 0x100)) info.isItalic = true;
            if (fsSelection & 0x10) info.isBold = true;

            uint8_t panose[10];
            memcpy(panose, buffer.data() + os2TableOffset + 32, 10);

            // Refer to this: https://monotype.github.io/panose/pan2.htm for PANOSE docs
            if (panose[0] == 2 && panose[3] == 9) info.isMono = true;
            if (panose[0] == 2 && (panose[2] == 2 || panose[2] == 3 || panose[2] == 4)) info.isLight = true;
            if (panose[0] == 2 && (panose[1] == 11 || panose[1] == 12 || panose[1] == 13)) info.isSerif = false;
        }

        return info;
    }

#if __linux__
    struct FontFamilyInfo
    {
        std::string filename;
        std::string fontName;
        std::string style;
    };

    // Function to execute a command and return the output
    static std::string ExecCommand(const char* cmd)
    {
        std::array<char, 8192> buffer;
        std::string result;
        FILE* pipe = popen(cmd, "r");

        if (!pipe) return std::string{};

        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
            result += buffer.data();

        pclose(pipe);
        return result;
    }

    // Trim whitespace from a string_view and return as string
    static std::string_view Trim(std::string_view sv)
    {
        // Find first non-whitespace
        size_t start = 0;
        while (start < sv.size() && std::isspace(sv[start]))
            ++start;

        // Find last non-whitespace
        size_t end = sv.size();
        while (end > start && std::isspace(sv[end - 1]))
            --end;

        return sv.substr(start, end - start);
    }

    // Parse a single line from fc-list output
    static FontFamilyInfo ParseFcListLine(const std::string& line)
    {
        FontFamilyInfo info;
        std::string_view lineView(line);

        // fc-list typically outputs: filename: font name:style=style1,style2,...

        // Find the first colon for filename
        size_t firstColon = lineView.find(':');
        if (firstColon != std::string_view::npos)
        {
            info.filename = Trim(lineView.substr(0, firstColon));

            // Find the second colon for font name
            size_t secondColon = lineView.find(':', firstColon + 1);
            if (secondColon != std::string_view::npos)
            {
                info.fontName = Trim(lineView.substr(firstColon + 1, secondColon - firstColon - 1));

                // Find "style=" after the second colon
                std::string_view styleView = lineView.substr(secondColon + 1);
                size_t stylePos = styleView.find("style=");

                if (stylePos != std::string_view::npos)
                {
                    styleView = styleView.substr(stylePos + 6); // 6 is length of "style="

                    // Find the first comma in the style list
                    size_t commaPos = styleView.find(',');
                    if (commaPos != std::string_view::npos)
                        info.style = Trim(styleView.substr(0, commaPos));
                    else
                        info.style = Trim(styleView);
                }
                else
                    info.style = "Regular"; // Default style if not specified
            }
            else
            {
                // If there's no second colon, use everything after first colon as font name
                info.fontName = Trim(lineView.substr(firstColon + 1));
                info.style = "Regular";
            }
        }

        return info;
    }

    static bool PopulateFromFcList()
    {
        std::string output = ExecCommand("fc-list");

        if (!output.empty())
        {
            std::istringstream iss(output);
            std::string line;

            while (std::getline(iss, line))
            {
                if (!line.empty())
                {
                    auto info = ParseFcListLine(line);
                    auto isBold = info.style.find("Bold") != std::string::npos;
                    auto isItalics = (info.style.find("Oblique") != std::string::npos) ||
                        (info.style.find("Italic") != std::string::npos);
                    auto isMonospaced = info.fontName.find("Mono") != std::string::npos;
                    auto ft = isBold && isItalics ? FT_BoldItalics : isBold ? FT_Bold :
                        isItalics ? FT_Italics : FT_Normal;
                    auto isSerif = info.fontName.find("Serif") != std::string::npos;
                    FontLookup.Register(info.fontName, info.filename, ft, isMonospaced, isSerif);
                }
            }

            return true;
        }

        return false;
    }
#endif

#ifdef _WIN32
    static const std::string_view CommonFontNames[11]{
        "Arial", "Bookman Old Style", "Comic Sans MS", "Consolas", "Courier",
        "Georgia", "Lucida", "Segoe UI", "Tahoma", "Times New Roman", "Verdana"
    };
#elif __linux__
    static const std::string_view CommonFontNames[8]{
        "OpenSans", "FreeSans", "NotoSans", "Hack",
        "Bitstream Vera", "DejaVu", "Liberation", "Nimbus"
        // Add other common fonts expected
    };
#endif

    static void ProcessFileEntry(const std::filesystem::directory_entry& entry, bool cacheOnlyCommon)
    {
        auto fpath = entry.path().string();
        auto info = ExtractFontInfo(fpath);
#ifdef _DEBUG
        std::cout << "Checking font file: " << fpath << std::endl;
#endif

        if (cacheOnlyCommon)
        {
            for (const auto& fname : CommonFontNames)
            {
                if (info.fontFamily.find(fname) != std::string::npos)
                {
                    auto isBold = info.isBold || (info.weight >= 600);
                    auto ftype = isBold && info.isItalic ? FT_BoldItalics :
                        isBold ? FT_Bold : info.isItalic ? FT_Italics :
                        (info.weight < 400) || info.isLight ? FT_Light : FT_Normal;
                    FontLookup.Register(info.fontFamily, fpath, ftype, info.isMono, info.isSerif);
                    break;
                }
            }
        }
        else
        {
            auto isBold = info.isBold || (info.weight >= 600);
            auto ftype = isBold && info.isItalic ? FT_BoldItalics :
                isBold ? FT_Bold : info.isItalic ? FT_Italics :
                (info.weight < 400) || info.isLight ? FT_Light : FT_Normal;
            FontLookup.Register(info.fontFamily, fpath, ftype, info.isMono, info.isSerif);
        }
    }

    static void PreloadFontLookupInfoImpl(int timeoutMs, std::string_view* lookupPaths, int lookupSz)
    {
        std::unordered_set<std::string_view> notLookedUp;
        auto isDefaultPath = lookupSz == 0 || lookupPaths == nullptr;
        assert((lookupPaths == nullptr && lookupSz == 0) || (lookupPaths != nullptr && lookupSz > 0));

        for (auto idx = 0; idx < lookupSz; ++idx)
        {
            if (FontLookup.LookupPaths.count(lookupPaths[idx]) == 0)
                notLookedUp.insert(lookupPaths[idx]);
        }

        if (isDefaultPath)
#ifdef _WIN32
            notLookedUp.insert("C:\\Windows\\Fonts");
#elif __linux__
            notLookedUp.insert("/usr/share/fonts/");
#endif

        if (!notLookedUp.empty())
        {
#ifdef _WIN32
            auto start = std::chrono::system_clock().now().time_since_epoch().count();

            for (auto path : notLookedUp)
            {
                for (const auto& entry : std::filesystem::directory_iterator{ path })
                {
                    if (entry.is_regular_file() && ((entry.path().extension() == ".TTF") ||
                        (entry.path().extension() == ".ttf")))
                    {
                        ProcessFileEntry(entry, isDefaultPath);

                        auto current = std::chrono::system_clock().now().time_since_epoch().count();
                        if (timeoutMs != -1 && (int)(current - start) > timeoutMs) break;
                    }
                }
            }

#ifdef _DEBUG
            auto end = std::chrono::system_clock().now().time_since_epoch().count();
            std::printf("Font lookup completed in %lld ms\n", (end - start) / 1000000);
#endif
#elif __linux__
            if (isDefaultPath)
            {
                if (!PopulateFromFcList())
                {
                    auto start = std::chrono::system_clock().now().time_since_epoch().count();

                    for (const auto& entry : std::filesystem::recursive_directory_iterator{ "/usr/share/fonts/" })
                    {
                        if (entry.is_regular_file() && entry.path().extension() == ".ttf")
                        {
                            ProcessFileEntry(entry, true);

                            auto current = std::chrono::system_clock().now().time_since_epoch().count();
                            if (timeoutMs != -1 && (int)(current - start) > timeoutMs) break;
                        }
                    }

#ifdef _DEBUG
					auto end = std::chrono::system_clock().now().time_since_epoch().count();
					std::printf("Font lookup completed in %lld ms\n", (end - start) / 1000000);
#endif
                }
            }
            else
            {
                auto start = std::chrono::system_clock().now().time_since_epoch().count();

                for (auto path : notLookedUp)
                {
                    for (const auto& entry : std::filesystem::directory_iterator{ path })
                    {
                        if (entry.is_regular_file() && entry.path().extension() == ".ttf")
                        {
                            ProcessFileEntry(entry, false);

                            auto current = std::chrono::system_clock().now().time_since_epoch().count();
                            if (timeoutMs != -1 && (int)(current - start) > timeoutMs) break;
                        }
                    }
                }

#ifdef _DEBUG
                auto end = std::chrono::system_clock().now().time_since_epoch().count();
                std::printf("Font lookup completed in %lld ms\n", (end - start) / 1000000);
#endif
            }
#endif      
        }
    }

    std::string_view FindFontFile(std::string_view family, FontType ft, std::string_view* lookupPaths, int lookupSz)
    {
        PreloadFontLookupInfoImpl(-1, lookupPaths, lookupSz);
        auto it = FontLookup.ProportionalFontFamilies.find(family);

        if (it == FontLookup.ProportionalFontFamilies.end())
        {
            it = FontLookup.MonospaceFontFamilies.find(family);

            if (it == FontLookup.MonospaceFontFamilies.end())
            {
                auto isDefaultMonospace = family.find("monospace") != std::string_view::npos;
                auto isDefaultSerif = family.find("serif") != std::string_view::npos &&
                    family.find("sans") == std::string_view::npos;

#ifdef _WIN32
                it = isDefaultMonospace ? FontLookup.MonospaceFontFamilies.find("Consolas") :
                    isDefaultSerif ? FontLookup.ProportionalFontFamilies.find("Times New Roman") :
                    FontLookup.ProportionalFontFamilies.find("Segoe UI");
#elif __linux__
                it = isDefaultMonospace ? FontLookup.MonospaceFontFamilies.find("DejaVu Mono") :
                    isDefaultSerif ? FontLookup.ProportionalFontFamilies.find("DejaVu Serif") :
					FontLookup.ProportionalFontFamilies.find("DejaVu Sans");
#endif
                // TODO: Implement for Linux
            }
        }

        return FontLookup.info[it->second].files[ft];
    }

#ifndef GLIMMER_DISABLE_IMGUI_RENDERER
    static auto LookupFontFamily(std::string_view family)
    {
        auto famit = FontStore.find(family);

        if (famit == FontStore.end())
        {
            for (auto it = FontStore.begin(); it != FontStore.end(); ++it)
            {
                if (it->first.find(family) == 0u ||
                    family.find(it->first) == 0u)
                {
                    return it;
                }
            }
        }

        if (famit == FontStore.end())
            famit = FontStore.find(GLIMMER_DEFAULT_FONTFAMILY);

        if (famit == FontStore.end())
            famit = FontStore.begin();

        return famit;
    }

    void* GetFont(std::string_view family, float size, FontType ft)
    {
        auto famit = LookupFontFamily(family);
        const auto& fonts = famit->second.FontPtrs[ft];
        auto szit = fonts.find(size);

        if (szit == fonts.end() && !fonts.empty())
        {
#ifdef GLIMMER_ENABLE_ICON_FONT
            if (AreSame(family, "icon") || AreSame(family, "icons") ||
                AreSame(family, "icon-font") || AreSame(family, "Icon Font"))
                return Config.iconFont;
            else 
#endif    
            if (famit->second.AutoScale)
            {
                return fonts.begin()->second;
            }
            else
            {
                szit = fonts.lower_bound(size);
                szit = szit == fonts.begin() ? szit : std::prev(szit);
            }
        }

        return szit->second;
    }

    bool IsFontMonospace(void* font)
    {
        return FontLookup.MonospaceFonts.find(font) != FontLookup.MonospaceFonts.end();
    }

    bool IsFontLoaded()
    {
        return ImGui::GetIO().Fonts->IsBuilt();
    }

#endif
#ifndef GLIMMER_DISABLE_BLEND2D_RENDERER
    void PreloadFontLookupInfo(int timeoutMs)
    {
        PreloadFontLookupInfoImpl(timeoutMs, nullptr, 0);
    }

    void* GetFont(std::string_view family, float size, FontType type, FontExtraInfo extra)
    {
        auto famit = FontStore.find(family);

        if (famit != FontStore.end())
        {
            if (famit->second.Fonts[ft])
            {
                auto szit = famit->second.Fonts[ft].find(size);
                if (szit == famit->second.Fonts[ft].end())
                    CreateFont(family, ft, sz);
            }
            else CreateFont(family, ft, sz);
        }
        else
        {
            auto& ffamily = FontStore[family];
            ffamily.Files[ft] = extra.mapper != nullptr ? extra.mapper(family) :
                extra.filepath.empty() ? FindFontFile(family, type) : extra.filepath;
            assert(!ffamily.Files[ft].empty());
            CreateFont(ffamily, ft, sz);
        }

        return &(FontStore.at(family).Fonts[ft].at(size));
    }
#endif
}