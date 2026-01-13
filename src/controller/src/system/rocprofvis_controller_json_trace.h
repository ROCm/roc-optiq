// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <map>
#include <vector>
#include <future>

#include "rocprofvis_controller_json_structs.h"

std::future<bool> rocprofvis_controller_json_trace_async_load(std::string file_path, rocprofvis_controller_json_trace_data_t& trace_object);
bool rocprofvis_controller_json_trace_is_loading(std::future<bool>& future);
bool rocprofvis_controller_json_trace_is_loaded(std::future<bool>& future);    
 