// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

/*
 * Result type, enumerating success & failure conditions.
 */
typedef enum rocprofvis_result_t
{
    // Operation was successful
    kRocProfVisResultSuccess = 0,
    // Operation failed with a non-specific error
    kRocProfVisResultUnknownError = 1,
    // Operation failed due to a timeout
    kRocProfVisResultTimeout = 2,
    // Operation failed as the data was not yet loaded
    kRocProfVisResultNotLoaded = 3,
    // Operation failed as an argument was invalid
    kRocProfVisResultInvalidArgument = 4,
    // Operation failed as the operation is not supported
    kRocProfVisResultNotSupported = 5,
    // Operation failed as the callee is read-only
    kRocProfVisResultReadOnlyError = 6,
    // Operation failed as memory couldn't be allocated
    kRocProfVisResultMemoryAllocError = 7,
    // Operation failed as enumeration was invalid
    kRocProfVisResultInvalidEnum = 8,
    // Operation failed as value is wrong type
    kRocProfVisResultInvalidType = 9,
    // Operation failed as a value is out of range
    kRocProfVisResultOutOfRange = 10,
    // Operation was cancelled
    kRocProfVisResultCancelled = 11,
    // Operation is pending
    kRocProfVisResultPending = 12,
    // Operation failed as a value is duplicated
    kRocProfVisResultDuplicate = 13,
} rocprofvis_result_t;

/*
 * Common object properties
 */
typedef enum rocprofvis_common_property_t : uint32_t
{
    // Total memory usage for the object - including any owned sub-objects
    kRPVControllerCommonMemoryUsageInclusive = 0xFFFF0000,
    // Memory usage strictly for the object alone - excluding any owned sub-objects
    kRPVControllerCommonMemoryUsageExclusive = 0xFFFF0001,
} rocprofvis_common_property_t;

/*
 * Object types
 */
typedef enum rocprofvis_controller_object_type_t
{
    // Controller object
    kRPVControllerObjectTypeController = 0,
    // Timeline object
    kRPVControllerObjectTypeTimeline = 1,
    // Track object
    kRPVControllerObjectTypeTrack = 2,
    // Sample object
    kRPVControllerObjectTypeSample = 3,
    // Event object
    kRPVControllerObjectTypeEvent = 4,
    // Flow control object
    kRPVControllerObjectTypeFlowControl = 5,
    // Callstack object
    kRPVControllerObjectTypeCallstack = 6,
    // Future object
    kRPVControllerObjectTypeFuture = 7,
    // Graph object
    kRPVControllerObjectTypeGraph = 8,
    // Table object
    kRPVControllerObjectTypeTable = 9,
    // View object
    kRPVControllerObjectTypeView = 10,
    // Array object
    kRPVControllerObjectTypeArray = 11,
    // Args object
    kRPVControllerObjectTypeArguments = 12,
    // Node object
    kRPVControllerObjectTypeNode = 13,
    // Processor object
    kRPVControllerObjectTypeProcessor = 14,
    // Extended data object
    kRPVControllerObjectTypeExtData = 15,
#ifdef COMPUTE_UI_SUPPORT
    // Compute trace object
    kRPVControllerObjectTypeComputeTrace = 16,
    // Plot object
    kRPVControllerObjectTypePlot = 17,
    // Plot series object
    kRPVControllerObjectTypePlotSeries = 18,
#endif
    // Process object
    kRPVControllerObjectTypeProcess = 19,
    // Thread object
    kRPVControllerObjectTypeThread = 20,
    // Queue object
    kRPVControllerObjectTypeQueue = 21,
    // Stream object
    kRPVControllerObjectTypeStream = 22,
    // Counter metadata object
    kRPVControllerObjectTypeCounter = 23
} rocprofvis_controller_object_type_t;

/*
 * Primitive data types.
 */
typedef enum rocprofvis_controller_primitive_type_t
{
    // Data is UInt64
    kRPVControllerPrimitiveTypeUInt64 = 0,
    // Data is double precision floats
    kRPVControllerPrimitiveTypeDouble = 1,
    // Data is a string
    kRPVControllerPrimitiveTypeString = 2,
    // Data is an object pointer - see rocprofvis_controller_object_type_t
    kRPVControllerPrimitiveTypeObject = 3
} rocprofvis_controller_primitive_type_t;

/*
 * Properties for the controller which manages a trace.
 */
typedef enum rocprofvis_controller_properties_t : uint32_t
{
    __kRPVControllerPropertiesFirst = 0x00000000,
    // Id of the controller - one per trace
    kRPVControllerId = __kRPVControllerPropertiesFirst,
    // Global timeline view controller
    kRPVControllerTimeline,
    // Global event table controller
    kRPVControllerEventTable,
    // Number of initialized analysis views - these are driven by scripts
    kRPVControllerNumAnalysisView,
    // Indexed analysis views
    kRPVControllerAnalysisViewIndexed,
    // Number of nodes within the trace
    kRPVControllerNumNodes,
    // Indexed nodes
    kRPVControllerNodeIndexed,
    // Number of tracks in the trace
    kRPVControllerNumTracks,
    // Indexed tracks
    kRPVControllerTrackIndexed,
    // Global sample table controller
    kRPVControllerSampleTable,
#ifdef COMPUTE_UI_SUPPORT
    // Compute trace controller
    kRPVControllerComputeTrace,
#endif
    // Indexed event in the trace
    kRPVControllerEventIndexed,
    // Load Event Flow control properties
    kRPVControllerEventDataFlowControlIndexed,
    // Load Event Callstack properties
    kRPVControllerEventDataCallStackIndexed,
    // Load Event Extended data properties
    kRPVControllerEventDataExtDataIndexed,
    // Tracks by Id
    kRPVControllerTrackById,
    // Notify controller when user select the trace
    kRPVControllerNotifySelected,
    // Get last stored data-model message
    kRPVControllerGetDmMessage,
    // Get last stored data-model progress in percent
    kRPVControllerGetDmProgress,
    // Get histogram number of bucket
    kRPVControllerGetHistogramBucketsNumber,
    // Get histogram bucket size
    kRPVControllerGetHistogramBucketSize,
    // Get histogram bucket values
    kRPVControllerBucketDataValueIndexed,
    // Global event search table controller
    kRPVControllerSearchResultsTable,
    __kRPVControllerPropertiesLast
} rocprofvis_controller_properties_t;
/* JSON: RPVController
{
    id: Int,
    timeline: RPVTimeline,
    table: RPVTable,
    num_analysis_views: Int,
    analysis_views: Array[RPVView],
    num_nodes: Int,
    nodes: Array[RPVNode]
}
*/

/*
 * The timeline is the primary view on to the data, it contains tracks which can be
 * iterated
 */
typedef enum rocprofvis_controller_timeline_properties_t : uint32_t
{
    __kRPVControllerTimelinePropertiesFirst = 0x10000000,
    // Timeline ID
    kRPVControllerTimelineId = __kRPVControllerTimelinePropertiesFirst,
    // Number of graphs in the timeline view
    kRPVControllerTimelineNumGraphs,
    // Start timestamp in the view
    kRPVControllerTimelineMinTimestamp,
    // Final timestamp in the view
    kRPVControllerTimelineMaxTimestamp,
    // Indexed graphs
    kRPVControllerTimelineGraphIndexed,
    // Graphs by Id
    kRPVControllerTimelineGraphById,
    __kRPVControllerTimelinePropertiesLast
} rocprofvis_controller_timeline_properties_t;
/* JSON: RPVTimeline
{
    num_tracks: Int,
    min_timestamp: Double,
    max_timestamp: Double,
    tracks: Array[RPVTrack]
}
*/

/*
 * A view holds analysis views that display data generated from the raw contents of the
 * trace
 */
typedef enum rocprofvis_controller_view_properties_t : uint32_t
{
    __kRPVControllerViewPropertiesFirst = 0x20000000,
    // ID of view
    kRPVControllerViewId = __kRPVControllerViewPropertiesFirst,
    // Number of graphs within the view
    kRPVControllerViewNumGraphs,
    // Number of tables within the view
    kRPVControllerViewNumTables,
    // Indexed graph
    kRPVControllerViewGraphIndexed,
    // Indexed table
    kRPVControllerViewTableIndexed,
    __kRPVControllerViewPropertiesLast
} rocprofvis_controller_view_properties_t;
/* JSON: RPVView
{
    id: Int,
    num_graphs: Int,
    num_tables: Int,
    graphs: Array[RPVGraph],
    tables: Array[RPVTable]
}
*/

/*
 * Types of track data
 */
typedef enum rocprofvis_controller_track_type_t
{
    // Track contains sample data and will display a line/bar graph
    kRPVControllerTrackTypeSamples = 0,
    // Track contains event data and will display a flame graph
    kRPVControllerTrackTypeEvents = 1,
} rocprofvis_controller_track_type_t;

typedef enum rocprofvis_controller_node_properties_t : uint32_t
{
    __kRPVControllerNodePropertiesFirst = 0xC0000000,
    kRPVControllerNodeId                = __kRPVControllerNodePropertiesFirst,
    kRPVControllerNodeHostName,
    kRPVControllerNodeDomainName,
    kRPVControllerNodeOSName,
    kRPVControllerNodeOSRelease,
    kRPVControllerNodeOSVersion,
    kRPVControllerNodeHardwareName,
    kRPVControllerNodeMachineId,
    kRPVControllerNodeNumProcessors,
    kRPVControllerNodeProcessorIndexed,
    kRPVControllerNodeNumProcesses,
    kRPVControllerNodeProcessIndexed,
    kRPVControllerNodeMachineGuid,
    kRPVControllerNodeHash,
    __kRPVControllerNodePropertiesLast
} rocprofvis_controller_node_properties_t;
/* JSON: RPVNode
{
    id: Int,
    host_name: String,
    domain_name: String,
    os_name: String,
    os_release: String,
    os_version: String,
    hardware_name: String,
    machine_id: String,
    num_processors: Int,
    processors: Array[RPVProcessor],
    num_processes: Int,
    processes: Array[RPVProcess]
}
*/

typedef enum rocprofvis_controller_processor_properties_t : uint32_t
{
    __kRPVControllerProcessorPropertiesFirst = 0xD0000000,
    kRPVControllerProcessorId                = __kRPVControllerProcessorPropertiesFirst,
    kRPVControllerProcessorName,
    kRPVControllerProcessorModelName,
    kRPVControllerProcessorUserName,
    kRPVControllerProcessorVendorName,
    kRPVControllerProcessorProductName,
    kRPVControllerProcessorExtData,
    kRPVControllerProcessorUUID,
    kRPVControllerProcessorType,
    kRPVControllerProcessorTypeIndex,
    kRPVControllerProcessorNodeId,
    kRPVControllerProcessorNumQueues,
    kRPVControllerProcessorNumStreams,
    kRPVControllerProcessorQueueIndexed,
    kRPVControllerProcessorStreamIndexed,
    kRPVControllerProcessorIndex,
    kRPVControllerProcessorLogicalIndex,
    __kRPVControllerProcessorPropertiesLast
} rocprofvis_controller_processor_properties_t;
/* JSON: RPVProcessor
{
    id: Int,
    name: String,
    model_name: String,
    user_name: String,
    vendor_name: String,
    product_name: String,
    ext_data: String,
    uuid: String,
    type: String,
    type_index: Int,
    node_id: Int
}
*/

typedef enum rocprofvis_controller_thread_properties_t : uint32_t
{
    __kRPVControllerThreadPropertiesFirst = 0xF2000000,
    kRPVControllerThreadId                = __kRPVControllerThreadPropertiesFirst,
    kRPVControllerThreadNode,
    kRPVControllerThreadProcess,
    kRPVControllerThreadParentId,
    kRPVControllerThreadTid,
    kRPVControllerThreadName,
    kRPVControllerThreadExtData,
    kRPVControllerThreadStartTime,
    kRPVControllerThreadEndTime,
    kRPVControllerThreadTrack,
    kRPVControllerThreadType,
    __kRPVControllerThreadPropertiesLast
} rocprofvis_controller_thread_properties_t;

typedef enum rocprofvis_controller_thread_type_t
{
    kRPVControllerThreadTypeUndefined    = 0,
    kRPVControllerThreadTypeInstrumented = 1,
    kRPVControllerThreadTypeSampled      = 2,
} rocprofvis_controller_thread_type_t;

typedef enum rocprofvis_controller_queue_properties_t : uint32_t
{
    __kRPVControllerQueuePropertiesFirst = 0xF3000000,
    kRPVControllerQueueId                = __kRPVControllerQueuePropertiesFirst,
    kRPVControllerQueueNode,
    kRPVControllerQueueProcess,
    __kRPVControllerQueueReserved0,
    kRPVControllerQueueName,
    kRPVControllerQueueExtData,
    kRPVControllerQueueProcessor,
    kRPVControllerQueueTrack,
    __kRPVControllerQueuePropertiesLast
} rocprofvis_controller_queue_properties_t;

typedef enum rocprofvis_controller_counter_properties_t : uint32_t
{
    __kRPVControllerCounterPropertiesFirst = 0xF5000000,
    kRPVControllerCounterId                = __kRPVControllerCounterPropertiesFirst,
    kRPVControllerCounterNode,
    kRPVControllerCounterProcess,
    kRPVControllerCounterProcessor,
    kRPVControllerCounterName,
    kRPVControllerCounterSymbol,
    kRPVControllerCounterDescription,
    kRPVControllerCounterExtendedDesc,
    kRPVControllerCounterComponent,
    kRPVControllerCounterUnits,
    kRPVControllerCounterValueType,
    kRPVControllerCounterBlock,
    kRPVControllerCounterExpression,
    kRPVControllerCounterGuid,
    kRPVControllerCounterExtData,
    kRPVControllerCounterTargetArch,
    kRPVControllerCounterEventCode,
    kRPVControllerCounterInstanceId,
    kRPVControllerCounterIsConstant,
    kRPVControllerCounterIsDerived,
    kRPVControllerCounterTrack,
    __kRPVControllerCounterPropertiesLast
} rocprofvis_controller_counter_properties_t;

typedef enum rocprofvis_controller_stream_properties_t : uint32_t
{
    __kRPVControllerStreamPropertiesFirst = 0xF4000000,
    kRPVControllerStreamId      = __kRPVControllerStreamPropertiesFirst,
    kRPVControllerStreamNode,
    kRPVControllerStreamProcess,
    kRPVControllerStreamName,
    kRPVControllerStreamExtData,
    kRPVControllerStreamProcessor,
    kRPVControllerStreamNumQueues,
    kRPVControllerStreamQueueIndexed,
    kRPVControllerStreamTrack,
    __kRPVControllerStreamPropertiesLast
} rocprofvis_controller_stream_properties_t;

typedef enum rocprofvis_controller_process_properties_t : uint32_t
{
    __kRPVControllerProcessPropertiesFirst = 0xF1000000,
    kRPVControllerProcessId      = __kRPVControllerProcessPropertiesFirst,
    kRPVControllerProcessNodeId,
    kRPVControllerProcessInitTime,
    kRPVControllerProcessFinishTime,
    kRPVControllerProcessStartTime,
    kRPVControllerProcessEndTime,
    kRPVControllerProcessCommand,
    kRPVControllerProcessEnvironment,
    kRPVControllerProcessExtData,
    kRPVControllerProcessParentId,
    kRPVControllerProcessNumThreads,
    kRPVControllerProcessNumQueues,
    kRPVControllerProcessNumStreams,
    kRPVControllerProcessThreadIndexed,
    kRPVControllerProcessQueueIndexed,
    kRPVControllerProcessStreamIndexed,
    kRPVControllerProcessNumCounters,
    kRPVControllerProcessCounterIndexed,
    __kRPVControllerProcessPropertiesLast,
} rocprofvis_controller_process_properties_t;
/* JSON: RPVProcess
{
    id: Int,
    node_id: Int,
    init_time: Double,
    finish_time: Double,
    start_time: Double,
    end_time: Double,
    command: String,
    environment: String,
    ext_data: String
}
*/

/*
 * Properties for each track of data within the data set.
 */
typedef enum rocprofvis_controller_track_properties_t : uint32_t
{
    __kRPVControllerTrackPropertiesFirst = 0x30000000,
    kRPVControllerTrackId                = __kRPVControllerTrackPropertiesFirst,
    // Track type, see rocprofvis_controller_track_type_t.
    kRPVControllerTrackType,
    // Start timestamp for the track
    kRPVControllerTrackMinTimestamp,
    // Final timestamp for the track
    kRPVControllerTrackMaxTimestamp,
    // Number of entries in the track
    kRPVControllerTrackNumberOfEntries,
    // Entries are actually loaded via an async call to prepare for RPC
    kRPVControllerTrackEntry,
    // Name
    kRPVControllerTrackName,
    // Description
    kRPVControllerTrackDescription,
    // Min value for sample tracks
    kRPVControllerTrackMinValue,
    // Max value for sample tracks
    kRPVControllerTrackMaxValue,
    // The node that the track belongs to
    kRPVControllerTrackNode,
    // The processor that the track belongs to
    kRPVControllerTrackProcessor,
    // Track extended data number of entries
    kRPVControllerTrackExtDataNumberOfEntries,
    // Track extended data category
    kRPVControllerTrackExtDataCategoryIndexed,
    // Track extended data name
    kRPVControllerTrackExtDataNameIndexed,
    // Track extended data value
    kRPVControllerTrackExtDataValueIndexed,
    // The CPU thread that the track represents - can be NULL
    kRPVControllerTrackThread,
    // The GPU queue that the track represents - can be NULL
    kRPVControllerTrackQueue,
    // The HW counter that the track represents - can be NULL
    kRPVControllerTrackCounter,
    // The CPU stream that the track represents - can be NULL
    kRPVControllerTrackStream,
    // Get histogram bucket value
    kRPVControllerTrackHistogramBucketValueIndexed,
    __kRPVControllerTrackPropertiesLast
} rocprofvis_controller_track_properties_t;
/* JSON: RPVTrack
{
    track_type: rocprofvis_controller_track_type_t,
    min_timestamp: Double,
    max_timestamp: Double,
    num_entries: Int,
    node: RPVNode,
    processor: RPVProcessor,
    -> entries: Array[RPVSample/RPVEvent], -> Definitely should not always be loaded. Need
an API to load them in an array.
}
*/

/*
 * Properties for a sample in a track or graph
 */
typedef enum rocprofvis_controller_sample_properties_t : uint32_t
{
    __kRPVControllerSamplePropertiesFirst = 0x40000000,
    // Sample Id
    kRPVControllerSampleId = __kRPVControllerSamplePropertiesFirst,
    // Owning track
    kRPVControllerSampleTrack,
    // Sample value data type, see rocprofvis_controller_primitive_type_t.
    kRPVControllerSampleType,
    // Sample timestamp
    kRPVControllerSampleTimestamp,
    // Sample value
    kRPVControllerSampleValue,
    // Sample vector end timestamp
    kRPVControllerSampleEndTimestamp,

    // When a LOD is generated the sample will be synthetic and coalesce several real
    // samples These properties allow access to the contains samples.

    // Number of children within the synthetic sample
    kRPVControllerSampleNumChildren,
    // Indexed children
    kRPVControllerSampleChildIndex,
    // Min value for children
    kRPVControllerSampleChildMin,
    // Mean value for children
    kRPVControllerSampleChildMean,
    // Median value for children
    kRPVControllerSampleChildMedian,
    // Max value for children
    kRPVControllerSampleChildMax,
    // Min timestamp for children
    kRPVControllerSampleChildMinTimestamp,
    // Max timestamp for children
    kRPVControllerSampleChildMaxTimestamp,
    __kRPVControllerSamplePropertiesLast
} rocprofvis_controller_sample_properties_t;
/* JSON: RPVSample
{
    id: Int,
    sample_type: rocprofvis_controller_primitive_type_t,
    timestamp: Double,
    value: UInt64/Double,
    num_children: Int,
    mean_child_value: UInt64/Double,
    median_child_value: UInt64/Double,
    max_child_value: UInt64/Double,
    min_child_value: UInt64/Double,
    max_child_timestamp: Double,
    min_child_timestamp: Double,
    -> children: Array[RPVSample] -> Can we load these - or would that cause us to load
too much?
}
*/

/*
 * Properties for a flow-control entry.
 */
typedef enum rocprofvis_controller_flow_control_properties_t : uint32_t
{
    __kRPVControllerFlowControlPropertiesFirst = 0x50000000,
    // The target event ID
    kRPVControllerFlowControltId = __kRPVControllerFlowControlPropertiesFirst,
    // The target timestamp
    kRPVControllerFlowControlTimestamp,
    // The target track ID
    kRPVControllerFlowControlTrackId,
    // Flow direction 0 - outgoing, 1 - incoming
    kRPVControllerFlowControlDirection,
    // The target name
    kRPVControllerFlowControlName,
    // Level of target in track
    kRPVControllerFlowControlLevel,
    // The target end timestamp
    kRPVControllerFlowControlEndTimestamp,
    __kRPVControllerFlowControlPropertiesLast
} rocprofvis_controller_flow_control_properties_t;
/* JSON: RPVFlowControl
{
    id: UInt64,
    timestamp: UInt64,
    track_id: UInt64,
    direction: UInt64,
}
*/

/*
 * Each entry resolves the ISA/ASM function/file/line and if possible the human readable
 * source version. The backend is the right place to resolve the ISA to Source mapping so
 * we can reuse any code that already does it.
 */
typedef enum rocprofvis_controller_callstack_properties_t : uint32_t
{
    __kRPVControllerCallstackPropertiesFirst = 0x60000000,
    // Human readable source code function
    kRPVControllerCallstackFunction = __kRPVControllerCallstackPropertiesFirst,
    // Human readable source code function arguments
    kRPVControllerCallstackArguments,
    // Human readable source code file
    kRPVControllerCallstackFile,
    // Human readable source code line
    kRPVControllerCallstackLine,
    // ISA/ASM code function
    kRPVControllerCallstackISAFunction,
    // ISA/ASM code file
    kRPVControllerCallstackISAFile,
    // ISA/ASM code line
    kRPVControllerCallstackISALine,
    __kRPVControllerCallstackPropertiesLast
} rocprofvis_controller_callstack_properties_t;
/* JSON: RPVCallstack
{
    function: String,
    file: String,
    line: Int,
    isa_function: String,
    isa_file: String,
    isa_line: Int,
}
*/

/*
 * Properties for each event in a track or graph
 */
typedef enum rocprofvis_controller_event_properties_t : uint32_t
{
    __kRPVControllerEventPropertiesFirst = 0x70000000,
    // Unique ID for the event
    kRPVControllerEventId = __kRPVControllerEventPropertiesFirst,
    // Start time stamp for the event
    kRPVControllerEventStartTimestamp,
    // End timestamp for the event
    kRPVControllerEventEndTimestamp,
    // Name for the event
    kRPVControllerEventName,
    // Level in the stack of events
    kRPVControllerEventLevel,

    // // Reserved values retained for compatibility
    // __kRPVControllerEventReserved0,
    // __kRPVControllerEventReserved1,
    // __kRPVControllerEventReserved2,
    // __kRPVControllerEventReserved3,
    // __kRPVControllerEventReserved4,

    // When a LOD is generated the sample will be synthetic and coalesce several real
    // samples These properties allow access to the contains samples.

    // Number of children within the synthetic sample
    kRPVControllerEventNumChildren,
    // Notionally indexed child events
    kRPVControllerEventChildIndexed,
    // Notionally indexed callstack entries
    kRPVControllerEventCallstackEntryIndexed,
    // Category for the event
    kRPVControllerEventCategory,
    // Name of the event string index
    kRPVControllerEventNameStrIndex,
    // Category for the event string index
    kRPVControllerEventCategoryStrIndex,
    // Duration of the event
    kRPVControllerEventDuration,
    // Name of top combined event
    kRPVControllerEventTopCombinedName,
    // Name of top combined event string index
    kRPVControllerEventTopCombinedNameStrIndex,

    __kRPVControllerEventPropertiesLast
} rocprofvis_controller_event_properties_t;
/* JSON: RPVEvent
{
    id: UInt64,
    start_timestamp: Double,
    end_timestamp: Double,
    name: String,
    num_callstack_entries: Int,
    num_input_flow: Int,
    input_flow: Array[Object],
    num_output_flow: Int,
    output_flow: Array[Object],
    num_children: Int,
    -> callstack: Array[Object], -> Can this be loaded all the time - or would it lead to
loading too much?
    -> children: Array[UInt64]
}
*/

/*
 * Type of graphs supported by the controller
 */
typedef enum rocprofvis_controller_graph_type_t
{
    // Display a line graph
    kRPVControllerGraphTypeLine = 0,
    // Display a flame graph
    kRPVControllerGraphTypeFlame = 1,
    // Other graph types can be added
} rocprofvis_controller_graph_type_t;

/*
 * A graph belongs to a track but might not be the whole track.
 * Instead it represents a segment of the whole or a derived LOD with synthetic events.
 * This allows the controller to provide only the data required by the UI.
 */
typedef enum rocprofvis_controller_graph_properties_t : uint32_t
{
    __kRPVControllerGraphPropertiesFirst = 0x80000000,
    // The graph ID
    kRPVControllerGraphId = __kRPVControllerGraphPropertiesFirst,
    // See rocprofvis_controller_graph_type_t
    kRPVControllerGraphType,
    // The owning track if it is part of the timeline view - null if this is an analysis
    // graph
    kRPVControllerGraphTrack,
    // Start time for the graph
    kRPVControllerGraphStartTimestamp,
    // End time for the graph
    kRPVControllerGraphEndTimestamp,
    // Number of entries in the graph
    kRPVControllerGraphNumEntries,
    // Notionally indexed entries in the graph
    // Actually loaded via an async. API to prepare for RPC.
    kRPVControllerGraphEntryIndexed,
    __kRPVControllerGraphPropertiesLast
} rocprofvis_controller_graph_properties_t;
/* JSON: RPVGraph
{
    id: Int,
    graph_type: rocprofvis_controller_graph_type_t,
    start_timestamp: Double,
    end_timestamp: Double,
    num_entries: Int,
    -> entries: Array[RPVSample/RPVEvent] -> Needs an API to load in segments and at
appropriate LOD.
}
*/

/*
 * Properties for an array.
 */
typedef enum rocprofvis_controller_array_properties_t : uint32_t
{
    __kRPVControllerArrayPropertiesFirst = 0x90000000,
    // Number of entries in array.
    kRPVControllerArrayNumEntries = __kRPVControllerArrayPropertiesFirst,
    // Indexed entry.
    kRPVControllerArrayEntryIndexed,
    __kRPVControllerArrayPropertiesLast
} rocprofvis_controller_array_properties_t;
/* JSON: RPVArray
{
    num_entries: Int,
    entries: Array[Object]
}
*/

/*
 * Tables permit us to display organized text data
 */
typedef enum rocprofvis_controller_table_properties_t : uint32_t
{
    __kRPVControllerTablePropertiesFirst = 0xA0000000,
    // Id for the table
    kRPVControllerTableId = __kRPVControllerTablePropertiesFirst,
    // Number of columns
    kRPVControllerTableNumColumns,
    // Number of rows
    kRPVControllerTableNumRows,
    // Indexed column header names
    kRPVControllerTableColumnHeaderIndexed,
    // Indexed row header names
    kRPVControllerTableRowHeaderIndexed,
    // Indexed column type
    kRPVControllerTableColumnTypeIndexed,
    // Notionally would give you an array for all the cells in the row
    // But this needs to be Async if we separate the Front/Back end
    kRPVControllerTableRowIndexed,
    // Table title
    kRPVControllerTableTitle,
    __kRPVControllerTablePropertiesLast
} rocprofvis_controller_table_properties_t;
/* JSON: RPVTable
{
    id: Int,
    num_rows: Int,
    num_columns: Int,
    row_headers: Array[String],
    column_headers: Array[String],
    column_types: Array[rocprofvis_controller_primitive_type_t],
    -> rows: Array[Object] -> Needs an API to load in segments.
}
*/

typedef enum rocprofvis_controller_table_arguments_t : uint32_t
{
    kRPVControllerTableArgsType                      = 0xE0000000,
    kRPVControllerTableArgsNumTracks                 = 0xE0000001,
    kRPVControllerTableArgsTracksIndexed             = 0xE0000002,
    kRPVControllerTableArgsStartTime                 = 0xE0000003,
    kRPVControllerTableArgsEndTime                   = 0xE0000004,
    kRPVControllerTableArgsSortColumn                = 0xE0000005,
    kRPVControllerTableArgsSortOrder                 = 0xE0000006,
    kRPVControllerTableArgsStartIndex                = 0xE0000007,
    kRPVControllerTableArgsStartCount                = 0xE0000008,
    kRPVControllerTableArgsFilter                    = 0xE0000009,
    kRPVControllerTableArgsGroup                     = 0xE000000A,
    kRPVControllerTableArgsGroupColumns              = 0xE000000B,
    kRPVControllerTableArgsNumOpTypes                = 0xE000000C,
    kRPVControllerTableArgsOpTypesIndexed            = 0xE000000D,
    kRPVControllerTableArgsNumStringTableFilters     = 0xE000000E,
    kRPVControllerTableArgsStringTableFiltersIndexed = 0xE000000F,
    kRPVControllerTableArgsSummary                   = 0xE0000010,   
} rocprofvis_controller_table_arguments_t;

typedef enum rocprofvis_controller_table_type_t
{
    kRPVControllerTableTypeEvents        = 0xF0000000,
    kRPVControllerTableTypeSamples       = 0xF0000001,
    kRPVControllerTableTypeSearchResults = 0xF0000002,
} rocprofvis_controller_table_type_t;

/*
 * Properties for a future object
 */
typedef enum rocprofvis_controller_future_properties_t : uint32_t
{
    __kRPVControllerFuturePropertiesFirst = 0xB0000000,
    // Result code
    kRPVControllerFutureResult = __kRPVControllerFuturePropertiesFirst,
    // Data object
    kRPVControllerFutureObject,
    // Type of object, see rocprofvis_controller_object_type_t
    kRPVControllerFutureType,
    __kRPVControllerFuturePropertiesLast
} rocprofvis_controller_future_properties_t;

/*
 * Event extended properties . To be used by
 * rocprofvis_controller_get_indexed_property_async
 */
typedef enum rocprofvis_controller_event_data_properties_t : uint32_t
{
    __kRPVControllerEventDataPropertiesFirst = 0xC0000000,
    // Load Event Flow control properties
    kRPVControllerEventDataFlowControl = __kRPVControllerEventDataPropertiesFirst,
    // Load Event Callstack properties
    kRPVControllerEventDataCallStack,
    // Load Event Extended data properties
    kRPVControllerEventDataExtData,
    __kRPVControllerEventDataPropertiesLast
} rocprofvis_controller_event_data_properties_t;

/*
 * Properties for extended data
 */
typedef enum rocprofvis_controller_extdata_properties_t : uint32_t
{
    __kRPVControllerExtDataPropertiesFirst = 0xD0000000,
    // Extended data category
    kRPVControllerExtDataCategory = __kRPVControllerExtDataPropertiesFirst,
    // Extended data name
    kRPVControllerExtDataName,
    // Extended data value
    kRPVControllerExtDataValue,
    // Extended data value
    kRPVControllerExtDataType,
    // Extended data category enumeration
    kRPVControllerExtDataCategoryEnum,
    __kRPVControllerExtDataPropertiesLast
} rocprofvis_controller_extdata_properties_t;
/* JSON: RPVCallstack
{
    category: String,
    name: String,
    value: String,
}
*/

typedef enum rocprofvis_controller_sort_order_t
{
    kRPVControllerSortOrderAscending,
    kRPVControllerSortOrderDescending,
} rocprofvis_controller_sort_order_t;

typedef enum rocprofvis_event_data_category_enum_t
{
    // Internal information, user should not see it
    kRocProfVisEventEssentialDataInternal,
    // Uncategorized information
    kRocProfVisEventEssentialDataUncategorized,
    // Event Id
    kRocProfVisEventEssentialDataId,
    // Event category
    kRocProfVisEventEssentialDataCategory,
    // Event name
    kRocProfVisEventEssentialDataName,
    // Event start
    kRocProfVisEventEssentialDataStart,
    // Event end
    kRocProfVisEventEssentialDataEnd,
    // Event duration
    kRocProfVisEventEssentialDataDuration,
    // Event node id
    kRocProfVisEventEssentialDataNode,
    // Event PID
    kRocProfVisEventEssentialDataProcess,
    // Event TID
    kRocProfVisEventEssentialDataThread,
    // Event Agent type - GPU/CPU
    kRocProfVisEventEssentialDataAgentType,
    // Event agent index
    kRocProfVisEventEssentialDataAgentIndex,
    // Event queue
    kRocProfVisEventEssentialDataQueue,
    // Event stream
    kRocProfVisEventEssentialDataStream,
    // Event track
    kRocProfVisEventEssentialDataTrack,
    // Event stream track
    kRocProfVisEventEssentialDataStreamTrack,
    // Event track level
    kRocProfVisEventEssentialDataLevel,
    // Event stream track level
    kRocProfVisEventEssentialDataStreamLevel,

} rocprofvis_event_data_category_enum_t;

#ifdef COMPUTE_UI_SUPPORT
/*
 * Identifiers for each table in a compute trace.
 */
typedef enum rocprofvis_controller_compute_table_types_t
{
    kRPVControllerComputeTableTypeKernelList = 0,
    kRPVControllerComputeTableTypeDispatchList,
    kRPVControllerComputeTableTypeSysInfo,
    kRPVControllerComputeTableTypeSpeedOfLight,
    kRPVControllerComputeTableTypeMemoryChart,
    kRPVControllerComputeTableTypeCPFetcher,
    kRPVControllerComputeTableTypeCPPacketProcessor,
    kRPVControllerComputeTableTypeWorkgroupMngrUtil,
    kRPVControllerComputeTableTypeWorkgroupMngrRescAlloc,
    kRPVControllerComputeTableTypeWavefrontLaunch,
    kRPVControllerComputeTableTypeWavefrontRuntime,
    kRPVControllerComputeTableTypeInstrMix,
    kRPVControllerComputeTableTypeVALUInstrMix,
    kRPVControllerComputeTableTypeVMEMInstrMix,
    kRPVControllerComputeTableTypeMFMAInstrMix,
    kRPVControllerComputeTableTypeCUSpeedOfLight,
    kRPVControllerComputeTableTypeCUPipelineStats,
    kRPVControllerComputeTableTypeCUOps,
    kRPVControllerComputeTableTypeLDSSpeedOfLight,
    kRPVControllerComputeTableTypeLDSStats,
    kRPVControllerComputeTableTypeInstrCacheSpeedOfLight,
    kRPVControllerComputeTableTypeInstrCacheAccesses,
    kRPVControllerComputeTableTypeInstrCacheL2Interface,
    kRPVControllerComputeTableTypeSL1CacheSpeedOfLight,
    kRPVControllerComputeTableTypeSL1CacheAccesses,
    kRPVControllerComputeTableTypeSL1CacheL2Transactions,
    kRPVControllerComputeTableTypeAddrProcessorStats,
    kRPVControllerComputeTableTypeAddrProcessorReturnPath,
    kRPVControllerComputeTableTypeVL1CacheSpeedOfLight,
    kRPVControllerComputeTableTypeVL1CacheStalls,
    kRPVControllerComputeTableTypeVL1CacheAccesses,
    kRPVControllerComputeTableTypeVL1CacheL2Transactions,
    kRPVControllerComputeTableTypeVL1CacheAddrTranslations,
    kRPVControllerComputeTableTypeL2CacheSpeedOfLight,
    kRPVControllerComputeTableTypeL2CacheFabricTransactions,
    kRPVControllerComputeTableTypeL2CacheAccesses,
    kRPVControllerComputeTableTypeL2CacheFabricStalls,
    kRPVControllerComputeTableTypeL2CacheFabricTransactionsDetailed,
    kRPVControllerComputeTableTypeL2CacheStats,
    kRPVControllerComputeTableTypeL2CacheHitRate,
    kRPVControllerComputeTableTypeL2CacheReqs1,
    kRPVControllerComputeTableTypeL2CacheReqs2,
    kRPVControllerComputeTableTypeL2CacheFabricReqs,
    kRPVControllerComputeTableTypeL2CacheFabricRdLat,
    kRPVControllerComputeTableTypeL2CacheFabricWrAtomLat,
    kRPVControllerComputeTableTypeL2CacheFabricAtomLat,
    kRPVControllerComputeTableTypeL2CacheRdStalls,
    kRPVControllerComputeTableTypeL2CacheWrAtomStalls,
    kRPVControllerComputeTableTypeL2Cache128Reqs,
    kRPVControllerComputeTableTypeRooflineBenchmarks,
    kRPVControllerComputeTableTypeRooflineCounters,
    kRPVControllerComputeTableTypeCount
} rocprofvis_controller_compute_table_types_t;

/*
 * Identifiers for each plot in a compute trace.
 */
typedef enum rocprofvis_controller_compute_plot_types_t
{
    kRPVControllerComputePlotTypeKernelDurationPercentage =
        kRPVControllerComputeTableTypeCount + 1,
    kRPVControllerComputePlotTypeKernelDuration,
    kRPVControllerComputePlotTypeL2CacheSpeedOfLight,
    kRPVControllerComputePlotTypeL2CacheFabricSpeedOfLight,
    kRPVControllerComputePlotTypeL2CacheFabricStallsRead,
    kRPVControllerComputePlotTypeL2CacheFabricStallsWrite,
    kRPVControllerComputePlotTypeInstrMix,
    kRPVControllerComputePlotTypeCUOps,
    kRPVControllerComputePlotTypeSL1CacheSpeedOfLight,
    kRPVControllerComputePlotTypeInstrCacheSpeedOfLight,
    kRPVControllerComputePlotTypeVL1CacheSpeedOfLight,
    kRPVControllerComputePlotTypeVL1CacheL2NCTransactions,
    kRPVControllerComputePlotTypeVL1CacheL2UCTransactions,
    kRPVControllerComputePlotTypeVL1CacheL2RWTransactions,
    kRPVControllerComputePlotTypeVL1CacheL2CCTransactions,
    kRPVControllerComputePlotTypeVALUInstrMix,
    kRPVControllerComputePlotTypeLDSSpeedOfLight,
    kRPVControllerComputePlotTypeRooflineFP64,
    kRPVControllerComputePlotTypeRooflineFP32,
    kRPVControllerComputePlotTypeRooflineFP16,
    kRPVControllerComputePlotTypeRooflineINT8,
    kRPVControllerComputePlotTypeCount,
} rocprofvis_controller_compute_plot_types_t;

/*
 * Identifiers for individual metrics in a compute trace.
 */
typedef enum rocprofvis_controller_compute_metric_types_t
{
    kRPVControllerComputeMetricTypeL2CacheRd = kRPVControllerComputePlotTypeCount + 1,
    kRPVControllerComputeMetricTypeL2CacheWr,
    kRPVControllerComputeMetricTypeL2CacheAtomic,
    kRPVControllerComputeMetricTypeL2CacheHitRate,
    kRPVControllerComputeMetricTypeFabricRd,
    kRPVControllerComputeMetricTypeFabricWr,
    kRPVControllerComputeMetricTypeFabricAtomic,
    kRPVControllerComputeMetricTypeSL1CacheHitRate,
    kRPVControllerComputeMetricTypeInstrCacheHitRate,
    kRPVControllerComputeMetricTypeInstrCacheLat,
    kRPVControllerComputeMetricTypeVL1CacheHitRate,
    kRPVControllerComputeMetricTypeVL1CacheCoales,
    kRPVControllerComputeMetricTypeVL1CacheStall,
    kRPVControllerComputeMetricTypeLDSUtil,
    kRPVControllerComputeMetricTypeLDSLat,
    kRPVcontrollerComputeMetricTypeLDSAlloc,
    kRPVControllerComputeMetricTypeVGPR,
    kRPVControllerComputeMetricTypeSGPR,
    kRPVControllerComputeMetricTypeScratchAlloc,
    kRPVControllerComputeMetricTypeWavefronts,
    kRPVControllerComputeMetricTypeWorkgroups,
    kRPVControllerComputeMetricTypeFabric_HBMRd,
    kRPVControllerComputeMetricTypeFabric_HBMWr,
    kRPVControllerComputeMetricTypeL2_FabricRd,
    kRPVControllerComputeMetricTypeL2_FabricWr,
    kRPVControllerComputeMetricTypeL2_FabricAtomic,
    kRPVControllerComputeMetricTypeVL1_L2Rd,
    kRPVControllerComputeMetricTypeVL1_L2Wr,
    kRPVControllerComputeMetricTypeVL1_L2Atomic,
    kRPVControllerComputeMetricTypeSL1_L2Rd,
    kRPVControllerComputeMetricTypeSL1_L2Wr,
    kRPVControllerComputeMetricTypeSL1_L2Atomic,
    kRPVControllerComputeMetricTypeInst_L2Req,
    kRPVControllerComputeMetricTypeCU_LDSReq,
    kRPVControllerComputeMetricTypeCU_VL1Rd,
    kRPVControllerComputeMetricTypeCU_VL1Wr,
    kRPVControllerComputeMetricTypeCU_VL1Atomic,
    kRPVControllerComputeMetricTypeCU_SL1Rd,
    kRPVControllerComputeMetricTypeCU_InstrReq,
    kRPVControllerComputeMetricTypeCount
} rocprofvis_controller_compute_metric_types_t;

/*
 * Properties of a Plot object
 */
typedef enum rocprofvis_controller_plot_properties_t : uint32_t
{
    __kRPVControllerPlotPropertiesFirst = 0x1A000000,
    // Id for the plot
    kRPVControllerPlotId = __kRPVControllerPlotPropertiesFirst,
    // Number of data series
    kRPVControllerPlotNumSeries,
    // Number of x axis tick labels
    kRPVControllerPlotNumXAxisLabels,
    // Number of y axis tick labels
    kRPVControllerPlotNumYAxisLabels,
    // Indexed x axis tick labels
    kRPVControllerPlotXAxisLabelsIndexed,
    // Indexed y axis tick labels
    kRPVControllerPlotYAxisLabelsIndexed,
    // X axis title
    kRPVControllerPlotXAxisTitle,
    // Y axis title
    kRPVControllerPlotYAxisTitle,
    // Plot title
    kRPVControllerPlotTitle,
    __kRPVControllerPlotPropertiesLast,
} rocprofvis_controller_plot_properties_t;

/*
 * Properties of a PlotSeries object
 */
typedef enum rocprofvis_controller_plot_series_properties_t : uint32_t
{
    __kRPVControllerPlotSeriesPropertiesFirst = 0x2A000000,
    // Number of x,y pairs in the series
    kRPVControllerPlotSeriesNumValues = __kRPVControllerPlotSeriesPropertiesFirst,
    // Indexed x values
    kRPVControllerPlotSeriesXValuesIndexed,
    // Indexed y values
    kRPVControllerPlotSeriesYValuesIndexed,
    // Series name
    kRPVControllerPlotSeriesName,
    __kRPVControllerPlotSeriesPropertiesLast
} rocprofvis_controller_plot_series_properties_t;
#endif