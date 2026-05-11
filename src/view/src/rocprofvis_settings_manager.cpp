// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_settings_manager.h"
#include "rocprofvis_hotkey_manager.h"
#include "imgui.h"
#include "implot.h"
#include "rocprofvis_core.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_panel.h"
#include "rocprofvis_utils.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace RocProfVis
{
namespace View
{


constexpr std::array DARK_THEME_COLORS = {
    IM_COL32(52, 54, 58, 255),     // Colors::kMetaDataColor
    IM_COL32(44, 46, 50, 255),     // Colors::kMetaDataColorSelected
    IM_COL32(68, 70, 74, 255),     // Colors::kMetaDataSeparator
    IM_COL32(0, 0, 0, 0),          // Colors::kTransparent
    IM_COL32(224, 62, 62, 255),    // Colors::kTextError
    IM_COL32(90, 200, 120, 255),   // Colors::kTextSuccess
    IM_COL32(186, 154, 160, 255),  // Colors::kFlameChartColor
    IM_COL32(180, 180, 190, 60),   // Colors::kGridColor
    IM_COL32(224, 62, 62, 255),    // Colors::kGridRed
    IM_COL32(255, 120, 120, 255),  // Colors::kSelectionBorder
    IM_COL32(224, 62, 62, 100),    // Colors::kSelection
    IM_COL32(170, 170, 180, 255),  // Colors::kBoundBox
    IM_COL32(36, 38, 42, 255),     // Colors::kFillerColor
    IM_COL32(90, 90, 100, 255),    // Colors::kScrollBarColor
    IM_COL32(255, 140, 140, 120),  // Colors::kHighlightChart
    IM_COL32(62, 64, 68, 255),     // Colors::kRulerBgColor
    IM_COL32(220, 220, 220, 255),  // Colors::kRulerTextColor
    IM_COL32(220, 220, 220, 255),  // Colors::kScrubberNumberColor
    IM_COL32(224, 62, 62, 180),    // Colors::kArrowColor
    IM_COL32(62, 64, 68, 255),     // Colors::kBorderColor
    IM_COL32(90, 90, 100, 255),    // Colors::kSplitterColor
    IM_COL32(28, 30, 34, 255),     // Colors::kBgMain
    IM_COL32(38, 40, 44, 255),     // Colors::kBgPanel
    IM_COL32(44, 46, 50, 255),     // Colors::kBgFrame
    IM_COL32(224, 62, 62, 255),    // Colors::kAccentRed
    IM_COL32(255, 140, 140, 255),  // Colors::kAccentRedHover
    IM_COL32(181, 40, 40, 255),    // Colors::kAccentRedActive
    IM_COL32(215, 85, 85, 255),    // Colors::kTabAccent
    IM_COL32(235, 115, 115, 255),  // Colors::kTabAccentHover
    IM_COL32(185, 65, 68, 255),    // Colors::kTabAccentActive
    IM_COL32(80, 80, 90, 255),     // Colors::kBorderGray
    IM_COL32(235, 235, 240, 255),  // Colors::kTextMain
    IM_COL32(170, 170, 180, 255),  // Colors::kTextDim
    IM_COL32(52, 54, 58, 255),     // Colors::kScrollBg
    IM_COL32(100, 100, 110, 255),  // Colors::kScrollGrab
    IM_COL32(80, 80, 90, 255),     // Colors::kTableHeaderBg
    IM_COL32(100, 100, 110, 255),  // Colors::kTableBorderStrong
    IM_COL32(62, 64, 68, 255),     // Colors::kTableBorderLight
    IM_COL32(52, 54, 58, 255),     // Colors::kTableRowBg
    IM_COL32(58, 60, 64, 255),     // Colors::kTableRowBgAlt
    IM_COL32(255, 128, 110, 220),  // Colors::kEventHighlight
    IM_COL32(50, 220, 100, 240),   // Colors::kEventSearchHighlight
    IM_COL32(235, 235, 240, 69),   // Colors::kLineChartColor
    IM_COL32(100, 100, 110, 255),  // Colors::kButton
    IM_COL32(130, 130, 140, 255),  // Colors::kButtonHovered
    IM_COL32(160, 160, 170, 255),  // Colors::kButtonActive
    IM_COL32(180, 160, 60, 255),   // Colors::kBgWarning
    IM_COL32(160, 60, 60, 255),    // Colors::kBgError
    IM_COL32(60, 160, 60, 255),    // Colors::kBgSuccess
    IM_COL32(60, 80, 100, 255),    // Colors::kStickyNoteYellow
    IM_COL32(230, 240, 255, 140),  // Colors::kLineChartColorAlt
    IM_COL32(255, 0, 0, 64),       // Colors::kTrackWarningBand
    IM_COL32(60, 80, 120, 255),    // Colors::kMinimapBin1
    IM_COL32(60, 0, 80, 255),      // Colors::kMinimapBin2
    IM_COL32(100, 0, 120, 255),    // Colors::kMinimapBin3
    IM_COL32(140, 20, 40, 255),    // Colors::kMinimapBin4
    IM_COL32(200, 50, 0, 255),     // Colors::kMinimapBin5
    IM_COL32(240, 120, 0, 255),    // Colors::kMinimapBin6
    IM_COL32(255, 240, 180, 255),  // Colors::kMinimapBin7
    IM_COL32(80, 80, 80, 255),     // Colors::kMinimapBinCounter1
    IM_COL32(110, 110, 110, 255),  // Colors::kMinimapBinCounter2
    IM_COL32(140, 140, 140, 255),  // Colors::kMinimapBinCounter3
    IM_COL32(170, 170, 170, 255),  // Colors::kMinimapBinCounter4
    IM_COL32(190, 190, 190, 255),  // Colors::kMinimapBinCounter5
    IM_COL32(210, 210, 210, 255),  // Colors::kMinimapBinCounter6
    IM_COL32(230, 230, 230, 255),  // Colors::kMinimapBinCounter7
    IM_COL32(0, 0, 0, 255),        // Colors::kMinimapBg
    IM_COL32(0, 0, 0, 180),        // Colors::kLoadingScreenColor
    IM_COL32(255, 255, 255, 255),  // Colors::kTextOnAccent
    IM_COL32(45, 60, 95, 255),     // Colors::kComparisonBase
    IM_COL32(30, 80, 75, 255),     // Colors::kComparisonTarget
    IM_COL32(95, 70, 30, 255),     // Colors::kComparisonLesser
    IM_COL32(70, 45, 90, 255),     // Colors::kComparisonGreater
    // This must follow the ordering of Colors enum.
};
constexpr std::array LIGHT_THEME_COLORS = {
    IM_COL32(252, 250, 248, 255),  // Colors::kMetaDataColor
    IM_COL32(242, 235, 230, 255),  // Colors::kMetaDataColorSelected
    IM_COL32(225, 220, 215, 255),  // Colors::kMetaDataSeparator
    IM_COL32(0, 0, 0, 0),          // Colors::kTransparent
    IM_COL32(242, 90, 70, 255),    // Colors::kTextError
    IM_COL32(60, 170, 60, 255),    // Colors::kTextSuccess
    IM_COL32(198, 150, 138, 255),  // Colors::kFlameChartColor
    IM_COL32(220, 210, 200, 80),   // Colors::kGridColor
    IM_COL32(242, 90, 70, 255),    // Colors::kGridRed
    IM_COL32(242, 90, 70, 255),    // Colors::kSelectionBorder
    IM_COL32(242, 90, 70, 40),     // Colors::kSelection
    IM_COL32(220, 210, 200, 180),  // Colors::kBoundBox
    IM_COL32(255, 253, 250, 255),  // Colors::kFillerColor
    IM_COL32(235, 230, 225, 255),  // Colors::kScrollBarColor
    IM_COL32(255, 160, 140, 80),   // Colors::kHighlightChart
    IM_COL32(235, 235, 235, 255),  // Colors::kRulerBgColor
    IM_COL32(0, 0, 0, 255),        // Colors::kRulerTextColor
    IM_COL32(30, 30, 30, 255),     // Colors::kScrubberNumberColor
    IM_COL32(242, 90, 70, 180),    // Colors::kArrowColor
    IM_COL32(225, 220, 215, 255),  // Colors::kBorderColor
    IM_COL32(235, 230, 225, 255),  // Colors::kSplitterColor
    IM_COL32(255, 253, 250, 255),  // Colors::kBgMain
    IM_COL32(250, 245, 240, 255),  // Colors::kBgPanel
    IM_COL32(240, 238, 232, 255),  // Colors::kBgFrame
    IM_COL32(242, 90, 70, 255),    // Colors::kAccentRed
    IM_COL32(255, 140, 120, 255),  // Colors::kAccentRedHover
    IM_COL32(255, 110, 90, 255),   // Colors::kAccentRedActive
    IM_COL32(242, 90, 70, 255),    // Colors::kTabAccent
    IM_COL32(255, 140, 120, 255),  // Colors::kTabAccentHover
    IM_COL32(255, 110, 90, 255),   // Colors::kTabAccentActive
    IM_COL32(230, 225, 220, 255),  // Colors::kBorderGray
    IM_COL32(40, 30, 25, 255),     // Colors::kTextMain
    IM_COL32(150, 130, 120, 255),  // Colors::kTextDim
    IM_COL32(250, 245, 240, 255),  // Colors::kScrollBg
    IM_COL32(230, 225, 220, 255),  // Colors::kScrollGrab
    IM_COL32(250, 245, 240, 255),  // Colors::kTableHeaderBg
    IM_COL32(230, 225, 220, 255),  // Colors::kTableBorderStrong
    IM_COL32(240, 235, 230, 255),  // Colors::kTableBorderLight
    IM_COL32(255, 253, 250, 255),  // Colors::kTableRowBg
    IM_COL32(252, 250, 248, 255),  // Colors::kTableRowBgAlt
    IM_COL32(224, 94, 78, 210),    // Colors::kEventHighlight
    IM_COL32(30, 180, 80, 230),    // Colors::kEventSearchHighlight
    IM_COL32(0, 0, 0, 69),         // Colors::kLineChartColor
    IM_COL32(230, 230, 230, 255),  // Colors::kButton
    IM_COL32(210, 210, 210, 255),  // Colors::kButtonHovered
    IM_COL32(180, 180, 180, 255),  // Colors::kButtonActive
    IM_COL32(250, 250, 100, 255),  // Colors::kBgWarning
    IM_COL32(250, 100, 100, 255),  // Colors::kBgError
    IM_COL32(100, 250, 100, 255),  // Colors::kBgSuccess
    IM_COL32(255, 235, 110, 255),  // Colors::kStickyNoteYellow
    IM_COL32(20, 30, 50, 140),     // Colors::kLineChartColorAlt
    IM_COL32(255, 0, 0, 64),       // Colors::kTrackWarningBand
    IM_COL32(180, 200, 220, 255),  // Colors::kMinimapBin1
    IM_COL32(150, 100, 180, 255),  // Colors::kMinimapBin2
    IM_COL32(180, 60, 140, 255),   // Colors::kMinimapBin3
    IM_COL32(220, 80, 80, 255),    // Colors::kMinimapBin4
    IM_COL32(240, 120, 40, 255),   // Colors::kMinimapBin5
    IM_COL32(255, 160, 60, 255),   // Colors::kMinimapBin6
    IM_COL32(255, 200, 120, 255),  // Colors::kMinimapBin7
    IM_COL32(230, 230, 230, 255),  // Colors::kMinimapBinCounter1
    IM_COL32(210, 210, 210, 255),  // Colors::kMinimapBinCounter2
    IM_COL32(190, 190, 190, 255),  // Colors::kMinimapBinCounter3
    IM_COL32(170, 170, 170, 255),  // Colors::kMinimapBinCounter4
    IM_COL32(140, 140, 140, 255),  // Colors::kMinimapBinCounter5
    IM_COL32(110, 110, 110, 255),  // Colors::kMinimapBinCounter6
    IM_COL32(80, 80, 80, 255),     // Colors::kMinimapBinCounter7
    IM_COL32(255, 255, 255, 255),  // Colors::kMinimapBg
    IM_COL32(0, 0, 0, 60),         // Colors::kLoadingScreenColor
    IM_COL32(255, 255, 255, 255),  // Colors::kTextOnAccent
    IM_COL32(180, 195, 230, 255),  // Colors::kComparisonBase
    IM_COL32(175, 220, 215, 255),  // Colors::kComparisonTarget
    IM_COL32(235, 215, 175, 255),  // Colors::kComparisonLesser
    IM_COL32(210, 190, 230, 255),  // Colors::kComparisonGreater
    // This must follow the ordering of Colors enum.
};
const std::vector<ImU32> DARK_FLAME_COLORS = {
    IM_COL32(50, 145, 210, 215),  IM_COL32(0, 158, 115, 215),
    IM_COL32(240, 228, 66, 215),  IM_COL32(204, 121, 167, 215),
    IM_COL32(86, 180, 233, 215),  IM_COL32(235, 130, 45, 215),
    IM_COL32(0, 204, 102, 215),   IM_COL32(230, 159, 0, 215),
    IM_COL32(153, 153, 255, 215), IM_COL32(255, 153, 51, 215)
};
const std::vector<ImU32> LIGHT_FLAME_COLORS = {
    IM_COL32(50, 145, 210, 220),  IM_COL32(0, 158, 115, 220),
    IM_COL32(240, 228, 66, 220),  IM_COL32(204, 121, 167, 220),
    IM_COL32(86, 180, 233, 220),  IM_COL32(235, 130, 45, 220),
    IM_COL32(0, 204, 102, 220),   IM_COL32(230, 159, 0, 220),
    IM_COL32(153, 153, 255, 220), IM_COL32(255, 153, 51, 220)
};
inline constexpr const char* FLAME_DARK_COLORMAP_NAME    = "flame_dark";
inline constexpr const char* FLAME_LIGHT_COLORMAP_NAME   = "flame_light";
inline constexpr const char* CONTRAST_DARK_COLORMAP_NAME = "contrast_dark";
inline constexpr const char* CONTRAST_LIGHT_COLORMAP_NAME = "contrast_light";
inline constexpr const char*  SETTINGS_FILE_NAME = "settings_application.json";
inline constexpr float        EVENT_LEVEL_HEIGHT = 40.0f;
inline constexpr float        COMPACT_EVENT_HEIGHT = 6.0f;
inline constexpr float        EVENT_LEVEL_PADDING = 1.0f;
inline constexpr float        EVENT_LEVEL_COMPACT_PADDING = 1.0f;

SettingsManager&
SettingsManager::GetInstance()
{
    static SettingsManager instance;
    return instance;
}

 

void
SettingsManager::ApplyColorStyling()
{
    ImGuiStyle& style = ImGui::GetStyle();

    ImVec4 bgMain    = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kBgMain));
    ImVec4 bgPanel   = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kBgPanel));
    ImVec4 bgFrame   = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kBgFrame));
    ImVec4 accentRed = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kAccentRed));
    ImVec4 accentRedHover =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kAccentRedHover));
    ImVec4 accentRedActive =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kAccentRedActive));
    ImVec4 tabAccent = ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTabAccent));
    ImVec4 tabAccentHover =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTabAccentHover));
    ImVec4 tabAccentActive =
        ImGui::ColorConvertU32ToFloat4(GetColor(Colors::kTabAccentActive));
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
    style.Colors[ImGuiCol_FrameBg]        = bgFrame;
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
    style.Colors[ImGuiCol_ScrollbarBg]          = scrollBg;
    style.Colors[ImGuiCol_ScrollbarGrab]        = scrollGrab;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = buttonHovered;
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = buttonActive;

    // Checkboxes, radio buttons
    style.Colors[ImGuiCol_CheckMark] = accentRed;

    // Slider
    style.Colors[ImGuiCol_SliderGrab]       = accentRed;
    style.Colors[ImGuiCol_SliderGrabActive] = accentRedActive;

    // Buttons
    style.Colors[ImGuiCol_Button]        = button;
    style.Colors[ImGuiCol_ButtonHovered] = buttonHovered;
    style.Colors[ImGuiCol_ButtonActive]  = accentRedActive;

    // Tabs
    style.Colors[ImGuiCol_Tab]                = bgPanel;
    style.Colors[ImGuiCol_TabHovered]         = tabAccentHover;
    style.Colors[ImGuiCol_TabActive]          = tabAccent;
    style.Colors[ImGuiCol_TabUnfocused]       = bgPanel;
    style.Colors[ImGuiCol_TabUnfocusedActive] = tabAccentActive;
    style.Colors[ImGuiCol_TabSelectedOverline] = tabAccentHover;
    style.Colors[ImGuiCol_TabDimmedSelectedOverline] = tabAccentActive;

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

FontManager&
SettingsManager::GetFontManager()
{
    return m_font_manager;
}

void
SettingsManager::SerializeDisplaySettings(jt::Json& json)
{
    jt::Json& ds = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_DISPLAY];
    ds[JSON_KEY_SETTINGS_DISPLAY_DARK_MODE] =
        m_usersettings.display_settings.use_dark_mode;
    ds[JSON_KEY_SETTINGS_DISPLAY_DPI_SCALING] =
        m_usersettings.display_settings.dpi_based_scaling;
    ds[JSON_KEY_SETTINGS_DISPLAY_FONT_SIZE] =
        m_usersettings.display_settings.font_size_index;
}

void
SettingsManager::DeserializeDisplaySettings(jt::Json& json)
{
    if(json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_DISPLAY].isObject())
    {
        jt::Json& ds = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_DISPLAY];
        if(ds[JSON_KEY_SETTINGS_DISPLAY_DARK_MODE].isBool())
        {
            m_usersettings.display_settings.use_dark_mode =
                ds[JSON_KEY_SETTINGS_DISPLAY_DARK_MODE].getBool();
        }
        if(ds[JSON_KEY_SETTINGS_DISPLAY_DPI_SCALING].isBool())
        {
            m_usersettings.display_settings.dpi_based_scaling =
                ds[JSON_KEY_SETTINGS_DISPLAY_DPI_SCALING].getBool();
        }
        if(ds[JSON_KEY_SETTINGS_DISPLAY_FONT_SIZE].isLong())
        {
            m_usersettings.display_settings.font_size_index =
                static_cast<int>(ds[JSON_KEY_SETTINGS_DISPLAY_FONT_SIZE].getLong());
        }
    }
}

void
SettingsManager::SaveSettingsJson()
{
    jt::Json settings_json;
    settings_json[JSON_KEY_VERSION] = "1.0";

    SerializeInternalSettings(settings_json);
    SerializeDisplaySettings(settings_json);
    SerializeUnitSettings(settings_json);
    SerializeOtherSettings(settings_json);
    SerializeHotkeySettings(settings_json);

    std::ofstream out_file(m_json_path);
    if(out_file.is_open())
    {
        out_file << settings_json.toStringPretty();
        out_file.close();
    }
}

void
SettingsManager::LoadSettingsJson()
{
    std::ifstream in_file(m_json_path);
    if(!in_file.is_open()) return;

    std::string json_str((std::istreambuf_iterator<char>(in_file)),
                         std::istreambuf_iterator<char>());
    in_file.close();

    std::pair<jt::Json::Status, jt::Json> result = jt::Json::parse(json_str);
    if(result.first != jt::Json::success || !result.second.isObject()) return;

    if(result.second[JSON_KEY_GROUP_SETTINGS].isObject())
    {
        DeserializeInternalSettings(result.second);
        DeserializeDisplaySettings(result.second);
        DeserializeUnitSettings(result.second);
        DeserializeOtherSettings(result.second);
        DeserializeHotkeySettings(result.second);
    }
    else
    {
        spdlog::warn("Settings file failed to load");
    }
}

std::filesystem::path
SettingsManager::GetStandardConfigPath()
{
    std::filesystem::path config_dir = get_application_config_path(true);
    return config_dir / SETTINGS_FILE_NAME;
}

void
SettingsManager::SetDPI(float dpi)
{
    m_display_dpi = dpi;
}

float
SettingsManager ::GetDPI()
{
    return m_display_dpi;
}

void
SettingsManager::ApplyUserDisplaySettings(const UserSettings& old_settings)
{
    (void) old_settings;  // currently unused
    if(m_usersettings.display_settings.use_dark_mode)
    {
        m_color_store = &DARK_THEME_COLORS;
        ImGui::StyleColorsDark();
        ImPlot::StyleColorsDark();
    }
    else
    {
        m_color_store = &LIGHT_THEME_COLORS;
        ImGui::StyleColorsLight();
        ImPlot::StyleColorsLight();
    }
    ApplyColorStyling();

    GetFontManager().SetFontSize((m_usersettings.display_settings.dpi_based_scaling)
                                     ? GetFontManager().GetDPIScaledFontIndex()
                                     : m_usersettings.display_settings.font_size_index);
}

void
SettingsManager::ApplyUserUnitSettings(const UserSettings& old_settings)
{
    // notify that time format has changed
    if(old_settings.unit_settings.time_format != m_usersettings.unit_settings.time_format)
    {
        EventManager::GetInstance()->AddEvent(
            std::make_shared<RocEvent>(static_cast<int>(RocEvents::kTimeFormatChanged)));
    }
}

ImU32
SettingsManager::GetColor(Colors color) const
{
    return (*m_color_store)[static_cast<int>(color)];
}

const std::vector<ImU32>&
SettingsManager::GetColorWheel() const
{
    return m_usersettings.display_settings.use_dark_mode ? DARK_FLAME_COLORS
                                                         : LIGHT_FLAME_COLORS;
}

const char*
SettingsManager::GetFlameColormapName() const
{
    return m_usersettings.display_settings.use_dark_mode ? FLAME_DARK_COLORMAP_NAME
                                                         : FLAME_LIGHT_COLORMAP_NAME;
}

const char*
SettingsManager::GetContrastColormapName() const
{
    return m_usersettings.display_settings.use_dark_mode ? CONTRAST_DARK_COLORMAP_NAME
                                                         : CONTRAST_LIGHT_COLORMAP_NAME;
}

SettingsManager::SettingsManager()
: m_color_store(nullptr)
, m_usersettings_default(
      { DisplaySettings{ false, true, 6 }, UnitSettings{ TimeFormat::kTimecode } })
, m_usersettings(m_usersettings_default)
, m_appwindowsettings({ AppWindowSettings{ true, true, true, true, false } })
, m_display_dpi(1.5f)
, m_json_path(GetStandardConfigPath())
{}

SettingsManager::~SettingsManager() { SaveSettingsJson(); }

bool
SettingsManager::Init()
{
    bool result = false;
    InitStyling();
    result = m_font_manager.Init();
    LoadSettingsJson();
    ApplyUserSettings(m_usersettings_default);
    return result;
}

UserSettings&
SettingsManager::GetUserSettings()
{
    return m_usersettings;
}

const UserSettings&
SettingsManager::GetDefaultUserSettings() const
{
    return m_usersettings_default;
}

void
SettingsManager::ApplyUserSettings(const UserSettings& old_settings, bool save_json)
{
    ApplyUserDisplaySettings(old_settings);
    ApplyUserUnitSettings(old_settings);
    if(save_json)
    {
        SaveSettingsJson();
    }
}

void
SettingsManager::InitStyling()
{
    ImGuiStyle& style     = ImGui::GetStyle();
    m_default_imgui_style = style;  // Store the default imgui style

    // Set sizes and rounding
    style.CellPadding       = ImVec2(10, 6);
    style.FrameBorderSize   = 0.0f;
    style.WindowBorderSize  = 1.0f;
    style.TabBorderSize     = 0.0f;
    style.FrameRounding     = 8.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.WindowRounding    = 8.0f;
    style.ScrollbarRounding = 12.0f;
    style.ScrollbarSize     = 12.0f;
    style.FramePadding  = ImVec2(10, 6);
    style.ItemSpacing   = ImVec2(10, 8);
    style.WindowPadding = ImVec2(4, 4);
    style.ChildRounding = 6.0f;
    style.PopupRounding = 6.0f;
 
    m_default_style = style;  // Store the our customized style

    const auto add_flame_colormap = [](const char* name,
                                       const std::vector<ImU32>& flame_colors) {
        std::vector<ImU32> colormap;
        colormap.reserve(flame_colors.size() + 1);
        for(const ImU32& flame_color : flame_colors)
        {
            colormap.push_back(255 << IM_COL32_A_SHIFT | flame_color);
        }
        colormap.push_back(IM_COL32(235, 98, 98, 255));
        ImPlot::AddColormap(name, colormap.data(), static_cast<int>(colormap.size()));
    };

    add_flame_colormap(FLAME_DARK_COLORMAP_NAME, DARK_FLAME_COLORS);
    add_flame_colormap(FLAME_LIGHT_COLORMAP_NAME, LIGHT_FLAME_COLORS);

    const std::vector<ImU32> contrast_dark_colormap = {
        IM_COL32(255, 255, 255, 255), IM_COL32(255, 255, 255, 255)
    };
    const std::vector<ImU32> contrast_light_colormap = {
        IM_COL32(25, 25, 25, 255), IM_COL32(25, 25, 25, 255)
    };
    ImPlot::AddColormap(CONTRAST_DARK_COLORMAP_NAME, contrast_dark_colormap.data(),
                        static_cast<int>(contrast_dark_colormap.size()));
    ImPlot::AddColormap(CONTRAST_LIGHT_COLORMAP_NAME, contrast_light_colormap.data(),
                        static_cast<int>(contrast_light_colormap.size()));
}

const ImGuiStyle&
SettingsManager::GetDefaultIMGUIStyle() const
{
    return m_default_imgui_style;
}

const ImGuiStyle&
SettingsManager::GetDefaultStyle() const
{
    return m_default_style;
}

InternalSettings&
SettingsManager::GetInternalSettings()
{
    return m_internalsettings;
}

AppWindowSettings&
SettingsManager::GetAppWindowSettings()
{
    return m_appwindowsettings;
}

void
SettingsManager::AddRecentFile(const std::string& file_path)
{
    RemoveRecentFile(file_path);
    m_internalsettings.recent_files.emplace_front(file_path);
    if(m_internalsettings.recent_files.size() > MAX_RECENT_FILES)
    {
        m_internalsettings.recent_files.pop_back();
    }
}

void
SettingsManager::RemoveRecentFile(const std::string& file_path)
{
    auto pos = std::find(m_internalsettings.recent_files.begin(),
                         m_internalsettings.recent_files.end(), file_path);
    if(pos != m_internalsettings.recent_files.end())
    {
        m_internalsettings.recent_files.erase(pos);
    }
}

void
SettingsManager::ClearRecentFiles()
{
    m_internalsettings.recent_files.clear();
}

void
SettingsManager::SerializeInternalSettings(jt::Json& json)
{
    jt::Json& is = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_INTERNAL];
    int       i  = 0;
    for(const std::string& file : m_internalsettings.recent_files)
    {
        is[JSON_KEY_SETTINGS_INTERNAL_RECENT_FILES][i++] = file;
    }
}

void
SettingsManager::DeserializeInternalSettings(jt::Json& json)
{
    jt::Json& is = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_INTERNAL];
    if(is[JSON_KEY_SETTINGS_INTERNAL_RECENT_FILES].isArray())
    {
        for(jt::Json& entry : is[JSON_KEY_SETTINGS_INTERNAL_RECENT_FILES].getArray())
        {
            if(entry.isString())
            {
                m_internalsettings.recent_files.emplace_back(entry.getString());
            }
        }
    }
}

void
SettingsManager::SerializeOtherSettings(jt::Json& json)
{
    jt::Json& os = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_OTHER];

    os[JSON_KEY_SETTINGS_DONT_ASK_BEFORE_EXIT] = m_usersettings.dont_ask_before_exit;
    os[JSON_KEY_SETTINGS_DONT_ASK_BEFORE_TAB_CLOSE] = m_usersettings.dont_ask_before_tab_closing;
}

void
SettingsManager::DeserializeOtherSettings(jt::Json& json)
{
    jt::Json& os = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_OTHER];
    if(os[JSON_KEY_SETTINGS_DONT_ASK_BEFORE_EXIT].isBool())
    {
        m_usersettings.dont_ask_before_exit =
            static_cast<bool>(os[JSON_KEY_SETTINGS_DONT_ASK_BEFORE_EXIT].getBool());
    }
    if(os[JSON_KEY_SETTINGS_DONT_ASK_BEFORE_TAB_CLOSE].isBool())
    {
        m_usersettings.dont_ask_before_tab_closing =
            static_cast<bool>(os[JSON_KEY_SETTINGS_DONT_ASK_BEFORE_TAB_CLOSE].getBool());
    }
}

void
SettingsManager::SerializeUnitSettings(jt::Json& json)
{
    jt::Json& us = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_UNITS];
    us[JSON_KEY_SETTINGS_UNITS_TIME_FORMAT] =
        static_cast<int>(m_usersettings.unit_settings.time_format);
}

void
SettingsManager::DeserializeUnitSettings(jt::Json& json)
{
    jt::Json& us = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_UNITS];
    if(us[JSON_KEY_SETTINGS_UNITS_TIME_FORMAT].isLong())
    {
        m_usersettings.unit_settings.time_format =
            static_cast<TimeFormat>(us[JSON_KEY_SETTINGS_UNITS_TIME_FORMAT].getLong());
    }
}

const float
SettingsManager::GetEventLevelHeight() const
{
    return EVENT_LEVEL_HEIGHT;
}

const float
SettingsManager::GetEventLevelCompactHeight() const
{
    return COMPACT_EVENT_HEIGHT;
}

const float
SettingsManager::GetEventLevelPadding() const
{
    return EVENT_LEVEL_PADDING;
}

const float
SettingsManager::GetEventLevelCompactPadding() const
{
    return EVENT_LEVEL_COMPACT_PADDING;
}

void
SettingsManager::SerializeHotkeySettings(jt::Json& json)
{
    auto& hk_mgr = HotkeyManager::GetInstance();
    jt::Json& hs = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_HOTKEYS];

    for(size_t i = 0; i < kHotkeyActionCount; ++i)
    {
        HotkeyActionId action_id = static_cast<HotkeyActionId>(i);
        const auto&    info      = HotkeyManager::GetActionInfo(action_id);
        HotkeyBinding  binding   = hk_mgr.GetBinding(action_id);

        if(binding.primary != info.default_binding.primary ||
           binding.alternate != info.default_binding.alternate)
        {
            jt::Json entry;
            entry["primary"]   = HotkeyManager::KeyChordToString(binding.primary);
            entry["alternate"] = HotkeyManager::KeyChordToString(binding.alternate);
            hs[info.key]       = entry;
        }
    }
}

void
SettingsManager::DeserializeHotkeySettings(jt::Json& json)
{
    jt::Json& hs = json[JSON_KEY_GROUP_SETTINGS][JSON_KEY_SETTINGS_CATEGORY_HOTKEYS];
    if(!hs.isObject())
        return;

    auto& hk_mgr = HotkeyManager::GetInstance();

    for(size_t i = 0; i < kHotkeyActionCount; ++i)
    {
        HotkeyActionId action_id = static_cast<HotkeyActionId>(i);
        const auto&    info      = HotkeyManager::GetActionInfo(action_id);
        jt::Json&      value     = hs[info.key];

        if(!value.isObject())
            continue;

        HotkeyBinding binding = info.default_binding;
        if(value["primary"].isString())
        {
            binding.primary = HotkeyManager::StringToKeyChord(
                value["primary"].getString());
        }
        if(value["alternate"].isString())
        {
            binding.alternate = HotkeyManager::StringToKeyChord(
                value["alternate"].getString());
        }

        hk_mgr.SetBinding(action_id, binding);
    }
}

void
SettingsManager::SaveHotkeySettings()
{
    SaveSettingsJson();
}

}  // namespace View
}  // namespace RocProfVis
