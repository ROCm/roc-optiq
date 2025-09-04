// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_settings_manager.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_panel.h"
#include "rocprofvis_core.h"
#include "imgui.h"
#include "implot.h"
#include <cstdlib>
#include <fstream>

namespace RocProfVis
{
namespace View
{

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
    colors[static_cast<int>(Colors::kBgFrame)]             = IM_COL32(38, 38, 38, 255);
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
    colors[static_cast<int>(Colors::kBgWarning)] = IM_COL32(100, 100, 10, 255);
    colors[static_cast<int>(Colors::kBgError)]   = IM_COL32(100, 10, 10, 255);
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
    colors[static_cast<int>(Colors::kBgFrame)]             = IM_COL32(240, 238, 232, 255);
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
    colors[static_cast<int>(Colors::kBgWarning)] = IM_COL32(250, 250, 100, 255);
    colors[static_cast<int>(Colors::kBgError)]   = IM_COL32(250, 100, 100, 255);
    return colors;
}();

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

void
SettingsManager::DarkMode()
{
    m_color_store = DARK_THEME_COLORS;
    ImGui::StyleColorsDark();
    ImPlot::StyleColorsDark();
    ApplyColorStyling();
    m_display_settings_current.use_dark_mode = true;
}

void
SettingsManager::LightMode()
{
    m_color_store = LIGHT_THEME_COLORS;
    ImGui::StyleColorsLight();
    ImPlot::StyleColorsLight();
    ApplyColorStyling();
    m_display_settings_current.use_dark_mode = false;
}

bool
SettingsManager::IsDarkMode() const
{
    return m_display_settings_current.use_dark_mode;
}

FontManager&
SettingsManager::GetFontManager()
{
    return m_font_manager;
}

void
SettingsManager::SerializeDisplaySettings(jt::Json&              parent,
                                          const DisplaySettings& settings)
{
    jt::Json display_settings;
    display_settings.setObject();
    display_settings["dpi"]               = settings.dpi;
    display_settings["use_dark_mode"]     = settings.use_dark_mode;
    display_settings["dpi_based_scaling"] = settings.dpi_based_scaling;
    display_settings["font_size_index"]   = settings.font_size_index;
    parent["display_settings"]            = display_settings;
}

bool
SettingsManager::DeserializeDisplaySettings(jt::Json&        saved_results,
                                            DisplaySettings& saved_settings)
{
    if(saved_results.contains("display_settings") &&
       saved_results["display_settings"].isObject())
    {
        jt::Json& ds = saved_results["display_settings"];

        if(ds.contains("dpi"))
        {
            if(ds["dpi"].isDouble())
                saved_settings.dpi = static_cast<float>(ds["dpi"].getDouble());
            else if(ds["dpi"].isLong())
                saved_settings.dpi = static_cast<float>(ds["dpi"].getLong());
            else
                saved_settings.dpi = 1.0f;
        }

        if(ds.contains("use_dark_mode") && ds["use_dark_mode"].isBool())
            saved_settings.use_dark_mode = ds["use_dark_mode"].getBool();
        else
            saved_settings.use_dark_mode = false;

        if(ds.contains("dpi_based_scaling") && ds["dpi_based_scaling"].isBool())
            saved_settings.dpi_based_scaling = ds["dpi_based_scaling"].getBool();
        else
            saved_settings.dpi_based_scaling = true;

        if(ds.contains("font_size_index") && ds["font_size_index"].isLong())
            saved_settings.font_size_index =
                static_cast<int>(ds["font_size_index"].getLong());
        else
            saved_settings.font_size_index = 6;  // Default to 12pt font size.

        return true;
    }
    return false;
}

void
SettingsManager::SaveSettings(const std::string&     filename,
                              const DisplaySettings& settings)
{
    jt::Json parent;
    parent.setObject();

    jt::Json settings_json;
    settings_json.setObject();
    settings_json["version"] = "1.0";
    SerializeDisplaySettings(settings_json, settings);

    parent["settings"] = settings_json;

    std::filesystem::path out_path = GetStandardConfigPath(filename);
    std::ofstream         out_file(out_path);
    if(out_file.is_open())
    {
        out_file << parent.toStringPretty();
        out_file.close();
    }
}

void
SettingsManager::LoadSettings(const std::string& filename)
{
    std::filesystem::path in_path = GetStandardConfigPath(filename);
    std::ifstream         in_file(in_path);
    if(!in_file.is_open()) return;

    std::string json_str((std::istreambuf_iterator<char>(in_file)),
                         std::istreambuf_iterator<char>());
    in_file.close();

    auto result = jt::Json::parse(json_str);
    if(result.first != jt::Json::success || !result.second.isObject()) return;

    DisplaySettings loaded_settings = m_display_settings_current;

    if(result.second.contains("settings") && result.second["settings"].isObject())
    {
        jt::Json& settings_json = result.second["settings"];

        if(DeserializeDisplaySettings(settings_json, loaded_settings))
            SetDisplaySettings(loaded_settings);
    }
    else
    {
        spdlog::warn("Settings file failed to load");
    }
}

std::filesystem::path
SettingsManager::GetStandardConfigPath(const std::string& filename)
{
#ifdef _WIN32
    const char*           appdata = std::getenv("APPDATA");
    std::filesystem::path config_dir =
        appdata ? appdata : std::filesystem::current_path();
    config_dir /= "rocprofvis";
#else
    const char*           xdg_config = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path config_dir =
        xdg_config ? xdg_config
                   : (std::filesystem::path(std::getenv("HOME")) / ".config");
    config_dir /= "rocprofvis";
#endif
    std::filesystem::create_directories(config_dir);
    return config_dir / filename;
}

void
SettingsManager::SetDPI(float DPI)
{
    m_display_settings_current.dpi = DPI;
}

float
SettingsManager ::GetDPI()
{
    return m_display_settings_current.dpi;
}
void
SettingsManager::SetDisplaySettings(const DisplaySettings& settings)
{
    bool font_changed =
        (m_display_settings_current.font_size_index != settings.font_size_index) ||
        (m_display_settings_current.dpi_based_scaling != settings.dpi_based_scaling);

    if(m_display_settings_current.use_dark_mode != settings.use_dark_mode)
    {
        if(settings.use_dark_mode)
            DarkMode();
        else
            LightMode();
    }

    m_display_settings_current = settings;

    if(font_changed)
    {
        if(m_display_settings_current.dpi_based_scaling)
        {
            int ideal_dpi_index =
                GetFontManager().GetFontSizeIndexForDPI(m_display_settings_current.dpi);
            GetFontManager().SetFontSize(ideal_dpi_index);
        }
        else
        {
            GetFontManager().SetFontSize(m_display_settings_current.font_size_index);
        }
    }
}

ImU32
SettingsManager::GetColor(int value) const
{
    return m_color_store[value];
}

ImU32
SettingsManager::GetColor(Colors color) const
{
    return m_color_store[static_cast<int>(color)];
}

const std::vector<ImU32>&
SettingsManager::GetColorWheel()
{
    return m_flame_color_wheel;
}
bool
SettingsManager::IsDPIBasedScaling() const
{
    return m_display_settings_current.dpi_based_scaling;
}

void
SettingsManager::SetDPIBasedScaling(bool enabled)
{
    m_display_settings_current.dpi_based_scaling = enabled;
}
SettingsManager::SettingsManager()
: m_color_store(static_cast<int>(Colors::__kLastColor))
, m_flame_color_wheel({

      IM_COL32(0, 114, 188, 204), IM_COL32(0, 158, 115, 204), IM_COL32(240, 228, 66, 204),
      IM_COL32(204, 121, 167, 204), IM_COL32(86, 180, 233, 204),
      IM_COL32(213, 94, 0, 204), IM_COL32(0, 204, 102, 204), IM_COL32(230, 159, 0, 204),
      IM_COL32(153, 153, 255, 204), IM_COL32(255, 153, 51, 204) })
, m_display_settings_initial({ 1.5, false, true, 6 })
, m_display_settings_current(m_display_settings_initial)
{}

SettingsManager::~SettingsManager() {}

bool
SettingsManager::Init()
{
    InitStyling();
    LightMode();
    return m_font_manager.Init();
}

DisplaySettings&
SettingsManager::GetCurrentDisplaySettings()
{
    return m_display_settings_current;
}
DisplaySettings&
SettingsManager::GetInitialDisplaySettings()
{
    return m_display_settings_initial;
}
void
SettingsManager::RestoreDisplaySettings(const DisplaySettings& settings)
{
    SetDPI(settings.dpi);
    SetDPIBasedScaling(settings.dpi_based_scaling);

    if(settings.use_dark_mode)
        DarkMode();
    else
        LightMode();

    if(settings.dpi_based_scaling)
    {
        int dpi_index = m_font_manager.GetFontSizeIndexForDPI(settings.dpi);
        m_font_manager.SetFontSize(dpi_index);
        m_display_settings_current.font_size_index = dpi_index;
    }
    else
    {
        m_font_manager.SetFontSize(settings.font_size_index);
        m_display_settings_current.font_size_index = settings.font_size_index;
    }
}

void
SettingsManager::InitStyling()
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
