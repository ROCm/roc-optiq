// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_core_assert.h"
#include <string>

namespace RocProfVis
{
namespace DataModel
{

// printf text colors
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[92m"
#define ANSI_COLOR_YELLOW  "\x1b[93m"
#define ANSI_COLOR_BLUE    "\x1b[94m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_GREY    "\x1b[90m"

// Error strings
extern const char* ERROR_INDEX_OUT_OF_RANGE;
extern const char* ERROR_TRACE_CANNOT_BE_NULL;
extern const char* ERROR_TRACK_CANNOT_BE_NULL;
extern const char* ERROR_SLICE_CANNOT_BE_NULL;
extern const char* ERROR_DATABASE_CANNOT_BE_NULL;
extern const char* ERROR_TRACK_PARAMETERS_NOT_ASSIGNED;
extern const char* ERROR_VIRTUAL_METHOD_CALL;
extern const char* ERROR_FUTURE_CANNOT_BE_NULL;
extern const char* ERROR_FUTURE_CANNOT_BE_USED;
extern const char* ERROR_METADATA_IS_NOT_LOADED;
extern const char* ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL;
extern const char* ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH;
extern const char* ERROR_MEMORY_ALLOCATION_FAILURE;
extern const char* ERROR_VIRTUAL_METHOD_PROPERTY;
extern const char* ERROR_INVALID_PROPERTY_GETTER;
extern const char* ERROR_UNSUPPORTED_PROPERTY;
extern const char* ERROR_UNSUPPORTED_FEATURE;
extern const char* ERROR_FLOW_TRACE_CANNOT_BE_NULL;
extern const char* ERROR_STACK_TRACE_CANNOT_BE_NULL;
extern const char* ERROR_TABLE_CANNOT_BE_NULL;
extern const char* ERROR_TABLE_ROW_CANNOT_BE_NULL;
extern const char* ERROR_EXT_DATA_CANNOT_BE_NULL;
extern const char* ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL;
extern const char* ERROR_REFERENCE_POINTER_CANNOT_BE_NULL;
extern const char* ERROR_NODE_KEY_CANNOT_BE_NULL;

}  // namespace DataModel
}  // namespace RocProfVis