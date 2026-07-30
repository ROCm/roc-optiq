// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_mw_serialize.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "json.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_mw_enums.h"
#include "rocprofvis_mw_json.h"

namespace RocProfVis
{
namespace Middleware
{
namespace Serialize
{

/*
 * Optional-property readers. The controller returns a failure code for a
 * property an object does not carry (a sample track has no queue, a counter
 * track has no thread), which is a normal outcome rather than an error, so
 * these report presence instead of logging.
 */
static bool
TryUInt(rocprofvis_handle_t* handle, rocprofvis_property_t property, uint64_t index,
        uint64_t& out)
{
    return rocprofvis_controller_get_uint64(handle, property, index, &out) ==
           kRocProfVisResultSuccess;
}

static bool
TryDouble(rocprofvis_handle_t* handle, rocprofvis_property_t property, uint64_t index,
          double& out)
{
    return rocprofvis_controller_get_double(handle, property, index, &out) ==
           kRocProfVisResultSuccess;
}

static bool
TryObject(rocprofvis_handle_t* handle, rocprofvis_property_t property, uint64_t index,
          rocprofvis_handle_t** out)
{
    return rocprofvis_controller_get_object(handle, property, index, out) ==
               kRocProfVisResultSuccess &&
           *out != nullptr;
}

/* Set key only when the property is present, so absent stays absent in JSON. */
static void
PutUInt(jt::Json& object, char const* key, rocprofvis_handle_t* handle,
        rocprofvis_property_t property)
{
    uint64_t value = 0;
    if(TryUInt(handle, property, 0, value))
    {
        object[key] = Json::MakeUInt(value);
    }
}

static void
PutDouble(jt::Json& object, char const* key, rocprofvis_handle_t* handle,
          rocprofvis_property_t property)
{
    double value = 0.0;
    if(TryDouble(handle, property, 0, value))
    {
        object[key] = Json::MakeDouble(value);
    }
}

static void
PutString(jt::Json& object, char const* key, rocprofvis_handle_t* handle,
          rocprofvis_property_t property)
{
    std::string value;
    if(GetString(handle, property, 0, value) && !value.empty())
    {
        object[key] = jt::Json(value);
    }
}

/*
 * Emit the id of a linked object (track -> queue, counter -> processor, ...)
 * without recursing into it, keeping cross-references flat.
 */
static void
PutLinkedId(jt::Json& object, char const* key, rocprofvis_handle_t* handle,
            rocprofvis_property_t link_property, rocprofvis_property_t id_property)
{
    rocprofvis_handle_t* linked = nullptr;
    if(TryObject(handle, link_property, 0, &linked))
    {
        uint64_t id = 0;
        if(TryUInt(linked, id_property, 0, id))
        {
            object[key] = Json::MakeUInt(id);
        }
    }
}

/* Collect the ids of an indexed child collection into a JSON array. */
static jt::Json
CollectChildIds(rocprofvis_handle_t* parent, rocprofvis_property_t count_property,
                rocprofvis_property_t child_property, rocprofvis_property_t id_property)
{
    jt::Json ids   = Json::MakeArray();
    uint64_t count = 0;
    if(TryUInt(parent, count_property, 0, count))
    {
        for(uint64_t i = 0; i < count; i++)
        {
            rocprofvis_handle_t* child = nullptr;
            if(TryObject(parent, child_property, i, &child))
            {
                uint64_t id = 0;
                if(TryUInt(child, id_property, 0, id))
                {
                    Json::Append(ids, Json::MakeUInt(id));
                }
            }
        }
    }
    return ids;
}

bool
GetString(rocprofvis_handle_t* handle, rocprofvis_property_t property, uint64_t index,
          std::string& out)
{
    out.clear();
    uint32_t            length = 0;
    rocprofvis_result_t result =
        rocprofvis_controller_get_string(handle, property, index, nullptr, &length);
    bool read = false;
    if(result == kRocProfVisResultSuccess && length > 0)
    {
        out.resize(length);
        result = rocprofvis_controller_get_string(handle, property, index,
                                                  const_cast<char*>(out.c_str()), &length);
        if(result == kRocProfVisResultSuccess)
        {
            /*
             * The controller reports the length it wrote, which may exclude a
             * terminator it appended. Trim any trailing NUL so the value does
             * not carry an embedded terminator into JSON.
             */
            while(!out.empty() && out.back() == '\0')
            {
                out.pop_back();
            }
            read = true;
        }
        else
        {
            out.clear();
        }
    }
    return read;
}

std::string
GetStringOrEmpty(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                 uint64_t index)
{
    std::string value;
    GetString(handle, property, index, value);
    return value;
}

jt::Json
Track(rocprofvis_handle_t* track)
{
    jt::Json object = Json::MakeObject();
    PutUInt(object, "id", track, kRPVControllerTrackId);

    uint64_t track_type = 0;
    if(TryUInt(track, kRPVControllerTrackType, 0, track_type))
    {
        object["type"] = jt::Json(std::string(Enums::TrackTypeToString(track_type)));
    }

    PutDouble(object, "min_timestamp", track, kRPVControllerTrackMinTimestamp);
    PutDouble(object, "max_timestamp", track, kRPVControllerTrackMaxTimestamp);
    PutUInt(object, "num_entries", track, kRPVControllerTrackNumberOfEntries);
    PutString(object, "category", track, kRPVControllerTrackCategory);
    PutString(object, "main_name", track, kRPVControllerTrackMainName);
    PutString(object, "sub_name", track, kRPVControllerTrackSubName);
    PutString(object, "description", track, kRPVControllerTrackDescription);
    PutDouble(object, "min_value", track, kRPVControllerTrackMinValue);
    PutDouble(object, "max_value", track, kRPVControllerTrackMaxValue);
    PutUInt(object, "agent_id_or_pid", track, kRPVControllerTrackAgentIdOrPid);
    PutUInt(object, "queue_id_or_tid", track, kRPVControllerTrackQueueIdOrTid);
    PutUInt(object, "instance_id", track, kRPVControllerTrackInstanceId);
    PutUInt(object, "file_id", track, kRPVControllerTrackFileId);
    PutUInt(object, "order_ranking", track, kRPVControllerTrackOrderRanking);

    PutLinkedId(object, "node_id", track, kRPVControllerTrackNode, kRPVControllerNodeId);
    PutLinkedId(object, "processor_id", track, kRPVControllerTrackProcessor,
                kRPVControllerProcessorId);
    PutLinkedId(object, "thread_id", track, kRPVControllerTrackThread,
                kRPVControllerThreadId);
    PutLinkedId(object, "queue_id", track, kRPVControllerTrackQueue, kRPVControllerQueueId);
    PutLinkedId(object, "stream_id", track, kRPVControllerTrackStream,
                kRPVControllerStreamId);
    PutLinkedId(object, "counter_id", track, kRPVControllerTrackCounter,
                kRPVControllerCounterId);

    uint64_t num_op_types = 0;
    if(TryUInt(track, kRPVControllerTrackNumberOfOperationTypes, 0, num_op_types) &&
       num_op_types > 0)
    {
        jt::Json op_types = Json::MakeArray();
        for(uint64_t i = 0; i < num_op_types; i++)
        {
            uint64_t op_type = 0;
            if(TryUInt(track, kRPVControllerTrackOperationTypeIndexed, i, op_type))
            {
                Json::Append(op_types, Json::MakeUInt(op_type));
            }
        }
        object["operation_types"] = op_types;
    }

    return object;
}

jt::Json
Event(rocprofvis_handle_t* event)
{
    jt::Json object = Json::MakeObject();
    PutUInt(object, "id", event, kRPVControllerEventId);
    PutDouble(object, "start_timestamp", event, kRPVControllerEventStartTimestamp);
    PutDouble(object, "end_timestamp", event, kRPVControllerEventEndTimestamp);
    PutUInt(object, "level", event, kRPVControllerEventLevel);
    PutUInt(object, "num_children", event, kRPVControllerEventNumChildren);
    PutString(object, "name", event, kRPVControllerEventName);
    PutString(object, "category", event, kRPVControllerEventCategory);

    /*
     * A coalesced (level-of-detail) event reports the name of its largest
     * constituent separately; clients use it as the label for the merged block.
     */
    PutString(object, "top_combined_name", event, kRPVControllerEventTopCombinedName);
    return object;
}

jt::Json
Sample(rocprofvis_handle_t* sample)
{
    jt::Json object = Json::MakeObject();
    PutUInt(object, "id", sample, kRPVControllerSampleId);
    PutDouble(object, "timestamp", sample, kRPVControllerSampleTimestamp);
    PutDouble(object, "end_timestamp", sample, kRPVControllerSampleEndTimestamp);
    PutDouble(object, "value", sample, kRPVControllerSampleValue);

    uint64_t num_children = 0;
    if(TryUInt(sample, kRPVControllerSampleNumChildren, 0, num_children) &&
       num_children > 1)
    {
        /*
         * Synthetic level-of-detail samples carry the aggregate of the samples
         * they replaced; a real sample has no useful values here.
         */
        object["num_children"] = Json::MakeUInt(num_children);
        PutDouble(object, "child_min", sample, kRPVControllerSampleChildMin);
        PutDouble(object, "child_mean", sample, kRPVControllerSampleChildMean);
        PutDouble(object, "child_median", sample, kRPVControllerSampleChildMedian);
        PutDouble(object, "child_max", sample, kRPVControllerSampleChildMax);
        PutDouble(object, "child_min_timestamp", sample,
                  kRPVControllerSampleChildMinTimestamp);
        PutDouble(object, "child_max_timestamp", sample,
                  kRPVControllerSampleChildMaxTimestamp);
    }
    return object;
}

jt::Json
ExtDataEntry(rocprofvis_handle_t* ext_data)
{
    jt::Json object = Json::MakeObject();
    PutString(object, "category", ext_data, kRPVControllerExtDataCategory);
    PutString(object, "name", ext_data, kRPVControllerExtDataName);
    PutString(object, "value", ext_data, kRPVControllerExtDataValue);

    uint64_t value_type = 0;
    if(TryUInt(ext_data, kRPVControllerExtDataType, 0, value_type))
    {
        object["value_type"] = jt::Json(std::string(Enums::PrimitiveTypeToString(
            static_cast<rocprofvis_controller_primitive_type_t>(value_type))));

        /*
         * Numeric entries are also exposed unstringified so a client does not
         * have to re-parse timestamps and durations out of display text.
         */
        if(value_type == kRPVControllerPrimitiveTypeUInt64)
        {
            uint64_t numeric = 0;
            if(TryUInt(ext_data, kRPVControllerExtDataValue, 0, numeric))
            {
                object["numeric_value"] = Json::MakeUInt(numeric);
            }
        }
        else if(value_type == kRPVControllerPrimitiveTypeDouble)
        {
            double numeric = 0.0;
            if(TryDouble(ext_data, kRPVControllerExtDataValue, 0, numeric))
            {
                object["numeric_value"] = Json::MakeDouble(numeric);
            }
        }
    }

    uint64_t category_enum = 0;
    if(TryUInt(ext_data, kRPVControllerExtDataCategoryEnum, 0, category_enum))
    {
        object["kind"] = jt::Json(std::string(Enums::EventDataCategoryToString(category_enum)));

        if(category_enum == kRocProfVisEventArgumentData)
        {
            PutUInt(object, "argument_position", ext_data,
                    kRPVControllerEventArgumentPosition);
            PutString(object, "argument_type", ext_data, kRPVControllerEventArgumentType);
        }
    }
    return object;
}

jt::Json
CallstackFrame(rocprofvis_handle_t* frame)
{
    jt::Json object = Json::MakeObject();
    PutString(object, "file", frame, kRPVControllerCallstackFile);
    PutString(object, "name", frame, kRPVControllerCallstackName);
    PutString(object, "line_name", frame, kRPVControllerCallstackLineName);
    PutUInt(object, "pc", frame, kRPVControllerCallstackPc);
    PutUInt(object, "line_address", frame, kRPVControllerCallstackLineAddress);
    PutUInt(object, "depth", frame, kRPVControllerCallstackDepth);
    PutUInt(object, "region_id", frame, kRPVControllerCallstackRegionId);
    return object;
}

jt::Json
FlowControlEntry(rocprofvis_handle_t* flow)
{
    jt::Json object = Json::MakeObject();

    /* kRPVControllerFlowControltId is the target event id; the name is a typo
     * in the controller header that is preserved here for source fidelity. */
    PutUInt(object, "event_id", flow, kRPVControllerFlowControltId);
    PutUInt(object, "track_id", flow, kRPVControllerFlowControlTrackId);
    PutUInt(object, "level", flow, kRPVControllerFlowControlLevel);
    PutDouble(object, "timestamp", flow, kRPVControllerFlowControlTimestamp);
    PutDouble(object, "end_timestamp", flow, kRPVControllerFlowControlEndTimestamp);
    PutString(object, "name", flow, kRPVControllerFlowControlName);

    uint64_t direction = 0;
    if(TryUInt(flow, kRPVControllerFlowControlDirection, 0, direction))
    {
        object["direction"] = jt::Json(std::string(Enums::FlowDirectionToString(direction)));
    }
    return object;
}

/*
 * Topology encoders below read a deliberately narrow set of properties.
 *
 * Node, Processor, Process, Thread, Queue, Stream and Counter are all the same
 * controller class backed by one type-erased property map keyed by the property
 * enum. Reading a key with the wrong getter is a fatal assert in the model
 * (ERROR_INVALID_PROPERTY_GETTER), and the stored type cannot be queried
 * through the C ABI, so a property can only be read once its type is known
 * statically. These encoders therefore mirror the property/getter pairings the
 * view's Parse*Data functions have proven against real traces. A missing key is
 * harmless; a mistyped one takes the process down. Do not add a field here
 * without confirming its stored type.
 *
 * Object-valued links are safe regardless: the controller answers those from an
 * explicit switch and reports unsupported ones as an error.
 */
jt::Json
Node(rocprofvis_handle_t* node)
{
    jt::Json object = Json::MakeObject();
    PutUInt(object, "id", node, kRPVControllerNodeId);
    PutString(object, "host_name", node, kRPVControllerNodeHostName);
    PutString(object, "os_name", node, kRPVControllerNodeOSName);
    PutString(object, "os_release", node, kRPVControllerNodeOSRelease);
    PutString(object, "os_version", node, kRPVControllerNodeOSVersion);
    object["processor_ids"] = CollectChildIds(node, kRPVControllerNodeNumProcessors,
                                              kRPVControllerNodeProcessorIndexed,
                                              kRPVControllerProcessorId);
    object["process_ids"]   = CollectChildIds(node, kRPVControllerNodeNumProcesses,
                                              kRPVControllerNodeProcessIndexed,
                                              kRPVControllerProcessId);
    return object;
}

jt::Json
Processor(rocprofvis_handle_t* processor)
{
    jt::Json object = Json::MakeObject();
    PutUInt(object, "id", processor, kRPVControllerProcessorId);
    PutString(object, "product_name", processor, kRPVControllerProcessorProductName);
    PutUInt(object, "type_index", processor, kRPVControllerProcessorTypeIndex);

    /* Stored as a string ("GPU"/"CPU"/"NIC"); the controller maps it to the enum. */
    uint64_t processor_type = 0;
    if(TryUInt(processor, kRPVControllerProcessorType, 0, processor_type))
    {
        object["type"] = jt::Json(std::string(Enums::ProcessorTypeToString(processor_type)));
    }

    object["queue_ids"]   = CollectChildIds(processor, kRPVControllerProcessorNumQueues,
                                            kRPVControllerProcessorQueueIndexed,
                                            kRPVControllerQueueId);
    object["counter_ids"] = CollectChildIds(processor, kRPVControllerProcessorNumCounters,
                                            kRPVControllerProcessorCounterIndexed,
                                            kRPVControllerCounterId);
    return object;
}

jt::Json
Process(rocprofvis_handle_t* process)
{
    jt::Json object = Json::MakeObject();
    PutUInt(object, "id", process, kRPVControllerProcessId);
    PutDouble(object, "start_time", process, kRPVControllerProcessStartTime);
    PutDouble(object, "end_time", process, kRPVControllerProcessEndTime);
    PutString(object, "command", process, kRPVControllerProcessCommand);
    PutString(object, "environment", process, kRPVControllerProcessEnvironment);
    object["thread_ids"] = CollectChildIds(process, kRPVControllerProcessNumThreads,
                                           kRPVControllerProcessThreadIndexed,
                                           kRPVControllerThreadId);
    object["stream_ids"] = CollectChildIds(process, kRPVControllerProcessNumStreams,
                                           kRPVControllerProcessStreamIndexed,
                                           kRPVControllerStreamId);
    return object;
}

jt::Json
Thread(rocprofvis_handle_t* thread)
{
    jt::Json object = Json::MakeObject();
    PutUInt(object, "id", thread, kRPVControllerThreadId);
    PutUInt(object, "tid", thread, kRPVControllerThreadTid);
    PutString(object, "name", thread, kRPVControllerThreadName);
    PutDouble(object, "start_time", thread, kRPVControllerThreadStartTime);
    PutDouble(object, "end_time", thread, kRPVControllerThreadEndTime);
    PutLinkedId(object, "node_id", thread, kRPVControllerThreadNode, kRPVControllerNodeId);
    PutLinkedId(object, "process_id", thread, kRPVControllerThreadProcess,
                kRPVControllerProcessId);
    PutLinkedId(object, "track_id", thread, kRPVControllerThreadTrack,
                kRPVControllerTrackId);

    uint64_t thread_type = 0;
    if(TryUInt(thread, kRPVControllerThreadType, 0, thread_type))
    {
        object["type"] = jt::Json(std::string(Enums::ThreadTypeToString(thread_type)));
    }
    return object;
}

jt::Json
Queue(rocprofvis_handle_t* queue)
{
    jt::Json object = Json::MakeObject();
    PutUInt(object, "id", queue, kRPVControllerQueueId);
    PutString(object, "name", queue, kRPVControllerQueueName);
    PutLinkedId(object, "node_id", queue, kRPVControllerQueueNode, kRPVControllerNodeId);
    PutLinkedId(object, "process_id", queue, kRPVControllerQueueProcess,
                kRPVControllerProcessId);
    PutLinkedId(object, "processor_id", queue, kRPVControllerQueueProcessor,
                kRPVControllerProcessorId);
    PutLinkedId(object, "track_id", queue, kRPVControllerQueueTrack, kRPVControllerTrackId);
    return object;
}

jt::Json
Stream(rocprofvis_handle_t* stream)
{
    jt::Json object = Json::MakeObject();
    PutUInt(object, "id", stream, kRPVControllerStreamId);
    PutString(object, "name", stream, kRPVControllerStreamName);
    PutLinkedId(object, "node_id", stream, kRPVControllerStreamNode, kRPVControllerNodeId);
    PutLinkedId(object, "process_id", stream, kRPVControllerStreamProcess,
                kRPVControllerProcessId);
    PutLinkedId(object, "track_id", stream, kRPVControllerStreamTrack,
                kRPVControllerTrackId);
    object["processor_ids"] = CollectChildIds(stream, kRPVControllerStreamNumProcessors,
                                              kRPVControllerStreamProcessorIndexed,
                                              kRPVControllerProcessorId);
    return object;
}

jt::Json
Counter(rocprofvis_handle_t* counter)
{
    jt::Json object = Json::MakeObject();
    PutUInt(object, "id", counter, kRPVControllerCounterId);
    PutString(object, "name", counter, kRPVControllerCounterName);
    PutString(object, "description", counter, kRPVControllerCounterDescription);
    PutString(object, "units", counter, kRPVControllerCounterUnits);

    /* Stored as a string naming the type, not as a type enumerator. */
    PutString(object, "value_type", counter, kRPVControllerCounterValueType);

    PutLinkedId(object, "node_id", counter, kRPVControllerCounterNode, kRPVControllerNodeId);
    PutLinkedId(object, "process_id", counter, kRPVControllerCounterProcess,
                kRPVControllerProcessId);
    PutLinkedId(object, "processor_id", counter, kRPVControllerCounterProcessor,
                kRPVControllerProcessorId);
    PutLinkedId(object, "track_id", counter, kRPVControllerCounterTrack,
                kRPVControllerTrackId);
    return object;
}

jt::Json
TableSchema(rocprofvis_handle_t* table)
{
    jt::Json object      = Json::MakeObject();
    uint64_t num_columns = 0;
    uint64_t num_rows    = 0;
    TryUInt(table, kRPVControllerTableNumColumns, 0, num_columns);
    TryUInt(table, kRPVControllerTableNumRows, 0, num_rows);

    jt::Json columns = Json::MakeArray();
    for(uint64_t i = 0; i < num_columns; i++)
    {
        jt::Json column     = Json::MakeObject();
        column["name"]      = jt::Json(GetStringOrEmpty(
            table, kRPVControllerTableColumnHeaderIndexed, i));
        uint64_t column_type = 0;
        if(TryUInt(table, kRPVControllerTableColumnTypeIndexed, i, column_type))
        {
            column["type"] = jt::Json(std::string(Enums::PrimitiveTypeToString(
                static_cast<rocprofvis_controller_primitive_type_t>(column_type))));
        }
        Json::Append(columns, column);
    }

    object["columns"] = columns;

    /* Row count for the whole query, not for the page this fetch returned. */
    object["total_rows"] = Json::MakeUInt(num_rows);
    return object;
}

jt::Json
TableRows(rocprofvis_handle_t* rows, rocprofvis_handle_t* table, uint64_t num_columns)
{
    jt::Json result   = Json::MakeArray();
    uint64_t num_rows = 0;
    if(!TryUInt(rows, kRPVControllerArrayNumEntries, 0, num_rows))
    {
        return result;
    }

    /*
     * Column types are a property of the table, not of the row, so they are
     * read once instead of once per cell as the view layer does.
     */
    std::vector<uint64_t> column_types;
    column_types.reserve(num_columns);
    for(uint64_t i = 0; i < num_columns; i++)
    {
        uint64_t column_type = kRPVControllerPrimitiveTypeString;
        TryUInt(table, kRPVControllerTableColumnTypeIndexed, i, column_type);
        column_types.push_back(column_type);
    }

    for(uint64_t i = 0; i < num_rows; i++)
    {
        rocprofvis_handle_t* row = nullptr;
        if(!TryObject(rows, kRPVControllerArrayEntryIndexed, i, &row))
        {
            continue;
        }

        jt::Json row_json = Json::MakeArray();
        for(uint64_t j = 0; j < num_columns; j++)
        {
            jt::Json cell;
            switch(column_types[j])
            {
                case kRPVControllerPrimitiveTypeUInt64:
                {
                    uint64_t value = 0;
                    if(TryUInt(row, kRPVControllerArrayEntryIndexed, j, value))
                    {
                        cell = Json::MakeUInt(value);
                    }
                    break;
                }
                case kRPVControllerPrimitiveTypeDouble:
                {
                    double value = 0.0;
                    if(TryDouble(row, kRPVControllerArrayEntryIndexed, j, value))
                    {
                        cell = Json::MakeDouble(value);
                    }
                    break;
                }
                case kRPVControllerPrimitiveTypeString:
                {
                    std::string value;
                    if(GetString(row, kRPVControllerArrayEntryIndexed, j, value))
                    {
                        cell = jt::Json(value);
                    }
                    break;
                }
                default:
                {
                    /* Object columns have no scalar form; emitted as null. */
                    break;
                }
            }
            Json::Append(row_json, cell);
        }
        Json::Append(result, row_json);
    }
    return result;
}

jt::Json
SummaryMetrics(rocprofvis_handle_t* metrics, uint32_t depth_budget)
{
    jt::Json object = Json::MakeObject();
    if(metrics == nullptr || depth_budget == 0)
    {
        return object;
    }

    uint64_t level = 0;
    if(TryUInt(metrics, kRPVControllerSummaryMetricPropertyAggregationLevel, 0, level))
    {
        object["level"] = jt::Json(std::string(Enums::AggregationLevelToString(level)));
    }

    PutUInt(object, "id", metrics, kRPVControllerSummaryMetricPropertyId);
    PutString(object, "name", metrics, kRPVControllerSummaryMetricPropertyName);
    PutUInt(object, "processor_type_index", metrics,
            kRPVControllerSummaryMetricPropertyProcessorTypeIndex);
    PutDouble(object, "gpu_gfx_util", metrics,
              kRPVControllerSummaryMetricPropertyGpuGfxUtil);
    PutDouble(object, "gpu_mem_util", metrics,
              kRPVControllerSummaryMetricPropertyGpuMemUtil);
    PutDouble(object, "kernels_exec_time_total", metrics,
              kRPVControllerSummaryMetricPropertyKernelsExecTimeTotal);

    uint64_t processor_type = 0;
    if(TryUInt(metrics, kRPVControllerSummaryMetricPropertyProcessorType, 0,
               processor_type))
    {
        object["processor_type"] =
            jt::Json(std::string(Enums::ProcessorTypeToString(processor_type)));
    }

    uint64_t num_kernels = 0;
    if(TryUInt(metrics, kRPVControllerSummaryMetricPropertyNumKernels, 0, num_kernels) &&
       num_kernels > 0)
    {
        jt::Json kernels = Json::MakeArray();
        for(uint64_t i = 0; i < num_kernels; i++)
        {
            jt::Json kernel = Json::MakeObject();
            kernel["name"]  = jt::Json(GetStringOrEmpty(
                metrics, kRPVControllerSummaryMetricPropertyKernelNameIndexed, i));

            uint64_t invocations = 0;
            if(TryUInt(metrics, kRPVControllerSummaryMetricPropertyKernelInvocationsIndexed,
                       i, invocations))
            {
                kernel["invocations"] = Json::MakeUInt(invocations);
            }

            double value = 0.0;
            if(TryDouble(metrics,
                         kRPVControllerSummaryMetricPropertyKernelExecTimeSumIndexed, i,
                         value))
            {
                kernel["exec_time_sum"] = Json::MakeDouble(value);
            }
            if(TryDouble(metrics,
                         kRPVControllerSummaryMetricPropertyKernelExecTimeMinIndexed, i,
                         value))
            {
                kernel["exec_time_min"] = Json::MakeDouble(value);
            }
            if(TryDouble(metrics,
                         kRPVControllerSummaryMetricPropertyKernelExecTimeMaxIndexed, i,
                         value))
            {
                kernel["exec_time_max"] = Json::MakeDouble(value);
            }
            if(TryDouble(metrics,
                         kRPVControllerSummaryMetricPropertyKernelExecTimePctIndexed, i,
                         value))
            {
                kernel["exec_time_pct"] = Json::MakeDouble(value);
            }
            Json::Append(kernels, kernel);
        }
        object["top_kernels"] = kernels;
    }

    uint64_t num_sub_metrics = 0;
    if(TryUInt(metrics, kRPVControllerSummaryMetricPropertyNumSubMetrics, 0,
               num_sub_metrics) &&
       num_sub_metrics > 0)
    {
        jt::Json children = Json::MakeArray();
        for(uint64_t i = 0; i < num_sub_metrics; i++)
        {
            rocprofvis_handle_t* child = nullptr;
            if(TryObject(metrics, kRPVControllerSummaryMetricPropertySubMetricsIndexed, i,
                         &child))
            {
                Json::Append(children, SummaryMetrics(child, depth_budget - 1));
            }
        }
        object["children"] = children;
    }

    return object;
}

}  // namespace Serialize
}  // namespace Middleware
}  // namespace RocProfVis
