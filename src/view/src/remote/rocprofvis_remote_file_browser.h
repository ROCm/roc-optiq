// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_ssh_uri.h"
#include "rocprofvis_ssh_fetch.h"
#include "rocprofvis_remote_trace_orchestrator.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

// Reusable modal remote file/directory picker driven over SSH.
//
// The browser reads the SSH connection (host/user/credentials) from the shared
// RemoteUri it is given, and owns its own RemoteTraceOrchestrator + SshSession
// for listing directories - so it can be embedded anywhere a remote path needs
// to be chosen (the "Open Remote Trace" dialog and the profiler launcher's
// Target section both use it). It uses the RemoteUri's transient browsing-path
// scratch fields while navigating.
//
// Usage: call Open() to pop the modal, then Render() every frame. When the user
// picks something, the on_pick callback fires with the chosen absolute POSIX
// path (a file in kFile mode, a directory in kDirectory mode).
class RemoteFileBrowser
{
public:
    enum class PickMode
    {
        kFile,       // choose a file; folders navigate, files commit
        kDirectory,  // choose a folder; files are inert, "Select Folder" commits
    };

    // File-type filter presets, in the order of the type dropdown.
    enum class TypeFilter
    {
        kAll,
        kTraces,
        kProjects,
        kTracesAndProjects,
    };

    explicit RemoteFileBrowser(std::shared_ptr<RemoteUri> uri);
    ~RemoteFileBrowser();

    // Pops the modal. Seeds the starting directory from seed_path's parent (or
    // the remote home when empty); if that folder cannot be listed, the browser
    // opens its nearest parent below the root, or else the remote home.
    // on_pick receives the chosen absolute path.
    void Open(const std::string& seed_path, PickMode mode,
              std::function<void(const std::string&)> on_pick);

    // Renders the modal and consumes async directory-listing updates. Must be
    // called every frame (it is a no-op while closed).
    void Render();

    // True while the modal is visible; callers use this to suppress their own
    // popups that would otherwise fight the browser for the SSH session.
    bool IsOpen() const { return m_show_remote_filesystem_popup; }

    // Hands over the browser's connected session so the caller can reuse it
    // instead of reconnecting. Call from inside on_pick; null when there is no
    // session.
    std::unique_ptr<RemoteTraceOrchestrator> TakeSession();

    // Turns the header's connection label into a button that closes the
    // browser and runs `action` (e.g. switch or edit the connection). Unset, the
    // label stays plain text.
    void SetConnectionAction(std::function<void()> action);

    // Selects the type filter; the user can still change it in the dropdown.
    void SetTypeFilter(TypeFilter filter);

    // Replaces the mode's default window title and the "Select" label of the
    // button that picks a file, for a browser with a single purpose.
    void SetTitle(std::string title) { m_title = std::move(title); }
    void SetAcceptLabel(std::string label) { m_accept_label = std::move(label); }

private:
    // Lazily creates the orchestrator (bound to the directory callback) and
    // reuses it across navigation so the SSH session stays connected.
    void EnsureBrowseOrchestrator();
    void BrowseRemotePath();
    void NavigateBrowserTo(const std::string& path, bool record_history);
    // Reacts to a failed listing: falls back from a start folder that does not
    // list, otherwise reports the error and keeps the last folder listed.
    void HandleBrowseFailure();
    void ActivateBrowserEntry(const RemoteDir::FileEntry& entry);
    void CommitPath(const std::string& path);
    // Closes the browser and runs the owner's connection action.
    void CloseForConnectionChange();

    std::shared_ptr<RemoteUri>               m_uri;
    std::unique_ptr<RemoteTraceOrchestrator> m_orchestrator;
    PickMode                                 m_mode;
    std::function<void(const std::string&)>  m_on_pick;
    std::function<void()>                    m_connection_action;

    bool                     m_show_remote_filesystem_popup;
    bool                     m_should_open_browser_popup;   // defer OpenPopup to render scope
    bool                     m_should_close_browser_popup;
    bool                     m_browser_busy;                // a listing is in flight
    std::string              m_browser_error;               // last listing failure
    std::string              m_browser_dir;                 // resolved current directory
    std::string              m_start_dir;                   // folder Open() asked for
    std::string              m_fallback_dir;                // next try once the session is idle
    RemoteDir::Snapshot      m_last_directory_state;        // entries for m_browser_dir
    std::vector<std::string> m_history_back;
    std::vector<std::string> m_history_forward;
    std::string              m_remote_file_filter;          // live filter text
    std::string              m_address_edit;                // editable address-bar buffer
    bool                     m_address_editing;             // breadcrumb vs. editable field
    bool                     m_show_hidden;
    int                      m_type_filter;                 // index into extension presets
    std::string              m_title;                       // empty: the mode's default
    std::string              m_accept_label;                // empty: "Select"
    // Selection: "" = none, ".." = the parent row, otherwise the entry's name.
    std::string              m_selected_name;
    bool                     m_scroll_to_selected;
};

}  // namespace View
}  // namespace RocProfVis
