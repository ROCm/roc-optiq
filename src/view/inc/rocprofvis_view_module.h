// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

typedef enum rocprofvis_view_render_options_t
{
    kRocProfVisViewRenderOption_None                = 0,
    kRocProfVisViewRenderOption_RequestExit         = 1,
} rocprofvis_view_render_options_t;

typedef enum rocprofvis_view_notification_t
{
    kRocProfVisViewNotification_Exit_App = 1,
    kRocProfVisViewNotification_Toggle_Fullscreen = 2,
} rocprofvis_view_notification_t;

typedef ImTextureID (*rocprofvis_view_create_texture_rgba32_t)(
    void* user_data, const unsigned char* pixels, int width, int height);

typedef void (*rocprofvis_view_destroy_texture_t)(void*       user_data,
                                                  ImTextureID texture_id);
typedef enum rocprofvis_view_file_dialog_preference_t
{
    kRocProfVisViewFileDialog_Auto   = 0,
    kRocProfVisViewFileDialog_Native = 1,
    kRocProfVisViewFileDialog_ImGui  = 2,
} rocprofvis_view_file_dialog_preference_t;

bool
rocprofvis_view_init(std::function<void(int)>                 notification_callback,
                     rocprofvis_view_file_dialog_preference_t file_dialog_pref =
                         kRocProfVisViewFileDialog_Auto);

void
rocprofvis_view_render(const rocprofvis_view_render_options_t& render_options);

void
rocprofvis_view_destroy();

void
rocprofvis_view_open_files(const std::vector<std::string>& file_paths);

void
rocprofvis_view_set_fullscreen_state(bool is_fullscreen);

void
rocprofvis_view_set_texture_backend(
    rocprofvis_view_create_texture_rgba32_t create_texture,
    rocprofvis_view_destroy_texture_t       destroy_texture,
    void*                                  user_data);

std::string
rocprofvis_get_application_config_path();

std::string
rocprofvis_get_application_log_path();

bool
rocprofvis_view_is_remote_display_session();

// Stored preference for the Linux/Wayland post-drag click-through workaround.
// Only meaningful on Linux; the setter persists the value so the --drag-repair
// flag does not have to be passed on every launch. Both require the view to
// have been initialized.
bool
rocprofvis_view_get_drag_repair_enabled();

void
rocprofvis_view_set_drag_repair_enabled(bool enabled);

bool
rocprofvis_view_wants_continuous_render();

// How far a scripted assistant run has got. kInactive when none was asked for,
// so the app shell can poll unconditionally.
typedef enum rocprofvis_view_assistant_batch_state_t
{
    kRocProfVisAssistantBatch_Inactive = 0,
    kRocProfVisAssistantBatch_Running  = 1,
    kRocProfVisAssistantBatch_Done     = 2,
    kRocProfVisAssistantBatch_Failed   = 3,
} rocprofvis_view_assistant_batch_state_t;

/**
 * @brief Asks the assistant one question without the UI and writes the answer
 * to @p output_path as JSON.
 *
 * Drives the same path the panel's own buttons take: optionally Explain this
 * view, then the question as a follow-up in that conversation. The run starts
 * once the trace has finished opening, so this may be called immediately after
 * rocprofvis_view_open_files. Poll rocprofvis_view_assistant_batch_state for
 * completion; the caller owns the decision to exit.
 *
 * A timeout of zero takes the default. In a build without the assistant the
 * request is refused and the state reports failed, rather than never finishing.
 */
void
rocprofvis_view_start_assistant_batch(const std::string& question,
                                      const std::string& output_path,
                                      bool explain_first, uint32_t timeout_seconds);

rocprofvis_view_assistant_batch_state_t
rocprofvis_view_assistant_batch_state();
