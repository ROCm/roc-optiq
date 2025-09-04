// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_font_manager.h"
#include "imgui.h"
#include "json.h"
#include <filesystem>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

typedef struct DisplaySettings
{
    float dpi;
    bool  use_dark_mode;
    bool  dpi_based_scaling;
    int   font_size_index;
} DisplaySettings;

enum class Colors
{
    kMetaDataColor,
    kMetaDataColorSelected,
    kMetaDataSeparator,
    kTransparent,
    kTextError,
    kTextSuccess,
    kFlameChartColor,
    kGridColor,
    kGridRed,
    kSelectionBorder,
    kSelection,
    kBoundBox,
    kFillerColor,
    kScrollBarColor,
    kHighlightChart,
    kRulerBgColor,
    kRulerTextColor,
    kScrubberNumberColor,
    kArrowColor,
    kBorderColor,
    kSplitterColor,
    kBgMain,
    kBgPanel,
    kBgFrame,
    kAccentRed,
    kAccentRedHover,
    kAccentRedActive,
    kBorderGray,
    kTextMain,
    kTextDim,
    kScrollBg,
    kScrollGrab,
    kTableHeaderBg,
    kTableBorderStrong,
    kTableBorderLight,
    kTableRowBg,
    kTableRowBgAlt,
    kEventHighlight,
    kLineChartColor,
    kButton,
    kButtonHovered,
    kButtonActive,
    kBgWarning,
    kBgError,
    // Used to get the size of the enum, insert new colors before this line
    __kLastColor
};

class SettingsManager
{
public:
    SettingsManager(const SettingsManager&)            = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    static SettingsManager& GetInstance();

    bool Init();

    void  SetDPI(float DPI);
    float GetDPI();

    ImU32                     GetColor(int value) const;
    ImU32                     GetColor(Colors color) const;
    const std::vector<ImU32>& GetColorWheel();

    void LoadSettings(const std::string& filename);
    void SaveSettings(const std::string& filename, const DisplaySettings& settings);
    void SerializeDisplaySettings(jt::Json& parent, const DisplaySettings& settings);
    bool DeserializeDisplaySettings(jt::Json&        saved_results,
                                    DisplaySettings& saved_settings);
    std::filesystem::path GetStandardConfigPath(const std::string& filename);

    void DarkMode();
    void LightMode();
    bool IsDarkMode() const;

    FontManager& GetFontManager();

    const ImGuiStyle& GetDefaultStyle() const { return m_default_style; }
    bool              IsDPIBasedScaling() const;
    void              SetDPIBasedScaling(bool enabled);
    DisplaySettings&  GetCurrentDisplaySettings();
    void              RestoreDisplaySettings(const DisplaySettings& settings);
    DisplaySettings&  GetInitialDisplaySettings();
    void              SetDisplaySettings(const DisplaySettings& settings);

private:
    SettingsManager();
    ~SettingsManager();

    void InitStyling();
    void ApplyColorStyling();

    std::vector<ImU32> m_color_store;
    std::vector<ImU32> m_flame_color_wheel;

    FontManager m_font_manager;
    ImGuiStyle  m_default_style;
    DisplaySettings
        m_display_settings_initial;  // Needed if you want to truly go back to factory
                                     // settings after changing settings and saving.
    DisplaySettings m_display_settings_current;
};

}  // namespace View
}  // namespace RocProfVis
