// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_widget.h"

#include <array>
#include <chrono>
#include <regex>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

// User-facing log viewer. Mirrors the application's spdlog output (the same
// stream written to the on-disk log file) into a virtualized, filterable,
// color-coded table. Drains entries from the core logger via
// rocprofvis_core_get_log_entries_ex.
class LogViewer : public RocWidget
{
public:
    // Number of spdlog severity levels the viewer distinguishes
    // (trace, debug, info, warn, error, critical).
    static constexpr int LEVEL_COUNT = 6;
    static constexpr int SEARCH_BUFFER_SIZE = 256;

    static LogViewer* GetInstance();
    static void       DestroyInstance();

    // Drains pending entries from the core logger. Call once per frame so log
    // history keeps accumulating even while the window is hidden.
    void Poll();

    void Render() override;

    bool* VisiblePtr();

private:
    LogViewer();

    struct LogEntry
    {
        int         level;
        std::string timestamp;  // wall-clock string from the core logger
        std::string text;
        double      relative_s = 0.0;  // seconds since the viewer started
    };

    static void        ProcessEntry(void* user_ptr, int level, char const* timestamp,
                                    char const* message);
    static const char* LevelLabel(int level);
    static Colors      LevelColor(int level);

    void AddEntry(int level, const char* timestamp, const char* message);

    // Resizes the in-memory ring buffer to match the user-configured cache
    // size when it changes. Cheap no-op when already in sync.
    void SyncCacheCapacity();

    // Invalidates the cached per-level counts and filtered view; call whenever
    // the entry buffer changes (new entry, clear, capacity resize).
    void MarkDataDirty();
    void RecomputeCounts();

    // Bitmask of the currently enabled severity levels (bit i == level i).
    int  LevelMask() const;

    // Rebuilds m_filtered and the cached search/regex state from the current
    // entries and filter inputs. Only invoked when an input actually changed.
    void RebuildFilter();

    // Maps the current per-level checkbox state onto a "show this severity and
    // above" preset, or -1 when the selection is custom.
    int  CurrentMinLevelPreset() const;
    void ApplyMinLevelPreset(int floor_level);

    std::string FormatLine(const LogEntry& entry) const;
    std::string RelativeTimeString(double relative_s) const;
    void        CopyVisibleToClipboard() const;

    // Mirrors the live UI toggles to/from the persisted user settings so the
    // viewer remembers its state across sessions.
    void LoadUiState();
    void SaveUiState() const;

    void RenderNav();
    void RenderLevelsPopup();
    void RenderLevelToggle(int level);
    void RenderOptionsPopup();
    void RenderJumpToLatest();
    void RenderTable();
    void OpenLogFile();

    static LogViewer* s_instance;

    CircularBuffer<LogEntry>      m_entries;
    int                           m_cache_capacity;
    std::array<bool, LEVEL_COUNT> m_level_enabled;
    std::array<int, LEVEL_COUNT>  m_level_counts;
    std::vector<const LogEntry*>  m_filtered;
    std::array<char, SEARCH_BUFFER_SIZE> m_search_buffer;
    bool                          m_visible;
    bool                          m_auto_scroll;
    bool                          m_use_regex;
    bool                          m_relative_time;
    bool                          m_paused;
    bool                          m_regex_error;
    bool                          m_scrolled_up;
    bool                          m_request_scroll_bottom;
    bool                          m_counts_dirty;

    // Cached filter state. m_filtered is recomputed only when one of the
    // applied-* values diverges from the live inputs (see RenderTable).
    bool        m_filter_dirty;
    int         m_applied_level_mask;
    bool        m_applied_use_regex;
    std::string m_applied_search;
    std::string m_search_lower;  // lowercased search for substring matching
    std::regex  m_regex;         // compiled when use_regex && search non-empty
    bool        m_regex_ok;

    std::chrono::steady_clock::time_point m_start_time;
};

}  // namespace View
}  // namespace RocProfVis
