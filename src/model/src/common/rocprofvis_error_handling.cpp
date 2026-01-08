// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_error_handling.h"

namespace RocProfVis
{
namespace DataModel
{

const char* ERROR_INDEX_OUT_OF_RANGE = "Error! Index out of range!";
const char* ERROR_TRACE_CANNOT_BE_NULL = "Error! Trace reference cannot be NULL!";
const char* ERROR_FLOW_TRACE_CANNOT_BE_NULL = "Error! Flow trace reference cannot be NULL!";
const char* ERROR_STACK_TRACE_CANNOT_BE_NULL = "Error! Stack trace reference cannot be NULL!";
const char* ERROR_SLICE_CANNOT_BE_NULL = "Error! Slice reference cannot be NULL!";
const char* ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL = "Error! Trace parameters reference cannot be NULL!";
const char* ERROR_TRACK_CANNOT_BE_NULL  = "Error! Track reference cannot be NULL!";
const char* ERROR_DATABASE_CANNOT_BE_NULL = "Error! Database reference cannot be NULL!";
const char* ERROR_TRACK_PARAMETERS_NOT_ASSIGNED = "Error! Trace parameters not yet assigned!";
const char* ERROR_FUTURE_CANNOT_BE_NULL = "Error! Future reference cannot be NULL!";
const char* ERROR_FUTURE_CANNOT_BE_USED = "Error! Future cannot be used at this time!";
const char* ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH = "Error! Database query parameters mismatch!";
const char* ERROR_METADATA_IS_NOT_LOADED = "Error! Metadata is not yet loaded!";
const char* ERROR_MEMORY_ALLOCATION_FAILURE = "Error! Memory allocation failure!";
const char* ERROR_VIRTUAL_METHOD_PROPERTY = "Error! Trying get property of virtual class!";
const char* ERROR_VIRTUAL_METHOD_CALL = "Error! Trying to call virtual method!";
const char* ERROR_INVALID_PROPERTY_GETTER = "Error! Incorrect property getter!";
const char* ERROR_REFERENCE_POINTER_CANNOT_BE_NULL = "Error! Reference pointer cannot be NULL!";
const char* ERROR_UNSUPPORTED_PROPERTY = "Error! Unsupported property!";
const char* ERROR_UNSUPPORTED_FEATURE = "Error! Unsupported feature!";
const char* ERROR_TABLE_CANNOT_BE_NULL = "Error! Table reference cannot be NULL!";
const char* ERROR_TABLE_ROW_CANNOT_BE_NULL = "Error! Table row reference cannot be NULL!";
const char* ERROR_EXT_DATA_CANNOT_BE_NULL = "Error! Extended data reference cannot be NULL!";
const char* ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL = "Error! SQL query parameters reference cannot be NULL!";
const char* ERROR_NODE_KEY_CANNOT_BE_NULL = "Error! Node key reference cannot be NULL!";

}  // namespace DataModel
}  // namespace RocProfVis