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
    // System trace controller object
    kRPVControllerObjectTypeControllerSystem = 0,
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
    // Process object
    kRPVControllerObjectTypeProcess = 16,
    // Thread object
    kRPVControllerObjectTypeThread = 17,
    // Queue object
    kRPVControllerObjectTypeQueue = 18,
    // Stream object
    kRPVControllerObjectTypeStream = 19,
    // Counter metadata object
    kRPVControllerObjectTypeCounter = 20,
    // Summary controller object
    kRPVControllerObjectTypeSummary = 21,
    // Summary metrics container object
    kRPVControllerObjectTypeSummaryMetrics = 22,
    // Extended data object
    kRPVControllerObjectTypeEventArgument = 23,
    // Topology node object
    kRPVControllerObjectTypeTopologyNode = 24,
#ifdef COMPUTE_UI_SUPPORT
    // Compute trace controller object
    kRPVControllerObjectTypeControllerCompute = 100,
    // Workload object
    kRPVControllerObjectTypeWorkload = 101,
    // Kernel object
    kRPVControllerObjectTypeKernel = 102,
    // Metrics container object
    kRPVControllerObjectTypeMetricsContainer = 103,
    // Roofline object
    kRPVControllerObjectTypeRoofline = 104,
#endif

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
 * Properties for the controller which manages a system trace.
 */
typedef enum rocprofvis_controller_system_properties_t : uint32_t
{
    __kRPVControllerSystemPropertiesFirst = 0x00000000,
    // Id of the controller - one per trace
    kRPVControllerSystemId = __kRPVControllerSystemPropertiesFirst,
    // Global timeline view controller
    kRPVControllerSystemTimeline,
    // Global event table controller
    kRPVControllerSystemEventTable,
    // Number of initialized analysis views - these are driven by scripts
    kRPVControllerSystemNumAnalysisView,
    // Indexed analysis views
    kRPVControllerSystemAnalysisViewIndexed,
    // Number of nodes within the trace
    kRPVControllerSystemNumNodes,
    // Indexed nodes
    kRPVControllerSystemNodeIndexed,
    // Number of tracks in the trace
    kRPVControllerSystemNumTracks,
    // Indexed tracks
    kRPVControllerSystemTrackIndexed,
    // Global sample table controller
    kRPVControllerSystemSampleTable,
    // Indexed event in the trace
    kRPVControllerSystemEventIndexed,
    // Load Event Flow control properties
    kRPVControllerSystemEventDataFlowControlIndexed,
    // Load Event Callstack properties
    kRPVControllerSystemEventDataCallStackIndexed,
    // Load Event Extended data properties
    kRPVControllerSystemEventDataExtDataIndexed,
    // Tracks by Id
    kRPVControllerSystemTrackById,
    // Notify controller when user select the trace
    kRPVControllerSystemNotifySelected,
    // Get histogram number of bucket
    kRPVControllerSystemGetHistogramBucketsNumber,
    // Get histogram bucket size
    kRPVControllerSystemGetHistogramBucketSize,
    // Get histogram bucket values
    kRPVControllerSystemBucketDataValueIndexed,
    // Global event search table controller
    kRPVControllerSystemSearchResultsTable,
    // Global summary view controller
    kRPVControllerSystemSummary,
    __kRPVControllerSystemPropertiesLast
} rocprofvis_controller_system_properties_t;
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
    // Category of the track
    kRPVControllerCategory,
    // Track main process string (PID, GPUID, etc)
    kRPVControllerMainName,
    // Track sub process string (TID, QueueID, PMC name)
    kRPVControllerSubName,
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
    // Get histogram bucket density
    kRPVControllerTrackHistogramBucketDensityIndexed,
    // Get histogram bucket value
    kRPVControllerTrackHistogramBucketValueIndexed,
    // Get track agent id or PID
    kRPVControllerTrackAgentIdOrPid,
    // Get track queue id or TID
    kRPVControllerTrackQueueIdOrTid,
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
 * Properties for a callstack entry.
 */
typedef enum rocprofvis_controller_callstack_properties_t : uint32_t
{
    __kRPVControllerCallstackPropertiesFirst = 0x60000000,
    kRPVControllerCallstackFile = __kRPVControllerCallstackPropertiesFirst,
    kRPVControllerCallstackPc,
    kRPVControllerCallstackName,
    kRPVControllerCallstackLineName,
    kRPVControllerCallstackLineAddress,
    kRPVControllerCallstackDepth,
    kRPVControllerCallstackRegionId,
    __kRPVControllerCallstackPropertiesLast
} rocprofvis_controller_callstack_properties_t;

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
    kRPVControllerTableArgsWhere                     = 0xE0000009,
    kRPVControllerTableArgsFilter                    = 0xE000000A,
    kRPVControllerTableArgsGroup                     = 0xE000000B,
    kRPVControllerTableArgsGroupColumns              = 0xE000000C,
    kRPVControllerTableArgsNumOpTypes                = 0xE000000D,
    kRPVControllerTableArgsOpTypesIndexed            = 0xE000000E,
    kRPVControllerTableArgsNumStringTableFilters     = 0xE000000F,
    kRPVControllerTableArgsStringTableFiltersIndexed = 0xE0000010,
    kRPVControllerTableArgsSummary                   = 0xE0000011,   
} rocprofvis_controller_table_arguments_t;

typedef enum rocprofvis_controller_table_type_t
{
    kRPVControllerTableTypeEvents                 = 0xF0000000,
    kRPVControllerTableTypeSamples                = 0xF0000001,
    kRPVControllerTableTypeSearchResults          = 0xF0000002,
    kRPVControllerTableTypeSummaryKernelInstances = 0xF0000003,
} rocprofvis_controller_table_type_t;

/*
 * Properties for a future object
 */
typedef enum rocprofvis_controller_future_properties_t : uint32_t
{
    __kRPVControllerFuturePropertiesFirst = 0xB0000000,
    // Result code
    kRPVControllerFutureResult = __kRPVControllerFuturePropertiesFirst,
    // Progress percentage
    kRPVControllerFutureProgressPercentage,
    // Progress message
    kRPVControllerFutureProgressMessage,
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
    // Event argument position
    kRPVControllerEventArgumentPosition,
    // Event argument type
    kRPVControllerEventArgumentType,
    __kRPVControllerExtDataPropertiesLast
} rocprofvis_controller_extdata_properties_t;
/* JSON: RPVCallstack
{
    category: String,
    name: String,
    value: String,
}
*/

/*
 * Properties for the summary controller
 */
typedef enum rocprofvis_controller_summary_properties_t : uint32_t
{
    __kRPVControllerSummaryPropertiesFirst = 0xE0000000,
    kRPVControllerSummaryPropertyKernelInstanceTable = __kRPVControllerSummaryPropertiesFirst,
    __kRPVControllerSummaryPropertiesLast
} rocprofvis_controller_summary_properties_t;

/*
 * Properties for the summary metrics container
 */
typedef enum rocprofvis_controller_summary_metric_properties_t : uint32_t
{
    __kRPVControllerSummaryMetricPropertiesFirst = 0xF0000000,
    // [Mandatory] Aggregation level (rocprofvis_controller_summary_aggregation_level_t)
    kRPVControllerSummaryMetricPropertyAggregationLevel = __kRPVControllerSummaryMetricPropertiesFirst,
    // [Mandatory] Number of child/sub-metrics
    kRPVControllerSummaryMetricPropertyNumSubMetrics,
    // [Mandatory] Indexed child/sub-metrics objects
    kRPVControllerSummaryMetricPropertySubMetricsIndexed,
    // [Optional] Level specific id (node id, processor id)
    kRPVControllerSummaryMetricPropertyId,
    // [Optional] Level specific name (node name, processor name)
    kRPVControllerSummaryMetricPropertyName,
    // [Optional] Processor level specific type (rocprofvis_controller_processor_type_t)
    kRPVControllerSummaryMetricPropertyProcessorType,
    // [Optional] Processor level specific type index (GPU 0, 1, CPU 0, CPU 1...etc)
    kRPVControllerSummaryMetricPropertyProcessorTypeIndex,
    // [Optional] Processor level gpu specific metrics...
    kRPVControllerSummaryMetricPropertyGpuGfxUtil,
    kRPVControllerSummaryMetricPropertyGpuMemUtil,
    // [Optional, Mandatory if GPU processor level] Number of top kernels
    kRPVControllerSummaryMetricPropertyNumKernels,
    // [Optional, Mandatory if GPU processor level] Total execution time for all kernels
    kRPVControllerSummaryMetricPropertyKernelsExecTimeTotal,
    // [Optional, Mandatory if GPU processor level] Indexed top kernel properties/metrics...
    kRPVControllerSummaryMetricPropertyKernelNameIndexed,
    kRPVControllerSummaryMetricPropertyKernelInvocationsIndexed,
    kRPVControllerSummaryMetricPropertyKernelExecTimeSumIndexed,
    kRPVControllerSummaryMetricPropertyKernelExecTimeMinIndexed,
    kRPVControllerSummaryMetricPropertyKernelExecTimeMaxIndexed,
    kRPVControllerSummaryMetricPropertyKernelExecTimePctIndexed,
    __kRPVControllerSummaryMetricPropertiesLast
} rocprofvis_controller_summary_metric_properties_t;

/*
 * Aggregation levels for summary metrics container
 */
typedef enum rocprofvis_controller_summary_aggregation_level_t : uint32_t
{
    __kRPVControllerSummaryAggregationLevelFirst = 0x11000000,
    kRPVControllerSummaryAggregationLevelTrace = __kRPVControllerSummaryAggregationLevelFirst,
    kRPVControllerSummaryAggregationLevelNode,
    kRPVControllerSummaryAggregationLevelProcessor,
    __kRPVControllerSummaryAggregationLevelLast
} rocprofvis_controller_summary_aggregation_level_t;

/*
 * Arguments to fetch summary metrics from the summary controller
 */
typedef enum rocprofvis_controller_summary_arguments_t : uint32_t
{
    kRPVControllerSummaryArgsStartTimestamp = 0x12000000,
    kRPVControllerSummaryArgsEndTimestamp,
} rocprofvis_controller_summary_arguments_t;

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

    // Event argument data
    kRocProfVisEventArgumentData,

} rocprofvis_event_data_category_enum_t;

#ifdef COMPUTE_UI_SUPPORT
/*
 * Properties for the controller which manages a compute trace.
 */
typedef enum rocprofvis_controller_compute_properties_t : uint32_t
{
    __kRPVControllerComputePropertiesFirst,
    kRPVControllerComputeId = __kRPVControllerComputePropertiesFirst,
    kRPVControllerNumWorkloads,
    kRPVControllerWorkloadIndexed,
    kRPVControllerKernelMetricTable,
    __kRPVControllerComputePropertiesLast
} rocprofvis_controller_compute_properties_t;

/*
 * Properties for a workload in a compute trace.
 */
typedef enum rocprofvis_controller_workload_properties_t : uint32_t
{
    __kRPVControllerWorkloadPropertiesFirst,
    kRPVControllerWorkloadId = __kRPVControllerWorkloadPropertiesFirst,
    kRPVControllerWorkloadName,
    kRPVControllerWorkloadSystemInfoNumEntries,
    kRPVControllerWorkloadSystemInfoEntryNameIndexed,
    kRPVControllerWorkloadSystemInfoEntryValueIndexed,
    kRPVControllerWorkloadConfigurationNumEntries,
    kRPVControllerWorkloadConfigurationEntryNameIndexed,
    kRPVControllerWorkloadConfigurationEntryValueIndexed,
    kRPVControllerWorkloadNumAvailableMetrics,
    kRPVControllerWorkloadAvailableMetricCategoryIdIndexed,
    kRPVControllerWorkloadAvailableMetricTableIdIndexed,
    kRPVControllerWorkloadAvailableMetricCategoryNameIndexed,
    kRPVControllerWorkloadAvailableMetricTableNameIndexed,
    kRPVControllerWorkloadAvailableMetricNameIndexed,
    kRPVControllerWorkloadAvailableMetricDescriptionIndexed,
    kRPVControllerWorkloadAvailableMetricUnitIndexed,
    kRPVControllerWorkloadNumMetricValueNames,
    kRPVControllerWorkloadMetricValueNameCategoryIdIndexed,
    kRPVControllerWorkloadMetricValueNameTableIdIndexed,
    kRPVControllerWorkloadMetricValueNameStringIndexed,
    kRPVControllerWorkloadRoofline,
    kRPVControllerWorkloadNumKernels,
    kRPVControllerWorkloadKernelIndexed,
    __kRPVControllerWorkloadPropertiesLast
} rocprofvis_controller_workload_properties_t;

/*
 * Properties for a kernel in a compute trace.
 */
typedef enum rocprofvis_controller_kernel_properties_t : uint32_t
{
    __kRPVControllerKernelPropertiesFirst,
    kRPVControllerKernelId = __kRPVControllerKernelPropertiesFirst,
    kRPVControllerKernelName,
    kRPVControllerKernelInvocationCount,
    kRPVControllerKernelDurationTotal,
    kRPVControllerKernelDurationMin,
    kRPVControllerKernelDurationMax,
    kRPVControllerKernelDurationMedian,
    kRPVControllerKernelDurationMean,
    __kRPVControllerKernelPropertiesLast
} rocprofvis_controller_kernel_properties_t;

/*
 * Arguments for fetching metric values.
 */
typedef enum rocprofvis_controller_metric_arguments_t : uint32_t
{
    kRPVControllerMetricArgsWorkloadId,
    kRPVControllerMetricArgsNumKernels,
    kRPVControllerMetricArgsKernelIdIndexed,
    kRPVControllerMetricArgsNumMetrics,
    kRPVControllerMetricArgsMetricCategoryIdIndexed,
    kRPVControllerMetricArgsMetricTableIdIndexed,
    kRPVControllerMetricArgsMetricEntryIdIndexed,
} rocprofvis_controller_metric_arguments_t;

/*
 * Arguments for fetching dynamic metrics matrix (pivot table).
 */
typedef enum rocprofvis_controller_compute_pivot_table_arguments_t : uint32_t
{
    // Workload ID to query (uint64)
    kRPVControllerCPTArgsWorkloadId = 0x13000000,
    // Number of metric selectors (uint64)
    kRPVControllerCPTArgsNumMetricSelectors,
    // Indexed metric selector strings in format "metric_id:value_name" (e.g., "2.1.4:peak")
    kRPVControllerCPTArgsMetricSelectorIndexed,
    // Optional sort column index (uint64) - 0=kernel_name, 1=duration_ns_sum (default), 2+=metrics
    kRPVControllerCPTArgsSortColumnIndex,
    // Optional sort order (rocprofvis_controller_sort_order_t) - 0=ascending, 1=descending (default = descending)
    kRPVControllerCPTArgsSortOrder,
    // Number of column filters (uint64)
    kRPVControllerCPTArgsNumColumnFilters,
    // Indexed filter column index (uint64)
    kRPVControllerCPTArgsFilterColumnIndexIndexed,
    // Indexed filter expression string (e.g., "> 1000", "LIKE '%kernel%'")
    kRPVControllerCPTArgsFilterExpressionIndexed,
} rocprofvis_controller_compute_pivot_table_arguments_t;

/*
 * Properties for a metrics container.
 */
typedef enum rocprofvis_controller_metrics_container_properties_t : uint32_t
{
    __kRPVControllerMetricsContainerPropertiesFirst,
    kRPVControllerMetricsContainerNumMetrics = __kRPVControllerMetricsContainerPropertiesFirst,
    kRPVControllerMetricsContainerMetricIdIndexed,
    kRPVControllerMetricsContainerMetricNameIndexed,
    kRPVControllerMetricsContainerMetricSourceTypeIndexed,
    kRPVControllerMetricsContainerWorkloadIdIndexed,
    kRPVControllerMetricsContainerKernelIdIndexed,
    kRPVControllerMetricsContainerMetricValueNameIndexed,
    kRPVControllerMetricsContainerMetricValueValueIndexed,
    __kRPVControllerMetricsContainerPropertiesLast
} rocprofvis_controller_metrics_container_properties_t;

/*
 * Metric source types.
 */
typedef enum rocprofvis_controller_metric_source_type_t : uint32_t
{
    kRPVControllerMetricSourceTypeWorkload,
    kRPVControllerMetricSourceTypeKernel,
} rocprofvis_controller_metric_aggregation_type_t;

/*
 * Properties for roofline.
 */
typedef enum rocprofvis_controller_roofline_properties_t : uint32_t
{
    __kRPVControllerRooflinePropertiesFirst,
    kRPVControllerRooflineDataType = __kRPVControllerRooflinePropertiesFirst,
    kRPVControllerRooflineNumCeilingsCompute,
    kRPVControllerRooflineCeilingComputeTypeIndexed,
    kRPVControllerRooflineCeilingComputeXIndexed,
    kRPVControllerRooflineCeilingComputeYIndexed,
    kRPVControllerRooflineCeilingComputeThroughputIndexed,
    kRPVControllerRooflineNumCeilingsBandwidth,
    kRPVControllerRooflineCeilingBandwidthTypeIndexed,
    kRPVControllerRooflineCeilingBandwidthXIndexed,
    kRPVControllerRooflineCeilingBandwidthYIndexed,
    kRPVControllerRooflineCeilingBandwidthThroughputIndexed,
    kRPVControllerRooflineNumCeilingsRidge,
    kRPVControllerRooflineCeilingRidgeComputeTypeIndexed,
    kRPVControllerRooflineCeilingRidgeBandwidthTypeIndexed,
    kRPVControllerRooflineCeilingRidgeXIndexed,
    kRPVControllerRooflineCeilingRidgeYIndexed,
    kRPVControllerRooflineNumKernels,
    kRPVControllerRooflineKernelIdIndexed,
    kRPVControllerRooflineKernelIntensityTypeIndexed,
    kRPVControllerRooflineKernelIntensityXIndexed,
    kRPVControllerRooflineKernelIntensityYIndexed,
    __kRPVControllerRooflinePropertiesLast
} rocprofvis_controller_roofline_properties_t;

/*
 * Roofline compute ceiling types.
 */
typedef enum rocprofvis_controller_roofline_ceiling_compute_type_t : uint32_t
{
    __KRPVControllerRooflineCeilingComputeTypeFirst,
    kRPVControllerRooflineCeilingComputeMFMAFP4 = __KRPVControllerRooflineCeilingComputeTypeFirst,
    kRPVControllerRooflineCeilingComputeMFMAFP6,
    kRPVControllerRooflineCeilingComputeMFMAFP8,
    kRPVControllerRooflineCeilingComputeVALUI8,
    kRPVControllerRooflineCeilingComputeMFMAI8,
    kRPVControllerRooflineCeilingComputeVALUFP16,
    kRPVControllerRooflineCeilingComputeMFMAFP16,
    kRPVControllerRooflineCeilingComputeMFMABF16,
    kRPVControllerRooflineCeilingComputeVALUFP32,
    kRPVControllerRooflineCeilingComputeMFMAFP32,
    kRPVControllerRooflineCeilingComputeVALUI32,
    kRPVControllerRooflineCeilingComputeVALUFP64,
    kRPVControllerRooflineCeilingComputeMFMAFP64,
    kRPVControllerRooflineCeilingComputeVALUI64,
    __KRPVControllerRooflineCeilingComputeTypeLast
} rocprofvis_controller_roofline_ceiling_compute_type_t;

/*
* Roofline bandwidth ceiling types.
*/
typedef enum rocprofvis_controller_roofline_ceiling_bandwidth_type_t : uint32_t
{
    __KRPVControllerRooflineCeilingBandwidthTypeFirst,
    kRPVControllerRooflineCeilingTypeBandwidthHBM = __KRPVControllerRooflineCeilingBandwidthTypeFirst,
    kRPVControllerRooflineCeilingTypeBandwidthL2,
    kRPVControllerRooflineCeilingTypeBandwidthL1,
    kRPVControllerRooflineCeilingTypeBandwidthLDS,
    __KRPVControllerRooflineCeilingBandwidthTypeLast
} rocprofvis_controller_roofline_ceiling_bandwidth_type_t;

/*
* Roofline kernel intensity types.
*/
typedef enum rocprofvis_controller_roofline_kernel_intensity_type_t : uint32_t
{
    kRPVControllerRooflineKernelIntensityTypeHBM,
    kRPVControllerRooflineKernelIntensityTypeL2,
    kRPVControllerRooflineKernelIntensityTypeL1,
} rocprofvis_controller_roofline_kernel_intensity_type_t;
#endif

/*
 * Profiler types supported by the profiler launcher
 */
typedef enum rocprofvis_profiler_type_t
{
    // ROCm Systems Profiler - sampling mode (single-stage)
    kRPVProfilerTypeRocprofSysRun = 0,
    // ROCm Systems Profiler - instrumentation mode (two-stage: instrument + run)
    kRPVProfilerTypeRocprofSysInstrument = 1,
    // ROCm Compute Profiler v2 (rocprof)
    kRPVProfilerTypeRocprofCompute = 2,
    // ROCm Compute Profiler v3 (rocprofv3)
    kRPVProfilerTypeRocprofV3 = 3,
} rocprofvis_profiler_type_t;

/*
 * Profiler execution state
 */
typedef enum rocprofvis_profiler_state_t
{
    // Profiler has not been launched yet
    kRPVProfilerStateIdle = 0,
    // Profiler is currently running
    kRPVProfilerStateRunning = 1,
    // Profiler completed successfully
    kRPVProfilerStateCompleted = 2,
    // Profiler failed with an error
    kRPVProfilerStateFailed = 3,
    // Profiler was cancelled by user
    kRPVProfilerStateCancelled = 4,
} rocprofvis_profiler_state_t;