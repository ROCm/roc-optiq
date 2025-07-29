// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_settings.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"

#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

bool
FontManager::Init()
{
    ImGuiIO& io = ImGui::GetIO();
    m_fonts.clear();

    constexpr float font_sizes[] = { 10.0f, 13.0f, 16.0f, 20.0f };

    #ifdef _WIN32
        const char* font_paths[] = { "C:\\Windows\\Fonts\\arial.ttf" };
    #else
        const char* font_paths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
            "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf"
        };
    #endif

    const char* font_path = nullptr;

    // Check if any of the font paths exist
    for(const char* path : font_paths)
    {
        if(std::filesystem::exists(path))
        {
            font_path = path;
            break;
        }
    }

    m_fonts.resize(static_cast<int>(FontType::__kLastFont));
    for(int i = 0; i < static_cast<int>(FontType::__kLastFont); ++i)
    {
        ImFont* font = nullptr;
        if(font_path) font = io.Fonts->AddFontFromFileTTF(font_path, font_sizes[i]);
        if(!font)
            font = io.Fonts->AddFontDefault();  // Back to ImGUI font if good font cannot
                                                // be found
        m_fonts[i] = font;
    }

    io.FontDefault = m_fonts[static_cast<int>(FontType::kDefault)];
    return io.Fonts->Build();
}

ImFont*
FontManager::GetFont(FontType font_type)
{
    if(static_cast<int>(font_type) < 0 ||
       static_cast<int>(font_type) >= static_cast<int>(FontType::__kLastFont))
    {
        return nullptr;  // Invalid font type
    }
    return m_fonts[static_cast<int>(font_type)];
}

const std::vector<ImU32> DARK_THEME_COLORS = []() {
    std::vector<ImU32> colors(static_cast<int>(Colors::__kLastColor));
    colors[static_cast<int>(Colors::kMetaDataColor)]         = IM_COL32(20, 20, 20, 255);
    colors[static_cast<int>(Colors::kMetaDataColorSelected)] = IM_COL32(40, 40, 40, 255);
    colors[static_cast<int>(Colors::kMetaDataSeparator)]     = IM_COL32(48, 48, 48, 255);
    colors[static_cast<int>(Colors::kTransparent)]           = IM_COL32(0, 0, 0, 0);
    colors[static_cast<int>(Colors::kTextError)]             = IM_COL32(255, 0, 0, 255);
    colors[static_cast<int>(Colors::kTextSuccess)]           = IM_COL32(0, 255, 0, 255);
    colors[static_cast<int>(Colors::kFlameChartColor)] = IM_COL32(128, 128, 128, 255);
    colors[static_cast<int>(Colors::kGridColor)]       = IM_COL32(255, 255, 255, 255);
    colors[static_cast<int>(Colors::kGridRed)]         = IM_COL32(255, 0, 0, 255);
    colors[static_cast<int>(Colors::kSelectionBorder)] = IM_COL32(0, 0, 200, 255);
    colors[static_cast<int>(Colors::kHighlightChart)]  = IM_COL32(0, 0, 200, 100);
    colors[static_cast<int>(Colors::kSelection)]       = IM_COL32(0, 0, 100, 80);
    colors[static_cast<int>(Colors::kBoundBox)]        = IM_COL32(180, 180, 180, 255);
    colors[static_cast<int>(Colors::kFillerColor)]     = IM_COL32(0, 0, 0, 255);
    colors[static_cast<int>(Colors::kScrollBarColor)]  = IM_COL32(255, 255, 255, 50);
    colors[static_cast<int>(Colors::kRulerBgColor)]    = IM_COL32(48, 48, 20, 255);
    colors[static_cast<int>(Colors::kBorderColor)]     = IM_COL32(64, 64, 64, 255);
    colors[static_cast<int>(Colors::kSplitterColor)]   = IM_COL32(100, 100, 100, 255);
    colors[static_cast<int>(Colors::kArrowColor)]      = IM_COL32(0, 0, 210, 80);

    return colors;
}();

const std::vector<ImU32> LIGHT_THEME_COLORS = []() {
    std::vector<ImU32> colors(static_cast<int>(Colors::__kLastColor));
    colors[static_cast<int>(Colors::kMetaDataColor)] = IM_COL32(240, 240, 240, 255);
    colors[static_cast<int>(Colors::kMetaDataColorSelected)] =
        IM_COL32(200, 200, 200, 255);
    colors[static_cast<int>(Colors::kMetaDataSeparator)] = IM_COL32(248, 248, 248, 255);
    colors[static_cast<int>(Colors::kTransparent)]       = IM_COL32(0, 0, 0, 0);
    colors[static_cast<int>(Colors::kTextError)]         = IM_COL32(255, 0, 0, 255);
    colors[static_cast<int>(Colors::kTextSuccess)]       = IM_COL32(0, 255, 0, 255);
    colors[static_cast<int>(Colors::kFlameChartColor)]   = IM_COL32(128, 128, 128, 255);
    colors[static_cast<int>(Colors::kHighlightChart)]    = IM_COL32(66, 150, 250, 50);
    colors[static_cast<int>(Colors::kGridColor)]         = IM_COL32(0, 0, 0, 255);
    colors[static_cast<int>(Colors::kGridRed)]           = IM_COL32(255, 0, 0, 255);
    colors[static_cast<int>(Colors::kSelectionBorder)]   = IM_COL32(0, 0, 200, 255);
    colors[static_cast<int>(Colors::kSelection)]         = IM_COL32(0, 0, 100, 80);
    colors[static_cast<int>(Colors::kBoundBox)]          = IM_COL32(100, 100, 100, 150);
    colors[static_cast<int>(Colors::kFillerColor)]       = IM_COL32(255, 255, 255, 255);
    colors[static_cast<int>(Colors::kScrollBarColor)]    = IM_COL32(200, 200, 200, 255);
    colors[static_cast<int>(Colors::kRulerBgColor)]      = IM_COL32(250, 250, 220, 255);
    colors[static_cast<int>(Colors::kBorderColor)]       = IM_COL32(178, 178, 178, 255);
    colors[static_cast<int>(Colors::kSplitterColor)]     = IM_COL32(200, 200, 200, 255);
    colors[static_cast<int>(Colors::kArrowColor)]        = IM_COL32(0, 0, 210, 80);

    return colors;
}();

Settings&
Settings::GetInstance()
{
    static Settings instance;
    return instance;
}

bool
Settings::IsHorizontalRender()
{
    return m_use_horizontal_rendering;
}

bool
Settings::HorizontalRender()
{
    m_use_horizontal_rendering = !m_use_horizontal_rendering;
    spdlog::info("Enable Dynamic Loading: {0}", (uint32_t) m_use_horizontal_rendering);
    return m_use_horizontal_rendering;
}

void
Settings::DarkMode()
{
    m_color_store = DARK_THEME_COLORS;
    ImGui::StyleColorsDark();
    ImPlot::StyleColorsDark();
    m_use_dark_mode = true;
}

void
Settings::LightMode()
{
    m_color_store = LIGHT_THEME_COLORS;
    ImGui::StyleColorsLight();
    ImPlot::StyleColorsLight();
    m_use_dark_mode = false;
}

bool
Settings::IsDarkMode() const
{
    return m_use_dark_mode;
}

void
Settings::SetDPI(float DPI)
{
    m_DPI = DPI;
}

float
Settings ::GetDPI()
{
    return m_DPI;
}

ImU32
Settings::GetColor(int value) const
{
    return m_color_store[value];
}

ImU32
Settings::GetColor(Colors color) const
{
    return m_color_store[static_cast<int>(color)];
}

const std::vector<ImU32>&
Settings::GetColorWheel()
{
    return m_flame_color_wheel;
}

Settings::Settings()
: m_color_store(static_cast<int>(Colors::__kLastColor))
, m_DPI(1)
, m_flame_color_wheel({

      IM_COL32(0, 114, 188, 204), IM_COL32(0, 158, 115, 204), IM_COL32(240, 228, 66, 204),
      IM_COL32(204, 121, 167, 204), IM_COL32(86, 180, 233, 204),
      IM_COL32(213, 94, 0, 204), IM_COL32(0, 204, 102, 204), IM_COL32(230, 159, 0, 204),
      IM_COL32(153, 153, 255, 204), IM_COL32(255, 153, 51, 204) })
, m_use_horizontal_rendering(true)
{
    LightMode();
}

Settings::~Settings() {}

}  // namespace View
}  // namespace RocProfVis
