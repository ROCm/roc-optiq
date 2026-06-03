// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_ssh_uri.h"

#include <functional>

namespace RocProfVis
{
namespace View
{

// Transient modal dialog for editing SSH connection settings (host, port,
// user, password, key, passphrase). It is created on demand by the owner,
// edits a working copy of the configuration, and on OK commits the values
// back through the supplied callback. On Cancel the working copy is discarded.
//
// The dialog does not own any long-lived configuration: lifetime is controlled
// by the owner, which should destroy it once Render() reports it has closed.
class SshSettingsDialog
{
public:
    // current  - seed values shown when the dialog opens.
    // on_commit - invoked with the edited configuration when the user accepts.
    SshSettingsDialog(const RemoteUri& current, std::function<void(RemoteUri)> on_commit);
    ~SshSettingsDialog();

    // Renders the modal each frame. Returns true while the dialog is still
    // open; returns false once the user accepted or cancelled, signalling the
    // owner to destroy this instance.
    bool Render();

private:
    RemoteUri                       m_working;
    std::function<void(RemoteUri)>  m_on_commit;
    bool                            m_show_password;
    bool                            m_show_passphrase;
    bool                            m_open;
    bool                            m_requested_open;
};

}  // namespace View
}  // namespace RocProfVis
