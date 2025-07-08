// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

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
} rocprofvis_result_t;

/*
* Common object properties
*/
typedef enum rocprofvis_common_property_t
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
typedef enum rocprofvis_controller_properties_t
{
    // Id of the controller - one per trace
    kRPVControllerId = 0x00000000,
    // Global timeline view controller
    kRPVControllerTimeline = 0x00000001,
    // Global event table controller
    kRPVControllerEventTable = 0x00000002,
    // Number of initialized analysis views - these are driven by scripts
    kRPVControllerNumAnalysisView = 0x00000003,
    // Indexed analysis views
    kRPVControllerAnalysisViewIndexed = 0x00000004,
    // Number of nodes within the trace
    kRPVControllerNumNodes = 0x00000005,
    // Indexed nodes
    kRPVControllerNodeIndexed = 0x00000006,
    // Number of tracks in the trace
    kRPVControllerNumTracks = 0x00000007,
    // Indexed tracks
    kRPVControllerTrackIndexed = 0x00000008,
    // Global sample table controller
    kRPVControllerSampleTable = 0x00000009,
#ifdef COMPUTE_UI_SUPPORT
    // Compute trace controller
    kRPVControllerComputeTrace = 0x00000010,
#endif
    // Indexed event in the trace
    kRPVControllerEventIndexed = 0x00000011,
    // Load Event Flow control properties
    kRPVControllerEventDataFlowControlIndexed = 0x00000012,
    // Load Event Callstack properties
    kRPVControllerEventDataCallStackIndexed = 0x00000013,
    // Load Event Extended data properties
    kRPVControllerEventDataExtDataIndexed = 0x00000014,
    // Tracks by Id
    kRPVControllerTrackById = 0x00000015,
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
* The timeline is the primary view on to the data, it contains tracks which can be iterated
*/
typedef enum rocprofvis_controller_timeline_properties_t
{
    // Timeline ID
    kRPVControllerTimelineId = 0x10000000,
    // Number of graphs in the timeline view
    kRPVControllerTimelineNumGraphs = 0x10000001,
    // Start timestamp in the view
    kRPVControllerTimelineMinTimestamp = 0x10000002,
    // Final timestamp in the view
    kRPVControllerTimelineMaxTimestamp = 0x10000003,
    // Indexed graphs
    kRPVControllerTimelineGraphIndexed = 0x10000004,
    // Graphs by Id
    kRPVControllerTimelineGraphById = 0x10000005,
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
* A view holds analysis views that display data generated from the raw contents of the trace
*/
typedef enum rocprofvis_controller_view_properties_t
{
    // ID of view
    kRPVControllerViewId = 0x20000000,
    // Number of graphs within the view
    kRPVControllerViewNumGraphs = 0x20000001,
    // Number of tables within the view
    kRPVControllerViewNumTables = 0x20000002,
    // Indexed graph
    kRPVControllerViewGraphIndexed = 0x20000003,
    // Indexed table
    kRPVControllerViewTableIndexed = 0x20000004
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

typedef enum rocprofvis_controller_node_properties_t
{
    kRPVControllerNodeId = 0xC0000000,
    kRPVControllerNodeName = 0xC0000001,
    kRPVControllerNodeDescription = 0xC0000002,
    kRPVControllerNodeNumProcessors = 0xC0000003,
    kRPVControllerNodeProcessorIndexed = 0xC0000004,
} rocprofvis_controller_node_properties_t;
/* JSON: RPVNode
{
    id: Int,
    name: String,
    description: String,
    num_processors: Int,
    processors: Array[RPVProcessor]
}
*/

typedef enum rocprofvis_controller_processor_properties_t
{
    kRPVControllerProcessorId = 0xD0000000,
    kRPVControllerProcessorName = 0xD0000001,
    kRPVControllerProcessorDescription = 0xD0000002,
    kRPVControllerProcessorNumTracks = 0xD0000003,
    kRPVControllerProcessorTrackIndexed = 0xD0000004,
    kRPVControllerProcessorNode = 0xD0000005,
} rocprofvis_controller_processor_properties_t;
/* JSON: RPVProcessor
{
    id: Int,
    name: String,
    description: String,
    num_track: Int,
    track: Array[RPVTrack],
    node: RPVNode
}
*/

/*
* Properties for each track of data within the data set.
*/
typedef enum rocprofvis_controller_track_properties_t
{
    kRPVControllerTrackId = 0x30000000,
    // Track type, see rocprofvis_controller_track_type_t.
    kRPVControllerTrackType = 0x30000001,
    // Start timestamp for the track
    kRPVControllerTrackMinTimestamp = 0x30000002,
    // Final timestamp for the track
    kRPVControllerTrackMaxTimestamp = 0x30000003,
    // Number of entries in the track
    kRPVControllerTrackNumberOfEntries = 0x30000004,
    // Entries are actually loaded via an async call to prepare for RPC
    kRPVControllerTrackEntry = 0x30000005,
    // Name
    kRPVControllerTrackName = 0x30000006,
    // Description
    kRPVControllerTrackDescription = 0x30000007,
    // Min value for sample tracks
    kRPVControllerTrackMinValue = 0x30000008,
    // Max value for sample tracks
    kRPVControllerTrackMaxValue = 0x30000009,
    // The node that the track belongs to
    kRPVControllerTrackNode = 0x3000000A,
    // The processor that the track belongs to
    kRPVControllerTrackProcessor = 0x3000000B,
    // Track extended data number of entries
    kRPVControllerTrackExtDataNumberOfEntries = 0x3000000C,
    // Track extended data category
    kRPVControllerTrackExtDataCategoryIndexed = 0x3000000D,
    // Track extended data name
    kRPVControllerTrackExtDataNameIndexed = 0x3000000E,
    // Track extended data value
    kRPVControllerTrackExtDataValueIndexed = 0x3000000F,
} rocprofvis_controller_track_properties_t;
/* JSON: RPVTrack
{
    track_type: rocprofvis_controller_track_type_t,
    min_timestamp: Double,
    max_timestamp: Double,
    num_entries: Int,
    node: RPVNode,
    processor: RPVProcessor,
    -> entries: Array[RPVSample/RPVEvent], -> Definitely should not always be loaded. Need an API to load them in an array.
}
*/

/*
* Properties for a sample in a track or graph
*/
typedef enum rocprofvis_controller_sample_properties_t
{
    // Sample Id
    kRPVControllerSampleId = 0x40000000,
    // Owning track
    kRPVControllerSampleTrack = 0x40000001,
    // Sample value data type, see rocprofvis_controller_primitive_type_t.
    kRPVControllerSampleType = 0x40000002,
    // Sample timestamp
    kRPVControllerSampleTimestamp = 0x40000003,
    // Sample value
    kRPVControllerSampleValue = 0x40000004,

    // When a LOD is generated the sample will be synthetic and coalesce several real samples
    // These properties allow access to the contains samples.

    // Number of children within the synthetic sample
    kRPVControllerSampleNumChildren = 0x40000005,
    // Indexed children
    kRPVControllerSampleChildIndex = 0x40000006,
    // Min value for children
    kRPVControllerSampleChildMin = 0x40000007,
    // Mean value for children
    kRPVControllerSampleChildMean = 0x40000008,
    // Median value for children
    kRPVControllerSampleChildMedian = 0x40000009,
    // Max value for children
    kRPVControllerSampleChildMax = 0x4000000A,
    // Min timestamp for children
    kRPVControllerSampleChildMinTimestamp = 0x4000000B,
    // Max timestamp for children
    kRPVControllerSampleChildMaxTimestamp = 0x4000000C,
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
    -> children: Array[RPVSample] -> Can we load these - or would that cause us to load too much?
}
*/

/*
 * Properties for a flow-control entry.
 */
typedef enum rocprofvis_controller_flow_control_properties_t
{
    // The target event ID
    kRPVControllerFlowControltId       = 0x50000000,
    // The target timestamp
    kRPVControllerFlowControlTimestamp = 0x50000001,
    // The target track ID
    kRPVControllerFlowControlTrackId   = 0x50000002,
    // Flow direction 0 - outgoing, 1 - incoming
    kRPVControllerFlowControlDirection = 0x50000003,
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
* Each entry resolves the ISA/ASM function/file/line and if possible the human readable source version.
* The backend is the right place to resolve the ISA to Source mapping so we can reuse any code that already does it. 
*/
typedef enum rocprofvis_controller_callstack_properties_t
{
    // Human readable source code function
    kRPVControllerCallstackFunction = 0x60000000,
    // Human readable source code function arguments
    kRPVControllerCallstackArguments = 0x60000001,
    // Human readable source code file
    kRPVControllerCallstackFile = 0x60000002,
    // Human readable source code line
    kRPVControllerCallstackLine = 0x60000003,
    // ISA/ASM code function
    kRPVControllerCallstackISAFunction = 0x60000004,
    // ISA/ASM code file
    kRPVControllerCallstackISAFile = 0x60000005,
    // ISA/ASM code line
    kRPVControllerCallstackISALine = 0x60000006,
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
typedef enum rocprofvis_controller_event_properties_t
{
    // Unique ID for the event
    kRPVControllerEventId = 0x70000000,
    // Start time stamp for the event
    kRPVControllerEventStartTimestamp = 0x70000001,
    // End timestamp for the event
    kRPVControllerEventEndTimestamp = 0x70000002,
    // Name for the event
    kRPVControllerEventName = 0x70000003,
    // Level in the stack of events
    kRPVControllerEventLevel = 0x70000004,

    // When a LOD is generated the sample will be synthetic and coalesce several real samples
    // These properties allow access to the contains samples.

    // Number of children within the synthetic sample
    kRPVControllerEventNumChildren = 0x7000000A,
    // Notionally indexed child events
    kRPVControllerEventChildIndexed = 0x7000000B,
    // Notionally indexed callstack entries
    kRPVControllerEventCallstackEntryIndexed = 0x7000000C,
    // Category for the event
    kRPVControllerEventCategory = 0x7000000D,
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
    -> callstack: Array[Object], -> Can this be loaded all the time - or would it lead to loading too much?
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
typedef enum rocprofvis_controller_graph_properties_t
{
    // The graph ID
    kRPVControllerGraphId = 0x80000000,
    // See rocprofvis_controller_graph_type_t
    kRPVControllerGraphType = 0x80000001,
    // The owning track if it is part of the timeline view - null if this is an analysis graph
    kRPVControllerGraphTrack = 0x80000002,
    // Start time for the graph
    kRPVControllerGraphStartTimestamp = 0x80000003,
    // End time for the graph
    kRPVControllerGraphEndTimestamp = 0x80000004,
    // Number of entries in the graph
    kRPVControllerGraphNumEntries = 0x80000005,
    // Notionally indexed entries in the graph
    // Actually loaded via an async. API to prepare for RPC.
    kRPVControllerGraphEntryIndexed = 0x80000006,
} rocprofvis_controller_graph_properties_t;
/* JSON: RPVGraph
{
    id: Int,
    graph_type: rocprofvis_controller_graph_type_t,
    start_timestamp: Double,
    end_timestamp: Double,
    num_entries: Int,
    -> entries: Array[RPVSample/RPVEvent] -> Needs an API to load in segments and at appropriate LOD.
}
*/

/*
* Properties for an array.
*/
typedef enum rocprofvis_controller_array_properties_t
{
    // Number of entries in array.
    kRPVControllerArrayNumEntries = 0x90000000,
    // Indexed entry.
    kRPVControllerArrayEntryIndexed = 0x90000001,
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
typedef enum rocprofvis_controller_table_properties_t
{
    // Id for the table
    kRPVControllerTableId = 0xA0000000,
    // Number of columns
    kRPVControllerTableNumColumns = 0xA0000001,
    // Number of rows
    kRPVControllerTableNumRows = 0xA0000002,
    // Indexed column header names
    kRPVControllerTableColumnHeaderIndexed = 0xA0000003,
    // Indexed row header names
    kRPVControllerTableRowHeaderIndexed = 0xA0000004,
    // Indexed column type
    kRPVControllerTableColumnTypeIndexed = 0xA0000005,
    // Notionally would give you an array for all the cells in the row
    // But this needs to be Async if we separate the Front/Back end
    kRPVControllerTableRowIndexed = 0xA0000006,
    // Table title
    kRPVControllerTableTitle = 0xA0000007,
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

typedef enum rocprofvis_controller_table_arguments_t
{
    kRPVControllerTableArgsType = 0xE0000000,
    kRPVControllerTableArgsNumTracks = 0xE0000001,
    kRPVControllerTableArgsTracksIndexed = 0xE0000002,
    kRPVControllerTableArgsStartTime = 0xE0000003,
    kRPVControllerTableArgsEndTime = 0xE0000004,
    kRPVControllerTableArgsSortColumn = 0xE0000005,
    kRPVControllerTableArgsSortOrder = 0xE0000006,
    kRPVControllerTableArgsStartIndex = 0xE0000007,
    kRPVControllerTableArgsStartCount = 0xE0000008,
} rocprofvis_controller_table_arguments_t;

typedef enum rocprofvis_controller_table_type_t
{
    kRPVControllerTableTypeEvents = 0xF0000000,
    kRPVControllerTableTypeSamples = 0xF0000001,
} rocprofvis_controller_table_type_t;

/*
* Properties for a future object
*/
typedef enum rocprofvis_controller_future_properties_t
{
    // Result code
    kRPVControllerFutureResult = 0xB0000000,
    // Data object
    kRPVControllerFutureObject = 0xB0000001,
    // Type of object, see rocprofvis_controller_object_type_t
    kRPVControllerFutureType = 0xB0000002,
} rocprofvis_controller_future_properties_t;

/*
* Event extended properties . To be used by rocprofvis_controller_get_indexed_property_async 
*/
typedef enum rocprofvis_controller_event_data_properties_t
{
    // Load Event Flow control properties  
    kRPVControllerEventDataFlowControl = 0xC0000000,
    // Load Event Callstack properties  
    kRPVControllerEventDataCallStack   = 0xC0000001,
    // Load Event Extended data properties
    kRPVControllerEventDataExtData     = 0xC0000002,
} rocprofvis_controller_event_data_properties_t;

/*
* Properties for extended data
*/
typedef enum rocprofvis_controller_extdata_properties_t
{
    // Extended data category
    kRPVControllerExtDataCategory = 0xD0000000,
    // Extended data name
    kRPVControllerExtDataName = 0xD0000001,
    // Extended data value
    kRPVControllerExtDataValue = 0xD0000002,
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
    kRPVControllerComputePlotTypeKernelDurationPercentage = kRPVControllerComputeTableTypeCount + 1,
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
typedef enum rocprofvis_controller_plot_properties_t
{
    // Id for the plot
    kRPVControllerPlotId = 0x1A000000,
    // Number of data series
    kRPVControllerPlotNumSeries = 0x1A000001,
    // Number of x axis tick labels
    kRPVControllerPlotNumXAxisLabels = 0x1A000002,
    // Number of y axis tick labels
    kRPVControllerPlotNumYAxisLabels = 0x1A000003,
    // Indexed x axis tick labels
    kRPVControllerPlotXAxisLabelsIndexed = 0x1A000004,
    // Indexed y axis tick labels
    kRPVControllerPlotYAxisLabelsIndexed = 0x1A000005,
    // X axis title
    kRPVControllerPlotXAxisTitle = 0x1A000006,
    // Y axis title
    kRPVControllerPlotYAxisTitle = 0x1A000007,
    // Plot title
    kRPVControllerPlotTitle = 0x1A000008,
} rocprofvis_controller_plot_properties_t;

/*
* Properties of a PlotSeries object
*/
typedef enum rocprofvis_controller_plot_series_properties_t
{
    // Number of x,y pairs in the series
    kRPVControllerPlotSeriesNumValues = 0x2A000000,
    // Indexed x values
    kRPVControllerPlotSeriesXValuesIndexed = 0x2A000001,
    // Indexed y values
    kRPVControllerPlotSeriesYValuesIndexed = 0x2A000002,
    // Series name
    kRPVControllerPlotSeriesName = 0x2A000003,
} rocprofvis_controller_plot_series_properties_t;
#endif