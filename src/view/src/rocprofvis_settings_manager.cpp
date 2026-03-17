// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_settings_manager.h"
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
    IM_COL32(45, 50, 58, 255),     // kMetaDataColor
    IM_COL32(35, 40, 46, 255),     // kMetaDataColorSelected
    IM_COL32(55, 62, 70, 255),     // kMetaDataSeparator
    IM_COL32(0, 0, 0, 0),          // kTransparent
    IM_COL32(255, 107, 107, 255),  // kTextError
    IM_COL32(78, 195, 185, 255),   // kTextSuccess
    IM_COL32(105, 112, 122, 255),  // kFlameChartColor
    IM_COL32(105, 112, 122, 35),   // kGridColor
    IM_COL32(235, 98, 98, 255),    // kGridRed
    IM_COL32(245, 135, 135, 255),  // kSelectionBorder
    IM_COL32(235, 98, 98, 50),     // kSelection
    IM_COL32(105, 112, 122, 255),  // kBoundBox
    IM_COL32(20, 22, 28, 255),     // kFillerColor
    IM_COL32(55, 62, 72, 255),     // kScrollBarColor
    IM_COL32(235, 98, 98, 60),     // kHighlightChart
    IM_COL32(42, 48, 55, 255),     // kRulerBgColor
    IM_COL32(235, 242, 235, 255),  // kRulerTextColor
    IM_COL32(235, 242, 235, 255),  // kScrubberNumberColor
    IM_COL32(235, 98, 98, 145),    // kArrowColor
    IM_COL32(48, 55, 62, 255),     // kBorderColor
    IM_COL32(55, 62, 70, 255),     // kSplitterColor
    IM_COL32(22, 25, 30, 255),     // kBgMain
    IM_COL32(28, 32, 38, 255),     // kBgPanel
    IM_COL32(35, 40, 46, 255),     // kBgFrame
    IM_COL32(235, 98, 98, 255),    // kAccentRed
    IM_COL32(245, 135, 135, 255),  // kAccentRedHover
    IM_COL32(195, 75, 78, 255),    // kAccentRedActive
    IM_COL32(215, 85, 85, 255),    // kTabAccent
    IM_COL32(235, 115, 115, 255),  // kTabAccentHover
    IM_COL32(185, 65, 68, 255),    // kTabAccentActive
    IM_COL32(52, 58, 66, 255),     // kBorderGray
    IM_COL32(235, 242, 235, 255),  // kTextMain
    IM_COL32(128, 135, 142, 255),  // kTextDim
    IM_COL32(28, 32, 38, 255),     // kScrollBg
    IM_COL32(55, 62, 72, 255),     // kScrollGrab
    IM_COL32(42, 48, 55, 255),     // kTableHeaderBg
    IM_COL32(55, 62, 70, 255),     // kTableBorderStrong
    IM_COL32(42, 48, 55, 255),     // kTableBorderLight
    IM_COL32(30, 35, 42, 255),     // kTableRowBg
    IM_COL32(35, 40, 46, 255),     // kTableRowBgAlt
    IM_COL32(235, 108, 95, 170),   // kEventHighlight
    IM_COL32(72, 188, 180, 95),    // kLineChartColor
    IM_COL32(55, 62, 72, 255),     // kButton
    IM_COL32(72, 78, 88, 255),     // kButtonHovered
    IM_COL32(88, 95, 105, 255),    // kButtonActive
    IM_COL32(215, 195, 72, 255),   // kBgWarning
    IM_COL32(165, 55, 55, 255),    // kBgError
    IM_COL32(45, 145, 135, 255),   // kBgSuccess
    IM_COL32(35, 40, 45, 255),     // kStickyNoteYellow
    IM_COL32(55, 155, 152, 180),   // Colors::kLineChartColorAlt
    IM_COL32(235, 98, 98, 35),     // Colors::kTrackWarningBand
    IM_COL32(30, 35, 42, 255),     // Colors::kMinimapBin1
    IM_COL32(52, 38, 42, 255),     // Colors::kMinimapBin2
    IM_COL32(82, 42, 45, 255),     // Colors::kMinimapBin3
    IM_COL32(118, 52, 52, 255),    // Colors::kMinimapBin4
    IM_COL32(158, 65, 62, 255),    // Colors::kMinimapBin5
    IM_COL32(195, 82, 78, 255),    // Colors::kMinimapBin6
    IM_COL32(228, 98, 92, 255),    // Colors::kMinimapBin7
    IM_COL32(42, 48, 55, 255),     // Colors::kMinimapBinCounter1
    IM_COL32(58, 65, 75, 255),     // Colors::kMinimapBinCounter2
    IM_COL32(78, 85, 95, 255),     // Colors::kMinimapBinCounter3
    IM_COL32(102, 108, 118, 255),  // Colors::kMinimapBinCounter4
    IM_COL32(128, 135, 145, 255),  // Colors::kMinimapBinCounter5
    IM_COL32(158, 165, 172, 255),  // Colors::kMinimapBinCounter6
    IM_COL32(192, 198, 205, 255),  // Colors::kMinimapBinCounter7
    IM_COL32(0, 0, 0, 255),        // Colors::kMinimapBg
    IM_COL32(0, 0, 0, 155),        // Colors::kLoadingScreenColor
    IM_COL32(255, 255, 255, 255),  // Colors::kTextOnAccent

    // This must follow the ordering of Colors enum.
};
constexpr std::array LIGHT_THEME_COLORS = {
    IM_COL32(225, 222, 205, 255),  // kMetaDataColor
    IM_COL32(218, 215, 198, 255),  // kMetaDataColorSelected
    IM_COL32(208, 205, 188, 255),  // kMetaDataSeparator
    IM_COL32(0, 0, 0, 0),          // kTransparent
    IM_COL32(218, 48, 55, 255),    // kTextError
    IM_COL32(55, 148, 78, 255),    // kTextSuccess
    IM_COL32(178, 175, 162, 255),  // kFlameChartColor
    IM_COL32(178, 175, 162, 45),   // kGridColor
    IM_COL32(218, 48, 55, 255),    // kGridRed
    IM_COL32(218, 48, 55, 255),    // kSelectionBorder
    IM_COL32(218, 48, 55, 28),     // kSelection
    IM_COL32(178, 175, 162, 155),  // kBoundBox
    IM_COL32(238, 235, 222, 255),  // kFillerColor
    IM_COL32(208, 204, 188, 255),  // kScrollBarColor
    IM_COL32(218, 48, 55, 75),     // kHighlightChart
    IM_COL32(232, 228, 215, 255),  // kRulerBgColor
    IM_COL32(48, 45, 35, 255),     // kRulerTextColor
    IM_COL32(58, 55, 45, 255),     // kScrubberNumberColor
    IM_COL32(218, 48, 55, 140),    // kArrowColor
    IM_COL32(212, 208, 192, 255),  // kBorderColor
    IM_COL32(208, 204, 188, 255),  // kSplitterColor
    IM_COL32(238, 235, 222, 255),  // kBgMain
    IM_COL32(232, 228, 215, 255),  // kBgPanel
    IM_COL32(225, 222, 208, 255),  // kBgFrame
    IM_COL32(218, 48, 55, 255),    // kAccentRed
    IM_COL32(238, 85, 85, 255),    // kAccentRedHover
    IM_COL32(185, 38, 45, 255),    // kAccentRedActive
    IM_COL32(218, 48, 55, 255),    // kTabAccent
    IM_COL32(238, 85, 85, 255),    // kTabAccentHover
    IM_COL32(185, 38, 45, 255),    // kTabAccentActive
    IM_COL32(215, 212, 198, 255),  // kBorderGray
    IM_COL32(48, 45, 35, 255),     // kTextMain
    IM_COL32(108, 105, 88, 255),   // kTextDim
    IM_COL32(232, 228, 215, 255),  // kScrollBg
    IM_COL32(198, 195, 178, 255),  // kScrollGrab
    IM_COL32(232, 228, 215, 255),  // kTableHeaderBg
    IM_COL32(208, 204, 188, 255),  // kTableBorderStrong
    IM_COL32(218, 215, 200, 255),  // kTableBorderLight
    IM_COL32(238, 235, 222, 255),  // kTableRowBg
    IM_COL32(232, 228, 215, 255),  // kTableRowBgAlt
    IM_COL32(218, 68, 58, 162),    // kEventHighlight
    IM_COL32(38, 138, 158, 92),    // kLineChartColor
    IM_COL32(215, 212, 198, 255),  // kButton
    IM_COL32(202, 198, 182, 255),  // kButtonHovered
    IM_COL32(188, 185, 168, 255),  // kButtonActive
    IM_COL32(222, 165, 35, 255),   // kBgWarning
    IM_COL32(218, 85, 78, 255),    // kBgError
    IM_COL32(58, 165, 82, 255),    // kBgSuccess
    IM_COL32(225, 220, 198, 255),  // kStickyNoteYellow
    IM_COL32(25, 108, 128, 195),   // Colors::kLineChartColorAlt
    IM_COL32(218, 48, 55, 32),     // Colors::kTrackWarningBand
    IM_COL32(225, 222, 208, 255),  // Colors::kMinimapBin1
    IM_COL32(225, 215, 185, 255),  // Colors::kMinimapBin2
    IM_COL32(228, 195, 148, 255),  // Colors::kMinimapBin3
    IM_COL32(228, 162, 95, 255),   // Colors::kMinimapBin4
    IM_COL32(225, 118, 62, 255),   // Colors::kMinimapBin5
    IM_COL32(222, 82, 58, 255),    // Colors::kMinimapBin6
    IM_COL32(218, 48, 48, 255),    // Colors::kMinimapBin7
    IM_COL32(215, 212, 198, 255),  // Colors::kMinimapBinCounter1
    IM_COL32(198, 195, 178, 255),  // Colors::kMinimapBinCounter2
    IM_COL32(178, 175, 158, 255),  // Colors::kMinimapBinCounter3
    IM_COL32(155, 152, 138, 255),  // Colors::kMinimapBinCounter4
    IM_COL32(132, 128, 115, 255),  // Colors::kMinimapBinCounter5
    IM_COL32(108, 105, 92, 255),   // Colors::kMinimapBinCounter6
    IM_COL32(82, 78, 68, 255),     // Colors::kMinimapBinCounter7
    IM_COL32(235, 232, 218, 255),  // Colors::kMinimapBg
    IM_COL32(0, 0, 0, 48),         // Colors::kLoadingScreenColor
    IM_COL32(255, 255, 255, 255),  // Colors::kTextOnAccent

    // This must follow the ordering of Colors enum.
};
const std::vector<ImU32> DARK_FLAME_COLORS = {
    IM_COL32(235, 98, 98, 215),   IM_COL32(72, 188, 180, 215),
    IM_COL32(185, 168, 55, 215),  IM_COL32(68, 152, 98, 215),
    IM_COL32(185, 115, 72, 215),  IM_COL32(52, 138, 142, 215),
    IM_COL32(108, 128, 52, 215),  IM_COL32(178, 95, 105, 215)
};
const std::vector<ImU32> LIGHT_FLAME_COLORS = {
    IM_COL32(58, 155, 92, 220),   IM_COL32(218, 48, 55, 220),
    IM_COL32(222, 152, 15, 220),  IM_COL32(38, 138, 158, 220),
    IM_COL32(185, 178, 148, 220), IM_COL32(138, 155, 62, 220),
    IM_COL32(42, 148, 135, 220),  IM_COL32(225, 108, 55, 220)
};
inline constexpr const char* FLAME_DARK_COLORMAP_NAME    = "flame_dark";
inline constexpr const char* FLAME_LIGHT_COLORMAP_NAME   = "flame_light";
inline constexpr const char* CONTRAST_DARK_COLORMAP_NAME = "contrast_dark";
inline constexpr const char* CONTRAST_LIGHT_COLORMAP_NAME = "contrast_light";
inline constexpr const char*  SETTINGS_FILE_NAME = "settings_application.json";
inline constexpr float        EVENT_LEVEL_HEIGHT = 40.0f;
inline constexpr float        COMPACT_EVENT_HEIGHT = 6.0f;

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
    style.Colors[ImGuiCol_ScrollbarBg]   = scrollBg;
    style.Colors[ImGuiCol_ScrollbarGrab] = scrollGrab;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] =
        ImVec4(72.0f / 255.0f, 78.0f / 255.0f, 88.0f / 255.0f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] =
        ImVec4(88.0f / 255.0f, 95.0f / 255.0f, 105.0f / 255.0f, 1.0f);

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
    style.ScrollbarRounding = 8.0f;
    style.FramePadding  = ImVec2(10, 6);
    style.ItemSpacing   = ImVec2(10, 8);
    style.WindowPadding = ImVec2(4, 4);
    style.ChildRounding = 6.0f;
 
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

}  // namespace View
}  // namespace RocProfVis
