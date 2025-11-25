// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT


#pragma once

#include "rocprofvis_core.h"

#if defined(_DEBUG) && !defined(NDEBUG)
#define ROCPROFVIS_ASSERT_LOG(msg) spdlog::critical(msg)
#else
#define ROCPROFVIS_ASSERT_LOG(msg) ((void)0)
#endif

#define ROCPROFVIS_ASSERT(cond) if (!(cond)) ROCPROFVIS_ASSERT_LOG(#cond); assert(cond) 

#define ROCPROFVIS_ASSERT_MSG(cond, msg) if (!(cond)) ROCPROFVIS_ASSERT_LOG(msg); assert(cond && msg) 

#define ROCPROFVIS_ASSERT_RETURN(cond, retval) if (!(cond)) { ROCPROFVIS_ASSERT_LOG(#cond); return retval;} assert(cond)
    
#define ROCPROFVIS_ASSERT_MSG_RETURN(cond, msg, retval) if (!(cond)) { ROCPROFVIS_ASSERT_LOG(msg); return retval; } assert(cond && msg)

#define ROCPROFVIS_ASSERT_MSG_BREAK(cond, msg) assert(cond && msg); if (!(cond)) break

#define ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(msg, retval) ROCPROFVIS_ASSERT_LOG(msg); assert(false && msg); return retval

#define ROCPROFVIS_UNIMPLEMENTED ROCPROFVIS_ASSERT_MSG(0, "Unimplemented.")
