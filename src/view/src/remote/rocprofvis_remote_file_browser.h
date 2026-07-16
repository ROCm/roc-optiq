// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_ssh_fetch.h"

#include <functional>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class SettingsManager;

// A full-featured, Windows-Explorer-style file browser for picking a remote
// file/directory. The widget is transport-agnostic: it never talks to SSH
// directly. Instead the owner supplies callbacks to fetch a directory listing
// (or run a recursive search) and pushes the results back in via SetListing /
// SetSearchResults. This keeps the widget reusable for any file source.
//
// Feature set:
//   - Back / Forward / Up / Refresh / Home navigation with history.
//   - Editable address bar and clickable breadcrumb segments.
//   - Instant, case-insensitive filtering of the current folder (fast search).
//   - Recursive "search subfolders" (Enter) delegated to the owner.
//   - Sortable Name / Size / Type / Modified columns (folders first).
//   - Show/hide dotfiles, keyboard navigation (arrows, Enter, Backspace).
//   - Themed styling, elided names with tooltips, and a busy indicator.
//
// Directory entries reuse RemoteDir::FileEntry so no parallel record type is
// introduced.
class RemoteFileBrowser
{
public:
    struct Callbacks
    {
        // Requests a listing for the given absolute path. The owner performs the
        // fetch and, when it completes, calls SetListing(path, snapshot).
        std::function<void(const std::string& path)> request_listing;

        // Requests a recursive search for query rooted at dir. The owner runs the
        // search and calls SetSearchResults(results) when it completes. May be
        // left empty to disable recursive search (instant filtering still works).
        std::function<void(const std::string& query, const std::string& dir)> request_search;

        // Invoked once the user commits a file selection.
        std::function<void(const std::string& file_path)> on_file_chosen;

        // Invoked when the user dismisses the browser without choosing a file.
        std::function<void()> on_cancel;
    };

    RemoteFileBrowser();

    void SetCallbacks(Callbacks callbacks);

    // Opens the browser at initial_dir and immediately requests its listing.
    void Open(const std::string& initial_dir);

    // Programmatically closes the browser (no on_cancel is fired).
    void Close();

    bool IsOpen() const { return m_open; }

    // Pushed by the owner when a directory listing completes. dir is the path
    // that was requested; snapshot holds the entries.
    void SetListing(const std::string& dir, const RemoteDir::Snapshot& snapshot);

    // Pushed by the owner when a recursive search completes. Each entry's name
    // is the path (relative to the search root or absolute) of a match.
    void SetSearchResults(std::vector<RemoteDir::FileEntry> results);

    // Toggles the in-flight indicator (owner sets true while a request runs).
    // Clearing it also ends any "searching" state.
    void SetBusy(bool busy)
    {
        m_busy = busy;
        if(!busy)
        {
            m_searching = false;
        }
    }

    // Surfaces an error banner in the browser (e.g. a failed listing).
    void SetError(const std::string& message)
    {
        m_error     = message;
        m_searching = false;
    }

    // Draws the browser. Call every frame while the owner keeps it alive.
    void Render();

private:
    enum class SortColumn : uint8_t
    {
        Name,
        Size,
        Type,
        Modified,
    };

    // Selection sentinels used by m_selected in addition to real (>= 0) indices
    // into m_visible.
    static constexpr int SELECTION_NONE   = -1;
    static constexpr int SELECTION_PARENT = -2;

    void NavigateTo(const std::string& path, bool record_history);
    void NavigateUp();
    void NavigateBack();
    void NavigateForward();
    void NavigateHome();
    void Refresh();

    void ActivateVisible(int visible_index);
    void CommitSelection();
    void ClearSearch();
    void RunSearch();

    // Resolves the full remote path of an entry (accounts for search results,
    // which may carry relative or absolute paths).
    std::string EntryFullPath(const RemoteDir::FileEntry& entry) const;

    void RebuildVisible();
    void ApplySort();
    void HandleKeyboard();
    void SetSelected(int selection);
    bool PassesTypeFilter(const RemoteDir::FileEntry& entry) const;

    const std::vector<RemoteDir::FileEntry>& ActiveSource() const;

    void RenderToolbar(SettingsManager& settings);
    void RenderAddressBar(SettingsManager& settings);
    void RenderSearchRow(SettingsManager& settings);
    void RenderTable(SettingsManager& settings, float table_height);
    void RenderEmptyState(SettingsManager& settings);
    void RenderFooter(SettingsManager& settings);

    static std::string NormalizePath(const std::string& path);
    static std::string JoinPath(const std::string& dir, const std::string& name);
    static std::string ParentPath(const std::string& dir);
    static bool        IsRoot(const std::string& dir);

    Callbacks m_callbacks;

    bool        m_open;
    bool        m_should_open_popup;
    bool        m_busy;
    std::string m_error;

    std::string                       m_current_dir;
    std::vector<RemoteDir::FileEntry> m_entries;         // current directory
    std::vector<RemoteDir::FileEntry> m_search_entries;  // recursive search hits
    bool                              m_in_search_mode;
    std::string                       m_search_root;     // dir the search ran in

    std::vector<size_t> m_visible;   // indices into ActiveSource(), filtered+sorted
    int                 m_selected;  // SELECTION_* sentinel or index into m_visible

    std::vector<std::string> m_back_stack;
    std::vector<std::string> m_forward_stack;

    std::string m_filter;         // instant filter text (applies in all modes)
    std::string m_path_edit;      // editable address-bar buffer
    bool        m_show_hidden;
    bool        m_searching;      // a recursive search request is in flight
    int         m_type_filter;    // index into the extension-filter presets
    std::string m_selected_name;  // remembered selection to survive a refresh

    SortColumn m_sort_column;
    bool       m_sort_ascending;

    bool m_needs_rebuild;        // m_visible must be refiltered/resorted
    bool m_scroll_to_selected;   // keep keyboard selection visible
};

}  // namespace View
}  // namespace RocProfVis
