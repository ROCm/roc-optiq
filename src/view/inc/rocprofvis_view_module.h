// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <vector>
#include <string>

bool rocprofvis_view_init();
void rocprofvis_view_render();
void rocprofvis_view_destroy();
void rocprofvis_view_set_dpi(float dpi);
void rocprofvis_view_open_files(const std::vector<std::string>& file_paths);

