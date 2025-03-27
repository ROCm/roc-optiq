// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <string>
#include <map>
#include <vector>
#include <future>

#include "rocprofvis_controller_json_structs.h"

std::future<bool> rocprofvis_controller_json_trace_async_load(std::string file_path, rocprofvis_controller_json_trace_data_t& trace_object);
bool rocprofvis_controller_json_trace_is_loading(std::future<bool>& future);
bool rocprofvis_controller_json_trace_is_loaded(std::future<bool>& future);    
 