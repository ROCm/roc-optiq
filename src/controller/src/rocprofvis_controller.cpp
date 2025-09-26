// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_sample_lod.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_timeline.h"
#include "rocprofvis_controller_trace.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_graph.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_table.h"
#include "rocprofvis_controller_plot.h"
#include "rocprofvis_controller_json_trace.h"
#include "rocprofvis_core_assert.h"

#include <cstring>

namespace RocProfVis
{
namespace Controller
{
typedef Reference<rocprofvis_controller_t, Trace, kRPVControllerObjectTypeController> TraceRef;
typedef Reference<rocprofvis_controller_timeline_t, Timeline, kRPVControllerObjectTypeTimeline> TimelineRef;
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;
typedef Reference<rocprofvis_controller_event_t, Event, kRPVControllerObjectTypeEvent> EventRef;
typedef Reference<rocprofvis_controller_sample_t, Sample, kRPVControllerObjectTypeSample> SampleRef;
typedef Reference<rocprofvis_controller_array_t, Array, kRPVControllerObjectTypeArray> ArrayRef;
typedef Reference<rocprofvis_controller_future_t, Future, kRPVControllerObjectTypeFuture> FutureRef;
typedef Reference<rocprofvis_controller_graph_t, Graph, kRPVControllerObjectTypeGraph> GraphRef;
typedef Reference<rocprofvis_controller_table_t, Table, kRPVControllerObjectTypeTable> TableRef;
typedef Reference<rocprofvis_controller_arguments_t, Arguments, kRPVControllerObjectTypeArguments> ArgumentsRef;
#ifdef COMPUTE_UI_SUPPORT
typedef Reference<rocprofvis_controller_plot_t, Plot, kRPVControllerObjectTypePlot> PlotRef;
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
rocprofvis_result_t rocprofvis_controller_set_string(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(object && value)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*) object;
        result = handle->SetString(property, index, value, length);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_save_trimmed_trace(rocprofvis_handle_t* object, double start, double end, char const* path, rocprofvis_controller_future_t* future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(object && path)
    {
        RocProfVis::Controller::TraceRef trace(object);
        RocProfVis::Controller::FutureRef future_ref(future);
        if (trace.IsValid() && future_ref.IsValid())
        {
            result = trace->SaveTrimmedTrace(*future_ref.Get(), start, end, path);
        }
    }
    return result;
}
rocprofvis_controller_t* rocprofvis_controller_alloc()
{
    rocprofvis_controller_t* controller = nullptr;
    try
    {
        RocProfVis::Controller::Trace* trace = new RocProfVis::Controller::Trace();
        if(trace->Init() == kRocProfVisResultSuccess)
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
        spdlog::error("Failed to allocate controller");
    }
    return controller;
}


rocprofvis_result_t rocprofvis_controller_get_event_density_histogram(
    rocprofvis_controller_t* controller, double start_time, double end_time,
    size_t num_bins, uint64_t* out_bins)
{
    if(!controller || !out_bins || num_bins == 0) return kRocProfVisResultInvalidArgument;

    RocProfVis::Controller::TraceRef trace(controller);
    if(!trace.IsValid()) return kRocProfVisResultInvalidArgument;

    auto bins = trace->BuildEventDensityHistogram(start_time, end_time, num_bins);
    std::copy(bins.begin(), bins.end(), out_bins);
    return kRocProfVisResultSuccess;
}


rocprofvis_result_t rocprofvis_controller_load_async(rocprofvis_controller_t* controller, char const* const filename, rocprofvis_controller_future_t* future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;

    RocProfVis::Controller::TraceRef trace(controller);
    RocProfVis::Controller::FutureRef future_ref(future);
    if(trace.IsValid() && future_ref.IsValid() && filename && strlen(filename))
    {
        result = trace->Load(filename, *future_ref);
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
    RocProfVis::Controller::TraceRef trace(controller);
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
    RocProfVis::Controller::TraceRef  trace(controller);
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
    RocProfVis::Controller::TraceRef  trace(controller);
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
            case kRPVControllerObjectTypeController:
            {
                error = trace->AsyncFetch(property, *future, *array, index, count);
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
    RocProfVis::Controller::TraceRef trace(controller);
    RocProfVis::Controller::TableRef table_ref(table);
    RocProfVis::Controller::ArgumentsRef args_ref(args);
    RocProfVis::Controller::FutureRef future(result);
    RocProfVis::Controller::ArrayRef array(output);
    if (trace.IsValid() && table_ref.IsValid() && args_ref.IsValid() && future.IsValid() &&
        array.IsValid())
    {
        error = trace->AsyncFetch(*table_ref, *args_ref, *future, *array);
    }
    return error;
}

#ifdef COMPUTE_UI_SUPPORT
rocprofvis_result_t rocprofvis_controller_plot_fetch_async(
    rocprofvis_controller_t* controller, rocprofvis_controller_plot_t* plot,
    rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result,
    rocprofvis_controller_array_t* output)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::TraceRef trace(controller);
    RocProfVis::Controller::PlotRef plot_ref(plot);
    RocProfVis::Controller::ArgumentsRef args_ref(args);
    RocProfVis::Controller::FutureRef future(result);
    RocProfVis::Controller::ArrayRef array(output);
    if (trace.IsValid() && plot_ref.IsValid() && args_ref.IsValid() && future.IsValid() &&
        array.IsValid())
    {
        error = trace->AsyncFetch(*plot_ref, *args_ref, *future, *array);
    }
    return error;
}
#endif

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
           ((RocProfVis::Controller::Trace*) array.Get()->GetContext())->GetMemoryManager())
        {
            ((RocProfVis::Controller::Trace*)array.Get()->GetContext())->GetMemoryManager()->CancelArrayOwnership(&array.Get()->GetVector(),
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
    RocProfVis::Controller::TraceRef trace(object);
    if (trace.IsValid())
    {
        delete trace.Get();
    }
}
}