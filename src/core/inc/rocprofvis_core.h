// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

typedef void (*rocprofvis_core_process_log_t)(void* user_ptr, char const* log);

#ifdef __cplusplus
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include "spdlog/spdlog.h"
extern "C"
{
#endif

    void rocprofvis_core_enable_log(const char* log_path);

    void rocprofvis_core_get_log_entries(void* user_ptr, rocprofvis_core_process_log_t handler);

#ifdef __cplusplus
}
#endif
