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
    colors[static_cast<int>(Colors::kFlameChartColor)] = IM_COL32(160, 160, 160, 255);
    colors[static_cast<int>(Colors::kGridColor)]       = IM_COL32(255, 255, 255, 255);
    colors[static_cast<int>(Colors::kGridRed)]         = IM_COL32(220, 38, 38, 255);
    colors[static_cast<int>(Colors::kSelectionBorder)] = IM_COL32(255, 71, 87, 255);
    colors[static_cast<int>(Colors::kHighlightChart)]  = IM_COL32(255, 71, 87, 100);
    colors[static_cast<int>(Colors::kSelection)]       = IM_COL32(220, 38, 38, 80);
    colors[static_cast<int>(Colors::kBoundBox)]        = IM_COL32(160, 160, 160, 255);
    colors[static_cast<int>(Colors::kFillerColor)]     = IM_COL32(18, 18, 18, 255);
    colors[static_cast<int>(Colors::kScrollBarColor)]  = IM_COL32(64, 64, 64, 255);
    colors[static_cast<int>(Colors::kRulerBgColor)]    = IM_COL32(40, 40, 40, 255);
    colors[static_cast<int>(Colors::kBorderColor)]     = IM_COL32(40, 40, 40, 255);
    colors[static_cast<int>(Colors::kSplitterColor)]   = IM_COL32(64, 64, 64, 255);
    colors[static_cast<int>(Colors::kArrowColor)]      = IM_COL32(220, 38, 38, 180);

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

    ImGuiStyle& style = ImGui::GetStyle();

    ImVec4 bgMain          = ImVec4(0.07f, 0.07f, 0.07f, 1.0f);
    ImVec4 bgPanel         = ImVec4(0.11f, 0.11f, 0.11f, 1.0f);
    ImVec4 accentRed       = ImVec4(0.86f, 0.15f, 0.15f, 1.0f);
    ImVec4 accentRedHover  = ImVec4(1.00f, 0.28f, 0.34f, 1.0f);
    ImVec4 accentRedActive = ImVec4(0.71f, 0.12f, 0.12f, 1.0f);
    ImVec4 borderGray      = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
    ImVec4 textMain        = ImVec4(1.00f, 1.00f, 1.00f, 1.0f);
    ImVec4 textDim         = ImVec4(0.63f, 0.63f, 0.63f, 1.0f);
    ImVec4 scrollBg        = ImVec4(0.13f, 0.13f, 0.13f, 1.0f);
    ImVec4 scrollGrab      = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);

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
    style.Colors[ImGuiCol_TableHeaderBg] =
        ImVec4(0.16f, 0.16f, 0.16f, 1.0f);  // Slightly lighter than panel
    style.Colors[ImGuiCol_TableBorderStrong] =
        ImVec4(0.20f, 0.20f, 0.20f, 1.0f);  // Soft border
    style.Colors[ImGuiCol_TableBorderLight] =
        ImVec4(0.13f, 0.13f, 0.13f, 1.0f);  // Even softer
    style.Colors[ImGuiCol_TableRowBg] =
        ImVec4(0.13f, 0.13f, 0.13f, 1.0f);  // Alternating row background
    style.Colors[ImGuiCol_TableRowBgAlt] =
        ImVec4(0.15f, 0.15f, 0.15f, 1.0f);  // Slightly lighter for alt rows

    style.CellPadding      = ImVec2(8, 4);  // More space in cells
    style.FrameBorderSize  = 0.0f;          // Remove thick borders
    style.WindowBorderSize = 1.0f;          // Keep window border thin
    style.TabBorderSize    = 0.0f;          // Remove tab border

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

    // Optional: tweak rounding and spacing for a modern look
    style.FrameRounding     = 8.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.WindowRounding    = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.FramePadding      = ImVec2(10, 6);
    style.ItemSpacing       = ImVec2(10, 8);
    style.WindowPadding     = ImVec2(14, 10);

    // Optional: make fonts slightly larger for readability
    ImGui::GetIO().FontGlobalScale = 1.08f;

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
