// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The controller C ABI for a thin client (ROCPROFVIS_THIN_CLIENT): a roc-optiq
// that opens traces only through a roc-optiq-server, so it links neither the
// controller nor the data model nor SQLite. Everything that would touch a trace
// on this machine reports kRocProfVisResultNotSupported or allocates nothing,
// and the UI treats that as a trace that cannot be opened. Profiler tool names
// are real, as the launcher previews the command a server will run.
//
// Every function the controller headers declare is defined here, so a new one
// fails the thin build at link time until it is added.

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_analysis.h"
#include "rocprofvis_controller_script.h"
#include "rocprofvis_profiler.h"
#include "rocprofvis_controller_profiler_tool.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace
{

// The controller's string-getter convention: a null buffer or zero length
// queries the length; otherwise up to *length bytes are copied, unterminated.
rocprofvis_result_t
copy_string_to_buffer(const std::string& source, char* buffer, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        if(buffer == nullptr || *length == 0)
        {
            *length = static_cast<uint32_t>(source.size());
        }
        else
        {
            *length = std::min(*length, static_cast<uint32_t>(source.size()));
            std::memcpy(buffer, source.data(), *length);
        }
        result = kRocProfVisResultSuccess;
    }
    return result;
}

}  // namespace

extern "C"
{

rocprofvis_result_t
rocprofvis_profiler_tool_get_binary_name(rocprofvis_profiler_tool_t tool, char* buffer,
                                         uint32_t* length)
{
    const char* name = RocProfVis::Controller::ProfilerTool::GetBinaryName(tool);
    return name ? copy_string_to_buffer(name, buffer, length) : kRocProfVisResultInvalidEnum;
}

rocprofvis_result_t
rocprofvis_profiler_tool_resolve_path(rocprofvis_profiler_tool_t tool, char const* tool_directory,
                                      char* buffer, uint32_t* length)
{
    std::string         resolved;
    rocprofvis_result_t result = RocProfVis::Controller::ProfilerTool::ResolvePath(
        tool, tool_directory ? tool_directory : "", resolved);
    return result == kRocProfVisResultSuccess ? copy_string_to_buffer(resolved, buffer, length)
                                              : result;
}

// rocprofvis_controller.h

rocprofvis_controller_t*
rocprofvis_controller_alloc(char const* const filename, char const* const config_path)
{
    return nullptr;
}
#ifdef ROCPROFVIS_ENABLE_TRACE_COMPARE

rocprofvis_controller_t*
rocprofvis_controller_alloc_compare(char const* const* filenames, uint64_t count)
{
    return nullptr;
}
#endif  // ROCPROFVIS_ENABLE_TRACE_COMPARE

rocprofvis_result_t
rocprofvis_controller_load_async(rocprofvis_controller_t* controller,
                                 rocprofvis_controller_future_t* future)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_controller_future_t*
rocprofvis_controller_future_alloc(void)
{
    return nullptr;
}

rocprofvis_controller_array_t*
rocprofvis_controller_array_alloc(uint32_t initial_size)
{
    return nullptr;
}

rocprofvis_controller_arguments_t*
rocprofvis_controller_arguments_alloc(void)
{
    return nullptr;
}

rocprofvis_controller_summary_metrics_t*
rocprofvis_controller_summary_metrics_alloc(void)
{
    return nullptr;
}

rocprofvis_result_t
rocprofvis_controller_get_uint64(rocprofvis_handle_t* object,
                                 rocprofvis_property_t property,
                                 uint64_t index,
                                 uint64_t* value)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_get_double(rocprofvis_handle_t* object,
                                 rocprofvis_property_t property,
                                 uint64_t index,
                                 double* value)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_get_object(rocprofvis_handle_t* object,
                                 rocprofvis_property_t property,
                                 uint64_t index,
                                 rocprofvis_handle_t** value)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_get_object_type(rocprofvis_handle_t* object,
                                      rocprofvis_controller_object_type_t* type)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_get_string(rocprofvis_handle_t* object,
                                 rocprofvis_property_t property,
                                 uint64_t index,
                                 char* value,
                                 uint32_t* length)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_future_wait(rocprofvis_controller_future_t* object, float timeout)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_future_cancel(rocprofvis_controller_future_t* object)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_create_analysis_view_async(rocprofvis_controller_t* controller,
                                                 rocprofvis_controller_arguments_t* args,
                                                 rocprofvis_controller_future_t* result)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_close_view(rocprofvis_controller_t* controller,
                                 rocprofvis_controller_view_t* view)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_track_fetch_async(rocprofvis_controller_t* controller,
                                        rocprofvis_controller_track_t* track,
                                        double start_time,
                                        double end_time,
                                        rocprofvis_controller_future_t* result,
                                        rocprofvis_controller_array_t* output)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_graph_fetch_async(rocprofvis_controller_t* controller,
                                        rocprofvis_controller_graph_t* graph,
                                        double start_time,
                                        double end_time,
                                        uint32_t x_resolution,
                                        rocprofvis_controller_future_t* result,
                                        rocprofvis_controller_array_t* output)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_table_fetch_async(rocprofvis_controller_t* controller,
                                        rocprofvis_controller_table_t* table,
                                        rocprofvis_controller_arguments_t* args,
                                        rocprofvis_controller_future_t* result,
                                        rocprofvis_controller_array_t* output)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_table_export_csv(rocprofvis_controller_t* controller,
                                       rocprofvis_controller_table_t* table,
                                       rocprofvis_controller_arguments_t* args,
                                       rocprofvis_controller_future_t* result,
                                       char const* path)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_controller_metrics_container_t*
rocprofvis_controller_metrics_container_alloc(void)
{
    return nullptr;
}

void
rocprofvis_controller_metrics_container_free(rocprofvis_controller_metrics_container_t* object)
{}

rocprofvis_result_t
rocprofvis_controller_metric_fetch_async(rocprofvis_controller_t* controller,
                                         rocprofvis_controller_arguments_t* args,
                                         rocprofvis_controller_future_t* result,
                                         rocprofvis_controller_metrics_container_t* output)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_pc_sampling_fetch_isa_lines_async(rocprofvis_controller_t* controller,
                                                        rocprofvis_controller_arguments_t* args,
                                                        rocprofvis_controller_future_t* result,
                                                        rocprofvis_handle_t* output)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_pc_sampling_fetch_source_async(rocprofvis_controller_t* controller,
                                                     rocprofvis_controller_arguments_t* args,
                                                     rocprofvis_controller_future_t* result,
                                                     rocprofvis_handle_t* output)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_pc_sampling_fetch_stalls_async(rocprofvis_controller_t* controller,
                                                     rocprofvis_controller_arguments_t* args,
                                                     rocprofvis_controller_future_t* result,
                                                     rocprofvis_handle_t* output)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_get_indexed_property_async(rocprofvis_controller_t* controller,
                                                 rocprofvis_handle_t* object,
                                                 rocprofvis_property_t property,
                                                 uint64_t index,
                                                 uint32_t count,
                                                 rocprofvis_controller_future_t* result,
                                                 rocprofvis_controller_array_t* output)
{
    return kRocProfVisResultNotSupported;
}
#ifdef ROCPROFVIS_ENABLE_REMOTE

rocprofvis_result_t
rocprofvis_controller_ssh_connection_alloc(rocprofvis_controller_arguments_t* args,
                                           rocprofvis_controller_array_t* output)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_ssh_connection_free(rocprofvis_controller_connection_t* connection)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_remote_connect_async(rocprofvis_controller_future_t* future,
                                           rocprofvis_controller_connection_t* connection)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_remote_authenticate_async(rocprofvis_controller_future_t* future,
                                                rocprofvis_controller_connection_t* connection,
                                                rocprofvis_controller_arguments_t* args)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_remote_execute_async(rocprofvis_controller_future_t* future,
                                           rocprofvis_controller_connection_t* connection,
                                           rocprofvis_controller_arguments_t* args)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_remote_transfer_async(rocprofvis_controller_future_t* future,
                                            rocprofvis_controller_connection_t* connection,
                                            rocprofvis_controller_arguments_t* args)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_remote_browser_async(rocprofvis_controller_future_t* future,
                                           rocprofvis_controller_connection_t* connection,
                                           rocprofvis_controller_arguments_t* args)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_remote_submit_hostkey_decision(rocprofvis_controller_connection_t* connection,
                                                     uint64_t decision)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_remote_submit_responses(rocprofvis_controller_connection_t* connection,
                                              rocprofvis_controller_arguments_t* args)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_remote_cancel_prompt(rocprofvis_controller_connection_t* connection)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_remote_reset(rocprofvis_controller_connection_t* connection)
{
    return kRocProfVisResultNotSupported;
}
#endif  // ROCPROFVIS_ENABLE_REMOTE

void
rocprofvis_controller_summary_metric_free(rocprofvis_controller_summary_metrics_t* object)
{}

void
rocprofvis_controller_arguments_free(rocprofvis_controller_arguments_t* args)
{}

void
rocprofvis_controller_array_free(rocprofvis_controller_array_t* object)
{}

void
rocprofvis_controller_future_free(rocprofvis_controller_future_t* object)
{}

void
rocprofvis_controller_free(rocprofvis_controller_t* object)
{}

rocprofvis_controller_track_t*
rocprofvis_controller_track_alloc(void)
{
    return nullptr;
}

rocprofvis_controller_graph_t*
rocprofvis_controller_graph_alloc(rocprofvis_controller_track_t* track)
{
    return nullptr;
}

rocprofvis_controller_table_t*
rocprofvis_controller_table_alloc(void)
{
    return nullptr;
}

rocprofvis_controller_table_t*
rocprofvis_controller_search_table_alloc(void)
{
    return nullptr;
}

rocprofvis_controller_sample_t*
rocprofvis_controller_sample_alloc(rocprofvis_controller_track_t* track)
{
    return nullptr;
}

rocprofvis_controller_event_t*
rocprofvis_controller_event_alloc(rocprofvis_controller_track_t* track)
{
    return nullptr;
}

rocprofvis_controller_flow_control_t*
rocprofvis_controller_flow_control_alloc(void)
{
    return nullptr;
}

rocprofvis_result_t
rocprofvis_controller_set_uint64(rocprofvis_handle_t* object,
                                 rocprofvis_property_t property,
                                 uint64_t index,
                                 uint64_t value)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_set_double(rocprofvis_handle_t* object,
                                 rocprofvis_property_t property,
                                 uint64_t index,
                                 double value)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_set_object(rocprofvis_handle_t* object,
                                 rocprofvis_property_t property,
                                 uint64_t index,
                                 rocprofvis_handle_t* value)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_set_string(rocprofvis_handle_t* object,
                                 rocprofvis_property_t property,
                                 uint64_t index,
                                 char const* value)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_save_trimmed_trace(rocprofvis_handle_t* object,
                                         double start,
                                         double end,
                                         char const* path,
                                         rocprofvis_controller_future_t* future)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_controller_cleanup_trace_database(rocprofvis_handle_t* object,
                                             bool rebuild,
                                             rocprofvis_controller_future_t* future)
{
    return kRocProfVisResultNotSupported;
}

void
rocprofvis_controller_track_free(rocprofvis_controller_track_t* track)
{}

void
rocprofvis_controller_graph_free(rocprofvis_controller_graph_t* graph)
{}

void
rocprofvis_controller_table_free(rocprofvis_controller_table_t* table)
{}

void
rocprofvis_controller_sample_free(rocprofvis_controller_sample_t* sample)
{}

void
rocprofvis_controller_event_free(rocprofvis_controller_event_t* event)
{}

void
rocprofvis_controller_flow_control_free(rocprofvis_controller_flow_control_t* flow_control)
{}

// rocprofvis_controller_analysis.h

rocprofvis_result_t
rocprofvis_analysis_fetch_counter_statistics(rocprofvis_controller_t* controller,
                                             rocprofvis_controller_track_t* track,
                                             double start_time,
                                             double end_time,
                                             rocprofvis_controller_future_t* result,
                                             rocprofvis_analysis_counter_statistics_t* output)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_fetch_queue_utilization(rocprofvis_controller_t* controller,
                                            rocprofvis_controller_track_t* track,
                                            double start_time,
                                            double end_time,
                                            rocprofvis_controller_future_t* result,
                                            double* output)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_fetch_summary(rocprofvis_controller_t* controller,
                                  rocprofvis_controller_arguments_t* args,
                                  rocprofvis_controller_future_t* result,
                                  rocprofvis_controller_summary_metrics_t* output)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_get_summary_kernel_instance_table(rocprofvis_controller_t* controller,
                                                      rocprofvis_handle_t** table)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_get_summary_memory_instance_table(rocprofvis_controller_t* controller,
                                                      rocprofvis_handle_t** table)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_get_summary_thread_instance_table(rocprofvis_controller_t* controller,
                                                      rocprofvis_handle_t** table)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_get_instrumented_events_table(rocprofvis_controller_t* controller,
                                                  rocprofvis_handle_t** table)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_get_dispatch_events_table(rocprofvis_controller_t* controller,
                                              rocprofvis_handle_t** table)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_get_memory_allocation_events_table(rocprofvis_controller_t* controller,
                                                       rocprofvis_handle_t** table)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_get_memory_copy_events_table(rocprofvis_controller_t* controller,
                                                 rocprofvis_handle_t** table)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_get_sampled_events_table(rocprofvis_controller_t* controller,
                                             rocprofvis_handle_t** table)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_events_table_alloc(uint64_t op, rocprofvis_handle_t** table)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_fetch_table(rocprofvis_controller_t* controller,
                                rocprofvis_controller_table_t* table,
                                rocprofvis_controller_arguments_t* args,
                                rocprofvis_controller_future_t* result,
                                rocprofvis_controller_array_t* output)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_analysis_table_export_csv(rocprofvis_controller_t* controller,
                                     rocprofvis_controller_table_t* table,
                                     rocprofvis_controller_arguments_t* args,
                                     rocprofvis_controller_future_t* result,
                                     char const* path)
{
    return kRocProfVisResultNotSupported;
}

void
rocprofvis_analysis_free_trace_data(rocprofvis_controller_t* controller)
{}

// rocprofvis_controller_script.h

rocprofvis_result_t
rocprofvis_script_execute_async(rocprofvis_controller_t* controller,
                                char const* source,
                                rocprofvis_controller_arguments_t* context,
                                rocprofvis_controller_future_t* future,
                                rocprofvis_controller_script_result_t** result)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_script_cancel(rocprofvis_controller_future_t* future)
{
    return kRocProfVisResultNotSupported;
}

void
rocprofvis_script_result_free(rocprofvis_controller_script_result_t* result)
{}

// rocprofvis_profiler.h

rocprofvis_profiler_config_t*
rocprofvis_profiler_config_alloc(void)
{
    return nullptr;
}

void
rocprofvis_profiler_config_free(rocprofvis_profiler_config_t* config)
{}

rocprofvis_result_t
rocprofvis_profiler_config_set_tool(rocprofvis_profiler_config_t* config,
                                    rocprofvis_profiler_tool_t tool)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_profiler_config_set_tool_directory(rocprofvis_profiler_config_t* config,
                                              char const* tool_directory)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_profiler_config_set_output_directory(rocprofvis_profiler_config_t* config,
                                                char const* output_directory)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_profiler_config_set_working_directory(rocprofvis_profiler_config_t* config,
                                                 char const* working_directory)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_profiler_config_add_env_var(rocprofvis_profiler_config_t* config,
                                       char const* name,
                                       char const* value)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_profiler_config_add_profiler_arg(rocprofvis_profiler_config_t* config,
                                            char const* arg)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_profiler_config_set_connection_local(rocprofvis_profiler_config_t* config)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_profiler_config_set_connection_ssh(rocprofvis_profiler_config_t* config,
                                              char const* host,
                                              char const* user,
                                              int port,
                                              char const* identity_file,
                                              char const* remote_stage_dir)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_profiler_t*
rocprofvis_profiler_alloc(void)
{
    return nullptr;
}

void
rocprofvis_profiler_free(rocprofvis_profiler_t* profiler)
{}

rocprofvis_result_t
rocprofvis_profiler_launch_async(rocprofvis_profiler_t* profiler,
                                 rocprofvis_profiler_config_t* config,
                                 rocprofvis_controller_future_t* future)
{
    return kRocProfVisResultNotSupported;
}
#ifdef ROCPROFVIS_ENABLE_REMOTE

rocprofvis_result_t
rocprofvis_profiler_launch_remote_async(rocprofvis_profiler_t* profiler,
                                        rocprofvis_profiler_config_t* config,
                                        rocprofvis_controller_connection_t* connection,
                                        rocprofvis_controller_future_t* future)
{
    return kRocProfVisResultNotSupported;
}
#endif  // ROCPROFVIS_ENABLE_REMOTE

rocprofvis_result_t
rocprofvis_profiler_get_state(rocprofvis_profiler_t* profiler,
                              rocprofvis_profiler_state_t* state)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_profiler_get_output(rocprofvis_profiler_t* profiler, char* buffer, uint32_t* length)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_profiler_clear_output(rocprofvis_profiler_t* profiler)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_profiler_get_exit_code(rocprofvis_profiler_t* profiler, int32_t* exit_code)
{
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
rocprofvis_profiler_cancel(rocprofvis_profiler_t* profiler)
{
    return kRocProfVisResultNotSupported;
}

}  // extern "C"
