// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>

struct ImGuiTestEngine;

void RegisterAppTests(ImGuiTestEngine* e);

// Pause switch for the soak drivers, toggled by F9 from the GLFW key callback.
// While set, the running driver stops issuing input and hands the application
// back to the user; the app keeps rendering and stays fully interactive.
extern std::atomic<bool> g_soak_paused;

// Set when the application is shutting down. The soak drivers loop forever by
// design, and a test coroutine only advances while the render loop is drawing
// frames, so a driver still looping when that loop ends would deadlock against
// the join in ImGuiTestEngine_Stop(). Every soak wait must break on this.
extern std::atomic<bool> g_soak_exit_requested;
