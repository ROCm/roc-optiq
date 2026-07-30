// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#include <string>

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"

namespace RocProfVis
{
namespace Middleware
{

/*
 * Stable string spellings for the controller enums that appear on the wire.
 *
 * The numeric values of the controller enums are an internal detail (several
 * banks are bit-packed at fixed bases and would be meaningless to a client), so
 * the protocol uses lower_snake_case names instead. These names are part of the
 * protocol contract: renaming one is a breaking change, and new enumerators must
 * be appended rather than reordered.
 *
 * Every ToString returns a non-null literal; unrecognised values map to
 * "unknown" so an unexpected enumerator degrades a field rather than the whole
 * response.
 */
namespace Enums
{

char const* ResultToString(rocprofvis_result_t value);
char const* ObjectTypeToString(rocprofvis_controller_object_type_t value);
char const* PrimitiveTypeToString(rocprofvis_controller_primitive_type_t value);
char const* TrackTypeToString(uint64_t value);
char const* GraphTypeToString(uint64_t value);
char const* TableTypeToString(rocprofvis_controller_table_type_t value);
char const* SortOrderToString(rocprofvis_controller_sort_order_t value);
char const* AggregationLevelToString(uint64_t value);
char const* ProcessorTypeToString(uint64_t value);
char const* ThreadTypeToString(uint64_t value);
char const* EventDataCategoryToString(uint64_t value);
char const* FlowDirectionToString(uint64_t value);

/*
 * Client-supplied spellings. Each returns false when the name is not
 * recognised, leaving out untouched, so the caller can report a precise
 * "unknown table_type" style error instead of silently defaulting.
 */
bool TableTypeFromString(const std::string& name, rocprofvis_controller_table_type_t& out);
bool SortOrderFromString(const std::string& name, rocprofvis_controller_sort_order_t& out);

}  // namespace Enums
}  // namespace Middleware
}  // namespace RocProfVis
