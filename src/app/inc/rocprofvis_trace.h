// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#pragma once
#include <string>
#include <map>
#include <vector>
#include <future>
#include "rocprofvis_structs.h"


std::future<bool> rocprofvis_trace_async_load_json_trace(std::string file_path, rocprofvis_trace_data_t& trace_object);
bool rocprofvis_trace_is_loading(std::future<bool>& future);
bool rocprofvis_trace_is_loaded(std::future<bool>& future);
void rocprofvis_trace_setup();      
 