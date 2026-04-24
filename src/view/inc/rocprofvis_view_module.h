// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <string>
#include <vector>

typedef enum rocprofvis_view_render_options_t
{
    kRocProfVisViewRenderOption_None        = 0,
    kRocProfVisViewRenderOption_RequestExit = 1,
} rocprofvis_view_render_options_t;

typedef enum rocprofvis_view_notification_t
{
    kRocProfVisViewNotification_Exit_App = 1,
    kRocProfVisViewNotification_Toggle_Fullscreen = 2,
} rocprofvis_view_notification_t;

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
rocprofvis_view_set_dpi(float dpi);

void
rocprofvis_view_open_files(const std::vector<std::string>& file_paths);

void
rocprofvis_view_set_fullscreen_state(bool is_fullscreen);

std::string
rocprofvis_get_application_config_path();

bool
rocprofvis_view_is_remote_display_session();
