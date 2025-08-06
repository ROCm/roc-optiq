// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_settings.h"
#include "icons/rocprofvis_icon_data.h"
#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"
#include <cmath>

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
int
FontManager::GetFontSizeIndexForDPI(float dpi)
{
    // DPI returns the dots per inch of the display. Essentially, it is a scaling factor.
    constexpr float font_sizes[] = { 5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
                                     18, 19, 21, 23, 25, 27, 31, 35, 39, 47, 55, 63 };

    float base_size = 13.0f;
    float scaled_size =
        base_size *
        std::sqrt(dpi);  // Scale the base size by the square root of DPI. SQRT needed to
                         // prevent scaling from becoming comical on higher res monitors.

    // Find the index of the font size closest to scaled_size
    int best_index = 0;
    for(int i = 1; i < static_cast<int>(std::size(font_sizes)); ++i)
    {
        if(std::abs(font_sizes[i] - scaled_size) <
           std::abs(font_sizes[best_index] - scaled_size))
            best_index = i;
    }
    return best_index;
}

void
FontManager::SetFontSize(int size_index)
{
    constexpr int num_types = static_cast<int>(FontType::__kLastFont);

    if(num_types == 0 || m_all_fonts.empty()) return;
    if(size_index < 0 || size_index >= static_cast<int>(m_all_fonts.size())) return;

    for(int i = 0; i < num_types; ++i)
    {
        m_font_size_indices[i] = size_index;
        m_fonts[i]             = m_all_fonts[size_index];
    }

    ImGui::GetIO().FontDefault = m_fonts[static_cast<int>(FontType::kDefault)];
}
bool
FontManager::Init()
{
    ImGuiIO& io = ImGui::GetIO();
    m_fonts.clear();

    constexpr float font_sizes[] = { 5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17,
                                     18, 19, 21, 23, 25, 27, 31, 35, 39, 47, 55, 63 };

    constexpr int num_sizes = sizeof(font_sizes) / sizeof(font_sizes[0]);
    constexpr int num_types = static_cast<int>(FontType::__kLastFont);

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
    for(const char* path : font_paths)
    {
        if(std::filesystem::exists(path))
        {
            font_path = path;
            break;
        }
    }

    // Prepare storage
    m_all_fonts.clear();
    m_all_fonts.resize(num_sizes, nullptr);
    m_font_size_indices.resize(num_types, 6);  // Default to index 6 (12pt)
    m_fonts.resize(num_types);
    m_icon_fonts.resize(num_types);

    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;

    // Load all font sizes once
    for(int sz = 0; sz < num_sizes; ++sz)
    {
        ImFont* font = nullptr;
        if(font_path) font = io.Fonts->AddFontFromFileTTF(font_path, font_sizes[sz]);
        if(!font) font = io.Fonts->AddFontDefault();
        m_all_fonts[sz] = font;
    }

    // Set m_fonts to currently selected size for each FontType
    for(int type = 0; type < num_types; ++type)
    {
        int sz_idx    = m_font_size_indices[type];
        m_fonts[type] = m_all_fonts[sz_idx];

        // Load icon font for each type at the selected size
        m_icon_fonts[type] = io.Fonts->AddFontFromMemoryCompressedTTF(
            &icon_font_compressed_data, icon_font_compressed_size, font_sizes[sz_idx],
            &config, icon_ranges);
    }

    // Set ImGui's default font
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

ImFont*
FontManager::GetIconFont(FontType font_type)
{
    if(static_cast<int>(font_type) < 0 ||
       static_cast<int>(font_type) >= static_cast<int>(FontType::__kLastFont))
    {
        return nullptr;  // Invalid font type
    }
    return m_icon_fonts[static_cast<int>(font_type)];
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
    colors[static_cast<int>(Colors::kLineChartColor)]      = IM_COL32(250, 250, 250, 255);
    colors[static_cast<int>(Colors::kEventHighlight)]      = IM_COL32(0, 200, 255, 180);
    colors[static_cast<int>(Colors::kButton)] =
        IM_COL32(60, 60, 60, 255);  // Neutral dark gray
    colors[static_cast<int>(Colors::kButtonHovered)] =
        IM_COL32(90, 90, 90, 255);  // Lighter gray on hover
    colors[static_cast<int>(Colors::kButtonActive)] =
        IM_COL32(120, 120, 120, 255);  // Even lighter when active

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
    colors[static_cast<int>(Colors::kLineChartColor)]      = IM_COL32(0, 0, 0, 255);
    colors[static_cast<int>(Colors::kEventHighlight)]      = IM_COL32(0, 140, 200, 180);
    colors[static_cast<int>(Colors::kButton)] =
        IM_COL32(230, 230, 230, 255);  // Neutral light gray
    colors[static_cast<int>(Colors::kButtonHovered)] =
        IM_COL32(210, 210, 210, 255);  // Slightly darker on hover
    colors[static_cast<int>(Colors::kButtonActive)] =
        IM_COL32(180, 180, 180, 255);  // Even darker when active
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
Settings::ApplyColorStyling()
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
    ImVec4 button     = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kButton));
    ImVec4 buttonHovered =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kButtonHovered));
    ImVec4 buttonActive = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kButtonActive));

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
    style.Colors[ImGuiCol_Button]        = button;
    style.Colors[ImGuiCol_ButtonHovered] = buttonHovered;
    style.Colors[ImGuiCol_ButtonActive]  = buttonActive;

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

    return;
}

void
Settings::DarkMode()
{
    m_color_store = DARK_THEME_COLORS;
    ImGui::StyleColorsDark();
    ImPlot::StyleColorsDark();
    ApplyColorStyling();
    m_use_dark_mode = true;
}

void
Settings::LightMode()
{
    m_color_store = LIGHT_THEME_COLORS;
    ImGui::StyleColorsLight();
    ImPlot::StyleColorsLight();
    ApplyColorStyling();
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
    InitStyling();
    LightMode();
}

Settings::~Settings() {}

void
Settings::InitStyling()
{
    ImGuiStyle& style = ImGui::GetStyle();
    m_default_style   = style;  // Store the default style

    // Set sizes and rounding
    style.CellPadding       = ImVec2(10, 6);
    style.FrameBorderSize   = 0.0f;
    style.WindowBorderSize  = 1.0f;
    style.TabBorderSize     = 0.0f;
    style.FrameRounding     = 6.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.WindowRounding    = 8.0f;
    style.ScrollbarRounding = 8.0f;

    // Rounding the windows to look modern
    style.FrameRounding     = 8.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.WindowRounding    = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.FramePadding      = ImVec2(10, 6);
    style.ItemSpacing       = ImVec2(10, 8);
    style.WindowPadding     = ImVec2(14, 10);
}

}  // namespace View
}  // namespace RocProfVis
