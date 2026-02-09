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
    IM_COL32(52, 54, 58, 255),     // kMetaDataColor
    IM_COL32(44, 46, 50, 255),     // kMetaDataColorSelected
    IM_COL32(68, 70, 74, 255),     // kMetaDataSeparator
    IM_COL32(0, 0, 0, 0),          // kTransparent
    IM_COL32(224, 62, 62, 255),    // kTextError
    IM_COL32(90, 200, 120, 255),   // kTextSuccess
    IM_COL32(200, 200, 210, 255),  // kFlameChartColor
    IM_COL32(180, 180, 190, 60),   // kGridColor
    IM_COL32(224, 62, 62, 255),    // kGridRed
    IM_COL32(255, 120, 120, 255),  // kSelectionBorder
    IM_COL32(224, 62, 62, 100),    // kSelection
    IM_COL32(170, 170, 180, 255),  // kBoundBox
    IM_COL32(36, 38, 42, 255),     // kFillerColor
    IM_COL32(90, 90, 100, 255),    // kScrollBarColor
    IM_COL32(255, 140, 140, 120),  // kHighlightChart
    IM_COL32(62, 64, 68, 255),     // kRulerBgColor
    IM_COL32(220, 220, 220, 255),  // kRulerTextColor
    IM_COL32(220, 220, 220, 255),  // kScrubberNumberColor
    IM_COL32(224, 62, 62, 180),    // kArrowColor
    IM_COL32(62, 64, 68, 255),     // kBorderColor
    IM_COL32(90, 90, 100, 255),    // kSplitterColor
    IM_COL32(28, 30, 34, 255),     // kBgMain
    IM_COL32(38, 40, 44, 255),     // kBgPanel
    IM_COL32(52, 54, 58, 255),     // kBgFrame
    IM_COL32(224, 62, 62, 255),    // kAccentRed
    IM_COL32(255, 140, 140, 255),  // kAccentRedHover
    IM_COL32(181, 40, 40, 255),    // kAccentRedActive
    IM_COL32(80, 80, 90, 255),     // kBorderGray
    IM_COL32(235, 235, 240, 255),  // kTextMain
    IM_COL32(170, 170, 180, 255),  // kTextDim
    IM_COL32(52, 54, 58, 255),     // kScrollBg
    IM_COL32(100, 100, 110, 255),  // kScrollGrab
    IM_COL32(80, 80, 90, 255),     // kTableHeaderBg
    IM_COL32(100, 100, 110, 255),  // kTableBorderStrong
    IM_COL32(62, 64, 68, 255),     // kTableBorderLight
    IM_COL32(52, 54, 58, 255),     // kTableRowBg
    IM_COL32(58, 60, 64, 255),     // kTableRowBgAlt
    IM_COL32(0, 200, 255, 160),    // kEventHighlight
    IM_COL32(255, 160, 40, 180),   // kSearchHighlight
    IM_COL32(235, 235, 240, 69),  // kLineChartColor
    IM_COL32(100, 100, 110, 255),  // kButton
    IM_COL32(130, 130, 140, 255),  // kButtonHovered
    IM_COL32(160, 160, 170, 255),  // kButtonActive
    IM_COL32(180, 160, 60, 255),   // kBgWarning
    IM_COL32(160, 60, 60, 255),    // kBgError
    IM_COL32(60, 160, 60, 255),    // kBgSuccess
    IM_COL32(60, 80, 100, 255),    // kStickyNoteYellow
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

    // This must follow the ordering of Colors enum.
};
constexpr std::array LIGHT_THEME_COLORS = {
    IM_COL32(252, 250, 248, 255),  // Colors::kMetaDataColor
    IM_COL32(242, 235, 230, 255),  // Colors::kMetaDataColorSelected
    IM_COL32(225, 220, 215, 255),  // Colors::kMetaDataSeparator
    IM_COL32(0, 0, 0, 0),          // Colors::kTransparent
    IM_COL32(242, 90, 70, 255),    // Colors::kTextError
    IM_COL32(60, 170, 60, 255),    // Colors::kTextSuccess
    IM_COL32(170, 140, 120, 255),  // Colors::kFlameChartColor
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
    IM_COL32(0, 140, 200, 180),    // Colors::kEventHighlight
    IM_COL32(220, 130, 20, 200),   // Colors::kSearchHighlight
    IM_COL32(0, 0, 0, 69),        // Colors::kLineChartColor
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
    IM_COL32(0, 0, 0, 60),        //Colors::kLoadingScreenColor


    // This must follow the ordering of Colors enum.
};
const std::vector<ImU32> FLAME_COLORS = {
    IM_COL32(0, 114, 188, 204),   IM_COL32(0, 158, 115, 204),
    IM_COL32(240, 228, 66, 204),  IM_COL32(204, 121, 167, 204),
    IM_COL32(86, 180, 233, 204),  IM_COL32(213, 94, 0, 204),
    IM_COL32(0, 204, 102, 204),   IM_COL32(230, 159, 0, 204),
    IM_COL32(153, 153, 255, 204), IM_COL32(255, 153, 51, 204)
};
inline constexpr const char*  SETTINGS_FILE_NAME = "settings_application.json";
inline constexpr size_t       RECENT_FILES_LIMIT = 5;
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
SettingsManager::GetColorWheel()
{
    return FLAME_COLORS;
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

    std::vector<ImU32> colormap;
    for(const ImU32& flame_color : GetColorWheel())
    {
        colormap.push_back(255 << IM_COL32_A_SHIFT | flame_color);
    }
    colormap.push_back(IM_COL32(220, 50, 50, 255));
    ImPlot::AddColormap("flame", colormap.data(), colormap.size());
    colormap = { IM_COL32(255, 255, 255, 255), IM_COL32(255, 255, 255, 255) };
    ImPlot::AddColormap("white", colormap.data(), colormap.size());
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
    if(m_internalsettings.recent_files.size() > RECENT_FILES_LIMIT)
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
