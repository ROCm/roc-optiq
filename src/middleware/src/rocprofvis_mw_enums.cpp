// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_mw_enums.h"

#include <stdint.h>

#include <string>

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"

namespace RocProfVis
{
namespace Middleware
{
namespace Enums
{

static char const* const UNKNOWN_NAME = "unknown";

struct table_type_name_t
{
    rocprofvis_controller_table_type_t value;
    char const*                        name;
};

/*
 * The full set of system-trace table types. Compute tables are not exposed by
 * this build of the middleware.
 */
static const table_type_name_t TABLE_TYPE_NAMES[] = {
    { kRPVControllerTableTypeEvents, "events" },
    { kRPVControllerTableTypeSamples, "samples" },
    { kRPVControllerTableTypeSearchResults, "search_results" },
    { kRPVControllerTableTypeSummaryKernelInstances, "summary_kernel_instances" },
    { kRPVControllerTableTypeInstrumentedEvents, "instrumented_events" },
    { kRPVControllerTableTypeDispatchEvents, "dispatch_events" },
    { kRPVControllerTableTypeMemoryAllocationEvents, "memory_allocation_events" },
    { kRPVControllerTableTypeMemoryCopyEvents, "memory_copy_events" },
    { kRPVControllerTableTypeSampledEvents, "sampled_events" },
};

static constexpr size_t TABLE_TYPE_NAME_COUNT =
    sizeof(TABLE_TYPE_NAMES) / sizeof(TABLE_TYPE_NAMES[0]);

char const*
ResultToString(rocprofvis_result_t value)
{
    char const* name = UNKNOWN_NAME;
    switch(value)
    {
        case kRocProfVisResultSuccess: name = "success"; break;
        case kRocProfVisResultUnknownError: name = "unknown_error"; break;
        case kRocProfVisResultTimeout: name = "timeout"; break;
        case kRocProfVisResultNotLoaded: name = "not_loaded"; break;
        case kRocProfVisResultInvalidArgument: name = "invalid_argument"; break;
        case kRocProfVisResultNotSupported: name = "not_supported"; break;
        case kRocProfVisResultReadOnlyError: name = "read_only"; break;
        case kRocProfVisResultMemoryAllocError: name = "memory_alloc_error"; break;
        case kRocProfVisResultInvalidEnum: name = "invalid_enum"; break;
        case kRocProfVisResultInvalidType: name = "invalid_type"; break;
        case kRocProfVisResultOutOfRange: name = "out_of_range"; break;
        case kRocProfVisResultCancelled: name = "cancelled"; break;
        case kRocProfVisResultPending: name = "pending"; break;
        case kRocProfVisResultDuplicate: name = "duplicate"; break;
        case kRocProfVisResultFailedSshCommunication: name = "ssh_failure"; break;
        default: break;
    }
    return name;
}

char const*
ObjectTypeToString(rocprofvis_controller_object_type_t value)
{
    char const* name = UNKNOWN_NAME;
    switch(value)
    {
        case kRPVControllerObjectTypeControllerSystem: name = "controller_system"; break;
        case kRPVControllerObjectTypeTimeline: name = "timeline"; break;
        case kRPVControllerObjectTypeTrack: name = "track"; break;
        case kRPVControllerObjectTypeSample: name = "sample"; break;
        case kRPVControllerObjectTypeEvent: name = "event"; break;
        case kRPVControllerObjectTypeFlowControl: name = "flow_control"; break;
        case kRPVControllerObjectTypeCallstack: name = "callstack"; break;
        case kRPVControllerObjectTypeFuture: name = "future"; break;
        case kRPVControllerObjectTypeGraph: name = "graph"; break;
        case kRPVControllerObjectTypeTable: name = "table"; break;
        case kRPVControllerObjectTypeView: name = "view"; break;
        case kRPVControllerObjectTypeArray: name = "array"; break;
        case kRPVControllerObjectTypeArguments: name = "arguments"; break;
        case kRPVControllerObjectTypeNode: name = "node"; break;
        case kRPVControllerObjectTypeProcessor: name = "processor"; break;
        case kRPVControllerObjectTypeExtData: name = "ext_data"; break;
        case kRPVControllerObjectTypeProcess: name = "process"; break;
        case kRPVControllerObjectTypeThread: name = "thread"; break;
        case kRPVControllerObjectTypeQueue: name = "queue"; break;
        case kRPVControllerObjectTypeStream: name = "stream"; break;
        case kRPVControllerObjectTypeCounter: name = "counter"; break;
        case kRPVControllerObjectTypeSummary: name = "summary"; break;
        case kRPVControllerObjectTypeSummaryMetrics: name = "summary_metrics"; break;
        case kRPVControllerObjectTypeEventArgument: name = "event_argument"; break;
        case kRPVControllerObjectTypeTopologyNode: name = "topology_node"; break;
        case kRPVControllerObjectTypeControllerCompute: name = "controller_compute"; break;
        case kRPVControllerObjectTypeWorkload: name = "workload"; break;
        case kRPVControllerObjectTypeKernel: name = "kernel"; break;
        case kRPVControllerObjectTypeMetricsContainer: name = "metrics_container"; break;
        case kRPVControllerObjectTypeRoofline: name = "roofline"; break;
        case kRPVControllerObjectTypePCSampling: name = "pc_sampling"; break;
        default: break;
    }
    return name;
}

char const*
PrimitiveTypeToString(rocprofvis_controller_primitive_type_t value)
{
    char const* name = UNKNOWN_NAME;
    switch(value)
    {
        case kRPVControllerPrimitiveTypeUInt64: name = "uint64"; break;
        case kRPVControllerPrimitiveTypeDouble: name = "double"; break;
        case kRPVControllerPrimitiveTypeString: name = "string"; break;
        case kRPVControllerPrimitiveTypeObject: name = "object"; break;
        default: break;
    }
    return name;
}

char const*
TrackTypeToString(uint64_t value)
{
    char const* name = UNKNOWN_NAME;
    if(value == kRPVControllerTrackTypeSamples)
    {
        name = "samples";
    }
    else if(value == kRPVControllerTrackTypeEvents)
    {
        name = "events";
    }
    return name;
}

char const*
GraphTypeToString(uint64_t value)
{
    char const* name = UNKNOWN_NAME;
    if(value == kRPVControllerGraphTypeLine)
    {
        name = "line";
    }
    else if(value == kRPVControllerGraphTypeFlame)
    {
        name = "flame";
    }
    return name;
}

char const*
TableTypeToString(rocprofvis_controller_table_type_t value)
{
    char const* name = UNKNOWN_NAME;
    for(size_t i = 0; i < TABLE_TYPE_NAME_COUNT; i++)
    {
        if(TABLE_TYPE_NAMES[i].value == value)
        {
            name = TABLE_TYPE_NAMES[i].name;
            break;
        }
    }
    return name;
}

char const*
SortOrderToString(rocprofvis_controller_sort_order_t value)
{
    return (value == kRPVControllerSortOrderDescending) ? "desc" : "asc";
}

char const*
AggregationLevelToString(uint64_t value)
{
    char const* name = UNKNOWN_NAME;
    if(value == kRPVControllerSummaryAggregationLevelTrace)
    {
        name = "trace";
    }
    else if(value == kRPVControllerSummaryAggregationLevelNode)
    {
        name = "node";
    }
    else if(value == kRPVControllerSummaryAggregationLevelProcessor)
    {
        name = "processor";
    }
    return name;
}

char const*
ProcessorTypeToString(uint64_t value)
{
    char const* name = UNKNOWN_NAME;
    switch(value)
    {
        case kRPVControllerProcessorTypeUndefined: name = "undefined"; break;
        case kRPVControllerProcessorTypeGPU: name = "gpu"; break;
        case kRPVControllerProcessorTypeCPU: name = "cpu"; break;
        case kRPVControllerProcessorTypeNIC: name = "nic"; break;
        default: break;
    }
    return name;
}

char const*
ThreadTypeToString(uint64_t value)
{
    char const* name = UNKNOWN_NAME;
    switch(value)
    {
        case kRPVControllerThreadTypeUndefined: name = "undefined"; break;
        case kRPVControllerThreadTypeInstrumented: name = "instrumented"; break;
        case kRPVControllerThreadTypeSampled: name = "sampled"; break;
        default: break;
    }
    return name;
}

char const*
EventDataCategoryToString(uint64_t value)
{
    char const* name = UNKNOWN_NAME;
    switch(value)
    {
        case kRocProfVisEventEssentialDataInternal: name = "internal"; break;
        case kRocProfVisEventEssentialDataUncategorized: name = "uncategorized"; break;
        case kRocProfVisEventEssentialDataId: name = "id"; break;
        case kRocProfVisEventEssentialDataCategory: name = "category"; break;
        case kRocProfVisEventEssentialDataName: name = "name"; break;
        case kRocProfVisEventEssentialDataStart: name = "start"; break;
        case kRocProfVisEventEssentialDataEnd: name = "end"; break;
        case kRocProfVisEventEssentialDataDuration: name = "duration"; break;
        case kRocProfVisEventEssentialDataNode: name = "node"; break;
        case kRocProfVisEventEssentialDataProcess: name = "process"; break;
        case kRocProfVisEventEssentialDataThread: name = "thread"; break;
        case kRocProfVisEventEssentialDataAgentType: name = "agent_type"; break;
        case kRocProfVisEventEssentialDataAgentIndex: name = "agent_index"; break;
        case kRocProfVisEventEssentialDataQueue: name = "queue"; break;
        case kRocProfVisEventEssentialDataStream: name = "stream"; break;
        case kRocProfVisEventEssentialDataTrack: name = "track"; break;
        case kRocProfVisEventEssentialDataStreamTrack: name = "stream_track"; break;
        case kRocProfVisEventEssentialDataLevel: name = "level"; break;
        case kRocProfVisEventEssentialDataStreamLevel: name = "stream_level"; break;
        case kRocProfVisEventArgumentData: name = "argument"; break;
        default: break;
    }
    return name;
}

char const*
FlowDirectionToString(uint64_t value)
{
    return (value == 1) ? "incoming" : "outgoing";
}

bool
TableTypeFromString(const std::string& name, rocprofvis_controller_table_type_t& out)
{
    bool matched = false;
    for(size_t i = 0; i < TABLE_TYPE_NAME_COUNT; i++)
    {
        if(name == TABLE_TYPE_NAMES[i].name)
        {
            out     = TABLE_TYPE_NAMES[i].value;
            matched = true;
            break;
        }
    }
    return matched;
}

bool
SortOrderFromString(const std::string& name, rocprofvis_controller_sort_order_t& out)
{
    bool matched = true;
    if(name == "asc" || name == "ascending")
    {
        out = kRPVControllerSortOrderAscending;
    }
    else if(name == "desc" || name == "descending")
    {
        out = kRPVControllerSortOrderDescending;
    }
    else
    {
        matched = false;
    }
    return matched;
}

}  // namespace Enums
}  // namespace Middleware
}  // namespace RocProfVis
