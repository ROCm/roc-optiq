// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

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
} rocprofvis_result_t;

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
    // Number of tracks in the timeline view
    kRPVControllerTimelineNumTracks = 0x10000001,
    // Start timestamp in the view
    kRPVControllerTimelineMinTimestamp = 0x10000002,
    // Final timestamp in the view
    kRPVControllerTimelineMaxTimestamp = 0x10000003,
    // Indexed tracks
    kRPVControllerTimelineTrackIndexed = 0x10000004,
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
    // Graph for this track
    kRPVControllerTrackGraph = 0x30000002,
    // Start timestamp for the track
    kRPVControllerTrackMinTimestamp = 0x30000003,
    // Final timestamp for the track
    kRPVControllerTrackMaxTimestamp = 0x30000004,
    // Number of entries in the track
    kRPVControllerTrackNumberOfEntries = 0x30000005,
    // Entries are actually loaded via an async call to prepare for RPC
    kRPVControllerTrackEntry = 0x30000006,
    // Name
    kRPVControllerTrackName = 0x30000007,
    // Description
    kRPVControllerTrackDescription = 0x30000008,
    // Min value for sample tracks
    kRPVControllerTrackMinValue = 0x30000009,
    // Max value for sample tracks
    kRPVControllerTrackMaxValue = 0x3000000A,
    // The node that the track belongs to
    kRPVControllerTrackNode = 0x3000000B,
    // The processor that the track belongs to
    kRPVControllerTrackProcessor = 0x3000000C,
} rocprofvis_controller_track_properties_t;
/* JSON: RPVTrack
{
    track_type: rocprofvis_controller_track_type_t,
    min_timestamp: Double,
    max_timestamp: Double,
    num_entries: Int,
    graph: RPVGraph,
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
    // The source event ID
    kRPVControllerFlowControlSourceId = 0x50000000,
    // The number of target IDs
    kRPVControllerFlowControlNumTargetId = 0x50000001,
    // The indexed target IDs
    kRPVControllerFlowControlTargetIdIndexed = 0x50000002,
} rocprofvis_controller_flow_control_properties_t;
/* JSON: RPVFlowControl
{
    source_id: UInt64,
    num_targets: Int,
    targets: Array[UInt64]
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
    // Human readable source code file
    kRPVControllerCallstackFile = 0x60000001,
    // Human readable source code line
    kRPVControllerCallstackLine = 0x60000002,
    // ISA/ASM code function
    kRPVControllerCallstackISAFunction = 0x60000003,
    // ISA/ASM code file
    kRPVControllerCallstackISAFile = 0x60000004,
    // ISA/ASM code line
    kRPVControllerCallstackISALine = 0x60000005,
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
    // Owning track
    kRPVControllerEventTrack = 0x70000001,
    // Start time stamp for the event
    kRPVControllerEventStartTimestamp = 0x70000002,
    // End timestamp for the event
    kRPVControllerEventEndTimestamp = 0x70000003,
    // Name for the event
    kRPVControllerEventName = 0x70000004,
    // Number of callstack entries for the event
    kRPVControllerEventNumCallstackEntries = 0x70000005,
    // The number of input flow control entries
    kRPVControllerEventNumInputFlowControl = 0x70000006,
    // Indexed input flow control entries
    kRPVControllerEventInputFlowControlIndexed = 0x70000007,
    // The number of output flow control entries
    kRPVControllerEventNumOutputFlowControl = 0x70000008,
    // Indexed output flow control entries
    kRPVControllerEventOutputFlowControlIndexed = 0x70000009,

    // When a LOD is generated the sample will be synthetic and coalesce several real samples
    // These properties allow access to the contains samples.

    // Number of children within the synthetic sample
    kRPVControllerEventNumChildren = 0x7000000A,
    // Notionally indexed child events
    kRPVControllerEventChildIndexed = 0x7000000B,
    // Notionally indexed callstack entries
    kRPVControllerEventCallstackEntryIndexed = 0x7000000C,
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
    // Owning track
    kRPVControllerTableTrack = 0xA0000001,
    // Number of columns
    kRPVControllerTableNumColumns = 0xA0000002,
    // Number of rows
    kRPVControllerTableNumRows = 0xA0000003,
    // Indexed column header names
    kRPVControllerTableColumnHeaderIndexed = 0xA0000004,
    // Indexed row header names
    kRPVControllerTableRowHeaderIndexed = 0xA0000005,
    // Indexed column type
    kRPVControllerTableColumnTypeIndexed = 0xA0000006,
    // Notionally would give you an array for all the cells in the row
    // But this needs to be Async if we separate the Front/Back end
    kRPVControllerTableRowIndexed = 0xA0000007,
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

/*
* Properties for a future object
*/
typedef enum rocprofvis_controller_future_properties_t
{
    // Result object
    kRPVControllerFutureResult = 0xB0000000,
    // Type of result object, see rocprofvis_controller_object_type_t
    kRPVControllerFutureType = 0xB0000001,
} rocprofvis_controller_future_properties_t;

typedef uint32_t rocprofvis_property_t;
typedef struct rocprofvis_handle_t rocprofvis_handle_t;
typedef rocprofvis_handle_t rocprofvis_controller_t;
typedef rocprofvis_handle_t rocprofvis_controller_timeline_t;
typedef rocprofvis_handle_t rocprofvis_controller_view_t;
typedef rocprofvis_handle_t rocprofvis_controller_track_t;
typedef rocprofvis_handle_t rocprofvis_controller_graph_t;
typedef rocprofvis_handle_t rocprofvis_controller_table_t;
typedef rocprofvis_handle_t rocprofvis_controller_sample_t;
typedef rocprofvis_handle_t rocprofvis_controller_event_t;
typedef rocprofvis_handle_t rocprofvis_controller_flow_control_t;
typedef rocprofvis_handle_t rocprofvis_controller_callstack_t;
typedef rocprofvis_handle_t rocprofvis_controller_future_t;
typedef rocprofvis_handle_t rocprofvis_controller_array_t;
typedef rocprofvis_handle_t rocprofvis_controller_analysis_args_t;
typedef rocprofvis_handle_t rocprofvis_controller_node_t;
typedef rocprofvis_handle_t rocprofvis_controller_processor_t;

extern "C"
{
/*
* Create a controller for a given trace file.
* Future iterations may need a different mechanism, but for now we shall just assume everything is running on the same machine.
* @param filename File to open.
* @returns A valid controller, initialized to load the trace or nullptr on error.
*/
rocprofvis_controller_t* rocprofvis_controller_alloc(char const* const filename);
/* JSON: CreateController
{
    file_path: String,
}
->
{
    controller: RPVController
}
*/

/*
* Allocate a future object for use in async. calls.
* @returns A valid future object, or nullptr.
*/
rocprofvis_controller_future_t* rocprofvis_controller_future_alloc(void);

/*
* Allocate an array with an initial size for members (it will grow if needed).
* @param initial_size Initial array size.
* @returns A valid array object, or nullptr.
*/
rocprofvis_controller_array_t* rocprofvis_controller_array_alloc(uint32_t initial_size);

/*
* Gets the property value from the provided object or returns an error.
* @param object The object to access.
* @param property The property to access.
* @param index The index to access.
* @param value Value to write to.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_get_uint64(rocprofvis_handle_t* object, rocprofvis_property_t property, uint32_t index, uint64_t* value);

/*
* Gets the property value from the provided object or returns an error.
* @param object The object to access.
* @param property The property to access.
* @param index The index to access.
* @param value Value to write to.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_get_double(rocprofvis_handle_t* object, rocprofvis_property_t property, uint32_t index, double* value);

/*
* Gets the property value from the provided object or returns an error.
* @param object The object to access.
* @param property The property to access.
* @param index The index to access.
* @param value Value to write to.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_get_object(rocprofvis_handle_t* object, rocprofvis_property_t property, uint32_t index, rocprofvis_handle_t** value);

/*
* Gets the property value from the provided object or returns an error.
* @param object The object to access.
* @param property The property to access.
* @param index The index to access.
* @param value Buffer to write the string to, or null to access the length of the string.
* @param length Pointer to integer that contains the length of the buffer or to write the length of the string to.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_get_string(rocprofvis_handle_t* object, rocprofvis_property_t property, uint32_t index, char* value, uint32_t* length);

/*
* Waits on the future until it is completed or an error or timeout occurs.
* @param object The future to wait on.
* @param timeout The timeout to wait, 0.0f will not wait at all and FLT_MAX will wait infinitely.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_future_wait(rocprofvis_controller_future_t* object, float timeout);

/*
* Creates an analysis view from the data - the args object contains the specification for what analysis to run.
* This will require further definition depending on what analysis needs to be generated.
* In an ideal world this would actually be performed using Python scripts.
* @param controller The controller to analyze.
* @param args The arguments for the analysis - the properties and behaviour of this will be defined as analysis functions are developed.
* @param result The future to wait on and return the results.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_create_analysis_view_async(rocprofvis_controller_t* controller, rocprofvis_controller_analysis_args_t* args, rocprofvis_controller_future_t* result);
/* JSON: CreateAnalysisView
{
    controller: Int,
    args: Object,
}
->
{
    view: RPVView
}
*/

/*
* Closes the view and frees any associated memory
* @param controller The controller.
* @param view The view.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_close_view(rocprofvis_controller_t* controller, rocprofvis_controller_view_t* view);
/* JSON: CloseView
{
    controller: Int,
    view: Int,
}
*/

/*
* Fetches more data for the track.
* Arrays have to be owned by the caller, not the controller as this is a better match to an RPC API.
* The track loading returns the raw data only - it can do so by time segment or indexed
* @param controller The controller.
* @param track The track to fetch data from
* @param start_time The start time for the results
* @param end_time The end time for the results
* @param result The future to wait on
* @param output The array to write to
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_track_fetch_async(rocprofvis_controller_t* controller, rocprofvis_controller_track_t* track, double start_time, double end_time, rocprofvis_controller_future_t* result, rocprofvis_controller_array_t* output);
/* JSON: TrackFetch
{
    track: Int,
    start_time: Double,
    end_time: Double,
}
->
{
    results: Array[RPVEvent/RPVSample],
    type: rocprofvis_controller_track_type_t
}
*/

/*
* Fetches more data for the graph, generating a LOD if needded.
* Arrays have to be owned by the caller, not the controller as this is a better match to an RPC API.
* For a graph the array contains samples or events as appropriate - they may be real or synthetic depending on zoom level
* @param controller The controller.
* @param graph The graph to fetch data from
* @param start_time The start time for the results
* @param end_time The end time for the results
* @param x_resolution The number of pixels that can be displayed on the horizontal axis
* @param result The future to wait on
* @param output The array to write to
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_graph_fetch_async(rocprofvis_controller_t* controller, rocprofvis_controller_graph_t* graph, double start_time, double end_time, uint32_t x_resolution, rocprofvis_controller_future_t* result, rocprofvis_controller_array_t* output);
/* JSON: GraphFetch
{
    graph: Int,
    start_time: Double,
    end_time: Double,
    x_resolution: Int,
}
->
{
    results: Array[RPVEvent/RPVSample],
    type: rocprofvis_controller_graph_type_t
}
*/

/*
* Fetches more data for the table based on time.
* Fills in the array with an array for each row
* Arrays have to be owned by the caller, not the controller as this is a better match to an RPC API.
* Loading returns the raw data only - it can do so by time segment or indexed
* @param controller The controller.
* @param table The table to fetch data from
* @param start_time The start time for the results
* @param end_time The end time for the results
* @param result The future to wait on
* @param output The array to write to
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_table_fetch_async(rocprofvis_controller_t* controller, rocprofvis_controller_table_t* table, double start_time, double end_time, rocprofvis_controller_future_t* result, rocprofvis_controller_array_t* output);
/* JSON: TableFetch
{
    controller: Int,
    table: Int,
    start_time: Double,
    end_time: Double,
}
->
{
    results: Array[Array[UInt64/Double/String]]
}
*/

/*
* Get indexed properties from an object.
* @param controller The controller.
* @param object The event or sample to fetch data from
* @param property The property to access
* @param index The index to start at
* @param count The count of elements to get
* @param result The future to wait on
* @param output The array to write to
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_get_indexed_property_async(rocprofvis_controller_t* controller, rocprofvis_handle_t* object, rocprofvis_property_t property, uint32_t index, uint32_t count, rocprofvis_controller_future_t* result, rocprofvis_controller_array_t* output);
/* JSON: GetIndexedProperty
{
    controller: Int,
    object: Int,
    property: rocprofvis_property_t,
    start: Int,
    count: Int,
}
->
{
    results: Array[RPVEvent],
    type: rocprofvis_controller_object_type_t
}
*/

/*
* Frees the provided array
* @param object The object to free.
*/
void rocprofvis_controller_array_free(rocprofvis_controller_array_t* object);

/*
* Frees the provided future and any result it contains
* @param object The object to free.
*/
void rocprofvis_controller_future_free(rocprofvis_controller_future_t* object);

/*
* Frees the provided controller and all sub-objects
* @param object The object to free.
*/
void rocprofvis_controller_free(rocprofvis_controller_t* object);
/* JSON: FreeController
{
    controller: Int
}
*/
}

// The below would only be exposed to perform analysis - should be exposed to Python
extern "C"
{
/*
* Allocates a track
* @returns The track or nullptr
*/
rocprofvis_controller_track_t* rocprofvis_controller_track_alloc(void);

/*
* Allocates a graph
* @param track The owning track or nullptr
* @returns The graph or nullptr
*/
rocprofvis_controller_graph_t* rocprofvis_controller_graph_alloc(rocprofvis_controller_track_t* track);

/*
* Allocates a table
* @param track The owning track or nullptr
* @returns The table or nullptr
*/
rocprofvis_controller_table_t* rocprofvis_controller_table_alloc(rocprofvis_controller_track_t* track);

/*
* Allocates a sample
* @param track The owning track or nullptr
* @returns The sample or nullptr
*/
rocprofvis_controller_sample_t* rocprofvis_controller_sample_alloc(rocprofvis_controller_track_t* track);

/*
* Allocates an event
* @param track The owning track or nullptr
* @returns The event or nullptr
*/
rocprofvis_controller_event_t* rocprofvis_controller_event_alloc(rocprofvis_controller_track_t* track);

/*
* Allocates a flow-control entry
* @returns The flow-control or nullptr
*/
rocprofvis_controller_flow_control_t* rocprofvis_controller_flow_control_alloc(void);

/*
* Sets the property value from the provided object or returns an error.
* @param object The object to set.
* @param property The property to set.
* @param index The index to set.
* @param value Value to write.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_set_uint64(rocprofvis_handle_t* object, rocprofvis_property_t property, uint32_t index, uint64_t value);

/*
* Sets the property value from the provided object or returns an error.
* @param object The object to set.
* @param property The property to set.
* @param index The index to set.
* @param value Value to write.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_set_double(rocprofvis_handle_t* object, rocprofvis_property_t property, uint32_t index, double value);

/*
* Sets the property value from the provided object or returns an error.
* @param object The object to set.
* @param property The property to set.
* @param index The index to set.
* @param value Value to write.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_set_object(rocprofvis_handle_t* object, rocprofvis_property_t property, uint32_t index, rocprofvis_handle_t* value);

/*
* Sets the property value from the provided object or returns an error.
* @param object The object to set.
* @param property The property to set.
* @param index The index to set.
* @param value Value to write.
* @param length Length of string to write.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_set_string(rocprofvis_handle_t* object, rocprofvis_property_t property, uint32_t index, char* value, uint32_t length);

/*
* Frees the provided track
* @param track The track
*/
void rocprofvis_controller_track_free(rocprofvis_controller_track_t* track);

/*
* Frees the provided graph
* @param graph The graph
*/
void rocprofvis_controller_graph_free(rocprofvis_controller_graph_t* graph);

/*
* Frees the provided table
* @param table The table
*/
void rocprofvis_controller_table_free(rocprofvis_controller_table_t* table);

/*
* Frees the provided sample
* @param sample The sample
*/
void rocprofvis_controller_sample_free(rocprofvis_controller_sample_t* sample);

/*
* Frees the provided event
* @param event The event
*/
void rocprofvis_controller_event_free(rocprofvis_controller_event_t* event);

/*
* Frees the provided flow-control
* @param flow_control The flow-control
*/
void rocprofvis_controller_flow_control_free(rocprofvis_controller_flow_control_t* flow_control);
}
