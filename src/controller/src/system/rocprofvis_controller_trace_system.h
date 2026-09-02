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
#include <array>
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
class EventSearchTable;
class SystemTable;
class Summary;
class SummaryMetrics;
class TopologyNode;

class SystemTrace : public Trace
{
public:
    SystemTrace(const std::string& filename, const std::string& config_path);

    // Compare/combine: open several files as one trace. Each file becomes a source
    // instance whose index is exposed via kRPVControllerTrackInstanceId.
    SystemTrace(const std::vector<std::string>& filenames);

    virtual ~SystemTrace();

    virtual rocprofvis_result_t Init() override;

    rocprofvis_result_t Load(Future& future);

    // Incremental add: append one more trace file to this already-loaded trace, reading
    // only that file's metadata and appending its tracks. Existing tracks and their cached
    // segment data are left intact (no reprocessing / no refetch of the existing files).
    rocprofvis_result_t AddTraceSource(Future& future, const std::string& filepath);

    // In-place remove: drop one already-merged trace file from this trace, freeing its data
    // without reloading the others. Surviving tracks and their cached data are left intact.
    rocprofvis_result_t RemoveTraceSource(Future& future, const std::string& filepath);

    rocprofvis_result_t SaveTrimmedTrace(Future& future, double start, double end, char const* path);

    rocprofvis_result_t CleanupTraceDatabase(Future& future, bool rebuild);

    rocprofvis_result_t AsyncFetch(Track& track, Future& future, Array& array,
                                   double start, double end);

    rocprofvis_result_t AsyncFetch(Graph& graph, Future& future, Array& array,
                                   double start, double end, uint32_t pixels);

    rocprofvis_result_t AsyncFetch(Event& event, Future& future, Array& array,
                                   rocprofvis_property_t property);

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

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final;

    MemoryManager* GetMemoryManager();

    std::mutex& GetTableMutex(rocprofvis_dm_table_use_case_enum_t use_case);

private:
    std::vector<std::string>                       m_files;  // >1 entry => combined/compare load
    std::vector<Track*>                            m_tracks;
    Timeline*                                      m_timeline;
    SystemTable*                                   m_event_table;
    SystemTable*                                   m_sample_table;
    EventSearchTable*                              m_search_table;
    Summary*                                       m_summary;
    MemoryManager*                                 m_mem_mgmt;
    TopologyNode*                                  m_topology_root;
    std::array<std::mutex, kRPVDMTableNumUsecases> m_table_mutex;
    std::string                                    m_config_path;

private:
    rocprofvis_result_t LoadRocpd(Future* future);

    // Build controller Track/Graph objects for data-model tracks in the half-open index
    // range [start_index, num_tracks) and append them to m_tracks / m_timeline. Used by the
    // incremental AddTraceSource path.
    rocprofvis_result_t BuildTracksFromDataModel(uint64_t start_index, uint64_t num_tracks,
                                                 rocprofvis_dm_timestamp_t end_time,
                                                 uint64_t& graph_index, size_t& trace_size);

    // Re-pin every sample/counter track's max timestamp to the current data-model end. Their end
    // is the trace end (not their last reading), so an add/remove must re-pin it or the timeline
    // window won't follow the new extent.
    void ClampSampleTracksToTraceEnd();

    void DbgPrintTopologyNodeData(rocprofvis_dm_topology_node node, int level);

};

}  // namespace Controller
}  // namespace RocProfVis