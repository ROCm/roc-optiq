// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_settings.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"

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

bool
FontManager::Init()
{
    ImGuiIO&     io = ImGui::GetIO();
    ImFontConfig config;

    ImFont* font = nullptr;
    if(io.Fonts->Fonts.empty())
    {
        font = io.Fonts->AddFontDefault();  // Adds ProggyClean @ 13px
    }
    else
    {
        font = io.Fonts->Fonts[0];  // Assume the first one is the default
    }
    ROCPROFVIS_ASSERT(font != nullptr);

    // Store the default font in the manager also known as "Medium" font
    m_fonts.resize(static_cast<int>(FontType::__kLastFont));
    m_fonts[static_cast<int>(FontType::kDefault)] = font;

    // Small font
    config.SizePixels = 10.0f;
    font              = io.Fonts->AddFontDefault(&config);
    ROCPROFVIS_ASSERT(font != nullptr);
    m_fonts[static_cast<int>(FontType::kSmall)] = font;

    // Medium large font
    config.SizePixels = 16.0f;
    font              = io.Fonts->AddFontDefault(&config);
    ROCPROFVIS_ASSERT(font != nullptr);
    m_fonts[static_cast<int>(FontType::kMedLarge)] = font;

    // Large font
    config.SizePixels = 20.0f;
    font              = io.Fonts->AddFontDefault(&config);
    ROCPROFVIS_ASSERT(font != nullptr);
    m_fonts[static_cast<int>(FontType::kLarge)] = font;

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
    colors[static_cast<int>(Colors::kMetaDataColor)]         = IM_COL32(35, 35, 35, 255);
    colors[static_cast<int>(Colors::kMetaDataColorSelected)] = IM_COL32(28, 28, 28, 255);
    colors[static_cast<int>(Colors::kMetaDataSeparator)]     = IM_COL32(40, 40, 40, 255);
    colors[static_cast<int>(Colors::kTransparent)]           = IM_COL32(0, 0, 0, 0);
    colors[static_cast<int>(Colors::kTextError)]             = IM_COL32(220, 38, 38, 255);
    colors[static_cast<int>(Colors::kTextSuccess)]           = IM_COL32(0, 255, 0, 255);
    colors[static_cast<int>(Colors::kFlameChartColor)]     = IM_COL32(160, 160, 160, 255);
    colors[static_cast<int>(Colors::kGridColor)]           = IM_COL32(255, 255, 255, 255);
    colors[static_cast<int>(Colors::kGridRed)]             = IM_COL32(220, 38, 38, 255);
    colors[static_cast<int>(Colors::kSelectionBorder)]     = IM_COL32(255, 71, 87, 255);
    colors[static_cast<int>(Colors::kHighlightChart)]      = IM_COL32(255, 71, 87, 100);
    colors[static_cast<int>(Colors::kSelection)]           = IM_COL32(220, 38, 38, 80);
    colors[static_cast<int>(Colors::kBoundBox)]            = IM_COL32(160, 160, 160, 255);
    colors[static_cast<int>(Colors::kFillerColor)]         = IM_COL32(18, 18, 18, 255);
    colors[static_cast<int>(Colors::kScrollBarColor)]      = IM_COL32(64, 64, 64, 255);
    colors[static_cast<int>(Colors::kRulerBgColor)]        = IM_COL32(40, 40, 40, 255);
    colors[static_cast<int>(Colors::kRulerTextColor)]      = IM_COL32(250, 250, 250, 255);
    colors[static_cast<int>(Colors::kBorderColor)]         = IM_COL32(40, 40, 40, 255);
    colors[static_cast<int>(Colors::kScrubberNumberColor)] = IM_COL32(255, 255, 255, 255);
    colors[static_cast<int>(Colors::kSplitterColor)]       = IM_COL32(64, 64, 64, 255);
    colors[static_cast<int>(Colors::kArrowColor)]          = IM_COL32(220, 38, 38, 180);
    colors[static_cast<int>(Colors::kBgMain)]              = IM_COL32(18, 18, 18, 255);
    colors[static_cast<int>(Colors::kBgPanel)]             = IM_COL32(28, 28, 28, 255);
    colors[static_cast<int>(Colors::kAccentRed)]           = IM_COL32(219, 38, 38, 255);
    colors[static_cast<int>(Colors::kAccentRedHover)]      = IM_COL32(255, 71, 87, 255);
    colors[static_cast<int>(Colors::kAccentRedActive)]     = IM_COL32(181, 30, 30, 255);
    colors[static_cast<int>(Colors::kBorderGray)]          = IM_COL32(41, 41, 41, 255);
    colors[static_cast<int>(Colors::kTextMain)]            = IM_COL32(255, 255, 255, 255);
    colors[static_cast<int>(Colors::kTextDim)]             = IM_COL32(161, 161, 161, 255);
    colors[static_cast<int>(Colors::kScrollBg)]            = IM_COL32(33, 33, 33, 255);
    colors[static_cast<int>(Colors::kScrollGrab)]          = IM_COL32(64, 64, 64, 255);
    colors[static_cast<int>(Colors::kTableHeaderBg)]       = IM_COL32(41, 41, 41, 255);
    colors[static_cast<int>(Colors::kTableBorderStrong)]   = IM_COL32(51, 51, 51, 255);
    colors[static_cast<int>(Colors::kTableBorderLight)]    = IM_COL32(33, 33, 33, 255);
    colors[static_cast<int>(Colors::kTableRowBg)]          = IM_COL32(33, 33, 33, 255);
    colors[static_cast<int>(Colors::kTableRowBgAlt)]       = IM_COL32(38, 38, 38, 255);

    return colors;
}();

const std::vector<ImU32> LIGHT_THEME_COLORS = []() {
    std::vector<ImU32> colors(static_cast<int>(Colors::__kLastColor));
    colors[static_cast<int>(Colors::kMetaDataColor)] = IM_COL32(252, 250, 248, 255);
    colors[static_cast<int>(Colors::kMetaDataColorSelected)] =
        IM_COL32(242, 235, 230, 255);
    colors[static_cast<int>(Colors::kMetaDataSeparator)]   = IM_COL32(225, 220, 215, 255);
    colors[static_cast<int>(Colors::kTransparent)]         = IM_COL32(0, 0, 0, 0);
    colors[static_cast<int>(Colors::kTextError)]           = IM_COL32(242, 90, 70, 255);
    colors[static_cast<int>(Colors::kTextSuccess)]         = IM_COL32(60, 170, 60, 255);
    colors[static_cast<int>(Colors::kFlameChartColor)]     = IM_COL32(170, 140, 120, 255);
    colors[static_cast<int>(Colors::kGridColor)]           = IM_COL32(220, 210, 200, 80);
    colors[static_cast<int>(Colors::kGridRed)]             = IM_COL32(242, 90, 70, 255);
    colors[static_cast<int>(Colors::kSelectionBorder)]     = IM_COL32(242, 90, 70, 255);
    colors[static_cast<int>(Colors::kHighlightChart)]      = IM_COL32(255, 160, 140, 80);
    colors[static_cast<int>(Colors::kSelection)]           = IM_COL32(242, 90, 70, 40);
    colors[static_cast<int>(Colors::kBoundBox)]            = IM_COL32(220, 210, 200, 180);
    colors[static_cast<int>(Colors::kFillerColor)]         = IM_COL32(255, 253, 250, 255);
    colors[static_cast<int>(Colors::kScrollBarColor)]      = IM_COL32(235, 230, 225, 255);
    colors[static_cast<int>(Colors::kRulerBgColor)]        = IM_COL32(235, 235, 235, 255);
    colors[static_cast<int>(Colors::kRulerTextColor)]      = IM_COL32(0, 0, 0, 255);
    colors[static_cast<int>(Colors::kBorderColor)]         = IM_COL32(225, 220, 215, 255);
    colors[static_cast<int>(Colors::kSplitterColor)]       = IM_COL32(235, 230, 225, 255);
    colors[static_cast<int>(Colors::kArrowColor)]          = IM_COL32(242, 90, 70, 180);
    colors[static_cast<int>(Colors::kScrubberNumberColor)] = IM_COL32(30, 30, 30, 255);
    colors[static_cast<int>(Colors::kBgMain)]              = IM_COL32(255, 253, 250, 255);
    colors[static_cast<int>(Colors::kBgPanel)]             = IM_COL32(250, 245, 240, 255);
    colors[static_cast<int>(Colors::kAccentRed)]           = IM_COL32(242, 90, 70, 255);
    colors[static_cast<int>(Colors::kAccentRedHover)]      = IM_COL32(255, 140, 120, 255);
    colors[static_cast<int>(Colors::kAccentRedActive)]     = IM_COL32(255, 110, 90, 255);
    colors[static_cast<int>(Colors::kBorderGray)]          = IM_COL32(230, 225, 220, 255);
    colors[static_cast<int>(Colors::kTextMain)]            = IM_COL32(40, 30, 25, 255);
    colors[static_cast<int>(Colors::kTextDim)]             = IM_COL32(150, 130, 120, 255);
    colors[static_cast<int>(Colors::kScrollBg)]            = IM_COL32(250, 245, 240, 255);
    colors[static_cast<int>(Colors::kScrollGrab)]          = IM_COL32(230, 225, 220, 255);
    colors[static_cast<int>(Colors::kTableHeaderBg)]       = IM_COL32(250, 245, 240, 255);
    colors[static_cast<int>(Colors::kTableBorderStrong)]   = IM_COL32(230, 225, 220, 255);
    colors[static_cast<int>(Colors::kTableBorderLight)]    = IM_COL32(240, 235, 230, 255);
    colors[static_cast<int>(Colors::kTableRowBg)]          = IM_COL32(255, 253, 250, 255);
    colors[static_cast<int>(Colors::kTableRowBgAlt)]       = IM_COL32(252, 250, 248, 255);

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
Settings::Styling()
{
    ImGuiStyle& style = ImGui::GetStyle();

    ImVec4 bgMain    = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kBgMain));
    ImVec4 bgPanel   = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kBgPanel));
    ImVec4 accentRed = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kAccentRed));
    ImVec4 accentRedHover =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kAccentRedHover));
    ImVec4 accentRedActive =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kAccentRedActive));
    ImVec4 borderGray = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kBorderGray));
    ImVec4 textMain   = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTextMain));
    ImVec4 textDim    = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTextDim));
    ImVec4 scrollBg   = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kScrollBg));
    ImVec4 scrollGrab = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kScrollGrab));

    // Window
    style.Colors[ImGuiCol_WindowBg]     = bgMain;
    style.Colors[ImGuiCol_ChildBg]      = bgPanel;
    style.Colors[ImGuiCol_PopupBg]      = bgPanel;
    style.Colors[ImGuiCol_Border]       = borderGray;
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    // Frame
    style.Colors[ImGuiCol_FrameBg]        = bgPanel;
    style.Colors[ImGuiCol_FrameBgHovered] = accentRedHover;
    style.Colors[ImGuiCol_FrameBgActive]  = accentRedActive;

    // Title bar
    style.Colors[ImGuiCol_TitleBg]          = bgPanel;
    style.Colors[ImGuiCol_TitleBgActive]    = accentRed;
    style.Colors[ImGuiCol_TitleBgCollapsed] = borderGray;

    // Menu bar
    style.Colors[ImGuiCol_MenuBarBg] = bgPanel;

    // Modern table styling
    ImVec4 tableHeaderBg =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTableHeaderBg));
    ImVec4 tableBorderStrong =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTableBorderStrong));
    ImVec4 tableBorderLight =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTableBorderLight));
    ImVec4 tableRowBg = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTableRowBg));
    ImVec4 tableRowBgAlt =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTableRowBgAlt));

    style.Colors[ImGuiCol_TableHeaderBg]     = tableHeaderBg;
    style.Colors[ImGuiCol_TableBorderStrong] = tableBorderStrong;
    style.Colors[ImGuiCol_TableBorderLight]  = tableBorderLight;
    style.Colors[ImGuiCol_TableRowBg]        = tableRowBg;
    style.Colors[ImGuiCol_TableRowBgAlt]     = tableRowBgAlt;

    style.CellPadding       = ImVec2(10, 6);
    style.FrameBorderSize   = 0.0f;
    style.WindowBorderSize  = 1.0f;
    style.TabBorderSize     = 0.0f;
    style.FrameRounding     = 6.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.WindowRounding    = 8.0f;
    style.ScrollbarRounding = 8.0f;

    // Scrollbar
    style.Colors[ImGuiCol_ScrollbarBg]   = scrollBg;
    style.Colors[ImGuiCol_ScrollbarGrab] = scrollGrab;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] =
        ImVec4(96.0f / 255.0f, 96.0f / 255.0f, 96.0f / 255.0f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] =
        ImVec4(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f);

    // Checkboxes, radio buttons
    style.Colors[ImGuiCol_CheckMark] = accentRed;

    // Slider
    style.Colors[ImGuiCol_SliderGrab]       = accentRed;
    style.Colors[ImGuiCol_SliderGrabActive] = accentRedActive;

    // Buttons
    style.Colors[ImGuiCol_Button]        = accentRed;
    style.Colors[ImGuiCol_ButtonHovered] = accentRedHover;
    style.Colors[ImGuiCol_ButtonActive]  = accentRedActive;

    // Tabs
    style.Colors[ImGuiCol_Tab]                = bgPanel;
    style.Colors[ImGuiCol_TabHovered]         = accentRedHover;
    style.Colors[ImGuiCol_TabActive]          = accentRed;
    style.Colors[ImGuiCol_TabUnfocused]       = bgPanel;
    style.Colors[ImGuiCol_TabUnfocusedActive] = accentRedActive;

    // Headers (collapsing, selectable, etc)
    style.Colors[ImGuiCol_Header]        = accentRed;
    style.Colors[ImGuiCol_HeaderHovered] = accentRedHover;
    style.Colors[ImGuiCol_HeaderActive]  = accentRedActive;

    // Separator, resize grip
    style.Colors[ImGuiCol_Separator]         = borderGray;
    style.Colors[ImGuiCol_SeparatorHovered]  = accentRedHover;
    style.Colors[ImGuiCol_SeparatorActive]   = accentRedActive;
    style.Colors[ImGuiCol_ResizeGrip]        = accentRed;
    style.Colors[ImGuiCol_ResizeGripHovered] = accentRedHover;
    style.Colors[ImGuiCol_ResizeGripActive]  = accentRedActive;

    // Text
    style.Colors[ImGuiCol_Text]         = textMain;
    style.Colors[ImGuiCol_TextDisabled] = textDim;

    // Drag and drop
    style.Colors[ImGuiCol_DragDropTarget] = accentRed;

    // Navigation highlight
    style.Colors[ImGuiCol_NavHighlight] = accentRedHover;

    // Plot/graph backgrounds (if you use ImPlot)
    style.Colors[ImGuiCol_PlotLines]            = accentRed;
    style.Colors[ImGuiCol_PlotLinesHovered]     = accentRedHover;
    style.Colors[ImGuiCol_PlotHistogram]        = accentRed;
    style.Colors[ImGuiCol_PlotHistogramHovered] = accentRedHover;

    // Modal window dim
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.7f);

    // Rounding the windows to look modern
    style.FrameRounding     = 8.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.WindowRounding    = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.FramePadding      = ImVec2(10, 6);
    style.ItemSpacing       = ImVec2(10, 8);
    style.WindowPadding     = ImVec2(14, 10);

    return;
}

void
Settings::DarkMode()
{
    m_color_store = DARK_THEME_COLORS;
    ImGui::StyleColorsDark();
    ImPlot::StyleColorsDark();
    Styling();
    m_use_dark_mode = true;
}

void
Settings::LightMode()
{
    m_color_store = LIGHT_THEME_COLORS;
    ImGui::StyleColorsLight();
    ImPlot::StyleColorsLight();
    Styling();
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
