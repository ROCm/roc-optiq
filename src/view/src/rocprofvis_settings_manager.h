// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

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

enum class ColorScheme
{
    Default,       // Standard dark/light theme
    HighContrast,  // High contrast for accessibility
    Solarized,     // Solarized theme
    Nord,          // Nord theme
    Monokai,       // Monokai theme
    Dracula        // Dracula theme
};

typedef struct DisplaySettings
{
    bool use_dark_mode;
    bool dpi_based_scaling;
    int  font_size_index;
    ColorScheme color_scheme;

} DisplaySettings;

typedef struct UnitSettings
{
    TimeFormat time_format;
} UnitSettings;

// Keyboard shortcut configuration
typedef struct KeyboardShortcuts
{
    // Zoom
    int zoom_in_key = ImGuiKey_Equal;       // + or =
    int zoom_out_key = ImGuiKey_Minus;      // -
    int reset_zoom_key = ImGuiKey_0;        // Ctrl+0
    bool reset_zoom_ctrl = true;

    // Pan
    int pan_left_key = ImGuiKey_LeftArrow;
    int pan_right_key = ImGuiKey_RightArrow;
    int jump_start_key = ImGuiKey_Home;
    int jump_end_key = ImGuiKey_End;

    // Vertical scroll
    int scroll_up_key = ImGuiKey_UpArrow;
    int scroll_down_key = ImGuiKey_DownArrow;
    int page_up_key = ImGuiKey_PageUp;
    int page_down_key = ImGuiKey_PageDown;

    // Selection
    int select_all_key = ImGuiKey_A;
    bool select_all_ctrl = true;
    int clear_selection_key = ImGuiKey_Escape;
    int frame_selection_key = ImGuiKey_F;
    bool frame_selection_ctrl = true;
    int zoom_to_selection_key = ImGuiKey_Z;
    bool zoom_to_selection_ctrl = true;
    bool zoom_to_selection_shift = true;
} KeyboardShortcuts;

typedef struct UserSettings
{
    DisplaySettings display_settings;
    UnitSettings    unit_settings;
    KeyboardShortcuts keyboard_shortcuts;
    bool            dont_ask_before_tab_closing;
    bool            dont_ask_before_exit;
    bool            auto_run_ai_analysis;  // Auto-run AI analysis after profiling
    std::string     anthropic_api_key;     // Saved Anthropic API key
    std::string     openai_api_key;        // Saved OpenAI API key
} UserSettings;

struct RecentConnection
{
    std::string name;           // User-defined name or auto-generated
    std::string ssh_host;
    std::string ssh_user;
    int ssh_port = 22;
    std::string remote_app_path;
    std::string remote_output_path;
};

typedef struct InternalSettings
{
    std::list<std::string> recent_files;
    std::list<RecentConnection> recent_connections;
} InternalSettings;

typedef struct AppWindowSettings
{
    bool show_toolbar;
    bool show_details_panel;
    bool show_sidebar;
    bool show_histogram;
    bool show_summary;
} AppWindowSettings;

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

    kMinimapBin1,
    kMinimapBin2,
    kMinimapBin3,
    kMinimapBin4,
    kMinimapBin5,
    kMinimapBin6,
    kMinimapBin7,

    kMinimapBinCounter1,
    kMinimapBinCounter2,
    kMinimapBinCounter3,
    kMinimapBinCounter4,
    kMinimapBinCounter5,
    kMinimapBinCounter6,
    kMinimapBinCounter7,

    kMinimapBg,
    kLoadingScreenColor,
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
constexpr const char* JSON_KEY_SETTINGS_INTERNAL_RECENT_CONNECTIONS = "recent_connections";

constexpr const char* JSON_KEY_SETTINGS_DONT_ASK_BEFORE_EXIT = "dont_ask_before_exit";
constexpr const char* JSON_KEY_SETTINGS_DONT_ASK_BEFORE_TAB_CLOSE = "dont_ask_before_tab_close";
constexpr const char* JSON_KEY_SETTINGS_AUTO_RUN_AI_ANALYSIS = "auto_run_ai_analysis";
constexpr const char* JSON_KEY_SETTINGS_ANTHROPIC_API_KEY = "anthropic_api_key";
constexpr const char* JSON_KEY_SETTINGS_OPENAI_API_KEY = "openai_api_key";

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
    void              AddRecentConnection(const RecentConnection& connection);
    void              RemoveRecentConnection(const std::string& name);

    AppWindowSettings& GetAppWindowSettings();

    // Constant for event height;
    const float GetEventLevelHeight() const;
    const float GetEventLevelCompactHeight() const;


private:
    SettingsManager();
    ~SettingsManager();

    void InitStyling();
    void ApplyColorStyling();
    void ApplyColorScheme(ColorScheme scheme);

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
    AppWindowSettings  m_appwindowsettings;


    std::filesystem::path m_json_path;
};

}  // namespace View
}  // namespace RocProfVis
