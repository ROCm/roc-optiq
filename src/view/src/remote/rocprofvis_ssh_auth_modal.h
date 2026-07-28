// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

namespace RocProfVis
{
namespace View
{

	class SshSession;

// Polls one session's bridge; if a kbdint prompt or host-key request is
// pending, opens the corresponding modal popup and submits the user's response
// (or Cancel) back to the bridge. Returns true when a request was pending (and
// its modal rendered) this frame, so callers can serialize across sessions.
//
// Handle every *blocking* SshBridge rendezvous here (currently the kbdint
// prompt and host-key confirmation); non-blocking status popups (download
// progress, execution output) stay with the workflow that initiated them.
	bool RenderSshAuthModal(SshSession* ssh_session);

// Renders the auth modal for every live SshSession (see
// SshSession::ActiveSessions), so any session - including one owned privately
// by a widget such as the remote file browser - has its blocking prompts drawn
// and can never wedge its worker. Renders at most one session's modal per frame
// (ImGui modals are exclusive and each request is level-held until answered).
//
// Must be called every frame from AppWindow::Render(), at top-level popup scope.
	void RenderSshAuthModals();

}  // namespace View
}  // namespace RocProfVis
