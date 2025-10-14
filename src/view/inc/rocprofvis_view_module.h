// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

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
} rocprofvis_view_notification_t;

bool
rocprofvis_view_init(std::function<void(int)> notification_callback);

void
rocprofvis_view_render(const rocprofvis_view_render_options_t& render_options);

void
rocprofvis_view_destroy();

void
rocprofvis_view_set_dpi(float dpi);

void
rocprofvis_view_open_files(const std::vector<std::string>& file_paths);

std::string
rocprofvis_get_application_config_path();
