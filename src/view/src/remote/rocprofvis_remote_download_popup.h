// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_ssh_fetch.h"

namespace RocProfVis
{
namespace View
{

// Modal progress card for an SSH file download, shared by the remote trace
// opener and the profiler launcher so a transfer looks the same wherever it was
// started from.
//
// The caller owns the popup's lifetime: it calls ImGui::OpenPopup(popup_id) and
// keeps open true while the modal should stay on screen. The two workflows
// detect completion differently, so the caller passes its own verdict in
// finished; when set, the modal closes and open is cleared. id_prefix scopes the
// internal card ids so two instances never collide, and idle_label is shown
// until the first byte count arrives. Returns true while the modal is on the
// ImGui stack; false means it was never opened or has been dismissed.
bool
RenderRemoteDownloadPopup(const char* popup_id, const char* id_prefix,
                          const FileStat::Snapshot& progress, const char* idle_label,
                          bool finished, bool& open);

}  // namespace View
}  // namespace RocProfVis
