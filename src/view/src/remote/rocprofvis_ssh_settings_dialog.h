// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_ssh_connection_config.h"
#include "rocprofvis_ssh_connection_store.h"

#include <functional>
#include <string>

namespace RocProfVis
{
namespace View
{

// Transient modal dialog for managing named SSH connection profiles. It edits a
// working copy of the selected connection (host, port, user, password, key,
// passphrase, plus an editable display name) and supports selecting, creating,
// saving, and deleting profiles against a SshConnectionStore.
//
// On OK the working copy is saved into the store and the selected connection's
// id is reported back via the on_commit callback so the owner can bind it to
// the active workflow. On Cancel the working copy is discarded.
//
// The dialog does not own the store: lifetime is controlled by the owner, which
// should destroy the dialog once Render() reports it has closed.
class SshSettingsDialog
{
public:
    // store         - registry the dialog reads/writes profiles against.
    // initial_id    - connection id to select when the dialog opens ("" => first
    //                 available, or a fresh unsaved profile if the store is empty).
    // on_commit     - invoked with the committed connection id when the user
    //                 accepts.
    SshSettingsDialog(SshConnectionStore& store, const std::string& initial_id,
                      std::function<void(const std::string&)> on_commit);
    ~SshSettingsDialog();

    // Renders the modal each frame. Returns true while the dialog is still open;
    // returns false once the user accepted or cancelled, signalling the owner to
    // destroy this instance.
    bool Render();

private:
    void SelectConnection(const std::string& id);
    void BeginNewConnection();

    SshConnectionStore&                     m_store;
    SshConnectionConfig                     m_working;
    std::function<void(const std::string&)> m_on_commit;

    bool m_show_password;
    bool m_show_passphrase;
    bool m_open;
    bool m_requested_open;
};

}  // namespace View
}  // namespace RocProfVis
