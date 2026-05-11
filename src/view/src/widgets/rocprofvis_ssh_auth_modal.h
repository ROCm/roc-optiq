// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

namespace RocProfVis
{
namespace DataModel
{
class AuthBridge;
}

namespace View
{

// Polls the bridge each frame; if a kbdint prompt or host-key request is
// pending, opens the corresponding modal popup. Submits the user's response
// (or Cancel) back to the bridge.
//
// Must be called every frame from AppWindow::Render(), after any user-
// initiated SSH connection has been started.
void RenderSshAuthModal(DataModel::AuthBridge& bridge);

}  // namespace View
}  // namespace RocProfVis
