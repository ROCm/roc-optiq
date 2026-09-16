// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include "json.h"
#include "rocprofvis_font_manager.h"
#include <array>
#include <filesystem>
#include <list>
#include <map>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

enum class TimeFormat;

typedef struct DisplaySettings
{
    bool use_dark_mode;
    int  font_size_index;
    bool show_node_colors;  // color-code timeline tracks by node
    bool compact_sidebar;   // drop the per-row icons in the topology sidebar

} DisplaySettings;

typedef struct UnitSettings
{
    TimeFormat time_format;
} UnitSettings;

// Persisted UI state for the log viewer window. level_mask packs one bit per
// spdlog severity level (bit i => level i enabled).
typedef struct LogViewerSettings
{
    int  level_mask;
    bool auto_scroll;
    bool use_regex;
    bool relative_time;
    bool visible;
} LogViewerSettings;

// Chat endpoint. The settings panel asks for URL, model, and API key; how the
// key and model are presented is worked out from the URL by the client, so
// nothing about the endpoint shape is stored here.
constexpr const char* ASSISTANT_DEFAULT_PROVIDER_NAME = "Default";
constexpr const char* ASSISTANT_DEFAULT_MODEL         = "gpt-5.6-luna";

// One saved endpoint. Name is the credential-store key, not a display label.
typedef struct AssistantProvider
{
    std::string name = ASSISTANT_DEFAULT_PROVIDER_NAME;
    std::string endpoint_url;
    std::string model = ASSISTANT_DEFAULT_MODEL;
} AssistantProvider;

// Fills in an empty name or model, so a half-written settings file still runs.
inline void
ApplyAssistantEndpointDefaults(AssistantProvider& provider)
{
    if(provider.name.empty())
    {
        provider.name = ASSISTANT_DEFAULT_PROVIDER_NAME;
    }
    if(provider.model.empty())
    {
        provider.model = ASSISTANT_DEFAULT_MODEL;
    }
}

// A provider with nothing configured but the defaults.
inline AssistantProvider
MakeDefaultAssistantProvider()
{
    AssistantProvider provider;
    ApplyAssistantEndpointDefaults(provider);
    return provider;
}

typedef struct AssistantSettings
{
    std::vector<AssistantProvider> providers;
    size_t                         active = 0;
} AssistantSettings;

typedef struct UserSettings
{
    DisplaySettings   display_settings;
    UnitSettings      unit_settings;
    bool              dont_ask_before_tab_closing;
    bool              dont_ask_before_exit;
    int               log_viewer_max_entries;
    LogViewerSettings log_viewer;
    AssistantSettings assistant;
    // Linux/Wayland only: repair pointer routing after a floating window is
    // dragged, at the cost of a brief flicker on each drag-release. Set with
    // --drag-repair and ignored on other platforms. See
    // raise_dragged_viewport_after_release() in rocprofvis_platform_helpers.cpp.
    bool              linux_drag_repair;
} UserSettings;

typedef struct InternalSettings
{
    std::list<std::string> recent_files;
} InternalSettings;

// Deliberately holds no path to a profiler binary. Which binary runs is chosen
// by a rocprofvis_profiler_tool_t, so a settings file that is edited, corrupted,
// or copied from another machine cannot decide what gets executed. A ROCm install
// in a non-standard location is handled by LaunchConfig::tool_directory, which
// names a directory rather than an executable and belongs to the launch profile
// because a remote profile needs a directory on the remote host.
typedef struct ProfilerSettings
{
    std::string profiler_output_directory;
    bool        auto_load_trace = true;
    std::vector<std::string> recent_targets;
    std::string last_preset_name;
    std::string last_profiler_id;
    std::string last_ssh_connection_id;
} ProfilerSettings;

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
    kTextWarning,
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
    kComboFill,
    kAccent,
    kAccentHover,
    kAccentActive,
    kTabAccent,
    kTabAccentHover,
    kTabAccentActive,
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
    kTableBorderInner,
    kTableBorderOuter,
    kPanelBorderSubtle,
    kEventHighlight,
    kEventSearchHighlight,
    kAreaOfInterest,
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
    kTextOnAccent,
    kMeasurementColor,
    kMeasurementLabelBg,
    kMeasurementLabelEdge,
    kMeasurementLabelText,
    kMeasurementNotch,

    kComparisonBase,
    kComparisonTarget,
    kComparisonLesser,
    kComparisonGreater,

    // Memory chart (compute view) palette
    kMemChartBg,
    kMemChartPanel,
    kMemChartPanelAlt,
    kMemChartBorder,
    kMemChartBorderHot,
    kMemChartTextMain,
    kMemChartTextDim,
    kMemChartRead,
    kMemChartWrite,
    kMemChartAtomic,
    kMemChartUtil,
    kMemChartHit,
    kMemChartStall,
    kMemChartShadow,

    // Sticky note annotation
    kStickyNoteBg,
    kStickyNoteBorder,
    kStickyNoteHeader,
    kStickyNoteShadow,
    kStickyNoteText,
    kStickyNoteTextMuted,
    kStickyNoteAccent,
    kStickyNoteResize,
    kStickyNoteResizeActive,

    // Internal build banner + debug nav bar
    kBannerFill,
    kBannerBorder,
    kBannerText,
    kDebugNavBarBg,

    // Log viewer per-level text colors
    kLogTrace,
    kLogDebug,
    kLogInfo,
    kLogWarning,
    kLogError,
    kLogCritical,

    // Used to get the size of the enum, insert new colors before this line
    __kLastColor
};

constexpr const char* JSON_KEY_VERSION = "version";

constexpr const char* JSON_KEY_GROUP_SETTINGS             = "settings";
constexpr const char* JSON_KEY_SETTINGS_CATEGORY_DISPLAY  = "display_settings";
constexpr const char* JSON_KEY_SETTINGS_CATEGORY_UNITS    = "units";
constexpr const char* JSON_KEY_SETTINGS_CATEGORY_OTHER    = "other";
constexpr const char* JSON_KEY_SETTINGS_CATEGORY_INTERNAL = "internal";

constexpr const char* JSON_KEY_SETTINGS_DISPLAY_DARK_MODE       = "use_dark_mode";
constexpr const char* JSON_KEY_SETTINGS_DISPLAY_FONT_SIZE       = "font_size_index";
constexpr const char* JSON_KEY_SETTINGS_DISPLAY_NODE_COLORS     = "show_node_colors";
constexpr const char* JSON_KEY_SETTINGS_DISPLAY_COMPACT_SIDEBAR = "compact_sidebar";

constexpr const char* JSON_KEY_SETTINGS_UNITS_TIME_FORMAT = "time_format";

constexpr const char* JSON_KEY_SETTINGS_INTERNAL_RECENT_FILES = "recent_files";
constexpr size_t      MAX_RECENT_FILES                       = 5;

constexpr const char* JSON_KEY_SETTINGS_DONT_ASK_BEFORE_EXIT = "dont_ask_before_exit";
constexpr const char* JSON_KEY_SETTINGS_DONT_ASK_BEFORE_TAB_CLOSE = "dont_ask_before_tab_close";
constexpr const char* JSON_KEY_SETTINGS_LINUX_DRAG_REPAIR         = "linux_drag_repair";

constexpr const char* JSON_KEY_SETTINGS_LOG_VIEWER_MAX_ENTRIES = "log_viewer_max_entries";
// Bounds for the log viewer's in-memory cache size. Older lines fall off the
// ring buffer once the cap is reached; the full history lives in the log file.
constexpr int LOG_VIEWER_MAX_ENTRIES_DEFAULT   = 512;
constexpr int LOG_VIEWER_MAX_ENTRIES_MIN       = 64;
constexpr int LOG_VIEWER_MAX_ENTRIES_MAX       = 100000;
constexpr int LOG_VIEWER_MAX_ENTRIES_STEP      = 64;
constexpr int LOG_VIEWER_MAX_ENTRIES_STEP_FAST = 512;

constexpr const char* JSON_KEY_SETTINGS_LOG_VIEWER_LEVEL_MASK    = "log_viewer_level_mask";
constexpr const char* JSON_KEY_SETTINGS_LOG_VIEWER_AUTO_SCROLL   = "log_viewer_auto_scroll";
constexpr const char* JSON_KEY_SETTINGS_LOG_VIEWER_USE_REGEX     = "log_viewer_use_regex";
constexpr const char* JSON_KEY_SETTINGS_LOG_VIEWER_RELATIVE_TIME = "log_viewer_relative_time";
constexpr const char* JSON_KEY_SETTINGS_LOG_VIEWER_VISIBLE       = "log_viewer_visible";
// All six severity levels enabled (bits 0..5).
constexpr int LOG_VIEWER_DEFAULT_LEVEL_MASK = 0x3F;

constexpr const char* JSON_KEY_SETTINGS_CATEGORY_APP_WINDOW      = "app_window";
constexpr const char* JSON_KEY_SETTINGS_APP_WINDOW_TOOLBAR       = "show_toolbar";
constexpr const char* JSON_KEY_SETTINGS_APP_WINDOW_DETAILS_PANEL = "show_details_panel";
constexpr const char* JSON_KEY_SETTINGS_APP_WINDOW_SIDEBAR       = "show_sidebar";
constexpr const char* JSON_KEY_SETTINGS_APP_WINDOW_HISTOGRAM     = "show_histogram";
constexpr const char* JSON_KEY_SETTINGS_APP_WINDOW_SUMMARY       = "show_summary";

constexpr const char* JSON_KEY_SETTINGS_CATEGORY_ASSISTANT = "assistant";
constexpr const char* JSON_KEY_SETTINGS_ASSISTANT_ENDPOINT_URL = "endpoint_url";
constexpr const char* JSON_KEY_SETTINGS_ASSISTANT_MODEL        = "model";
constexpr const char* JSON_KEY_SETTINGS_ASSISTANT_PROVIDERS    = "providers";
constexpr const char* JSON_KEY_SETTINGS_ASSISTANT_ACTIVE       = "active";
constexpr const char* JSON_KEY_SETTINGS_ASSISTANT_NAME         = "name";
// OS credential-store key for the assistant API token. Never written to JSON.
constexpr const char* ASSISTANT_TOKEN_SECRET_KEY = "assistant-api-token";

constexpr const char* JSON_KEY_SETTINGS_CATEGORY_HOTKEYS = "hotkeys";
constexpr const char* JSON_KEY_SETTINGS_CATEGORY_PROFILER = "profiler";
constexpr const char* JSON_KEY_SETTINGS_PROFILER_OUTPUT_DIR = "profiler_output_directory";
constexpr const char* JSON_KEY_SETTINGS_PROFILER_AUTO_LOAD = "auto_load_trace";

class SettingsManager
{
public:
    SettingsManager(const SettingsManager&)            = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    static SettingsManager& GetInstance();

    bool Init();

    // Fonts
    FontManager& GetFontManager();

    // Styling
    bool ShowNodeColors() const { return m_usersettings.display_settings.show_node_colors; }
    bool CompactSidebar() const { return m_usersettings.display_settings.compact_sidebar; }
    ImU32                     GetColor(Colors color) const;
    const std::vector<ImU32>& GetColorWheel() const;
    const std::vector<ImU32>& GetHighlightedEventColorWheel() const;
    const char*               GetFlameColormapName() const;
    const char*               GetContrastColormapName() const;
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
    void              ClearRecentFiles();

    AppWindowSettings& GetAppWindowSettings();

    void SaveHotkeySettings();
    // Profiler settings
    ProfilerSettings& GetProfilerSettings();
    void SaveProfilerSettings();

    // The route the assistant should use, or nullptr when none is configured.
    const AssistantProvider* GetActiveAssistantProvider() const;

    // Assistant API token, one per provider. Stored in the OS credential vault
    // when available, otherwise held in process memory only. Never written to
    // settings JSON. Guarded because these are the only users of SecretStore
    // outside remote; the provider URL and model above stay compiled either way,
    // so a settings file survives a build that has the assistant switched off.
#ifdef ROCPROFVIS_ENABLE_AGENTIC_PROFILING
    bool HasAssistantToken(const std::string& provider_name) const;
    bool GetAssistantToken(const std::string& provider_name, std::string& out_token) const;
    bool SetAssistantToken(const std::string& provider_name, const std::string& token);
    bool ClearAssistantToken(const std::string& provider_name);
    // Names the endpoint that owns a key saved before endpoints had names.
    // Called once on load; see the definition for why it is not a live lookup.
    void MigrateLegacyAssistantToken();
#endif

    // Constant for event height;
    const float GetEventLevelHeight() const;
    const float GetEventLevelCompactHeight() const;
    const float GetEventLevelSpacing() const;

    // Convenience static method (alias for GetInstance)
    static SettingsManager& Get() { return GetInstance(); }

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

    void SerializeHotkeySettings(jt::Json& json);
    void DeserializeHotkeySettings(jt::Json& json);
    void SerializeProfilerSettings(jt::Json& json);
    void DeserializeProfilerSettings(jt::Json& json);
    void SerializeAssistantSettings(jt::Json& json);
    void DeserializeAssistantSettings(jt::Json& json);
    // The endpoint name is the credential-store key, so it has to be unique.
    void MakeAssistantProviderNamesUnique();
    void SerializeAppWindowSettings(jt::Json& json);
    void DeserializeAppWindowSettings(jt::Json& json);

    const std::array<ImU32, static_cast<size_t>(Colors::__kLastColor)>* m_color_store;

    FontManager        m_font_manager;
    ImGuiStyle         m_default_imgui_style;
    ImGuiStyle         m_default_style;
    const UserSettings m_usersettings_default;
    UserSettings       m_usersettings;
    InternalSettings   m_internalsettings;
    AppWindowSettings  m_appwindowsettings;
    ProfilerSettings   m_profilersettings;
    // Provider name -> token, used only when no OS credential store exists.
    std::map<std::string, std::string> m_assistant_token_session;

    std::filesystem::path m_json_path;
};

}  // namespace View
}  // namespace RocProfVis
