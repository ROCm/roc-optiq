// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_c_interface.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_job_system.h"
#include "rocprofvis_c_interface.h"
#include "rocprofvis_controller_mem_mgmt.h"
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class Array;
class Future;
class Track;
class Graph;
class Timeline;
class Event;
class Table;
class SystemTable;
#ifdef COMPUTE_UI_SUPPORT
class Plot;
class ComputeTrace;
#endif

class Trace : public Handle
{
public:
    Trace();

    virtual ~Trace();

    rocprofvis_result_t Load(char const* const filename, Future& future);

    rocprofvis_result_t AsyncFetch(Track& track, Future& future, Array& array,
                                   double start, double end);

    rocprofvis_result_t AsyncFetch(Graph& graph, Future& future, Array& array,
                                   double start, double end, uint32_t pixels);

    rocprofvis_result_t AsyncFetch(Event& event, Future& future, Array& array,
                                   rocprofvis_property_t property);

    rocprofvis_result_t AsyncFetch(Table& table, Future& future, Array& array,
                                   uint64_t index, uint64_t count);

    rocprofvis_result_t AsyncFetch(Table& table, Arguments& args, Future& future,
                                   Array& array);
#ifdef COMPUTE_UI_SUPPORT
    rocprofvis_result_t AsyncFetch(Plot& plot, Arguments& args, Future& future,
                                   Array& array);
#endif

    rocprofvis_result_t AsyncFetch(rocprofvis_property_t property, Future& future,
                                          Array& array, uint64_t index, uint64_t count);

    rocprofvis_controller_object_type_t GetType(void) final;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final;

    MemoryManager* GetMemoryManager();

private:
    std::vector<Track*>   m_tracks;
    uint64_t              m_id;
    Timeline*             m_timeline;
    SystemTable*          m_event_table;
    SystemTable*          m_sample_table;
    rocprofvis_dm_trace_t m_dm_handle;
    MemoryManager*        m_mem_mgmt;
#ifdef COMPUTE_UI_SUPPORT
    ComputeTrace* m_compute_trace;
#endif

private:
#ifdef JSON_SUPPORT
    rocprofvis_result_t LoadJson(char const* const filename);
#endif
    rocprofvis_result_t LoadRocpd(char const* const filename);
};

}  // namespace Controller
}  // namespace RocProfVis
