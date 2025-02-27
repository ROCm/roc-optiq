// MIT License
//
// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
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
#ifndef RPV_DATAMODEL_ERROR_HANDLING_H
#define RPV_DATAMODEL_ERROR_HANDLING_H
#include <cassert>
#include <string>

extern const char* ERROR_INDEX_OUT_OF_RANGE;
extern const char* ERROR_TRACE_CANNOT_BE_NULL;
extern const char* ERROR_TRACK_CANNOT_BE_NULL;
extern const char* ERROR_SLICE_CANNOT_BE_NULL;
extern const char* ERROR_DATABASE_CANNOT_BE_NULL;
extern const char* ERROR_TRACK_PARAMETERS_NOT_ASSIGNED;
extern const char* ERROR_VIRTUAL_METHOD_CALL;
extern const char* ERROR_FUTURE_CANNOT_BE_NULL;
extern const char* ERROR_METADATA_IS_NOT_LOADED;
extern const char* ERROR_TRACE_PARAMETERS_CANNOT_BE_NULL;
extern const char* ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH;
extern const char* ERROR_MEMORY_ALLOCATION_FAILURE;
extern const char* ERROR_VIRTUAL_METHOD_PROPERTY;
extern const char* ERROR_INVALID_PROPERTY_GETTER;
extern const char* ERROR_UNSUPPORTED_PROPERTY;
extern std::string g_last_error_string;

#define LOG(msg) g_last_error_string=msg
#define ADD_LOG(msg) g_last_error_string+=msg

#define ASSERT(cond) assert(cond); (!(cond)) g_last_error_string=#cond

#define ASSERT_MSG(cond, msg) assert(cond && msg); if (!(cond)) g_last_error_string=msg

#define ASSERT_RETURN(cond, retval) assert(cond); if (!(cond)) { g_last_error_string=#cond; return retval;}
    
#define ASSERT_MSG_RETURN(cond, msg, retval) assert(cond && msg); if (!(cond)) { g_last_error_string=msg; return retval; }

#define ASSERT_MSG_BREAK(cond, msg) assert(cond && msg); if (!(cond)) break

#define ASSERT_ALWAYS_MSG_RETURN(msg, retval) assert(false && msg); g_last_error_string=msg; return retval

#endif //RPV_DATAMODEL_ERROR_HANDLING_H