// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

/* Callback for rocprofvis_core_get_log_entries_ex. Each drained entry is split
 * into severity level (trace=0, debug=1, info=2, warn=3, error=4, critical=5),
 * a formatted timestamp string, and the bare message text (without the level or
 * logger-name prefix). */
typedef void (*rocprofvis_core_process_log_entry_t)(void* user_ptr, int level,
                                                    char const* timestamp,
                                                    char const* message);

#ifdef __cplusplus
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include "spdlog/spdlog.h"
extern "C"
{
#endif

    void rocprofvis_core_enable_log(const char* log_path, int log_level);

    void rocprofvis_core_get_log_entries_ex(void*                               user_ptr,
                                            rocprofvis_core_process_log_entry_t handler);

    /* Returns the path of the active log file, or an empty string if logging has
     * not been enabled. The returned pointer is owned by the core library. */
    const char* rocprofvis_core_get_log_path(void);

#ifdef __cplusplus
}
#endif
