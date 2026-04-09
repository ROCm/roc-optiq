// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_table.h"
#include "rocprofvis_controller_trace.h"
#include "rocprofvis_controller_profiler_process.h"
#include "rocprofvis_core_assert.h"
#include "system/rocprofvis_controller_event.h"
#include "system/rocprofvis_controller_sample.h"
#include "system/rocprofvis_controller_sample_lod.h"
#include "system/rocprofvis_controller_track.h"
#include "system/rocprofvis_controller_timeline.h"
#include "system/rocprofvis_controller_trace_system.h"
#include "system/rocprofvis_controller_graph.h"
#include "system/rocprofvis_controller_summary.h"
#include "system/rocprofvis_controller_summary_metrics.h"
#ifdef COMPUTE_UI_SUPPORT
#include "compute/rocprofvis_controller_metrics_container.h"
#include "compute/rocprofvis_controller_trace_compute.h"
#endif
#include "rocprofvis_c_interface.h"

#include <cstring>

namespace RocProfVis
{
namespace Controller
{
typedef Reference<rocprofvis_controller_t, SystemTrace, kRPVControllerObjectTypeControllerSystem> SystemTraceRef;
typedef Reference<rocprofvis_controller_timeline_t, Timeline, kRPVControllerObjectTypeTimeline> TimelineRef;
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;
typedef Reference<rocprofvis_controller_event_t, Event, kRPVControllerObjectTypeEvent> EventRef;
typedef Reference<rocprofvis_controller_sample_t, Sample, kRPVControllerObjectTypeSample> SampleRef;
typedef Reference<rocprofvis_controller_array_t, Array, kRPVControllerObjectTypeArray> ArrayRef;
typedef Reference<rocprofvis_controller_future_t, Future, kRPVControllerObjectTypeFuture> FutureRef;
typedef Reference<rocprofvis_controller_graph_t, Graph, kRPVControllerObjectTypeGraph> GraphRef;
typedef Reference<rocprofvis_controller_table_t, Table, kRPVControllerObjectTypeTable> TableRef;
typedef Reference<rocprofvis_controller_arguments_t, Arguments, kRPVControllerObjectTypeArguments> ArgumentsRef;
typedef Reference<rocprofvis_controller_table_t, Summary, kRPVControllerObjectTypeSummary> SummaryRef;
typedef Reference<rocprofvis_controller_summary_metrics_t, SummaryMetrics, kRPVControllerObjectTypeSummaryMetrics> SummaryMetricsRef;
#ifdef COMPUTE_UI_SUPPORT
typedef Reference<rocprofvis_controller_t, ComputeTrace, kRPVControllerObjectTypeControllerCompute> ComputeTraceRef;
typedef Reference<rocprofvis_controller_t, MetricsContainer, kRPVControllerObjectTypeMetricsContainer> MetricsContainerRef;
#endif
}
}

extern "C"
{
rocprofvis_result_t rocprofvis_controller_get_uint64(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object && value)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*)object;
        result = handle->GetUInt64(property, index, value);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_get_double(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object && value)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*)object;
        result = handle->GetDouble(property, index, value);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_get_object(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object && value)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*)object;
        result = handle->GetObject(property, index, value);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_get_object_type(rocprofvis_handle_t* object, rocprofvis_controller_object_type_t* type)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(object && type)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*) object;
        *type = handle->GetType();
        result = kRocProfVisResultSuccess;
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_set_uint64(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(object)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*) object;
        result = handle->SetUInt64(property, index, value);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_set_double(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(object)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*) object;
        result = handle->SetDouble(property, index, value);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_set_object(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object && value)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*)object;
        result = handle->SetObject(property, index, value);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_get_string(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object && length)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*)object;
        result = handle->GetString(property, index, value, length);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_set_string(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, char const* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(object && value)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*) object;
        result = handle->SetString(property, index, value);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_save_trimmed_trace(rocprofvis_handle_t* object, double start, double end, char const* path, rocprofvis_controller_future_t* future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(object && path)
    {
        RocProfVis::Controller::SystemTraceRef trace(object);
        RocProfVis::Controller::FutureRef future_ref(future);
        if (trace.IsValid() && future_ref.IsValid())
        {
            result = trace->SaveTrimmedTrace(*future_ref.Get(), start, end, path);
        }
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_cleanup_trace_database(rocprofvis_handle_t* object, bool rebuild, rocprofvis_controller_future_t* future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(object)
    {
        RocProfVis::Controller::SystemTraceRef trace(object);
        RocProfVis::Controller::FutureRef future_ref(future);
        if (trace.IsValid() && future_ref.IsValid())
        {
            result = trace->CleanupTraceDatabase(*future_ref.Get(), rebuild);
        }
    }
    return result;
}
rocprofvis_controller_t* rocprofvis_controller_alloc(char const* const filename)
{
    rocprofvis_controller_t* controller = nullptr;
    if(filename)
    {
        try
        {
            RocProfVis::Controller::Trace* trace = nullptr;
            switch(rocprofvis_db_identify_type(filename))
            {
                case kRocpdSqlite:
                case kRocprofSqlite:
                case kRocprofMultinodeSqlite:
                {
                    trace = new RocProfVis::Controller::SystemTrace(filename);                 
                    break;
                }
#ifdef COMPUTE_UI_SUPPORT
                case kComputeSqlite:
                {
                    trace = new RocProfVis::Controller::ComputeTrace(filename);                  
                    break;
                }
#endif
            }
            if(trace && trace->Init() == kRocProfVisResultSuccess)
            {
                controller = (rocprofvis_controller_t*) trace;
            }
            else
            {
                delete trace;
            }
        }
        catch(const std::exception& e)
        {
            spdlog::error("Failed to allocate controller: {}", e.what());
        }
    }    
    return controller;
}
rocprofvis_result_t rocprofvis_controller_load_async(rocprofvis_controller_t* controller, rocprofvis_controller_future_t* future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;

    RocProfVis::Controller::SystemTraceRef system_trace(controller);
#ifdef COMPUTE_UI_SUPPORT
    RocProfVis::Controller::ComputeTraceRef compute_trace(controller);
#endif
    RocProfVis::Controller::FutureRef future_ref(future);
    if(future_ref.IsValid())
    {
        if(system_trace.IsValid())
        {
            result = system_trace->Load(*future_ref);
        }
#ifdef COMPUTE_UI_SUPPORT
        else if(compute_trace.IsValid())
        {
            result = compute_trace->Load(*future_ref);
        }
#endif
    }

    return result;
}
rocprofvis_controller_future_t* rocprofvis_controller_future_alloc(void)
{
    rocprofvis_controller_future_t* future = (rocprofvis_controller_future_t*)new RocProfVis::Controller::Future();
    return future;
}
rocprofvis_controller_array_t* rocprofvis_controller_array_alloc(uint32_t initial_size)
{
    RocProfVis::Controller::Array* array = new RocProfVis::Controller::Array();
    if (initial_size)
    {
        rocprofvis_result_t result = array->SetUInt64(kRPVControllerArrayNumEntries, 0, initial_size);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    }
    return (rocprofvis_controller_array_t*)array;
}
rocprofvis_controller_arguments_t* rocprofvis_controller_arguments_alloc(void)
{
    rocprofvis_controller_arguments_t* args = (rocprofvis_controller_arguments_t*)new RocProfVis::Controller::Arguments();
    return args;
}
rocprofvis_controller_summary_metrics_t* rocprofvis_controller_summary_metrics_alloc(void)
{
    rocprofvis_controller_summary_metrics_t* summary = (rocprofvis_controller_summary_metrics_t*)new RocProfVis::Controller::SummaryMetrics();
    return summary;
}
rocprofvis_result_t rocprofvis_controller_future_wait(rocprofvis_controller_future_t* object, float timeout)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    RocProfVis::Controller::FutureRef future(object);
    if (future.IsValid())
    {
        result = future->Wait(timeout);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_future_cancel(rocprofvis_controller_future_t* object)
{
    rocprofvis_result_t               result = kRocProfVisResultUnknownError;
    RocProfVis::Controller::FutureRef future(object);
    if(future.IsValid())
    {
        result = future->Cancel();
    }
    return result;
}

rocprofvis_result_t rocprofvis_controller_track_fetch_async(
    rocprofvis_controller_t* controller, rocprofvis_controller_track_t* track,
    double start_time, double end_time, rocprofvis_controller_future_t* result,
    rocprofvis_controller_array_t* output)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    RocProfVis::Controller::TrackRef track_ref(track);
    RocProfVis::Controller::FutureRef future(result);
    RocProfVis::Controller::ArrayRef array(output);
    if (trace.IsValid() && track_ref.IsValid() && future.IsValid() && array.IsValid())
    {
        error = trace->AsyncFetch(*track_ref, *future, *array, start_time, end_time);
    }
    return error;
}
rocprofvis_result_t rocprofvis_controller_graph_fetch_async(
    rocprofvis_controller_t* controller, rocprofvis_controller_graph_t* graph,
    double start_time, double end_time, uint32_t x_resolution,
    rocprofvis_controller_future_t* result, rocprofvis_controller_array_t* output)
{
    rocprofvis_result_t               error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef  trace(controller);
    RocProfVis::Controller::GraphRef  graph_ref(graph);
    RocProfVis::Controller::FutureRef future(result);
    RocProfVis::Controller::ArrayRef  array(output);
    if(trace.IsValid() && graph_ref.IsValid() && future.IsValid() && array.IsValid())
    {
        error = trace->AsyncFetch(*graph_ref, *future, *array, start_time, end_time, x_resolution);
    }
    return error;
}

rocprofvis_result_t rocprofvis_controller_get_indexed_property_async(
    rocprofvis_controller_t* controller, rocprofvis_handle_t* object,
    rocprofvis_property_t property, uint64_t index, uint32_t count,
    rocprofvis_controller_future_t* result, rocprofvis_controller_array_t* output)
{
    rocprofvis_result_t               error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef  trace(controller);
    RocProfVis::Controller::Handle*   handle = (RocProfVis::Controller::Handle*) object;
    RocProfVis::Controller::EventRef  event_ref(object);
    RocProfVis::Controller::FutureRef future(result);
    RocProfVis::Controller::ArrayRef  array(output);
    if(trace.IsValid() && handle && future.IsValid() && array.IsValid())
    {
        switch(handle->GetType())
        {
            case kRPVControllerObjectTypeEvent:
            {
                error = trace->AsyncFetch(*((RocProfVis::Controller::Event*) handle),
                                          *future, *array, property);
                break;
            }
            case kRPVControllerObjectTypeTable:
            {
                error = trace->AsyncFetch(*((RocProfVis::Controller::Table*) handle),
                                          *future, *array, index, count);
                break;
            }
            case kRPVControllerObjectTypeControllerSystem:
            {
                error = trace->AsyncFetch(property, *future, *array, index, count);
                break;
            }
            default:
            {
                break;
            }
        }
    }
    return error;
}

rocprofvis_result_t rocprofvis_controller_table_fetch_async(
    rocprofvis_controller_t* controller, rocprofvis_controller_table_t* table,
    rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result,
    rocprofvis_controller_array_t* output)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    
    // Determine actual controller type first to avoid ambiguous reference validation
    rocprofvis_controller_object_type_t controller_type;
    if(rocprofvis_controller_get_object_type(controller, &controller_type) != kRocProfVisResultSuccess)
    {
        return error;
    }
    
    RocProfVis::Controller::TableRef table_ref(table);
    RocProfVis::Controller::ArgumentsRef args_ref(args);
    RocProfVis::Controller::FutureRef future(result);
    RocProfVis::Controller::ArrayRef array(output);

    if (table_ref.IsValid() && args_ref.IsValid() && future.IsValid() && array.IsValid())
    {
        if (controller_type == kRPVControllerObjectTypeControllerSystem)
        {
            RocProfVis::Controller::SystemTraceRef system_trace(controller);
            if(system_trace.IsValid())
            {
                error = system_trace->AsyncFetch(*table_ref, *args_ref, *future, *array);
            }
        }
#ifdef COMPUTE_UI_SUPPORT
        else if (controller_type == kRPVControllerObjectTypeControllerCompute)
        {
            RocProfVis::Controller::ComputeTraceRef compute_trace(controller);
            if(compute_trace.IsValid())
            {
                error = compute_trace->AsyncFetch(*table_ref, *args_ref, *future, *array);
            }
        }
#endif
    }
    return error;
}

rocprofvis_result_t rocprofvis_controller_table_export_csv(
    rocprofvis_controller_t* controller, rocprofvis_controller_table_t* table,
    rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result,
    char const* path)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef system_trace(controller);
#ifdef COMPUTE_UI_SUPPORT
    RocProfVis::Controller::ComputeTraceRef compute_trace(controller);
#endif
    RocProfVis::Controller::TableRef table_ref(table);
    RocProfVis::Controller::ArgumentsRef args_ref(args);
    RocProfVis::Controller::FutureRef future(result);

    if (table_ref.IsValid() && args_ref.IsValid() && future.IsValid() && path)
    {
        if (system_trace.IsValid())
        {
            error = system_trace->TableExportCSV(*table_ref, *args_ref, *future, path);
        }
#ifdef COMPUTE_UI_SUPPORT
        else if (compute_trace.IsValid())
        {
            // error = compute_trace->TableExportCSV(*table_ref, *args_ref, *future, path);
            error = kRocProfVisResultUnknownError;
        }
#endif
    }
    return error;
}

rocprofvis_result_t rocprofvis_controller_summary_fetch_async(
    rocprofvis_controller_t* controller, rocprofvis_controller_summary_t* summary,
    rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result,
    rocprofvis_controller_summary_metrics_t* output)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    RocProfVis::Controller::SummaryRef summary_ref(summary);
    RocProfVis::Controller::ArgumentsRef args_ref(args);
    RocProfVis::Controller::FutureRef future(result);
    RocProfVis::Controller::SummaryMetricsRef output_ref(output);
    if (trace.IsValid() && summary_ref.IsValid() && args_ref.IsValid() && future.IsValid() &&
        output_ref.IsValid())
    {
        error = trace->AsyncFetch(*summary_ref, *args_ref, *future, *output_ref);
    }
    return error;
}

#ifdef COMPUTE_UI_SUPPORT
rocprofvis_controller_metrics_container_t* rocprofvis_controller_metrics_container_alloc(void)
{
    rocprofvis_controller_metrics_container_t* container = (rocprofvis_controller_metrics_container_t*)new RocProfVis::Controller::MetricsContainer();
    return container;
}

void rocprofvis_controller_metrics_container_free(rocprofvis_controller_metrics_container_t* object)
{
    RocProfVis::Controller::MetricsContainerRef container(object);
    if(container.IsValid())
    {
        delete container.Get();
    }
}

rocprofvis_result_t rocprofvis_controller_metric_fetch_async(
    rocprofvis_controller_t* controller, rocprofvis_controller_arguments_t* args, 
    rocprofvis_controller_future_t* result, rocprofvis_controller_metrics_container_t* output)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::ComputeTraceRef trace(controller);
    RocProfVis::Controller::ArgumentsRef args_ref(args);
    RocProfVis::Controller::FutureRef future(result);
    RocProfVis::Controller::MetricsContainerRef container(output);
    if(trace.IsValid() && args_ref.IsValid() && future.IsValid() && container.IsValid())
    {
        error = trace->AsyncFetch(*args_ref, *future, *container);
    }
    return error;
}
#endif

void rocprofvis_controller_summary_metric_free(rocprofvis_controller_summary_metrics_t* object)
{
    RocProfVis::Controller::SummaryMetricsRef summary_metrics(object);
    if(summary_metrics.IsValid())
    {
        delete summary_metrics.Get();
    }
}

void rocprofvis_controller_arguments_free(rocprofvis_controller_arguments_t* args)
{
    RocProfVis::Controller::ArgumentsRef arguments(args);
    if(arguments.IsValid())
    {
        delete arguments.Get();
    }
}

void rocprofvis_controller_array_free(rocprofvis_controller_array_t* object)
{
    RocProfVis::Controller::ArrayRef array(object);
    if (array.IsValid())
    {
        if(array.Get()->GetContext() &&
           ((RocProfVis::Controller::SystemTrace*) array.Get()->GetContext())->GetMemoryManager())
        {
            ((RocProfVis::Controller::SystemTrace*)array.Get()->GetContext())->GetMemoryManager()->CancelArrayOwnership(&array.Get()->GetVector(),
                                      RocProfVis::Controller::kRocProfVisOwnerTypeGraph);
        }
        delete array.Get();
    }
}
void rocprofvis_controller_future_free(rocprofvis_controller_future_t* object)
{
    RocProfVis::Controller::FutureRef future(object);
    if (future.IsValid())
    {
        delete future.Get();
    }
}
void rocprofvis_controller_free(rocprofvis_controller_t* object)
{
    RocProfVis::Controller::SystemTraceRef trace(object);
    if (trace.IsValid())
    {
        delete trace.Get();
    }
}

// ==================================================================================
// Profiler Launcher C API Implementation
// ==================================================================================

rocprofvis_profiler_config_t* rocprofvis_profiler_config_alloc(void)
{
    RocProfVis::Controller::ProfilerConfig* config = new RocProfVis::Controller::ProfilerConfig();
    return (rocprofvis_profiler_config_t*)config;
}

void rocprofvis_profiler_config_free(rocprofvis_profiler_config_t* config)
{
    if (config)
    {
        RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
        delete cfg;
    }
}

rocprofvis_result_t rocprofvis_profiler_config_set_type(rocprofvis_profiler_config_t* config, rocprofvis_profiler_type_t profiler_type)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetProfilerType(profiler_type);
}

rocprofvis_result_t rocprofvis_profiler_config_set_profiler_path(rocprofvis_profiler_config_t* config, char const* profiler_path)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetProfilerPath(profiler_path);
}

rocprofvis_result_t rocprofvis_profiler_config_set_target_executable(rocprofvis_profiler_config_t* config, char const* target_executable)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetTargetExecutable(target_executable);
}

rocprofvis_result_t rocprofvis_profiler_config_set_target_args(rocprofvis_profiler_config_t* config, char const* target_args)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetTargetArgs(target_args);
}

rocprofvis_result_t rocprofvis_profiler_config_set_profiler_args(rocprofvis_profiler_config_t* config, char const* profiler_args)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetProfilerArgs(profiler_args);
}

rocprofvis_result_t rocprofvis_profiler_config_set_output_directory(rocprofvis_profiler_config_t* config, char const* output_directory)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetOutputDirectory(output_directory);
}

rocprofvis_result_t rocprofvis_profiler_launch_async(rocprofvis_profiler_config_t* config, rocprofvis_controller_future_t* future)
{
    if (config == nullptr || future == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;

    RocProfVis::Controller::ProfilerProcessController* controller =
        new RocProfVis::Controller::ProfilerProcessController();

    rocprofvis_result_t result = controller->LaunchAsync(cfg);
    if (result != kRocProfVisResultSuccess)
    {
        delete controller;
        return result;
    }

    RocProfVis::Controller::FutureRef future_ref(future);
    if (future_ref.IsValid())
    {
        future_ref->SetUserData(controller, [](void* ptr)
        {
            delete static_cast<RocProfVis::Controller::ProfilerProcessController*>(ptr);
        });

        RocProfVis::Controller::Job* job = RocProfVis::Controller::JobSystem::Get().IssueJob(
            [controller](RocProfVis::Controller::Future* future) -> rocprofvis_result_t
            {
                RocProfVis::Controller::ProfilerProcessController::ExecuteJob(controller, future);
                return kRocProfVisResultSuccess;
            },
            future_ref.Get());
        future_ref->Set(job);
    }

    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_get_state(rocprofvis_controller_future_t* future, rocprofvis_profiler_state_t* state)
{
    if (future == nullptr || state == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::Future* fut = (RocProfVis::Controller::Future*)future;
    auto* controller = static_cast<RocProfVis::Controller::ProfilerProcessController*>(fut->GetUserData());
    if (controller == nullptr)
    {
        *state = kRPVProfilerStateIdle;
        return kRocProfVisResultSuccess;
    }

    *state = controller->GetState();
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_get_output(rocprofvis_controller_future_t* future, char* buffer, uint32_t* length)
{
    if (future == nullptr || length == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::Future* fut = (RocProfVis::Controller::Future*)future;
    auto* controller = static_cast<RocProfVis::Controller::ProfilerProcessController*>(fut->GetUserData());
    if (controller == nullptr)
    {
        *length = 0;
        return kRocProfVisResultSuccess;
    }

    std::string output = controller->GetOutput();

    if (buffer == nullptr || *length == 0)
    {
        *length = static_cast<uint32_t>(output.size());
        return kRocProfVisResultSuccess;
    }

    uint32_t copy_len = std::min(*length, static_cast<uint32_t>(output.size() + 1));
    std::strncpy(buffer, output.c_str(), copy_len);
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_get_trace_path(rocprofvis_controller_future_t* future, char* buffer, uint32_t* length)
{
    if (future == nullptr || length == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::Future* fut = (RocProfVis::Controller::Future*)future;
    auto* controller = static_cast<RocProfVis::Controller::ProfilerProcessController*>(fut->GetUserData());
    if (controller == nullptr)
    {
        *length = 0;
        return kRocProfVisResultSuccess;
    }

    std::string trace_path = controller->GetTracePath();

    if (buffer == nullptr || *length == 0)
    {
        *length = static_cast<uint32_t>(trace_path.size());
        return kRocProfVisResultSuccess;
    }

    uint32_t copy_len = std::min(*length, static_cast<uint32_t>(trace_path.size() + 1));
    std::strncpy(buffer, trace_path.c_str(), copy_len);
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_get_exit_code(rocprofvis_controller_future_t* future, int32_t* exit_code)
{
    if (future == nullptr || exit_code == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::Future* fut = (RocProfVis::Controller::Future*)future;
    auto* controller = static_cast<RocProfVis::Controller::ProfilerProcessController*>(fut->GetUserData());
    if (controller == nullptr)
    {
        *exit_code = -1;
        return kRocProfVisResultSuccess;
    }

    *exit_code = controller->GetExitCode();
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_cancel(rocprofvis_controller_future_t* future)
{
    if (future == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::Future* fut = (RocProfVis::Controller::Future*)future;

    fut->Cancel();

    auto* controller = static_cast<RocProfVis::Controller::ProfilerProcessController*>(fut->GetUserData());
    if (controller)
    {
        controller->Cancel();
    }

    return kRocProfVisResultSuccess;
}
}
