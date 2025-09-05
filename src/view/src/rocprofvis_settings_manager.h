// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "json.h"
#include "rocprofvis_font_manager.h"
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

typedef struct DisplaySettings
{
    bool use_dark_mode;
    bool dpi_based_scaling;
    int  font_size_index;
} DisplaySettings;

typedef struct UserSettings
{
    DisplaySettings display_settings;
} UserSettings;

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

constexpr char* JSON_KEY_VERSION = "version";

constexpr char* JSON_KEY_GROUP_SETTINGS            = "settings";
constexpr char* JSON_KEY_SETTINGS_CATEGORY_DISPLAY = "display_settings";

constexpr char* JSON_KEY_SETTINGS_DISPLAY_DARK_MODE   = "use_dark_mode";
constexpr char* JSON_KEY_SETTINGS_DISPLAY_DPI_SCALING = "dpi_based_scaling";
constexpr char* JSON_KEY_SETTINGS_DISPLAY_FONT_SIZE   = "font_size_index";

class SettingsManager
{
public:
    SettingsManager(const SettingsManager&)            = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    static SettingsManager& GetInstance();

    bool Init();

    // Fonts
    FontManager& GetFontManager();
    void         SetDPI(float DPI);
    float        GetDPI();

    // Styling
    ImU32                     GetColor(Colors color) const;
    const std::vector<ImU32>& GetColorWheel();
    const ImGuiStyle&         GetDefaultStyle() const;

    // User settings
    UserSettings&       GetUserSettings();
    const UserSettings& GetDefaultUserSettings() const;
    void                ApplyUserSettings();

private:
    SettingsManager();
    ~SettingsManager();

    void InitStyling();
    void ApplyColorStyling();

    void                  LoadSettingsJson();
    void                  SaveSettingsJson();
    std::filesystem::path GetStandardConfigPath();

    void SerializeDisplaySettings(jt::Json& json);
    void DeserializeDisplaySettings(jt::Json& json);
    void ApplyUserDisplaySettings();

    const std::array<ImU32, static_cast<size_t>(Colors::__kLastColor)>* m_color_store;

    FontManager m_font_manager;
    ImGuiStyle  m_default_style;
    float       m_display_dpi;

    UserSettings m_usersettings_default;
    UserSettings m_usersettings;

    std::filesystem::path m_json_path;
};

}  // namespace View
}  // namespace RocProfVis
