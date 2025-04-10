// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "rocprofvis_core.h"

#if defined(_DEBUG) && !defined(NDEBUG)
#define ROCPROFVIS_ASSERT_LOG(msg) spdlog::critical(msg)
#else
#define ROCPROFVIS_ASSERT_LOG(msg) 
#endif

#define ROCPROFVIS_ASSERT(cond) if (!(cond)) ROCPROFVIS_ASSERT_LOG(#cond); assert(cond) 

#define ROCPROFVIS_ASSERT_MSG(cond, msg) if (!(cond)) ROCPROFVIS_ASSERT_LOG(msg); assert(cond && msg) 

#define ROCPROFVIS_ASSERT_RETURN(cond, retval) if (!(cond)) { ROCPROFVIS_ASSERT_LOG(#cond); return retval;} assert(cond)
    
#define ROCPROFVIS_ASSERT_MSG_RETURN(cond, msg, retval) if (!(cond)) { ROCPROFVIS_ASSERT_LOG(msg); return retval; } assert(cond && msg)

#define ROCPROFVIS_ASSERT_MSG_BREAK(cond, msg) assert(cond && msg); if (!(cond)) break

#define ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(msg, retval) ROCPROFVIS_ASSERT_LOG(msg); assert(false && msg); return retval

#define ROCPROFVIS_UNIMPLEMENTED ROCPROFVIS_ASSERT_MSG(0, "Unimplemented.")
