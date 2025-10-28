// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

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
    IM_COL32(35, 35, 35, 255),     // Colors::kMetaDataColor
    IM_COL32(28, 28, 28, 255),     // Colors::kMetaDataColorSelected
    IM_COL32(70, 70, 70, 255),     // Colors::kMetaDataSeparator
    IM_COL32(0, 0, 0, 0),          // Colors::kTransparent
    IM_COL32(220, 38, 38, 255),    // Colors::kTextError
    IM_COL32(0, 255, 0, 255),      // Colors::kTextSuccess
    IM_COL32(160, 160, 160, 255),  // Colors::kFlameChartColor
    IM_COL32(200, 200, 200, 60),   // Colors::kGridColor
    IM_COL32(220, 38, 38, 255),    // Colors::kGridRed
    IM_COL32(255, 71, 87, 255),    // Colors::kSelectionBorder
    IM_COL32(220, 38, 38, 80),     // Colors::kSelection
    IM_COL32(160, 160, 160, 255),  // Colors::kBoundBox
    IM_COL32(18, 18, 18, 255),     // Colors::kFillerColor
    IM_COL32(64, 64, 64, 255),     // Colors::kScrollBarColor
    IM_COL32(255, 71, 87, 100),    // Colors::kHighlightChart
    IM_COL32(40, 40, 40, 255),     // Colors::kRulerBgColor
    IM_COL32(250, 250, 250, 255),  // Colors::kRulerTextColor
    IM_COL32(255, 255, 255, 255),  // Colors::kScrubberNumberColor
    IM_COL32(220, 38, 38, 180),    // Colors::kArrowColor
    IM_COL32(40, 40, 40, 255),     // Colors::kBorderColor
    IM_COL32(64, 64, 64, 255),     // Colors::kSplitterColor
    IM_COL32(18, 18, 18, 255),     // Colors::kBgMain
    IM_COL32(28, 28, 28, 255),     // Colors::kBgPanel
    IM_COL32(38, 38, 38, 255),     // Colors::kBgFrame
    IM_COL32(219, 38, 38, 255),    // Colors::kAccentRed
    IM_COL32(255, 71, 87, 255),    // Colors::kAccentRedHover
    IM_COL32(181, 30, 30, 255),    // Colors::kAccentRedActive
    IM_COL32(41, 41, 41, 255),     // Colors::kBorderGray
    IM_COL32(255, 255, 255, 255),  // Colors::kTextMain
    IM_COL32(161, 161, 161, 255),  // Colors::kTextDim
    IM_COL32(33, 33, 33, 255),     // Colors::kScrollBg
    IM_COL32(64, 64, 64, 255),     // Colors::kScrollGrab
    IM_COL32(41, 41, 41, 255),     // Colors::kTableHeaderBg
    IM_COL32(51, 51, 51, 255),     // Colors::kTableBorderStrong
    IM_COL32(33, 33, 33, 255),     // Colors::kTableBorderLight
    IM_COL32(33, 33, 33, 255),     // Colors::kTableRowBg
    IM_COL32(38, 38, 38, 255),     // Colors::kTableRowBgAlt
    IM_COL32(0, 200, 255, 180),    // Colors::kEventHighlight
    IM_COL32(250, 250, 250, 255),  // Colors::kLineChartColor
    IM_COL32(60, 60, 60, 255),     // Colors::kButton
    IM_COL32(90, 90, 90, 255),     // Colors::kButtonHovered
    IM_COL32(120, 120, 120, 255),  // Colors::kButtonActive
    IM_COL32(100, 100, 10, 255),   // Colors::kBgWarning
    IM_COL32(100, 10, 10, 255),    // Colors::kBgError
    IM_COL32(30, 30, 30, 255),     // Colors::kMMBin1
    IM_COL32(0, 80, 180, 255),     // Colors::kMMBin2
    IM_COL32(0, 180, 180, 255),    // Colors::kMMBin3
    IM_COL32(0, 180, 80, 255),     // Colors::kMMBin4
    IM_COL32(220, 180, 0, 255),    // Colors::kMMBin5
    IM_COL32(255, 100, 0, 255),    // Colors::kMMBin6
    IM_COL32(220, 40, 40, 255)     // Colors::kMMBin7
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
    IM_COL32(0, 0, 0, 255),        // Colors::kLineChartColor
    IM_COL32(230, 230, 230, 255),  // Colors::kButton
    IM_COL32(210, 210, 210, 255),  // Colors::kButtonHovered
    IM_COL32(180, 180, 180, 255),  // Colors::kButtonActive
    IM_COL32(250, 250, 100, 255),  // Colors::kBgWarning
    IM_COL32(250, 100, 100, 255),  // Colors::kBgError
    IM_COL32(240, 240, 240, 255),  // Colors::kMMBin1
    IM_COL32(0, 120, 255, 255),    // Colors::kMMBin2
    IM_COL32(0, 200, 200, 255),    // Colors::kMMBin3
    IM_COL32(0, 200, 0, 255),      // Colors::kMMBin4
    IM_COL32(255, 220, 0, 255),    // Colors::kMMBin5
    IM_COL32(255, 140, 0, 255),    // Colors::kMMBin6
    IM_COL32(220, 40, 40, 255)     // Colors::kMMBin7
    // This must follow the ordering of Colors enum.
};
const std::vector<ImU32> FLAME_COLORS = {
    IM_COL32(0, 114, 188, 204),   IM_COL32(0, 158, 115, 204),
    IM_COL32(240, 228, 66, 204),  IM_COL32(204, 121, 167, 204),
    IM_COL32(86, 180, 233, 204),  IM_COL32(213, 94, 0, 204),
    IM_COL32(0, 204, 102, 204),   IM_COL32(230, 159, 0, 204),
    IM_COL32(153, 153, 255, 204), IM_COL32(255, 153, 51, 204)
};
 
constexpr const char*  SETTINGS_FILE_NAME = "settings_application.json";
constexpr size_t RECENT_FILES_LIMIT = 5;
constexpr float  EVENT_LEVEL_HEIGHT = 40.0f;

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
    (void)old_settings; //currently unused
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
    if(old_settings.unit_settings.time_format !=
       m_usersettings.unit_settings.time_format)
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
    style.WindowPadding     = ImVec2(4, 4);

    m_default_style = style;  // Store the our customized style
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

}  // namespace View
}  // namespace RocProfVis
