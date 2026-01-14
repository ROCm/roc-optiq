// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
* Create a controller for a given trace file.
* Future iterations may need a different mechanism, but for now we shall just assume everything is running on the same machine.
* @param filename File to open.
* @returns A valid controller, initialized to load the trace or nullptr on error.
*/
rocprofvis_controller_t* rocprofvis_controller_alloc(char const* const filename);

/*
* Loads the file into the controller or returns an error.
* @param controller The controller to load into.
* @param future The object that tells you when the file has been loaded.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_load_async(rocprofvis_controller_t* controller, rocprofvis_controller_future_t* future);
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

rocprofvis_controller_arguments_t* rocprofvis_controller_arguments_alloc(void);

/*
* Allocate a summary metrics container.
* @returns A valid summary metrics container object, or nullptr.
*/
rocprofvis_controller_summary_metrics_t* rocprofvis_controller_summary_metrics_alloc(void);

/*
* Gets the property value from the provided object or returns an error.
* @param object The object to access.
* @param property The property to access.
* @param index The index to access.
* @param value Value to write to.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_get_uint64(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, uint64_t* value);

/*
* Gets the property value from the provided object or returns an error.
* @param object The object to access.
* @param property The property to access.
* @param index The index to access.
* @param value Value to write to.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_get_double(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, double* value);

/*
* Gets the property value from the provided object or returns an error.
* @param object The object to access.
* @param property The property to access.
* @param index The index to access.
* @param value Value to write to.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_get_object(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value);

/*
* Gets the object type of the provided object or returns an error.
* @param object The object to access.
* @param type Output to write to.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_get_object_type(rocprofvis_handle_t* object, rocprofvis_controller_object_type_t* type);

/*
* Gets the property value from the provided object or returns an error.
* @param object The object to access.
* @param property The property to access.
* @param index The index to access.
* @param value Buffer to write the string to, or null to access the length of the string.
* @param length Pointer to integer that contains the length of the buffer or to write the length of the string to.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_get_string(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length);

/*
* Waits on the future until it is completed or an error or timeout occurs.
* @param object The future to wait on.
* @param timeout The timeout to wait, 0.0f will not wait at all and FLT_MAX will wait infinitely.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_future_wait(rocprofvis_controller_future_t* object, float timeout);

/*
* Attempts to cancel the future. It is an error to assume operations can be cancelled.
* @param object The future to try to cancel.
* @returns kRocProfVisResultSuccess if cancelled or an error code if the operation could not be cancelled.
*/
rocprofvis_result_t rocprofvis_controller_future_cancel(rocprofvis_controller_future_t* object);

/*
* Creates an analysis view from the data - the args object contains the specification for what analysis to run.
* This will require further definition depending on what analysis needs to be generated.
* In an ideal world this would actually be performed using Python scripts.
* @param controller The controller to analyze.
* @param args The arguments for the analysis - the properties and behaviour of this will be defined as analysis functions are developed.
* @param result The future to wait on and return the results.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_create_analysis_view_async(rocprofvis_controller_t* controller, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result);
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
* Setup the table based on the values in 'args' and fetch initial data.
* Arrays have to be owned by the caller, not the controller as this is a better match to an RPC API.
* Loading returns the formatted data for each row.
* The caller can load more data using rocprofvis_controller_get_indexed_property_async to load by row index.
* @param controller The controller.
* @param table The table to fetch data from
* @param args The arguments that setup the table
* @param result The future to wait on
* @param output The array to write to
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_table_fetch_async(rocprofvis_controller_t* controller, rocprofvis_controller_table_t* table, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result, rocprofvis_controller_array_t* output);
/* JSON: TableFetch
{
    controller: Int,
    table: Int,
    args: Object,
}
->
{
    results: Array[Array[UInt64/Double/String]]
}
*/

/*
* Writes the results of a table query to a .CSV file.
* @param controller The controller
* @param table The table to fetch data from
* @param args The arguments that setup the table
* @param result The future to wait on
* @param path The path to write the .CSV
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_table_export_csv(rocprofvis_controller_t* controller, rocprofvis_controller_table_t* table, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result, char const* path);

/*
* Setup the summary based on the values in 'args' and fetch data.
* @param controller The controller.
* @param table The summary controller to fetch data from
* @param args The arguments that setup the summary
* @param result The future to wait on
* @param output The summary metrics container to write to
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_summary_fetch_async(rocprofvis_controller_t* controller, rocprofvis_controller_summary_t* summary, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result, rocprofvis_controller_summary_metrics_t* output);

#ifdef COMPUTE_UI_SUPPORT
/*
* Setup and fetches data for a plot, populating the passed in array with PlotSeries objects.
* Data within PlotSeries can be accessed using GetUInt64/GetDouble/GetString.
* @param controller The controller.
* @param plot The plot to fetch data from
* @param args The arguments that setup the plot
* @param result The future to wait on
* @param output The array to write to
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_plot_fetch_async(rocprofvis_controller_t* controller, rocprofvis_controller_plot_t* plot, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result, rocprofvis_controller_array_t* output);
#endif

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
rocprofvis_result_t rocprofvis_controller_get_indexed_property_async(rocprofvis_controller_t* controller, rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, uint32_t count, rocprofvis_controller_future_t* result, rocprofvis_controller_array_t* output);
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
* Frees the provided summary metrics container object
* @param object The object to free.
*/
void rocprofvis_controller_summary_metric_free(rocprofvis_controller_summary_metrics_t* object);

void rocprofvis_controller_arguments_free(rocprofvis_controller_arguments_t* args);

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

#ifdef __cplusplus
}
#endif

// The below would only be exposed to perform analysis - should be exposed to Python
#ifdef __cplusplus
extern "C"
{
#endif

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
rocprofvis_result_t rocprofvis_controller_set_uint64(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, uint64_t value);

/*
* Sets the property value from the provided object or returns an error.
* @param object The object to set.
* @param property The property to set.
* @param index The index to set.
* @param value Value to write.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_set_double(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, double value);

/*
* Sets the property value from the provided object or returns an error.
* @param object The object to set.
* @param property The property to set.
* @param index The index to set.
* @param value Value to write.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_set_object(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value);

/*
* Sets the property value from the provided object or returns an error.
* @param object The object to set.
* @param property The property to set.
* @param index The index to set.
* @param value Value to write.
* @param length Length of string to write.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_set_string(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, char const* value);

rocprofvis_result_t rocprofvis_controller_save_trimmed_trace(rocprofvis_handle_t* object, double start, double end, char const* path, rocprofvis_controller_future_t* future);

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

#ifdef __cplusplus
}
#endif
