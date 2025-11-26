// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "json.h"
#include "rocprofvis_font_manager.h"
#include <array>
#include <filesystem>
#include <list>
#include <string>

namespace RocProfVis
{
namespace View
{

enum class TimeFormat;

typedef struct DisplaySettings
{
    bool use_dark_mode;
    bool dpi_based_scaling;
    int  font_size_index;

} DisplaySettings;

typedef struct UnitSettings
{
    TimeFormat time_format;
} UnitSettings;

typedef struct UserSettings
{
    DisplaySettings display_settings;
    UnitSettings    unit_settings;
    bool            dont_ask_before_tab_closing;
    bool            dont_ask_before_exit;
} UserSettings;

typedef struct InternalSettings
{
    std::list<std::string> recent_files;
} InternalSettings;

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
    kBgSuccess,
    kStickyNote,
    kLineChartColorAlt,
    kTrackColorWarningBand,

    // Used to get the size of the enum, insert new colors before this line
    __kLastColor
};

constexpr const char* JSON_KEY_VERSION = "version";

constexpr const char* JSON_KEY_GROUP_SETTINGS             = "settings";
constexpr const char* JSON_KEY_SETTINGS_CATEGORY_DISPLAY  = "display_settings";
constexpr const char* JSON_KEY_SETTINGS_CATEGORY_UNITS    = "units";
constexpr const char* JSON_KEY_SETTINGS_CATEGORY_OTHER    = "other";
constexpr const char* JSON_KEY_SETTINGS_CATEGORY_INTERNAL = "internal";

constexpr const char* JSON_KEY_SETTINGS_DISPLAY_DARK_MODE   = "use_dark_mode";
constexpr const char* JSON_KEY_SETTINGS_DISPLAY_DPI_SCALING = "dpi_based_scaling";
constexpr const char* JSON_KEY_SETTINGS_DISPLAY_FONT_SIZE   = "font_size_index";

constexpr const char* JSON_KEY_SETTINGS_UNITS_TIME_FORMAT = "time_format";

constexpr const char* JSON_KEY_SETTINGS_INTERNAL_RECENT_FILES = "recent_files";

constexpr const char* JSON_KEY_SETTINGS_DONT_ASK_BEFORE_EXIT = "dont_ask_before_exit";
constexpr const char* JSON_KEY_SETTINGS_DONT_ASK_BEFORE_TAB_CLOSE = "dont_ask_before_tab_close";

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
    /**
     * Returns the default ImGui style.
     */
    const ImGuiStyle& GetDefaultIMGUIStyle() const;
    /**
     * Returns the default style as configured by this class
     */
    const ImGuiStyle& GetDefaultStyle() const;

    // User settings
    UserSettings&       GetUserSettings();
    const UserSettings& GetDefaultUserSettings() const;
    void ApplyUserSettings(const UserSettings& old_settings, bool save_json = false);

    // Internal settings
    InternalSettings& GetInternalSettings();
    void              AddRecentFile(const std::string& file_path);
    void              RemoveRecentFile(const std::string& file_path);

    // Constant for event height;
    const float GetEventLevelHeight() const;
    const float GetEventLevelCompactHeight() const;

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
    void ApplyUserDisplaySettings(const UserSettings& old_settings);

    void SerializeUnitSettings(jt::Json& json);
    void DeserializeUnitSettings(jt::Json& json);
    void ApplyUserUnitSettings(const UserSettings& old_settings);

    void SerializeInternalSettings(jt::Json& json);
    void DeserializeInternalSettings(jt::Json& json);

    void SerializeOtherSettings(jt::Json& json);
    void DeserializeOtherSettings(jt::Json& json);

    const std::array<ImU32, static_cast<size_t>(Colors::__kLastColor)>* m_color_store;

    FontManager        m_font_manager;
    ImGuiStyle         m_default_imgui_style;
    ImGuiStyle         m_default_style;
    float              m_display_dpi;
    const UserSettings m_usersettings_default;
    UserSettings       m_usersettings;
    InternalSettings   m_internalsettings;

    std::filesystem::path m_json_path;
};

}  // namespace View
}  // namespace RocProfVis
