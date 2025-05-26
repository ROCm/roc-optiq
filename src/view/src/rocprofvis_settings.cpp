// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_settings.h"
#include "rocprofvis_core.h"
#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

const std::vector<ImU32> DARK_THEME_COLORS = []() {
    std::vector<ImU32> colors(static_cast<int>(Colors::__kLastColor));
    colors[static_cast<int>(Colors::kMetaDataColor)]   = IM_COL32(20, 20, 20, 255);
    colors[static_cast<int>(Colors::kTransparent)]     = IM_COL32(0, 0, 0, 0);
    colors[static_cast<int>(Colors::kTextError)]       = IM_COL32(255, 0, 0, 255);
    colors[static_cast<int>(Colors::kTextSuccess)]     = IM_COL32(0, 255, 0, 255);
    colors[static_cast<int>(Colors::kFlameChartColor)] = IM_COL32(128, 128, 128, 255);
    colors[static_cast<int>(Colors::kGridColor)]       = IM_COL32(255, 255, 255, 255);
    colors[static_cast<int>(Colors::kGridRed)]         = IM_COL32(255, 0, 0, 255);
    colors[static_cast<int>(Colors::kSelectionBorder)] = IM_COL32(0, 0, 200, 255);
    colors[static_cast<int>(Colors::kHighlightChart)]  = IM_COL32(0, 0, 200, 100);
    colors[static_cast<int>(Colors::kSelection)]       = IM_COL32(0, 0, 100, 80);
    colors[static_cast<int>(Colors::kBoundBox)]        = IM_COL32(180, 180, 180, 255);
    colors[static_cast<int>(Colors::kFillerColor)]     = IM_COL32(0, 0, 0, 255);
    colors[static_cast<int>(Colors::kScrollBarColor)]  = IM_COL32(255, 255, 255, 50);
    return colors;
}();

const std::vector<ImU32> LIGHT_THEME_COLORS = []() {
    std::vector<ImU32> colors(static_cast<int>(Colors::__kLastColor));
    colors[static_cast<int>(Colors::kMetaDataColor)]   = IM_COL32(240, 240, 240, 55);
    colors[static_cast<int>(Colors::kTransparent)]     = IM_COL32(0, 0, 0, 0);
    colors[static_cast<int>(Colors::kTextError)]       = IM_COL32(255, 0, 0, 255);
    colors[static_cast<int>(Colors::kTextSuccess)]     = IM_COL32(0, 255, 0, 255);
    colors[static_cast<int>(Colors::kFlameChartColor)] = IM_COL32(128, 128, 128, 255);
    colors[static_cast<int>(Colors::kHighlightChart)]  = IM_COL32(0, 0, 200, 100);
    colors[static_cast<int>(Colors::kGridColor)]       = IM_COL32(0, 0, 0, 255);
    colors[static_cast<int>(Colors::kGridRed)]         = IM_COL32(255, 0, 0, 255);
    colors[static_cast<int>(Colors::kSelectionBorder)] = IM_COL32(0, 0, 200, 255);
    colors[static_cast<int>(Colors::kSelection)]       = IM_COL32(0, 0, 100, 80);
    colors[static_cast<int>(Colors::kBoundBox)]        = IM_COL32(100, 100, 100, 150);
    colors[static_cast<int>(Colors::kFillerColor)]     = IM_COL32(255, 255, 255, 255);
    colors[static_cast<int>(Colors::kScrollBarColor)]  = IM_COL32(200, 200, 200, 255);
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
    spdlog::info("Enable Dynamic Loading: {0}", (uint32_t)m_use_horizontal_rendering);
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
