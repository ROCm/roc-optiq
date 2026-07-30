// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#include <string>

#include "json.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"

namespace RocProfVis
{
namespace Middleware
{

/*
 * Handle-to-JSON encoders.
 *
 * Each function reads one controller object through the property ABI and
 * returns a self-contained JSON object. Encoders never take ownership of the
 * handle and never free it; the caller owns the array or arguments container
 * the handle was read out of.
 *
 * Properties that the controller reports as unavailable are omitted from the
 * output rather than emitted as zero, so a client can tell "not present" from
 * "present and zero". The one exception is an object's own id, which is always
 * emitted.
 */
namespace Serialize
{

/*
 * Read a string property using the controller's two-call length protocol.
 * @returns True when the property was read; out is cleared otherwise.
 */
bool GetString(rocprofvis_handle_t* handle, rocprofvis_property_t property, uint64_t index,
               std::string& out);

/* Convenience form returning an empty string when the property is unavailable. */
std::string GetStringOrEmpty(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                             uint64_t index);

/* Timeline track metadata, including its topology back-references. */
jt::Json Track(rocprofvis_handle_t* track);

/* A single event from a track or graph result array. */
jt::Json Event(rocprofvis_handle_t* event);

/* A single sample from a track or graph result array. */
jt::Json Sample(rocprofvis_handle_t* sample);

/* One entry of an event's extended-data array (includes argument entries). */
jt::Json ExtDataEntry(rocprofvis_handle_t* ext_data);

/* One frame of an event's call stack. */
jt::Json CallstackFrame(rocprofvis_handle_t* frame);

/* One incoming or outgoing flow-control edge for an event. */
jt::Json FlowControlEntry(rocprofvis_handle_t* flow);

/*
 * Topology objects. Child collections are emitted as id arrays, not nested
 * objects; the full tree is assembled once by the trace.topology method.
 */
jt::Json Node(rocprofvis_handle_t* node);
jt::Json Processor(rocprofvis_handle_t* processor);
jt::Json Process(rocprofvis_handle_t* process);
jt::Json Thread(rocprofvis_handle_t* thread);
jt::Json Queue(rocprofvis_handle_t* queue);
jt::Json Stream(rocprofvis_handle_t* stream);
jt::Json Counter(rocprofvis_handle_t* counter);

/*
 * Table column headers and types plus the table's total row count, which is the
 * count for the whole query rather than the fetched page.
 */
jt::Json TableSchema(rocprofvis_handle_t* table);

/*
 * Table rows from a fetch result array. Values keep their column type: uint64
 * and double columns become JSON numbers, string columns become JSON strings,
 * and object columns become null (the controller has no scalar form for them).
 * @param rows The result array returned by the fetch.
 * @param table The table handle the fetch was issued against, used for types.
 * @param num_columns Column count taken from the table handle.
 */
jt::Json TableRows(rocprofvis_handle_t* rows, rocprofvis_handle_t* table,
                   uint64_t num_columns);

/*
 * Summary metrics tree. Recurses through sub-metrics, emitting the optional
 * per-level fields only where the controller supplies them.
 * @param depth_budget Guards against a cyclic or pathologically deep tree.
 */
jt::Json SummaryMetrics(rocprofvis_handle_t* metrics, uint32_t depth_budget);

}  // namespace Serialize
}  // namespace Middleware
}  // namespace RocProfVis
