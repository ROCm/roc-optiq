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

#include "ErrorHandling.h"

const char* ERROR_INDEX_OUT_OF_RANGE = "Error! Index out of range!";
const char* ERROR_TRACE_CANNOT_BE_NULL = "Error! Trace reference cannot be NULL!";
const char* ERROR_FLOW_TRACE_CANNOT_BE_NULL = "Error! Flow trace reference cannot be NULL!";
const char* ERROR_STACK_TRACE_CANNOT_BE_NULL = "Error! Stack trace reference cannot be NULL!";
const char* ERROR_SLICE_CANNOT_BE_NULL = "Error! Slice reference cannot be NULL!";
const char* ERROR_TRACE_PARAMETERS_CANNOT_BE_NULL = "Error! Trace parameters reference cannot be NULL!";
const char* ERROR_TRACK_CANNOT_BE_NULL  = "Error! Track reference cannot be NULL!";
const char* ERROR_DATABASE_CANNOT_BE_NULL = "Error! Database reference cannot be NULL!";
const char* ERROR_TRACK_PARAMETERS_NOT_ASSIGNED = "Error! Trace parameters not yet assigned!";
const char* ERROR_FUTURE_CANNOT_BE_NULL = "Error! Future reference cannot be NULL!";
const char* ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH = "Error! Database query parameters mismatch!";
const char* ERROR_METADATA_IS_NOT_LOADED = "Error! Metadata is not yet loaded!";
const char* ERROR_MEMORY_ALLOCATION_FAILURE = "Error! Memory allocation failure!";
const char* ERROR_VIRTUAL_METHOD_PROPERTY = "Error! Trying get property of virtual class!";
const char* ERROR_VIRTUAL_METHOD_CALL = "Error! Trying to call virtual method!";
const char* ERROR_INVALID_PROPERTY_GETTER = "Error! Incorrect property getter!";
const char* ERROR_REFERENCE_POINTER_CANNOT_BE_NULL = "Error! Reference pointer cannot be NULL!";
const char* ERROR_UNSUPPORTED_PROPERTY = "Error! Unsupported property!";
const char* ERROR_TABLE_CANNOT_BE_NULL = "Error! Table reference cannot be NULL!";
const char* ERROR_TABLE_ROW_CANNOT_BE_NULL = "Error! Table row reference cannot be NULL!";
const char* ERROR_EXT_DATA_CANNOT_BE_NULL = "Error! Extended data reference cannot be NULL!";
const char* ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL = "Error! SQL query parameters reference cannot be NULL!";

std::string g_last_error_string;