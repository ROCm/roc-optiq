// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_c_interface.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_job_system.h"
#include "rocprofvis_controller_mem_mgmt.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_trace.h"
#include <string>
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
class Node;
class SystemTable;
class Summary;
class SummaryMetrics;

class SystemTrace : public Trace
{
public:
    SystemTrace(const std::string& filename);

    virtual ~SystemTrace();

    virtual rocprofvis_result_t Init() override;

    rocprofvis_result_t Load(Future& future);

    rocprofvis_result_t SaveTrimmedTrace(Future& future, double start, double end, char const* path);

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

    rocprofvis_result_t TableExportCSV(Table& table, Arguments& args, Future& future, 
                                       const char* path);

    rocprofvis_result_t AsyncFetch(Summary& summary, Arguments& args, Future& future,
                                   SummaryMetrics& output);

    rocprofvis_result_t AsyncFetch(rocprofvis_property_t property, Future& future,
                                          Array& array, uint64_t index, uint64_t count);

    rocprofvis_controller_object_type_t GetType(void) final;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final;

    MemoryManager* GetMemoryManager();

private:
    std::vector<Track*>   m_tracks;
    std::vector<Node*>    m_nodes;
    Timeline*             m_timeline;
    SystemTable*          m_event_table;
    SystemTable*          m_sample_table;
    SystemTable*          m_search_table;
    Summary*              m_summary;
    MemoryManager*        m_mem_mgmt;

private:
    rocprofvis_result_t LoadRocpd();

};

}  // namespace Controller
}  // namespace RocProfVis