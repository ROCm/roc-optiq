// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED

#include "rocprofvis_db_profiler_hub.h"
#include "rocprofvis_db_profiler_hub_ext.h"
#include "rocprofvis_db_query_builder.h"
#include "rocprofvis_db_table_processor.h"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
// #include "json.h"

namespace RocProfVis
{
namespace DataModel
{

struct hub_table_cell
{
    bool        text;
    bool        real;
    int64_t     number;
    double      real_value;
    const char* text_value;
};

struct hub_table_row
{
    std::vector<hub_table_cell> cells;
};

static const hub_table_cell*
hub_cell(void* handle, int index)
{
    hub_table_row* row = (hub_table_row*) handle;
    if(row == nullptr || index < 0 || (size_t) index >= row->cells.size())
    {
        return nullptr;
    }
    return &row->cells[(size_t) index];
}

std::string
ProfilerHub::TableColumnText(void* func, void* handle, char** azColName, int index)
{
    (void) func;
    (void) azColName;
    const hub_table_cell* cell = hub_cell(handle, index);
    if(cell == nullptr) return "";
    if(cell->text) return cell->text_value != nullptr ? cell->text_value : "";
    return std::to_string(cell->number);
}
int
ProfilerHub::TableColumnInt(void* func, void* handle, char** azColName, int index)
{
    (void) func;
    (void) azColName;
    const hub_table_cell* cell = hub_cell(handle, index);
    if(cell == nullptr || cell->text) return 0;
    return (int) cell->number;
}
int64_t
ProfilerHub::TableColumnInt64(void* func, void* handle, char** azColName, int index)
{
    (void) func;
    (void) azColName;
    const hub_table_cell* cell = hub_cell(handle, index);
    if(cell == nullptr || cell->text) return 0;
    return cell->number;
}
double
ProfilerHub::TableColumnDouble(void* func, void* handle, char** azColName, int index)
{
    (void) func;
    (void) azColName;
    const hub_table_cell* cell = hub_cell(handle, index);
    if(cell == nullptr || cell->text) return 0;
    if(cell->real) return cell->real_value;
    return (double) cell->number;
}

rocprofvis_db_type_t
ProfilerHub::Detect(rocprofvis_db_filename_t filename)
{
    ph_file_probe_t probe_result;
    if(PH_RESULT_SUCCESS == ph_probe_file(filename, &probe_result)) return kProfilerHub;
    return kAutodetect;
}

rocprofvis_dm_result_t
ProfilerHub::Open()
{
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t
ProfilerHub::Close()
{
    ph_result_t result = ph_ctx_free(m_ph_trace);
    return result == PH_RESULT_SUCCESS ? kRocProfVisDmResultSuccess
                                       : kRocProfVisDmResultUnknownError;
}

bool
ProfilerHub::RegionTracks(rocprofvis_dm_track_category_t category)
{
    return category == kRocProfVisDmRegionMainTrack ||
           category == kRocProfVisDmRegionSampleTrack;
}

bool
ProfilerHub::GpuTracks(rocprofvis_dm_track_category_t category)
{
    return category == kRocProfVisDmKernelDispatchTrack ||
           category == kRocProfVisDmMemoryAllocationTrack ||
           category == kRocProfVisDmMemoryCopyTrack ||
           category == kRocProfVisDmHipEventTrack;
}

bool
ProfilerHub::GpuAndPMCTracks(rocprofvis_dm_track_category_t category)
{
    return GpuTracks(category) || category == kRocProfVisDmPmcTrack;
}

void
ProfilerHub::IdentificatorNamesUpdate(rocprofvis_dm_track_params_t& track_params)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(track_params.track_indentifiers.db_instance != nullptr,
                                 ERROR_NODE_KEY_CANNOT_BE_NULL, );
    DbInstance* db_instance = (DbInstance*) track_params.track_indentifiers.db_instance;
    if(RegionTracks(track_params.track_indentifiers.category))
    {
        if(track_params.track_indentifiers.name[TRACK_ID_PID].empty())
        {
            track_params.track_indentifiers.name[TRACK_ID_PID] =
                CachedTables(db_instance->GuidIndex())
                    ->GetTableCell("Process",
                                   track_params.track_indentifiers.id[TRACK_ID_PID],
                                   "command");
            track_params.track_indentifiers.name[TRACK_ID_PID] += "(";
            track_params.track_indentifiers.name[TRACK_ID_PID] +=
                std::to_string(track_params.track_indentifiers.id[TRACK_ID_PID]);
            track_params.track_indentifiers.name[TRACK_ID_PID] += ")";
        }

        if(track_params.track_indentifiers.name[TRACK_ID_TID].empty())
        {
            track_params.track_indentifiers.name[TRACK_ID_TID] =
                CachedTables(db_instance->GuidIndex())
                    ->GetTableCell("Thread",
                                   track_params.track_indentifiers.id[TRACK_ID_TID],
                                   "name");
            track_params.track_indentifiers.name[TRACK_ID_TID] += "(";
            track_params.track_indentifiers.name[TRACK_ID_TID] +=
                std::to_string(track_params.track_indentifiers.id[TRACK_ID_TID]);
            track_params.track_indentifiers.name[TRACK_ID_TID] += ")";
        }
    }
    else if(GpuAndPMCTracks(track_params.track_indentifiers.category))
    {
        if(track_params.track_indentifiers.name[TRACK_ID_AGENT].empty())
        {
            track_params.track_indentifiers.name[TRACK_ID_AGENT] =
                CachedTables(db_instance->GuidIndex())
                    ->GetTableCell("Agent",
                                   track_params.track_indentifiers.id[TRACK_ID_AGENT],
                                   "product_name");
            track_params.track_indentifiers.name[TRACK_ID_AGENT] += "(";
            track_params.track_indentifiers.name[TRACK_ID_AGENT] +=
                CachedTables(db_instance->GuidIndex())
                    ->GetTableCell("Agent",
                                   track_params.track_indentifiers.id[TRACK_ID_AGENT],
                                   "type_index");
            track_params.track_indentifiers.name[TRACK_ID_AGENT] += ")";
        }
        if(GpuTracks(track_params.track_indentifiers.category))
        {
            if(track_params.track_indentifiers.name[TRACK_ID_QUEUE].empty())
            {
                track_params.track_indentifiers.name[TRACK_ID_QUEUE] =
                    CachedTables(db_instance->GuidIndex())
                        ->GetTableCell("Queue",
                                       track_params.track_indentifiers.id[TRACK_ID_QUEUE],
                                       "name");
            }
        }
        else
        {
            if(track_params.track_indentifiers.name[TRACK_ID_COUNTER].empty())
            {
                track_params.track_indentifiers.name[TRACK_ID_COUNTER] =
                    CachedTables(db_instance->GuidIndex())
                        ->GetTableCell("PMC",
                                       track_params.track_indentifiers.id[TRACK_ID_QUEUE],
                                       "symbol");
            }
        }
    }
    else if(track_params.track_indentifiers.category == kRocProfVisDmStreamTrack)
    {
        if(track_params.track_indentifiers.name[TRACK_ID_STREAM].empty())
        {
            track_params.track_indentifiers.name[TRACK_ID_STREAM] =
                CachedTables(db_instance->GuidIndex())
                    ->GetTableCell("Stream",
                                   track_params.track_indentifiers.id[TRACK_ID_STREAM],
                                   "name");
            track_params.track_indentifiers.name[TRACK_ID_PID] =
                track_params.track_indentifiers.name[TRACK_ID_STREAM];
        }
    }
}

rocprofvis_dm_result_t
ProfilerHub::ProcessTrack(rocprofvis_dm_track_params_t& track_params)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(track_params.track_indentifiers.db_instance != nullptr,
                                 ERROR_NODE_KEY_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    DbInstance* db_instance = (DbInstance*) track_params.track_indentifiers.db_instance;
    track_params.track_indentifiers.source_type = kRPVSystemSourceRocprof;
    rocprofvis_dm_track_params_it it            = TrackTracker()->FindTrackParamsIterator(
        track_params.track_indentifiers, db_instance->GuidIndex());
    if(it == TrackPropertiesEnd())
    {
        TrackTracker()->AddTrack(track_params.track_indentifiers,
                                 db_instance->GuidIndex(),
                                 track_params.track_indentifiers.track_id);

        IdentificatorNamesUpdate(track_params);

        if(kRocProfVisDmResultSuccess != AddTrackProperties(track_params))
            return kRocProfVisDmResultUnknownError;

        if(BindObject()->FuncAddTrack(BindObject()->trace_object,
                                      TrackPropertiesLast()) !=
           kRocProfVisDmResultSuccess)
            return kRocProfVisDmResultUnknownError;

        if(BindObject()->FuncAddTopologyNode(BindObject()->trace_object,
                                             &track_params.track_indentifiers) !=
           kRocProfVisDmResultSuccess)
            return kRocProfVisDmResultUnknownError;

        if(CachedTables(db_instance->GuidIndex())
               ->PopulateTrackExtendedDataTemplate(
                   this, db_instance->GuidIndex(), "Node",
                   track_params.track_indentifiers.id[TRACK_ID_NODE]) !=
           kRocProfVisDmResultSuccess)
            return kRocProfVisDmResultUnknownError;
        if(RegionTracks(track_params.track_indentifiers.category))
        {
            if(CachedTables(db_instance->GuidIndex())
                   ->PopulateTrackExtendedDataTemplate(
                       this, db_instance->GuidIndex(), "Process",
                       track_params.track_indentifiers.id[TRACK_ID_PID]) !=
               kRocProfVisDmResultSuccess)
                return kRocProfVisDmResultUnknownError;
            if(CachedTables(db_instance->GuidIndex())
                   ->PopulateTrackExtendedDataTemplate(
                       this, db_instance->GuidIndex(), "Thread",
                       track_params.track_indentifiers.id[TRACK_ID_TID]) !=
               kRocProfVisDmResultSuccess)
                return kRocProfVisDmResultUnknownError;
        }
        else if(track_params.track_indentifiers.category == kRocProfVisDmStreamTrack)
        {
            if(CachedTables(db_instance->GuidIndex())
                   ->PopulateTrackExtendedDataTemplate(
                       this, db_instance->GuidIndex(), "Process",
                       track_params.track_indentifiers.id[TRACK_ID_PID]) !=
               kRocProfVisDmResultSuccess)
                return kRocProfVisDmResultUnknownError;
            if(CachedTables(db_instance->GuidIndex())
                   ->PopulateTrackExtendedDataTemplate(
                       this, db_instance->GuidIndex(), "Stream",
                       track_params.track_indentifiers.id[TRACK_ID_STREAM]) !=
               kRocProfVisDmResultSuccess)
                return kRocProfVisDmResultUnknownError;
        }
        else if(GpuAndPMCTracks(track_params.track_indentifiers.category))
        {
            if(CachedTables(db_instance->GuidIndex())
                   ->PopulateTrackExtendedDataTemplate(
                       this, db_instance->GuidIndex(), "Agent",
                       track_params.track_indentifiers.id[TRACK_ID_AGENT]) !=
               kRocProfVisDmResultSuccess)
                return kRocProfVisDmResultUnknownError;
            if(GpuTracks(track_params.track_indentifiers.category))
            {
                if(CachedTables(db_instance->GuidIndex())
                       ->PopulateTrackExtendedDataTemplate(
                           this, db_instance->GuidIndex(), "Queue",
                           track_params.track_indentifiers.id[TRACK_ID_QUEUE]) !=
                   kRocProfVisDmResultSuccess)
                    return kRocProfVisDmResultUnknownError;
            }
            else
            {
                if(track_params.track_indentifiers.id[TRACK_ID_QUEUE] > 0)
                    if(CachedTables(db_instance->GuidIndex())
                           ->PopulateTrackExtendedDataTemplate(
                               this, db_instance->GuidIndex(), "PMC",
                               track_params.track_indentifiers.id[TRACK_ID_COUNTER]) !=
                       kRocProfVisDmResultSuccess)
                        return kRocProfVisDmResultUnknownError;
            }
        }
        return kRocProfVisDmResultSuccess;
    }

    return kRocProfVisDmResultNotLoaded;
}

static constexpr uint32_t kHubCatalogIndex = 0;

static std::string
catalog_text(const InformationCatalog& catalog, const char* table, uint64_t row_id,
             const char* column)
{
    return catalog.CellText(table, row_id, column);
}

static std::string
catalog_text_with_id(const InformationCatalog& catalog, const char* table,
                     uint64_t row_id, const char* column, uint64_t id)
{
    return catalog_text(catalog, table, row_id, column) + "(" + std::to_string(row_id) + ")";
}

static std::string
catalog_agent_name(const InformationCatalog& catalog, uint64_t agent_id)
{
    return catalog_text(catalog, "Agent", agent_id, "product_name") + "(" +
           catalog_text(catalog, "Agent", agent_id, "type_index") + ")";
}

rocprofvis_dm_result_t
ProfilerHub::AddTrack(ph_track_t& track, DbInstance* instance)
{
    rocprofvis_dm_track_params_t track_params;
    const InformationCatalog&    catalog = CachedTables(kHubCatalogIndex)->Catalog();
    rocprofvis_dm_string_t*      names   = track_params.track_indentifiers.name;
    track_params.track_indentifiers.db_instance               = instance;
    uint32_t db_instance                                      = instance->GuidIndex();
    track_params.track_indentifiers.is_numeric[TRACK_ID_NODE] = true;
    track_params.track_indentifiers.is_numeric[TRACK_ID_PID_OR_AGENT] = true;
    track_params.track_indentifiers.is_numeric[TRACK_ID_TID_OR_QUEUE] = true;

    switch(track.category)
    {
        case PH_TRACK_CATEGORY_THREAD:
            track_params.op                          = kRocProfVisDmOperationLaunch;
            track_params.track_indentifiers.category = kRocProfVisDmRegionMainTrack;
            track_params.track_indentifiers.id[TRACK_ID_PID] = track.process_row_id;
            track_params.track_indentifiers.id[TRACK_ID_TID] = track.thread_row_id;
            names[TRACK_ID_PID]                             = catalog_text_with_id(
                catalog, "Process", track.process_row_id, "command", track.pid);
            names[TRACK_ID_TID] = catalog_text_with_id(
                catalog, "Thread", track.thread_row_id, "name", track.tid);
            break;
        case PH_TRACK_CATEGORY_THREAD_SAMPLE:
            track_params.op = kRocProfVisDmOperationLaunchSample;
            track_params.track_indentifiers.id[TRACK_ID_PID] = track.process_row_id;
            track_params.track_indentifiers.id[TRACK_ID_TID] = track.thread_row_id;
            track_params.track_indentifiers.category = kRocProfVisDmRegionSampleTrack;
            names[TRACK_ID_PID]                     = catalog_text_with_id(
                catalog, "Process", track.process_row_id, "command", track.pid);
            names[TRACK_ID_TID] = track.track_name;
            // catalog_text_with_id(catalog, "Thread", track.tid, "name", track.tid);
            break;
        case PH_TRACK_CATEGORY_PMC_AGENT:
        case PH_TRACK_CATEGORY_PMC_KERNEL:
        case PH_TRACK_CATEGORY_PMC_MEMORY:
        {
            track_params.op                          = kRocProfVisDmOperationNoOp;
            track_params.track_indentifiers.category = kRocProfVisDmPmcTrack;
            track_params.track_indentifiers.id[TRACK_ID_AGENT] = track.agent_id;
            uint64_t counter_id = 0;
            if(track.category == PH_TRACK_CATEGORY_PMC_MEMORY)
            {
                if(track.pmc_id == 0) return kRocProfVisDmResultUnknownError;
                counter_id = track.pmc_id;
            }
            // Kernel and sample counters resolve through the PMC catalog.
            else if(!catalog.FindRowId("PMC", "name", track.track_name, counter_id))
            {
                if(track.pmc_id == 0) return kRocProfVisDmResultUnknownError;
                counter_id = track.pmc_id;
            }
            track_params.track_indentifiers.id[TRACK_ID_COUNTER] = counter_id;
            names[TRACK_ID_AGENT]   = catalog_agent_name(catalog, track.agent_id);
            names[TRACK_ID_COUNTER] = track.track_name;
            // catalog_text(catalog, "PMC", track.queue_id, "symbol");
            break;
        }
        case PH_TRACK_CATEGORY_KERNEL_DISPATCH_AGENT_QUEUE:
            track_params.op                          = kRocProfVisDmOperationDispatch;
            track_params.track_indentifiers.category = kRocProfVisDmKernelDispatchTrack;
            track_params.track_indentifiers.id[TRACK_ID_AGENT] = track.agent_id;
            track_params.track_indentifiers.id[TRACK_ID_QUEUE] = track.queue_id;
            names[TRACK_ID_AGENT] = catalog_agent_name(catalog, track.agent_id);
            names[TRACK_ID_QUEUE] = track.track_name;
            // catalog_text(catalog, "Queue", track.queue_id, "name");
            break;
        case PH_TRACK_CATEGORY_MEMORY_ALLOCATE_AGENT_QUEUE:
            track_params.op = kRocProfVisDmOperationMemoryAllocate;
            track_params.track_indentifiers.category = kRocProfVisDmMemoryAllocationTrack;
            track_params.track_indentifiers.id[TRACK_ID_AGENT] = track.agent_id;
            track_params.track_indentifiers.id[TRACK_ID_QUEUE] = track.queue_id;
            names[TRACK_ID_AGENT] = catalog_agent_name(catalog, track.agent_id);
            names[TRACK_ID_QUEUE] = track.track_name;
            // catalog_text(catalog, "Queue", track.queue_id, "name");
            break;
        case PH_TRACK_CATEGORY_MEMORY_COPY_AGENT_QUEUE:
            track_params.op                          = kRocProfVisDmOperationMemoryCopy;
            track_params.track_indentifiers.category = kRocProfVisDmMemoryCopyTrack;
            track_params.track_indentifiers.id[TRACK_ID_AGENT] = track.agent_id;
            track_params.track_indentifiers.id[TRACK_ID_QUEUE] = track.queue_id;
            names[TRACK_ID_AGENT] = catalog_agent_name(catalog, track.agent_id);
            names[TRACK_ID_QUEUE] = track.track_name;
            break;
        case PH_TRACK_CATEGORY_HIP_EVENT_AGENT_QUEUE:
            track_params.op                          = kRocProfVisDmOperationHipEvent;
            track_params.track_indentifiers.category = kRocProfVisDmHipEventTrack;
            track_params.track_indentifiers.id[TRACK_ID_AGENT] = track.agent_id;
            track_params.track_indentifiers.id[TRACK_ID_QUEUE] = track.queue_id;
            names[TRACK_ID_AGENT] = catalog_agent_name(catalog, track.agent_id);
            names[TRACK_ID_QUEUE] = track.track_name;
            break;
        case PH_TRACK_CATEGORY_STREAM:
            track_params.op                          = kRocProfVisDmMultipleOperations;
            track_params.track_indentifiers.category = kRocProfVisDmStreamTrack;
            track_params.track_indentifiers.id[TRACK_ID_PID]    = track.process_row_id;
            track_params.track_indentifiers.id[TRACK_ID_STREAM] = track.stream_id;
            names[TRACK_ID_STREAM]                              = track.track_name;
            // catalog_text(catalog, "Stream", track.stream_id, "name");
            names[TRACK_ID_PID] = names[TRACK_ID_STREAM];
            break;
        default: return kRocProfVisDmResultInvalidParameter;
    }
    track_params.track_indentifiers.track_id           = track.id;
    track_params.track_indentifiers.process_id         = track.pid;
    track_params.track_indentifiers.id[TRACK_ID_NODE]  = track.nid;
    track_params.track_indentifiers.tag[TRACK_ID_NODE] = Builder::NODE_ID_SERVICE_NAME;
    switch(track.category)
    {
        case PH_TRACK_CATEGORY_THREAD:
        case PH_TRACK_CATEGORY_THREAD_SAMPLE:
            track_params.track_indentifiers.tag[TRACK_ID_PID] =
                Builder::PROCESS_ID_SERVICE_NAME;
            track_params.track_indentifiers.tag[TRACK_ID_TID] =
                Builder::THREAD_ID_SERVICE_NAME;
            break;
        case PH_TRACK_CATEGORY_PMC_AGENT:
        case PH_TRACK_CATEGORY_PMC_KERNEL:
        case PH_TRACK_CATEGORY_PMC_MEMORY:
            track_params.track_indentifiers.tag[TRACK_ID_PID] =
                Builder::AGENT_ID_SERVICE_NAME;
            track_params.track_indentifiers.tag[TRACK_ID_TID] =
                Builder::COUNTER_ID_SERVICE_NAME;
            break;
        case PH_TRACK_CATEGORY_KERNEL_DISPATCH_AGENT_QUEUE:
        case PH_TRACK_CATEGORY_MEMORY_ALLOCATE_AGENT_QUEUE:
        case PH_TRACK_CATEGORY_MEMORY_COPY_AGENT_QUEUE:
        case PH_TRACK_CATEGORY_HIP_EVENT_AGENT_QUEUE:
            track_params.track_indentifiers.tag[TRACK_ID_PID] =
                Builder::AGENT_ID_SERVICE_NAME;
            track_params.track_indentifiers.tag[TRACK_ID_TID] =
                Builder::QUEUE_ID_SERVICE_NAME;
            break;
        case PH_TRACK_CATEGORY_STREAM:
            track_params.track_indentifiers.tag[TRACK_ID_PID] =
                Builder::PROCESS_ID_SERVICE_NAME;
            track_params.track_indentifiers.tag[TRACK_ID_TID] =
                Builder::STREAM_ID_SERVICE_NAME;
            break;
    }
    track_params.record_count = track.event_count;
    if(track_params.op < kRocProfVisDmNumOperation)
    {
        TraceProperties()->events_count[track_params.op] += track.event_count;
    }
    track_params.min_ts = track.start_ts;
    track_params.max_ts = track.end_ts;
    if((track.category == PH_TRACK_CATEGORY_PMC_AGENT ||
        track.category == PH_TRACK_CATEGORY_PMC_KERNEL ||
        track.category == PH_TRACK_CATEGORY_PMC_MEMORY) &&
       track.value_range.is_valid)
    {
        track_params.min_value = track.value_range.min;
        track_params.max_value = track.value_range.max;
    }
    else
    {
        track_params.min_value = 0;
        track_params.max_value = track.nesting_depth-1;
    }

    track_params.order_id = track.id;
    if(DbInstances().empty()) return kRocProfVisDmResultInvalidParameter;
    track_params.track_indentifiers.db_instance = DbInstancePtrAt(0);

    rocprofvis_dm_result_t result = ProcessTrack(track_params);
    if(result != kRocProfVisDmResultSuccess) return result;

    UpdateTraceDuration(db_instance, track.start_ts, track.end_ts);
    return result;
}

rocprofvis_dm_result_t
ProfilerHub::BuildHistogram(Future* future)
{
    ShowProgress(5, "Collecting track histogram", kRPVDbBusy, future);
    ph_future_t histogram_future = future->AddProfilerHubFuture(Path());
    if(histogram_future == nullptr) return kRocProfVisDmResultDbAccessFailed;

    ph_histogram_t histogram = {};
    ph_result_t    hub_result =
        ph_get_histogram(m_ph_trace, 500, &histogram, histogram_future);
    if(hub_result != PH_RESULT_SUCCESS)
    {
        future->DeleteProfilerHubFuture(histogram_future);
        return kRocProfVisDmResultDbAccessFailed;
    }

    rocprofvis_dm_result_t wait_result =
        future->WaitAndDeleteProfilerHubFuture(histogram_future);
    if(wait_result != kRocProfVisDmResultSuccess) return wait_result;
    if(future->Interrupted()) return kRocProfVisDmResultDbAbort;

    TraceProperties()->histogram_bucket_size  = histogram.bucket_size;
    TraceProperties()->histogram_bucket_count = histogram.bucket_count;
    // if(!TraceProperties()->db_inst_start_time.empty() && histogram.bucket_count > 0)
    //{
    //     TraceProperties()->db_inst_start_time[0] = histogram.origin_ts;
    // }

    if(histogram.tracks != nullptr)
    {
        for(uint32_t i = 0; i < histogram.track_count; ++i)
        {
            const ph_track_histogram_t&   source      = histogram.tracks[i];
            rocprofvis_dm_track_params_t* destination = nullptr;
            for(int track_index = 0; track_index < NumTracks(); ++track_index)
            {
                if(TrackPropertiesAt(track_index)->track_indentifiers.track_id ==
                   source.track_id)
                {
                    destination = TrackPropertiesAt(track_index);
                    break;
                }
            }
            if(destination == nullptr || source.buckets == nullptr) continue;
            for(uint32_t bucket_index = 0; bucket_index < source.list_size;
                ++bucket_index)
            {
                const ph_histogram_bucket_t& bucket = source.buckets[bucket_index];
                uint32_t                     count  = bucket.event_count > UINT32_MAX
                                                          ? UINT32_MAX
                                                          : static_cast<uint32_t>(bucket.event_count);
                destination->histogram[bucket.bucket_index] =
                    std::make_pair(count, bucket.value);
            }
        }
    }

    TraceProperties()->histogram.clear();
    for(int track_index = 0; track_index < NumTracks(); ++track_index)
    {
        rocprofvis_dm_track_params_t* track = TrackPropertiesAt(track_index);
        if(track->track_indentifiers.category == kRocProfVisDmPmcTrack) continue;
        for(const std::pair<const uint32_t, std::pair<uint32_t, double>>& entry :
            track->histogram)
        {
            TraceProperties()->histogram[entry.first] += entry.second.first;
        }
    }
    return kRocProfVisDmResultSuccess;
}

static void
set_optional_uint(InformationCatalog& catalog, const char* table, uint64_t row_id,
                  const char* column, ph_optional_uint64_t value,
                  std::optional<int64_t> default_value = std::nullopt)
{
    if(value.is_valid == 0)
    {
        if(default_value.has_value())
        {
            catalog.SetInt(table, row_id, column, *default_value);
        }
        else
        {
            catalog.SetNull(table, row_id, column);
        }
        return;
    }
    catalog.SetInt(table, row_id, column, static_cast<int64_t>(value.value));
}

static int64_t
stream_hardware_operation(ph_track_category_t category)
{
    if(category == PH_TRACK_CATEGORY_KERNEL_DISPATCH_AGENT_QUEUE)
    {
        return kRocProfVisDmOperationDispatch;
    }
    if(category == PH_TRACK_CATEGORY_MEMORY_ALLOCATE_AGENT_QUEUE)
    {
        return kRocProfVisDmOperationMemoryAllocate;
    }
    if(category == PH_TRACK_CATEGORY_MEMORY_COPY_AGENT_QUEUE)
    {
        return kRocProfVisDmOperationMemoryCopy;
    }
    if(category == PH_TRACK_CATEGORY_HIP_EVENT_AGENT_QUEUE)
    {
        return kRocProfVisDmOperationHipEvent;
    }
    return kRocProfVisDmOperationNoOp;
}

void
ProfilerHub::LoadInformationCatalog(const ph_node_t& node)
{
    InformationCatalog& catalog = CachedTables(kHubCatalogIndex)->Catalog();
    const uint64_t      node_id = node.info.id;

    catalog.SetInt("Node", node_id, "id", static_cast<int64_t>(node_id));
    catalog.SetText("Node", node_id, "machine_id", node.info.machine_id);
    catalog.SetText("Node", node_id, "system_name", node.info.system_name);
    catalog.SetText("Node", node_id, "hostname", node.info.hostname);
    catalog.SetText("Node", node_id, "release", node.info.release);
    catalog.SetText("Node", node_id, "version", node.info.version);
    catalog.SetText("Node", node_id, "hardware_name", node.info.hardware_name);
    catalog.SetText("Node", node_id, "domain_name", node.info.domain_name);

    if(node.agents.agents != nullptr)
    {
        for(uint32_t i = 0; i < node.agents.list_size; ++i)
        {
            const ph_agent_t& agent = node.agents.agents[i];
            const uint64_t    id    = agent.row_id;
            catalog.SetInt("Agent", id, "id", static_cast<int64_t>(id));
            catalog.SetInt("Agent", id, "type_index", static_cast<int64_t>(agent.id));
            catalog.SetText("Agent", id, "type", agent.agent_type);
            catalog.SetText("Agent", id, "name", agent.name);
            catalog.SetText("Agent", id, "model_name", agent.model_name);
            catalog.SetText("Agent", id, "vendor_name", agent.vendor_name);
            catalog.SetText("Agent", id, "product_name", agent.product_name);
            catalog.SetText("Agent", id, "user_name", agent.user_name);
            catalog.SetText("Agent", id, "extdata", agent.extdata);
            set_optional_uint(catalog, "Agent", id, "absolute_index",
                              agent.absolute_index);
            set_optional_uint(catalog, "Agent", id, "logical_index", agent.logical_index);
            set_optional_uint(catalog, "Agent", id, "uuid", agent.uuid);
        }
    }

    if(node.process_list.processes != nullptr)
    {
        for(uint32_t i = 0; i < node.process_list.list_size; ++i)
        {
            const ph_process_t& process = node.process_list.processes[i];
            const uint64_t      id      = process.row_id;
            catalog.SetInt("Process", id, "id", static_cast<int64_t>(process.row_id));
            catalog.SetInt("Process", id, "pid", static_cast<int64_t>(process.id));
            catalog.SetText("Process", id, "command", process.command);
            catalog.SetText("Process", id, "environment", process.environment);
            catalog.SetText("Process", id, "extdata", process.extdata);
            set_optional_uint(catalog, "Process", id, "ppid", process.ppid);
            set_optional_uint(catalog, "Process", id, "init", process.init);
            set_optional_uint(catalog, "Process", id, "fini", process.fini);
            set_optional_uint(catalog, "Process", id, "start", process.start, 0);
            set_optional_uint(catalog, "Process", id, "end", process.end, 0);
        }
    }

    if(node.thread_list.threads != nullptr)
    {
        for(uint32_t i = 0; i < node.thread_list.list_size; ++i)
        {
            const ph_thread_t& thread = node.thread_list.threads[i];
            const uint64_t     id     = thread.row_id;
            catalog.SetInt("Thread", id, "id", static_cast<int64_t>(id));
            catalog.SetInt("Thread", id, "tid", static_cast<int64_t>(thread.thread_id));
            catalog.SetText("Thread", id, "name", thread.name);
            catalog.SetText("Thread", id, "extdata", thread.extdata);
            set_optional_uint(catalog, "Thread", id, "ppid", thread.parent_process_id);
            set_optional_uint(catalog, "Thread", id, "start", thread.start, 0);
            set_optional_uint(catalog, "Thread", id, "end", thread.end, 0);
        }
    }

    if(node.stream_list.streams != nullptr)
    {
        for(uint32_t i = 0; i < node.stream_list.list_size; ++i)
        {
            const ph_stream_t& stream = node.stream_list.streams[i];
            catalog.SetInt("Stream", stream.stream_id, "id",
                           static_cast<int64_t>(stream.stream_id));
            catalog.SetInt("Stream", stream.stream_id, "nid",
                           static_cast<int64_t>(stream.nid));
            catalog.SetInt("Stream", stream.stream_id, "pid",
                           static_cast<int64_t>(stream.pid));
            catalog.SetText("Stream", stream.stream_id, "name", stream.name);
            catalog.SetText("Stream", stream.stream_id, "extdata", stream.extdata);
        }
    }

    if(node.queue_list.queues != nullptr)
    {
        for(uint32_t i = 0; i < node.queue_list.list_size; ++i)
        {
            const ph_queue_t& queue = node.queue_list.queues[i];
            catalog.SetInt("Queue", queue.queue_id, "id",
                           static_cast<int64_t>(queue.queue_id));
            catalog.SetInt("Queue", queue.queue_id, "nid",
                           static_cast<int64_t>(queue.nid));
            catalog.SetInt("Queue", queue.queue_id, "pid",
                           static_cast<int64_t>(queue.pid));
            catalog.SetText("Queue", queue.queue_id, "name", queue.name);
            catalog.SetText("Queue", queue.queue_id, "extdata", queue.extdata);
        }
    }

    if(node.pmc_info_list.pmc_infos != nullptr)
    {
        for(uint32_t i = 0; i < node.pmc_info_list.list_size; ++i)
        {
            const ph_pmc_info_t& pmc = node.pmc_info_list.pmc_infos[i];
            const uint64_t       id  = pmc.id;
            catalog.SetInt("PMC", id, "id", static_cast<int64_t>(id));
            catalog.SetText("PMC", id, "name", pmc.name);
            catalog.SetText("PMC", id, "symbol", pmc.symbol);
            catalog.SetText("PMC", id, "description", pmc.description);
            catalog.SetText("PMC", id, "long_description", pmc.long_description);
            catalog.SetText("PMC", id, "component", pmc.component);
            catalog.SetText("PMC", id, "units", pmc.units);
            catalog.SetText("PMC", id, "value_type", pmc.value_type);
            catalog.SetText("PMC", id, "block", pmc.block);
            catalog.SetText("PMC", id, "expression", pmc.expression);
            catalog.SetText("PMC", id, "extdata", pmc.extdata);
            catalog.SetText("PMC", id, "target_arch", pmc.target_arch);
            set_optional_uint(catalog, "PMC", id, "event_code", pmc.event_code);
            set_optional_uint(catalog, "PMC", id, "instance_id", pmc.instance_id);
            set_optional_uint(catalog, "PMC", id, "is_constant", pmc.is_constant);
            set_optional_uint(catalog, "PMC", id, "is_derived", pmc.is_derived);
        }
    }

    if(node.stream_hardware_links.links != nullptr)
    {
        for(uint32_t i = 0; i < node.stream_hardware_links.list_size; ++i)
        {
            const ph_stream_hardware_link_t& link = node.stream_hardware_links.links[i];
            stream_hardware_relation_t       relation = {};
            relation.category                         = link.category;
            relation.nid                              = link.nid;
            relation.pid                              = link.pid;
            relation.stream_id                        = link.stream_id;
            relation.agent_id                         = link.agent_id.value;
            relation.queue_id                         = link.queue_id.value;
            relation.stream_track_id                  = link.stream_track_id.value;
            relation.hardware_track_id                = link.hardware_track_id.value;
            relation.agent_is_valid        = link.agent_id.is_valid == 0 ? 0 : 1;
            relation.queue_is_valid        = link.queue_id.is_valid == 0 ? 0 : 1;
            relation.stream_track_is_valid = link.stream_track_id.is_valid == 0 ? 0 : 1;
            relation.hardware_track_is_valid =
                link.hardware_track_id.is_valid == 0 ? 0 : 1;
            catalog.AddStreamHardwareRelation(relation);

            catalog.SetInt("StreamToHw", i, Builder::STREAM_ID_SERVICE_NAME,
                           static_cast<int64_t>(link.stream_id));
            catalog.SetInt("StreamToHw", i, Builder::NODE_ID_SERVICE_NAME, link.nid);
            catalog.SetInt("StreamToHw", i, Builder::OPERATION_SERVICE_NAME,
                           stream_hardware_operation(link.category));
            set_optional_uint(catalog, "StreamToHw", i, Builder::AGENT_ID_SERVICE_NAME,
                              link.agent_id);
            set_optional_uint(catalog, "StreamToHw", i, Builder::QUEUE_ID_SERVICE_NAME,
                              link.queue_id);
        }
    }
}

rocprofvis_dm_result_t
ProfilerHub::PopulateStreamToHardwareFlowProperties(uint32_t stream_track_index,
                                                    uint32_t db_instance)
{
    CatalogTable* table =
        (CatalogTable*) CachedTables(db_instance)->GetTableHandle("StreamToHw");
    uint32_t stream_id = static_cast<uint32_t>(
        TrackPropertiesAt(stream_track_index)->track_indentifiers.id[TRACK_ID_STREAM]);

    for(int ind = 0; ind < table->NumRows(); ind++)
    {
        uint64_t id =
            std::atoll(table->CellTextByIndex(ind, Builder::STREAM_ID_SERVICE_NAME));
        if(id == stream_id)
        {
            uint32_t op = static_cast<uint32_t>(
                std::atoll(table->CellTextByIndex(ind, Builder::OPERATION_SERVICE_NAME)));
            std::string agent =
                table->CellTextByIndex(ind, Builder::AGENT_ID_SERVICE_NAME);
            uint32_t agent_id = agent_id = static_cast<uint32_t>(
                std::atoll(table->CellTextByIndex(ind, Builder::AGENT_ID_SERVICE_NAME)));
            uint32_t queue_id = static_cast<uint32_t>(
                std::atoll(table->CellTextByIndex(ind, Builder::QUEUE_ID_SERVICE_NAME)));
            uint32_t track;
            if(TrackTracker()->FindTrack(TrackTracker()->SearchCategoryMaskLookup(
                                             (rocprofvis_dm_event_operation_t) op),
                                         agent_id, queue_id, db_instance, track))
            {
                rocprofvis_dm_result_t result = BindObject()->FuncAddTopologyNodeProperty(
                    BindObject()->trace_object,
                    &TrackPropertiesAt(stream_track_index)->track_indentifiers,
                    kRPVTopologyDataTypeRef, "QueueRef", "TrackRef",
                    (void*) &TrackPropertiesAt(track)->track_indentifiers);
                if(result != kRocProfVisDmResultSuccess) return result;
            }
        }
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t
ProfilerHub::ReadTraceMetadata(Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    while(true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties,
                                    ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        TraceProperties()->trace_duration   = 0;
        TraceProperties()->num_db_instances = 1;
        TraceProperties()->db_inst_start_time.push_back(UINT64_MAX);
        TraceProperties()->db_inst_end_time.push_back(0);
        DbInstances().push_back({ SingleNodeDbInstance(), "" });

        ph_future_t hub_future = future->AddProfilerHubFuture(Path());
        if(hub_future == nullptr)
        {
            break;
        }
        ShowProgress(1, "Call profiler hub to read trace metadata", kRPVDbBusy, future);
        ph_result_t hub_result = ph_ctx_create(&m_ph_trace, Path(), hub_future);
        if(hub_result != PH_RESULT_SUCCESS)
        {
            future->DeleteProfilerHubFuture(hub_future);
            break;
        }
        const rocprofvis_dm_result_t wait_result =
            future->WaitAndDeleteProfilerHubFuture(hub_future);
        if(wait_result != kRocProfVisDmResultSuccess)
        {
            break;
        }

        ph_node_t node = {};
        hub_result     = ph_get_node(m_ph_trace, &node, NULL);
        if(hub_result != PH_RESULT_SUCCESS)
        {
            break;
        }
        ShowProgress(1, "Load information catalog", kRPVDbBusy, future);
        LoadInformationCatalog(node);
        bool tracks_added = true;
        if(node.track_list.tracks != nullptr)
        {
            for(uint32_t i = 0; i < node.track_list.list_size; i++)
            {
                if(AddTrack(node.track_list.tracks[i], DbInstancePtrAt(0)) !=
                   kRocProfVisDmResultSuccess)
                {
                    tracks_added = false;
                    break;
                }
            }
        }
        if(!tracks_added) break;

        rocprofvis_dm_result_t result = kRocProfVisDmResultUnknownError;
        for(int i = 0; i < NumTracks(); i++)
        {
            if(TrackPropertiesAt(i)->track_indentifiers.category ==
               kRocProfVisDmStreamTrack)
            {
                DbInstance* db_instance =
                    (DbInstance*) TrackPropertiesAt(i)->track_indentifiers.db_instance;
                result =
                    PopulateStreamToHardwareFlowProperties(i, db_instance->GuidIndex());
                if(result != kRocProfVisDmResultSuccess) break;
            }
        }
        if(result != kRocProfVisDmResultSuccess) break;

        if(BuildHistogram(future) != kRocProfVisDmResultSuccess) break;

        const uint32_t guid_index = DbInstancePtrAt(0)->GuidIndex();
        if(LoadStringCatalog(guid_index) != kRocProfVisDmResultSuccess) break;

        TraceProperties()->metadata_loaded = true;
        BindObject()->FuncMetadataLoaded(BindObject()->trace_object);
        ShowProgress(100 - future->Progress(), "Trace metadata successfully loaded",
                     kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Trace metadata not loaded!", kRPVDbError, future);
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort
                                                    : kRocProfVisDmResultDbAccessFailed);
}


std::string
ProfilerHub::BuildSemanticSourceFilter(rocprofvis_dm_timestamp_t        start,
                                       rocprofvis_dm_timestamp_t        end,
                                       rocprofvis_db_num_of_tracks_t    num,
                                       rocprofvis_db_track_selection_t  tracks,
                                       rocprofvis_dm_query_criteria_ptr source_filter)
{
    ParamSerializer ser;
    ser.set("start", start);
    ser.set("end", end);
    if(source_filter)
    {
        if(source_filter->node_id.has_value)
        {
            ser.set("node", source_filter->node_id.value);           
        }
        if(source_filter->agent_id.has_value)
        {
            ser.set("agent", source_filter->agent_id.value);
        }
        if(source_filter->process_id.has_value)
        {
            ser.set("process", source_filter->process_id.value);
        }
        if(source_filter->thread_id.has_value)
        {
            ser.set("thread", source_filter->thread_id.value);
        }
    }
    std::vector<uint32_t> v_tracks;
    for(int i = 0; i < num; i++)
    {
        v_tracks.push_back(tracks[i]);
    }
    ser.setArray("tracks", std::move(v_tracks));
    return ser.toString();
}

std::string
ProfilerHub::BuildSemanticSourceFilter(
    rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
    rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t tracks,
    rocprofvis_dm_query_criteria_ptr         source_filter,
    rocprofvis_dm_num_string_table_filters_t num_string_table_filters,
    rocprofvis_dm_string_table_filters_t string_table_filters, bool include_substring,
    bool include_category, bool partial_matching)
{
    ParamSerializer ser(BuildSemanticSourceFilter(start, end, num, tracks, source_filter));
    std::vector<std::string> v_filters;
    for(int i = 0; i < num_string_table_filters; i++)
    {
        v_filters.push_back(string_table_filters[i]);
    }
    ser.setArray("filters", std::move(v_filters));
    ser.set("include_substring", include_substring);
    ser.set("include_category", include_category);
    ser.set("partial_matching", partial_matching);
    return ser.toString();
}

rocprofvis_dm_result_t
ProfilerHub::BuildTableQuery(
    rocprofvis_dm_table_use_case_enum_t use_case, rocprofvis_dm_timestamp_t start,
    rocprofvis_dm_timestamp_t end, rocprofvis_db_num_of_tracks_t num,
    rocprofvis_db_track_selection_t  tracks,
    rocprofvis_dm_query_criteria_ptr source_filter, rocprofvis_dm_charptr_t filter,
    rocprofvis_dm_charptr_t group, rocprofvis_dm_charptr_t group_cols,
    rocprofvis_dm_charptr_t sort_column, rocprofvis_dm_sort_order_t sort_order,
    uint64_t max_count, uint64_t offset, bool count_only, rocprofvis_dm_string_t& query)
{
    query.clear();
    
    if (num == 0)
        kRocProfVisDmResultInvalidParameter;

    bool sample_query = false;
    if(TABLE_QUERY_UNPACK_OP_TYPE(tracks[0]) == 0)
    {
        sample_query = TrackPropertiesAt(tracks[0])->track_indentifiers.category ==
                       kRocProfVisDmPmcTrack;
        for (int i = 0; i < num; i++)
        {
            query += BuildSemanticSourceFilter(start, end, 1, &tracks[i], source_filter);
            query += ";";
            query += std::to_string(tracks[i]);
            query += ";";

            DbInstance* props_db_instance =
                (DbInstance*) TrackPropertiesAt(tracks[0])->track_indentifiers.db_instance;
            query += std::to_string(props_db_instance->GuidIndex());
            query += "\n";
        }
    }
    else
    {
        sample_query = (rocprofvis_dm_event_operation_t) TABLE_QUERY_UNPACK_OP_TYPE(
                           tracks[0]) == kRocProfVisDmOperationNoOp;
        for (int i = 0; i < num; i++)
        {
            std::string semantic_query = BuildSemanticSourceFilter(start, end, 1, &tracks[i], source_filter);
            for (auto& guid_info : DbInstances())
            {
                query += semantic_query;
                query += ";";
                query += std::to_string(tracks[i]);
                query += ";";
                query += std::to_string(guid_info.first.GuidIndex());
                query += "\n";
            }
        }
    }

    return TableProcessor::BuildTableSemanticSubQuery(
        use_case, filter, group, group_cols, sort_column, sort_order, max_count, offset,
        count_only, sample_query, query);
}

rocprofvis_dm_result_t
ProfilerHub::BuildEventSearchQuery(
    rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
    rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t ops,
    rocprofvis_dm_query_criteria_ptr         source_filter,
    rocprofvis_dm_charptr_t filter,
    rocprofvis_dm_num_string_table_filters_t num_string_table_filters,
    rocprofvis_dm_string_table_filters_t string_table_filters, bool include_substring,
    bool include_category, bool partial_matching, rocprofvis_dm_charptr_t sort_column,
    rocprofvis_dm_sort_order_t sort_order, uint64_t max_count, uint64_t offset,
    bool count_only, rocprofvis_dm_string_t& query)
{
    (void) filter;
    for (int i = 0; i < num; i++)
    {
        std::string semantic_query = BuildSemanticSourceFilter(
            start, end, 1, &ops[i], source_filter, num_string_table_filters,
            string_table_filters, include_substring, include_category, partial_matching);
        for (auto& guid_info : DbInstances())
        {
            query += semantic_query;
            query += ";";
            query += std::to_string(ops[i]);
            query += ";";
            query += std::to_string(guid_info.first.GuidIndex());
            query += "\n";
        }
    }

    return TableProcessor::BuildTableSemanticSubQuery(
        kRPVDMTableUseCaseEventSearch, nullptr, nullptr, nullptr, sort_column, sort_order,
        max_count, offset, count_only, false, query);
}

static rocprofvis_dm_result_t hub_query_result(ph_result_t result);

rocprofvis_dm_result_t
ProfilerHub::SaveTrimmedData(rocprofvis_dm_timestamp_t start,
                             rocprofvis_dm_timestamp_t end,
                             rocprofvis_dm_charptr_t new_db_path, Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(new_db_path, "New DB path cannot be NULL.",
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    while(true)
    {
        uint64_t abs_start = start;
        uint64_t abs_end   = end;
        if(TraceProperties() != nullptr && !TraceProperties()->db_inst_start_time.empty())
        {
            const uint64_t origin = TraceProperties()->db_inst_start_time[0];
            if(origin != UINT64_MAX && origin > 0)
            {
                if(abs_start <= UINT64_MAX - origin) abs_start += origin;
                if(abs_end <= UINT64_MAX - origin) abs_end += origin;
            }
        }

        ph_future_t ph_future = future->AddProfilerHubFuture(Path());
        if(ph_future == nullptr)
        {
            break;
        }
        ShowProgress(1, "Trimming trace", kRPVDbBusy, future);
        const ph_result_t hub_result =
            ph_trim_trace(m_ph_trace, abs_start, abs_end, new_db_path, ph_future);
        rocprofvis_dm_result_t result = hub_query_result(hub_result);
        if(hub_result != PH_RESULT_SUCCESS)
        {
            future->DeleteProfilerHubFuture(ph_future);
            ShowProgress(0, "Failed trimming trace!", kRPVDbError, future);
            return future->SetPromise(result);
        }
        result = future->WaitAndDeleteProfilerHubFuture(ph_future);
        if(result != kRocProfVisDmResultSuccess)
        {
            ShowProgress(0, "Failed trimming trace!", kRPVDbError, future);
            return future->SetPromise(result);
        }
        ShowProgress(100 - future->Progress(), "Trace successfully trimmed!",
                     kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }

    ShowProgress(0, "Failed trimming trace!", kRPVDbError, future);
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort
                                                    : kRocProfVisDmResultDbAccessFailed);
}

bool
convert_dm_ph_event_type(rocprofvis_dm_event_operation_t dm_type,
                         ph_event_type_t&                ph_type)
{
    switch(dm_type)
    {
        case kRocProfVisDmOperationLaunch:
        case kRocProfVisDmOperationLaunchSample: ph_type = PH_EVENT_TYPE_REGION; break;
        case kRocProfVisDmOperationDispatch:
            ph_type = PH_EVENT_TYPE_KERNEL_DISPATCH;
            break;
        case kRocProfVisDmOperationMemoryAllocate:
            ph_type = PH_EVENT_TYPE_MEMORY_ALLOCATE;
            break;
        case kRocProfVisDmOperationMemoryCopy:
            ph_type = PH_EVENT_TYPE_MEMORY_COPY;
            break;
        case kRocProfVisDmOperationHipEvent: ph_type = PH_EVENT_TYPE_HIP_EVENT; break;
        default: return false;
    }
    return true;
}

bool
convert_ph_dm_event_type(ph_event_type_t                  ph_type,
                         rocprofvis_dm_event_operation_t& dm_type)
{
    switch(ph_type)
    {
        case PH_EVENT_TYPE_REGION: dm_type = kRocProfVisDmOperationLaunch; break;
        case PH_EVENT_TYPE_KERNEL_DISPATCH:
            dm_type = kRocProfVisDmOperationDispatch;
            break;
        case PH_EVENT_TYPE_MEMORY_ALLOCATE:
            dm_type = kRocProfVisDmOperationMemoryAllocate;
            break;
        case PH_EVENT_TYPE_MEMORY_COPY:
            dm_type = kRocProfVisDmOperationMemoryCopy;
            break;
        case PH_EVENT_TYPE_HIP_EVENT: dm_type = kRocProfVisDmOperationHipEvent; break;
        default: return false;
    }
    return true;
}

rocprofvis_dm_result_t
ProfilerHub::ReadFlowTraceInfo(rocprofvis_dm_event_id_t event_id, Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    while(true)
    {
        rocprofvis_dm_flowtrace_t container =
            BindObject()->FuncAddFlowTrace(BindObject()->trace_object, event_id);

        ph_future_t ph_future = future->AddProfilerHubFuture(Path());
        if(ph_future == nullptr)
        {
            break;
        }
        ph_event_list_t ph_endpoints;
        ph_event_t      event;
        event.id = event_id.bitfield.event_id;

        if(!convert_dm_ph_event_type(
               (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op, event.type))
        {
            break;
        }
        rocprofvis_dm_result_t result = kRocProfVisDmResultUnknownError;
        ph_result_t            ph_result =
            ph_get_correlated_events(m_ph_trace, &event, &ph_endpoints, ph_future);
        if(ph_result != PH_RESULT_SUCCESS)
        {
            future->DeleteProfilerHubFuture(ph_future);
            break;
        }
        else
        {
            result = future->WaitAndDeleteProfilerHubFuture(ph_future);
        }

        if(result == kRocProfVisDmResultSuccess)
        {
            uint32_t db_instance = 0;  // todo: support multi-node
            for(int i = 0; i < ph_endpoints.list_size; i++)
            {
                rocprofvis_db_flow_data_t       record;
                rocprofvis_dm_event_operation_t op;
                if(!convert_ph_dm_event_type(ph_endpoints.events[i].type, op))
                {
                    break;
                }
                record.id.bitfield.event_op   = op;
                record.id.bitfield.event_node = db_instance;

                record.id.bitfield.event_id = ph_endpoints.events[i].id;
                record.time                 = ph_endpoints.events[i].start;
                record.time -= TraceProperties()->db_inst_start_time[db_instance];
                record.category_id = 0;
                record.symbol_id   = BindObject()->FuncAddString(
                    BindObject()->trace_object, ph_endpoints.events[i].name);
                record.level    = ph_endpoints.events[i].nesting_depth-1;
                record.track_id = ph_endpoints.events[i].track_id;
                record.end_time = ph_endpoints.events[i].end;
                record.end_time -= TraceProperties()->db_inst_start_time[db_instance];

                if(BindObject()->FuncAddFlow(container, record) !=
                   kRocProfVisDmResultSuccess)
                {
                    result = kRocProfVisDmResultUnknownError;
                    break;
                }
            }
        }
        if(result != kRocProfVisDmResultSuccess) break;
        ShowProgress(100 - future->Progress(), "Successfully read flow trace!",
                     kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Failed reading flow trace!", kRPVDbError, future);
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort
                                                    : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t
ProfilerHub::ReadStackTraceInfo(rocprofvis_dm_event_id_t event_id, Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    while(true)
    {
        
        rocprofvis_dm_stacktrace_t container =
            BindObject()->FuncAddStackTrace(BindObject()->trace_object, event_id);

        ph_future_t ph_future = future->AddProfilerHubFuture(Path());
        if(ph_future == nullptr)
        {
            break;
        }
        ph_call_stack_t ph_callstack;
        ph_event_t      event;
        event.id = event_id.bitfield.event_id;

        if(!convert_dm_ph_event_type(
            (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op, event.type))
        {
            break;
        }
        rocprofvis_dm_result_t result = kRocProfVisDmResultUnknownError;
        ph_result_t            ph_result =
            ph_get_call_stack(m_ph_trace, &event, &ph_callstack, ph_future);
        if(ph_result != PH_RESULT_SUCCESS)
        {
            future->DeleteProfilerHubFuture(ph_future);
            break;
        }
        else
        {
            result = future->WaitAndDeleteProfilerHubFuture(ph_future);
        }

        if(result == kRocProfVisDmResultSuccess)
        {
            uint32_t db_instance = 0;  // todo: support multi-node
            for(int i = 0; i < ph_callstack.list_size; i++)
            {
                rocprofvis_db_stack_data_t       record;
                jt::Json json_symbol;
                jt::Json json_line;
                std::string symbol_blob;
                std::string line_blob;
                json_symbol["name"] = ph_callstack.frames[i].program_counter.function;
                json_symbol["file"] = ph_callstack.frames[i].program_counter.filename;
                json_line["line_address"] = ph_callstack.frames[i].program_counter.line_number.is_valid ? std::to_string(ph_callstack.frames[i].program_counter.line_number.value).c_str() : "";
                symbol_blob = json_symbol.toString();
                line_blob = json_line.toString();
                record.symbol = symbol_blob.c_str();
                record.line = line_blob.c_str();
                record.depth = i;
                record.args = "";
                record.id = ph_callstack.frames[i].program_counter.event_id;           

                if(BindObject()->FuncAddStackFrame(container, record) !=
                    kRocProfVisDmResultSuccess)
                {
                    result = kRocProfVisDmResultUnknownError;
                    break;
                }
            }
        }
        if(result != kRocProfVisDmResultSuccess) 
        
        break;
        ShowProgress(100 - future->Progress(), "Successfully read flow trace!",
                     kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Failed reading flow trace!", kRPVDbError, future);
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort
                                                    : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t
ProfilerHub::ReadExtEventInfo(rocprofvis_dm_event_id_t event_id, Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    while(true)
    {
        rocprofvis_dm_extdata_t container =
            BindObject()->FuncAddExtData(BindObject()->trace_object, event_id);
        if(container == nullptr)
        {
            break;
        }
        ph_event_t event{};
        event.id = event_id.bitfield.event_id;
        if(!convert_dm_ph_event_type(
               (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op, event.type))
        {
            break;
        }
        ph_future_t ph_future = future->AddProfilerHubFuture(Path());
        if(ph_future == nullptr)
        {
            break;
        }

        rocprofvis_dm_result_t       result    = kRocProfVisDmResultUnknownError;
        ph_result_t                  ph_result = PH_RESULT_INVALID_ARGUMENT;
        ph_region_details_t          region{};
        ph_kernel_dispatch_details_t kernel{};
        ph_memory_copy_details_t     copy{};
        ph_memory_alloc_details_t    alloc{};
        switch(event.type)
        {
            case PH_EVENT_TYPE_REGION:
                ph_result = ph_get_region_details(m_ph_trace, &event, &region, ph_future);
                break;
            case PH_EVENT_TYPE_KERNEL_DISPATCH:
                ph_result = ph_get_kernel_dispatch_details(m_ph_trace, &event, &kernel,
                                                           ph_future);
                break;
            case PH_EVENT_TYPE_MEMORY_COPY:
                ph_result =
                    ph_get_memory_copy_details(m_ph_trace, &event, &copy, ph_future);
                break;
            case PH_EVENT_TYPE_MEMORY_ALLOCATE:
                ph_result =
                    ph_get_memory_alloc_details(m_ph_trace, &event, &alloc, ph_future);
                break;
        }
        if(ph_result != PH_RESULT_SUCCESS)
        {
            future->DeleteProfilerHubFuture(ph_future);
            break;
        }
        result = future->WaitAndDeleteProfilerHubFuture(ph_future);
        if(result != kRocProfVisDmResultSuccess) break;

        const uint32_t     db_instance = event_id.bitfield.event_node;
        ProfilerHubExtData writer(
            this, container, (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
            db_instance, TraceProperties()->db_inst_start_time[db_instance]);
        bool stored = writer.AddTrack(event.track_id, region.event);
        if(stored && event.type == PH_EVENT_TYPE_REGION && region.is_valid != 0)
        {
            stored = writer.AddRegion(region);
        }
        else if(stored && event.type == PH_EVENT_TYPE_KERNEL_DISPATCH &&
                kernel.is_valid != 0)
        {
            stored = writer.AddKernelDispatch(kernel);
        }
        else if(stored && event.type == PH_EVENT_TYPE_MEMORY_COPY && copy.is_valid != 0)
        {
            stored = writer.AddMemoryCopy(copy);
        }
        else if(stored && event.type == PH_EVENT_TYPE_MEMORY_ALLOCATE &&
                alloc.is_valid != 0)
        {
            stored = writer.AddMemoryAlloc(alloc);
        }
        if(!stored) break;

        ShowProgress(100 - future->Progress(), "Successfully read extended event info!",
                     kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Failed reading extended event info!", kRPVDbError, future);
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort
                                                    : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t
ProfilerHub::ReadTraceSlice(rocprofvis_dm_timestamp_t            start,
                            rocprofvis_dm_timestamp_t            end,
                            rocprofvis_dm_hashed_timestamp_tag_t tag,
                            rocprofvis_db_num_of_tracks_t        num,
                            rocprofvis_db_track_selection_t tracks, Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG(BindObject()->trace_properties,
                          ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
    ROCPROFVIS_ASSERT_MSG(BindObject()->trace_properties->metadata_loaded,
                          ERROR_METADATA_IS_NOT_LOADED);
    ROCPROFVIS_ASSERT_MSG(num == 1, ERROR_UNSUPPORTED_FEATURE);

    rocprofvis_dm_track_params_t* props = TrackPropertiesAt(*tracks);

    if(props->track_indentifiers.category == kRocProfVisDmPmcTrack)
    {
        return ReadTracePMCSlice(start, end, tag, tracks, true, true, future);
    }
    else
    {
        while(true)
        {
            DbInstance* props_db_instance =
                (DbInstance*) props->track_indentifiers.db_instance;
            ROCPROFVIS_ASSERT_MSG_RETURN(props_db_instance, ERROR_FUTURE_CANNOT_BE_NULL,
                                         kRocProfVisDmResultInvalidParameter);
            uint32_t db_instance = props_db_instance->GuidIndex();
            auto     it          = std::find_if(
                TrackPropertiesBegin(), TrackPropertiesEnd(),
                [tracks](std::unique_ptr<rocprofvis_dm_track_params_t>& params) {
                    return params.get()->track_indentifiers.track_id == *tracks;
                });
            if(it == TrackPropertiesEnd()) break;

            rocprofvis_dm_slice_t slice_handle = BindObject()->FuncAddSlice(
                BindObject()->trace_object, *tracks, start, end, tag);
            ph_future_t ph_future = future->AddProfilerHubFuture(Path());
            if(ph_future == nullptr)
            {
                BindObject()->FuncRemoveSlice(BindObject()->trace_object, *tracks,
                                              slice_handle);
                break;
            }
            ph_event_list_t        ph_slice;
            rocprofvis_dm_result_t result    = kRocProfVisDmResultUnknownError;
            ph_result_t            ph_result = ph_get_track_events(
                m_ph_trace, tracks[0],
                start + TraceProperties()->db_inst_start_time[db_instance],
                end + TraceProperties()->db_inst_start_time[db_instance], &ph_slice,
                ph_future);
            if(ph_result != PH_RESULT_SUCCESS)
            {
                future->DeleteProfilerHubFuture(ph_future);
                break;
            }
            else
            {
                result = future->WaitAndDeleteProfilerHubFuture(ph_future);
            }

            if(result == kRocProfVisDmResultSuccess)
            {
                for(int i = 0; i < ph_slice.list_size; i++)
                {
                    rocprofvis_db_record_data_t     record;
                    rocprofvis_dm_event_operation_t op;
                    record.event.id.bitfield.event_id   = ph_slice.events[i].id;
                    record.event.id.bitfield.event_node = db_instance;

                    if(!convert_ph_dm_event_type(ph_slice.events[i].type, op))
                    {
                        break;
                    }
                    record.event.id.bitfield.event_op = op;
                    record.event.timestamp            = ph_slice.events[i].start;
                    record.event.duration =
                        ph_slice.events[i].end - ph_slice.events[i].start;
                    record.event.timestamp -=
                        TraceProperties()->db_inst_start_time[db_instance];
                    record.event.category = 0;
                    record.event.symbol   = BindObject()->FuncAddString(
                        BindObject()->trace_object, ph_slice.events[i].name);
                    record.event.level = ph_slice.events[i].nesting_depth-1;
                    if(BindObject()->FuncAddRecord(slice_handle, record) !=
                       kRocProfVisDmResultSuccess)
                    {
                        result = kRocProfVisDmResultUnknownError;
                        break;
                    }
                }
                BindObject()->FuncCompleteSlice(slice_handle);
            }
            else
            {
                BindObject()->FuncRemoveSlice(BindObject()->trace_object, *tracks,
                                              slice_handle);
                break;
            }

            ShowProgress(100 - future->Progress(), "Time slice successfully loaded!",
                         kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }
    }

    ShowProgress(0, "Not all tracks are loaded!", kRPVDbError, future);
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort
                                                    : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t
ProfilerHub::ReadTracePMCSlice(rocprofvis_dm_timestamp_t            start,
                               rocprofvis_dm_timestamp_t            end,
                               rocprofvis_dm_hashed_timestamp_tag_t tag,
                               rocprofvis_db_track_selection_t tracks, bool left_neighbor,
                               bool right_neighbor, Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG(BindObject()->trace_properties,
                          ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
    ROCPROFVIS_ASSERT_MSG(BindObject()->trace_properties->metadata_loaded,
                          ERROR_METADATA_IS_NOT_LOADED);

    rocprofvis_dm_track_params_t* props = TrackPropertiesAt(*tracks);
    while(true)
    {
        DbInstance* props_db_instance =
            (DbInstance*) props->track_indentifiers.db_instance;
        ROCPROFVIS_ASSERT_MSG_RETURN(props_db_instance, ERROR_FUTURE_CANNOT_BE_NULL,
                                     kRocProfVisDmResultInvalidParameter);
        uint32_t db_instance = props_db_instance->GuidIndex();
        auto     it =
            std::find_if(TrackPropertiesBegin(), TrackPropertiesEnd(),
                         [tracks](std::unique_ptr<rocprofvis_dm_track_params_t>& params) {
                             return params.get()->track_indentifiers.track_id == *tracks;
                         });
        if(it == TrackPropertiesEnd()) break;

        rocprofvis_dm_slice_t slice_handle = BindObject()->FuncAddSlice(
            BindObject()->trace_object, *tracks, start, end, tag);
        ph_future_t ph_future = future->AddProfilerHubFuture(Path());
        if(ph_future == nullptr)
        {
            BindObject()->FuncRemoveSlice(BindObject()->trace_object, *tracks,
                                          slice_handle);
            break;
        }

        ph_sample_list_t       ph_slice;
        rocprofvis_dm_result_t result    = kRocProfVisDmResultUnknownError;
        ph_result_t            ph_result = ph_get_track_samples(
            m_ph_trace, tracks[0],
            start + TraceProperties()->db_inst_start_time[db_instance],
            end + TraceProperties()->db_inst_start_time[db_instance], &ph_slice,
            ph_future);
        if(ph_result != PH_RESULT_SUCCESS)
        {
            future->DeleteProfilerHubFuture(ph_future);
            break;
        }
        else
        {
            result = future->WaitAndDeleteProfilerHubFuture(ph_future);
        }
        if(result == kRocProfVisDmResultSuccess)
        {
            for(int i = 0; i < ph_slice.list_size; i++)
            {
                rocprofvis_db_record_data_t record;
                record.pmc.timestamp = ph_slice.samples[i].timestamp;
                record.pmc.timestamp -=
                    TraceProperties()->db_inst_start_time[db_instance];
                record.pmc.value = ph_slice.samples[i].value;
                if(BindObject()->FuncAddRecord(slice_handle, record) !=
                   kRocProfVisDmResultSuccess)
                {
                    result = kRocProfVisDmResultUnknownError;
                    break;
                }
            }
            BindObject()->FuncCompleteSlice(slice_handle);
        }
        else
        {
            BindObject()->FuncRemoveSlice(BindObject()->trace_object, *tracks,
                                          slice_handle);
            break;
        }
        ShowProgress(100 - future->Progress(), "Time slice successfully loaded!",
                     kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }

    ShowProgress(0, "Not all tracks are loaded!", kRPVDbError, future);
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort
                                                    : kRocProfVisDmResultDbAccessFailed);
}

static rocprofvis_dm_result_t
hub_query_result(ph_result_t result)
{
    switch(result)
    {
        case PH_RESULT_SUCCESS: return kRocProfVisDmResultSuccess;
        case PH_RESULT_INVALID_ARGUMENT: return kRocProfVisDmResultInvalidParameter;
        case PH_RESULT_CANCELLED: return kRocProfVisDmResultDbAbort;
        case PH_RESULT_UNSUPPORTED_FILE: return kRocProfVisDmResultNotSupported;
        default: return kRocProfVisDmResultUnknownError;
    }
}

static bool
hub_event_type(rocprofvis_dm_event_operation_t operation, uint32_t& event_type)
{
    switch(operation)
    {
        case kRocProfVisDmOperationLaunch:
            event_type = PH_EVENT_TYPE_REGION;
            return true;
        case kRocProfVisDmOperationLaunchSample:
            event_type = PH_EVENT_TYPE_REGION_SAMPLE;
            return true;
        case kRocProfVisDmOperationDispatch:
            event_type = PH_EVENT_TYPE_KERNEL_DISPATCH;
            return true;
        case kRocProfVisDmOperationMemoryAllocate:
            event_type = PH_EVENT_TYPE_MEMORY_ALLOCATE;
            return true;
        case kRocProfVisDmOperationMemoryCopy:
            event_type = PH_EVENT_TYPE_MEMORY_COPY;
            return true;
        case kRocProfVisDmOperationHipEvent:
            event_type = PH_EVENT_TYPE_HIP_EVENT;
            return true;
        default: return false;
    }
}

static rocprofvis_dm_result_t
open_event_query(ph_ctx_t ctx, bool has_time, uint64_t start, uint64_t end, bool has_node, uint64_t node,
                 bool has_agent, uint64_t agent, bool has_process, uint64_t process_id,
                 bool has_thread, uint64_t thread_id, const uint32_t* track_ids,
                 uint32_t track_count, const uint32_t* event_types,
                 uint32_t event_type_count, const std::vector<std::string>& filters,
                 bool include_substring, bool include_category, bool partial_matching,
                 ph_event_query_t& query)
{
    std::vector<const char*> texts;
    texts.reserve(filters.size());
    for(size_t index = 0; index < filters.size(); ++index)
    {
        texts.push_back(filters[index].c_str());
    }

    ph_event_filter_t filter{};
    filter.size                               = sizeof(filter);
    filter.time.start                         = start;
    filter.time.end                           = end;
    filter.time.is_valid                      = has_time;
    filter.tracks                             = track_ids;
    filter.track_count                        = track_count;
    filter.event_types                        = event_types;
    filter.event_type_count                   = event_type_count;
    filter.node_id.value                      = node;
    filter.node_id.is_valid                   = has_node ? 1u : 0u;
    filter.agent_id.value                     = agent;
    filter.agent_id.is_valid                  = has_agent ? 1u : 0u;
    filter.process_id.value                   = process_id;
    filter.process_id.is_valid                = has_process ? 1u : 0u;
    filter.thread_id.value                    = thread_id;
    filter.thread_id.is_valid                 = has_thread ? 1u : 0u;
    filter.string_filter.search_strings       = texts.empty() ? nullptr : texts.data();
    filter.string_filter.search_strings_count = static_cast<uint32_t>(texts.size());
    filter.string_filter.include_substring    = include_substring ? 1u : 0u;
    filter.string_filter.include_category     = include_category ? 1u : 0u;
    filter.string_filter.partial_matching     = partial_matching ? 1u : 0u;
    return hub_query_result(ph_event_query_create(ctx, &filter, &query));
}

static const uint32_t kHubTablePage = 100000;

static void
push_number(hub_table_row& row, int64_t value)
{
    hub_table_cell cell{};
    cell.text       = false;
    cell.number     = value;
    cell.text_value = "";
    row.cells.push_back(cell);
}

static void
push_text(hub_table_row& row, const char* value)
{
    hub_table_cell cell{};
    cell.text       = true;
    cell.number     = 0;
    cell.text_value = value != nullptr ? value : "";
    row.cells.push_back(cell);
}

static void
push_real(hub_table_row& row, double value)
{
    hub_table_cell cell{};
    cell.real       = true;
    cell.real_value = value;
    cell.text_value = "";
    row.cells.push_back(cell);
}

static void
push_agent(hub_table_row& row, uint64_t agent)
{
    push_number(row, (int64_t) agent);
    push_number(row, (int64_t) agent);
    push_number(row, (int64_t) agent);
    push_number(row, (int64_t) agent);
}

static int64_t
event_duration(const ph_table_event_t& event)
{
    if(event.end < event.start) return 0;
    return (int64_t) (event.end - event.start);
}

static char*
hub_column(const char* name)
{
    return const_cast<char*>(name);
}

static char* kRegionColumns[] = {
    hub_column(Builder::OPERATION_SERVICE_NAME),
    hub_column(Builder::ID_PUBLIC_NAME),
    hub_column(Builder::DB_ID_PUBLIC_NAME),
    hub_column(Builder::CATEGORY_REFERENCE),
    hub_column(Builder::EVENT_NAME_REFERENCE),
    hub_column(Builder::NODE_ID_SERVICE_NAME),
    hub_column(Builder::PROCESS_ID_PUBLIC_NAME),
    hub_column(Builder::THREAD_ID_PUBLIC_NAME),
    hub_column(Builder::START_SERVICE_NAME),
    hub_column(Builder::END_SERVICE_NAME),
    hub_column(Builder::DURATION_PUBLIC_NAME),
    hub_column(Builder::PROCESS_ID_SERVICE_NAME),
    hub_column(Builder::THREAD_ID_SERVICE_NAME),
    hub_column(Builder::STREAM_ID_SERVICE_NAME),
};

static char* kKernelColumns[] = {
    hub_column(Builder::OPERATION_SERVICE_NAME),
    hub_column(Builder::ID_PUBLIC_NAME),
    hub_column(Builder::DB_ID_PUBLIC_NAME),
    hub_column(Builder::CATEGORY_REFERENCE),
    hub_column(Builder::SYMBOL_NAME_REFERENCE),
    hub_column(Builder::STREAM_NAME_REFERENCE),
    hub_column(Builder::QUEUE_NAME_REFERENCE),
    hub_column(Builder::NODE_ID_SERVICE_NAME),
    hub_column(Builder::PROCESS_ID_PUBLIC_NAME),
    hub_column(Builder::THREAD_ID_PUBLIC_NAME),
    hub_column(Builder::AGENT_ABS_INDEX_REFERENCE),
    hub_column(Builder::AGENT_TYPE_REFERENCE),
    hub_column(Builder::AGENT_TYPE_INDEX_REFERENCE),
    hub_column(Builder::AGENT_NAME_REFERENCE),
    hub_column(Builder::START_SERVICE_NAME),
    hub_column(Builder::END_SERVICE_NAME),
    hub_column(Builder::DURATION_PUBLIC_NAME),
    hub_column(Builder::GRID_SIZEX_PUBLIC_NAME),
    hub_column(Builder::GRID_SIZEY_PUBLIC_NAME),
    hub_column(Builder::GRID_SIZEZ_PUBLIC_NAME),
    hub_column(Builder::WORKGROUP_SIZEX_PUBLIC_NAME),
    hub_column(Builder::WORKGROUP_SIZEY_PUBLIC_NAME),
    hub_column(Builder::WORKGROUP_SIZEZ_PUBLIC_NAME),
    hub_column(Builder::LDS_SIZE_PUBLIC_NAME),
    hub_column(Builder::SCRATCH_SIZE_PUBLIC_NAME),
    hub_column(Builder::STATIC_LDS_SIZE_PUBLIC_NAME),
    hub_column(Builder::STATIC_SCRATCH_SIZE_PUBLIC_NAME),
    hub_column(Builder::PROCESS_ID_SERVICE_NAME),
    hub_column(Builder::AGENT_ID_SERVICE_NAME),
    hub_column(Builder::QUEUE_ID_SERVICE_NAME),
    hub_column(Builder::STREAM_ID_SERVICE_NAME),
};

static char* kAllocColumns[] = {
    hub_column(Builder::OPERATION_SERVICE_NAME),
    hub_column(Builder::ID_PUBLIC_NAME),
    hub_column(Builder::DB_ID_PUBLIC_NAME),
    hub_column(Builder::CATEGORY_REFERENCE),
    hub_column(Builder::M_TYPE_REFERENCE),
    hub_column(Builder::STREAM_NAME_REFERENCE),
    hub_column(Builder::QUEUE_NAME_REFERENCE),
    hub_column(Builder::NODE_ID_SERVICE_NAME),
    hub_column(Builder::PROCESS_ID_PUBLIC_NAME),
    hub_column(Builder::THREAD_ID_PUBLIC_NAME),
    hub_column(Builder::AGENT_ABS_INDEX_REFERENCE),
    hub_column(Builder::AGENT_TYPE_REFERENCE),
    hub_column(Builder::AGENT_TYPE_INDEX_REFERENCE),
    hub_column(Builder::AGENT_NAME_REFERENCE),
    hub_column(Builder::START_SERVICE_NAME),
    hub_column(Builder::END_SERVICE_NAME),
    hub_column(Builder::DURATION_PUBLIC_NAME),
    hub_column(Builder::SIZE_PUBLIC_NAME),
    hub_column(Builder::ADDRESS_PUBLIC_NAME),
    hub_column(Builder::LEVEL_REFERENCE),
    hub_column(Builder::PROCESS_ID_SERVICE_NAME),
    hub_column(Builder::AGENT_ID_SERVICE_NAME),
    hub_column(Builder::QUEUE_ID_SERVICE_NAME),
    hub_column(Builder::STREAM_ID_SERVICE_NAME),
};

static char* kCopyColumns[] = {
    hub_column(Builder::OPERATION_SERVICE_NAME),
    hub_column(Builder::ID_PUBLIC_NAME),
    hub_column(Builder::DB_ID_PUBLIC_NAME),
    hub_column(Builder::CATEGORY_REFERENCE),
    hub_column(Builder::EVENT_NAME_REFERENCE),
    hub_column(Builder::STREAM_NAME_REFERENCE),
    hub_column(Builder::QUEUE_NAME_REFERENCE),
    hub_column(Builder::NODE_ID_SERVICE_NAME),
    hub_column(Builder::PROCESS_ID_PUBLIC_NAME),
    hub_column(Builder::THREAD_ID_PUBLIC_NAME),
    hub_column(Builder::AGENT_ABS_INDEX_REFERENCE),
    hub_column(Builder::AGENT_TYPE_REFERENCE),
    hub_column(Builder::AGENT_TYPE_INDEX_REFERENCE),
    hub_column(Builder::AGENT_NAME_REFERENCE),
    hub_column(Builder::START_SERVICE_NAME),
    hub_column(Builder::END_SERVICE_NAME),
    hub_column(Builder::DURATION_PUBLIC_NAME),
    hub_column(Builder::SIZE_PUBLIC_NAME),
    hub_column(Builder::ADDRESS_PUBLIC_NAME),
    hub_column(Builder::AGENT_SRC_ABS_INDEX_REFERENCE),
    hub_column(Builder::AGENT_SRC_TYPE_REFERENCE),
    hub_column(Builder::AGENT_SRC_TYPE_INDEX_REFERENCE),
    hub_column(Builder::AGENT_SRC_NAME_REFERENCE),
    hub_column(Builder::SRC_ADDRESS_PUBLIC_NAME),
    hub_column(Builder::PROCESS_ID_SERVICE_NAME),
    hub_column(Builder::AGENT_ID_SERVICE_NAME),
    hub_column(Builder::QUEUE_ID_SERVICE_NAME),
    hub_column(Builder::STREAM_ID_SERVICE_NAME),
};

static char* kHipColumns[] = {
    hub_column(Builder::OPERATION_SERVICE_NAME),
    hub_column(Builder::ID_PUBLIC_NAME),
    hub_column(Builder::DB_ID_PUBLIC_NAME),
    hub_column(Builder::CATEGORY_REFERENCE),
    hub_column(Builder::EVENT_NAME_REFERENCE),
    hub_column(Builder::STREAM_NAME_REFERENCE),
    hub_column(Builder::QUEUE_NAME_REFERENCE),
    hub_column(Builder::NODE_ID_SERVICE_NAME),
    hub_column(Builder::PROCESS_ID_PUBLIC_NAME),
    hub_column(Builder::THREAD_ID_PUBLIC_NAME),
    hub_column(Builder::AGENT_ABS_INDEX_REFERENCE),
    hub_column(Builder::AGENT_TYPE_REFERENCE),
    hub_column(Builder::AGENT_TYPE_INDEX_REFERENCE),
    hub_column(Builder::AGENT_NAME_REFERENCE),
    hub_column(Builder::START_SERVICE_NAME),
    hub_column(Builder::END_SERVICE_NAME),
    hub_column(Builder::DURATION_PUBLIC_NAME),
    hub_column(Builder::HANDLE_PUBLIC_NAME),
    hub_column(Builder::SRC_QUEUE_NAME_REFERENCE),
    hub_column(Builder::PROCESS_ID_SERVICE_NAME),
    hub_column(Builder::AGENT_ID_SERVICE_NAME),
    hub_column(Builder::QUEUE_ID_SERVICE_NAME),
    hub_column(Builder::STREAM_ID_SERVICE_NAME),
};

static void
fill_region_row(hub_table_row& row, const ph_table_event_t& event, int64_t operation)
{
    row.cells.clear();
    push_number(row, operation);
    push_number(row, (int64_t) event.id);
    push_number(row, (int64_t) event.id);
    push_number(row, (int64_t) event.category_id);
    push_number(row, (int64_t) event.name_id);
    push_number(row, (int64_t) event.nid);
    push_number(row, (int64_t) event.pid);
    push_number(row, (int64_t) event.tid);
    push_number(row, (int64_t) event.start);
    push_number(row, (int64_t) event.end);
    push_number(row, event_duration(event));
    push_number(row, (int64_t) event.process_id);
    push_number(row, (int64_t) event.thread_id);
    push_number(row, -1);
}

static void
fill_kernel_row(hub_table_row& row, const ph_table_event_t& event, int64_t operation)
{
    row.cells.clear();
    push_number(row, operation);
    push_number(row, (int64_t) event.id);
    push_number(row, (int64_t) event.id);
    push_number(row, (int64_t) event.category_id);
    push_number(row, (int64_t) event.kernel_id);
    push_number(row, (int64_t) event.stream_id);
    push_number(row, (int64_t) event.queue_id);
    push_number(row, (int64_t) event.nid);
    push_number(row, (int64_t) event.process_id);
    push_number(row, (int64_t) event.thread_id);
    push_agent(row, event.agent_id);
    push_number(row, (int64_t) event.start);
    push_number(row, (int64_t) event.end);
    push_number(row, event_duration(event));
    push_number(row, (int64_t) event.grid_size_x);
    push_number(row, (int64_t) event.grid_size_y);
    push_number(row, (int64_t) event.grid_size_z);
    push_number(row, (int64_t) event.workgroup_size_x);
    push_number(row, (int64_t) event.workgroup_size_y);
    push_number(row, (int64_t) event.workgroup_size_z);
    push_number(row, (int64_t) event.group_segment_size);
    push_number(row, (int64_t) event.private_segment_size);
    push_number(row, (int64_t) event.static_group_segment_size);
    push_number(row, (int64_t) event.static_private_segment_size);
    push_number(row, (int64_t) event.process_id);
    push_number(row, (int64_t) event.agent_id);
    push_number(row, (int64_t) event.queue_id);
    push_number(row, (int64_t) event.stream_id);
}

static void
fill_alloc_row(hub_table_row& row, const ph_table_event_t& event, int64_t operation)
{
    row.cells.clear();
    push_number(row, operation);
    push_number(row, (int64_t) event.id);
    push_number(row, (int64_t) event.id);
    push_number(row, (int64_t) event.category_id);
    push_text(row, event.mem_type);
    push_number(row, (int64_t) event.stream_id);
    push_number(row, (int64_t) event.queue_id);
    push_number(row, (int64_t) event.nid);
    push_number(row, (int64_t) event.process_id);
    push_number(row, (int64_t) event.thread_id);
    push_agent(row, event.agent_id);
    push_number(row, (int64_t) event.start);
    push_number(row, (int64_t) event.end);
    push_number(row, event_duration(event));
    push_number(row, (int64_t) event.size);
    push_number(row, (int64_t) event.address);
    push_text(row, event.level);
    push_number(row, (int64_t) event.process_id);
    push_number(row, (int64_t) event.agent_id);
    push_number(row, (int64_t) event.queue_id);
    push_number(row, (int64_t) event.stream_id);
}

static void
fill_copy_row(hub_table_row& row, const ph_table_event_t& event, int64_t operation)
{
    row.cells.clear();
    push_number(row, operation);
    push_number(row, (int64_t) event.id);
    push_number(row, (int64_t) event.id);
    push_number(row, (int64_t) event.category_id);
    push_number(row, (int64_t) event.name_id);
    push_number(row, (int64_t) event.stream_id);
    push_number(row, (int64_t) event.queue_id);
    push_number(row, (int64_t) event.nid);
    push_number(row, (int64_t) event.process_id);
    push_number(row, (int64_t) event.thread_id);
    push_agent(row, event.agent_id);
    push_number(row, (int64_t) event.start);
    push_number(row, (int64_t) event.end);
    push_number(row, event_duration(event));
    push_number(row, (int64_t) event.size);
    push_number(row, (int64_t) event.address);
    push_agent(row, event.src_agent_id);
    push_number(row, (int64_t) event.src_address);
    push_number(row, (int64_t) event.process_id);
    push_number(row, (int64_t) event.agent_id);
    push_number(row, (int64_t) event.queue_id);
    push_number(row, (int64_t) event.stream_id);
}

static void
fill_hip_row(hub_table_row& row, const ph_table_event_t& event, int64_t operation)
{
    row.cells.clear();
    push_number(row, operation);
    push_number(row, (int64_t) event.id);
    push_number(row, (int64_t) event.id);
    push_number(row, (int64_t) event.category_id);
    push_number(row, (int64_t) event.name_id);
    push_number(row, (int64_t) event.stream_id);
    push_number(row, (int64_t) event.queue_id);
    push_number(row, (int64_t) event.nid);
    push_number(row, (int64_t) event.process_id);
    push_number(row, (int64_t) event.thread_id);
    push_agent(row, event.agent_id);
    push_number(row, (int64_t) event.start);
    push_number(row, (int64_t) event.end);
    push_number(row, event_duration(event));
    push_number(row, (int64_t) event.size);
    push_number(row, (int64_t) event.address);
    push_number(row, (int64_t) event.process_id);
    push_number(row, (int64_t) event.agent_id);
    push_number(row, (int64_t) event.queue_id);
    push_number(row, (int64_t) event.stream_id);
}

static char* kSampleColumns[] = {
    hub_column(Builder::OPERATION_SERVICE_NAME),
    hub_column(Builder::START_SERVICE_NAME),
    hub_column(Builder::END_SERVICE_NAME),
    hub_column(Builder::NODE_ID_SERVICE_NAME),
    hub_column(Builder::AGENT_ID_SERVICE_NAME),
    hub_column(Builder::COUNTER_ID_SERVICE_NAME),
    hub_column(Builder::COUNTER_VALUE_SERVICE_NAME),
};

static void
fill_sample_row(hub_table_row& row, const ph_table_event_t& event, int64_t operation,
                uint64_t node, uint64_t agent, uint64_t counter)
{
    row.cells.clear();
    push_number(row, operation);
    push_number(row, (int64_t) event.start);
    push_number(row, (int64_t) event.end);
    push_number(row, (int64_t) node);
    push_number(row, (int64_t) agent);
    push_number(row, (int64_t) counter);
    push_real(row, event.value);
}

static bool
hub_sample_event(uint32_t event_type)
{
    return event_type == PH_EVENT_TYPE_PMC_COUNTER ||
           event_type == PH_EVENT_TYPE_PMC_SAMPLE ||
           event_type == PH_EVENT_TYPE_MEMORY_ACTIVITY;
}

static rocprofvis_dm_result_t
feed_event_query(ph_event_query_t query, uint32_t event_type, int64_t operation,
                 uint64_t first_after_id, uint64_t last_id, uint32_t page_size,
                 uint64_t sample_node, uint64_t sample_agent, uint64_t sample_counter,
                 rocprofvis_db_query_callback_parameters* params, RpvCallback callback,
                 Future* future, ph_future_t hub_future)
{
    if(query == nullptr || params == nullptr || callback == nullptr || future == nullptr ||
       page_size == 0)
    {
        return kRocProfVisDmResultInvalidParameter;
    }

    char** names = nullptr;
    int    count = 0;
    if(event_type == PH_EVENT_TYPE_REGION || event_type == PH_EVENT_TYPE_REGION_SAMPLE)
    {
        names = kRegionColumns;
        count = (int) (sizeof(kRegionColumns) / sizeof(kRegionColumns[0]));
    }
    else if(event_type == PH_EVENT_TYPE_KERNEL_DISPATCH)
    {
        names = kKernelColumns;
        count = (int) (sizeof(kKernelColumns) / sizeof(kKernelColumns[0]));
    }
    else if(event_type == PH_EVENT_TYPE_MEMORY_ALLOCATE)
    {
        names = kAllocColumns;
        count = (int) (sizeof(kAllocColumns) / sizeof(kAllocColumns[0]));
    }
    else if(event_type == PH_EVENT_TYPE_MEMORY_COPY)
    {
        names = kCopyColumns;
        count = (int) (sizeof(kCopyColumns) / sizeof(kCopyColumns[0]));
    }
    else if(event_type == PH_EVENT_TYPE_HIP_EVENT)
    {
        names = kHipColumns;
        count = (int) (sizeof(kHipColumns) / sizeof(kHipColumns[0]));
    }
    else if(hub_sample_event(event_type))
    {
        names = kSampleColumns;
        count = (int) (sizeof(kSampleColumns) / sizeof(kSampleColumns[0]));
    }
    else
    {
        return kRocProfVisDmResultSuccess;
    }

    std::vector<ph_table_event_t> page(page_size);
    hub_table_row                 row;
    row.cells.reserve((size_t) count);
    uint64_t after = first_after_id;
    while(true)
    {
        if(future->Interrupted()) return kRocProfVisDmResultDbAbort;
        uint32_t    written = 0;
        ph_result_t read    = ph_event_query_read(query, event_type, after, last_id,
                                                  page.data(), page_size, &written, hub_future);
        if(read != PH_RESULT_SUCCESS) return hub_query_result(read);
        for(uint32_t index = 0; index < written; ++index)
        {
            if(event_type == PH_EVENT_TYPE_REGION || event_type == PH_EVENT_TYPE_REGION_SAMPLE)
            {
                fill_region_row(row, page[index], operation);
            }
            else if(event_type == PH_EVENT_TYPE_KERNEL_DISPATCH)
            {
                fill_kernel_row(row, page[index], operation);
            }
            else if(event_type == PH_EVENT_TYPE_MEMORY_ALLOCATE)
            {
                fill_alloc_row(row, page[index], operation);
            }
            else if(event_type == PH_EVENT_TYPE_MEMORY_COPY)
            {
                fill_copy_row(row, page[index], operation);
            }
            else if(event_type == PH_EVENT_TYPE_HIP_EVENT)
            {
                fill_hip_row(row, page[index], operation);
            }
            else
            {
                fill_sample_row(row, page[index], operation, sample_node, sample_agent,
                                sample_counter);
            }
            if(callback(params, count, &row, names) != 0)
            {
                return future->Interrupted() ? kRocProfVisDmResultDbAbort
                                             : kRocProfVisDmResultUnknownError;
            }
        }
        if(written < page_size) return kRocProfVisDmResultSuccess;
        after = page[written - 1].id;
    }
}

struct hub_table_job
{
    ph_event_query_t      query;
    DbInstance*           db_instance;
    std::vector<uint32_t> event_types;
    std::vector<int64_t>  operations;
    std::vector<uint32_t> table_indices;
    uint64_t              record_count;
    uint64_t              after_id;
    uint64_t              last_id;
    uint32_t              page_size;
    uint64_t              sample_node;
    uint64_t              sample_agent;
    uint64_t              sample_counter;
};

static const uint32_t kHubTableMaxWorkers = 8;
static const uint32_t kHubTableMinPage    = 10000;
static const uint64_t kHubTableSplitRows  = 50000;

static bool
hub_job_splittable(const hub_table_job& job)
{
    if(job.event_types.size() != 1) return false;
    const uint32_t event_type = job.event_types[0];
    return event_type == PH_EVENT_TYPE_REGION || event_type == PH_EVENT_TYPE_REGION_SAMPLE ||
           event_type == PH_EVENT_TYPE_KERNEL_DISPATCH ||
           event_type == PH_EVENT_TYPE_PMC_COUNTER || event_type == PH_EVENT_TYPE_PMC_SAMPLE ||
           event_type == PH_EVENT_TYPE_MEMORY_ACTIVITY;
}

static void
free_event_queries(std::vector<ph_event_query_t>& event_queries)
{
    for(ph_event_query_t event_query : event_queries)
    {
        ph_event_query_free(event_query);
    }
    event_queries.clear();
}

rocprofvis_dm_result_t
ProfilerHub::GetEventTablesAsync(
    std::vector<std::pair<DbInstance*, std::string>>& queries, Future* future,
    rocprofvis_dm_handle_t handle, RpvCallback callback)
{
    TableProcessor*               table_processor = (TableProcessor*) handle;
    std::vector<ph_event_query_t> event_queries;
    std::vector<hub_table_job>    base_jobs;

    // Every packed table and hub query is created before any worker starts,
    // because AddPackedTable can reallocate the tables the workers write to.
    for(size_t query_index = 0; query_index < queries.size(); ++query_index)
    {
        std::pair<DbInstance*, std::string>& query = queries[query_index];
        ParamDeserializer        des(query.second);
        uint64_t                 start = 0;
        uint64_t                 end   = 0;
        uint64_t                 node  = 0;
        uint64_t                 agent = 0;
        uint64_t                 process_id = 0;
        uint64_t                 thread_id  = 0;
        std::vector<size_t>      tracks;
        std::vector<std::string> filters;
        bool                     include_substring = false;
        bool                     include_category  = false;
        bool                     partial_matching  = false;
        bool                     has_time = true;
        if (!des.get("start", start)) has_time = false;
        if (!des.get("end", end)) has_time=false;
        bool has_node       = des.get("node", node);
        bool has_agent      = des.get("agent", agent);
        bool has_process    = des.get("process", process_id);
        bool has_thread     = des.get("thread", thread_id);
        bool tracks_selected = des.getArray("tracks", tracks);
        bool search_query    = des.getArray("filters", filters);
        des.get("include_substring", include_substring);
        des.get("include_category", include_category);
        des.get("partial_matching", partial_matching);
        const bool by_operation = tracks_selected && tracks.size() > 0 &&
                                  TABLE_QUERY_UNPACK_OP_TYPE((uint32_t) tracks[0]) > 0;
        if(has_time && query.first != nullptr && TraceProperties() != nullptr)
        {
            const uint32_t guid = query.first->GuidIndex();
            const std::vector<rocprofvis_dm_timestamp_t>& origins =
                TraceProperties()->db_inst_start_time;
            if(guid < origins.size() && origins[guid] != UINT64_MAX && origins[guid] > 0)
            {
                const uint64_t origin = origins[guid];
                if(start <= UINT64_MAX - origin) start += origin;
                if(end <= UINT64_MAX - origin) end += origin;
            }
        }

        if((search_query && (!by_operation || filters.size() == 0)) ||
           (!search_query && !by_operation && (!tracks_selected || tracks.size() != 1)))
        {
            free_event_queries(event_queries);
            return kRocProfVisDmResultInvalidParameter;
        }

        std::vector<uint32_t> event_types;
        std::vector<int64_t>  operations;
        bool                  has_track         = false;
        bool                  one_type_per_job  = by_operation;
        uint32_t              hub_track         = 0;
        uint64_t              record_count      = 0;
        uint64_t              sample_node       = 0;
        uint64_t              sample_agent      = 0;
        uint64_t              sample_counter    = 0;
        if(by_operation)
        {
            for(size_t index = 0; index < tracks.size(); ++index)
            {
                rocprofvis_dm_event_operation_t operation =
                    (rocprofvis_dm_event_operation_t)
                        TABLE_QUERY_UNPACK_OP_TYPE((uint32_t) tracks[index]);
                uint32_t event_type = 0;
                if(hub_event_type(operation, event_type))
                {
                    event_types.push_back(event_type);
                    operations.push_back((int64_t) operation);
                }
            }
            record_count = kHubTableSplitRows;
        }
        else
        {
            rocprofvis_dm_track_params_t*   props = TrackPropertiesAt(tracks[0]);
            rocprofvis_dm_event_operation_t operation =
                (rocprofvis_dm_event_operation_t) props->op;
            has_track      = true;
            hub_track      = props->track_indentifiers.track_id;
            record_count   = props->record_count;
            sample_node    = props->track_indentifiers.id[TRACK_ID_NODE];
            sample_agent   = props->track_indentifiers.id[TRACK_ID_AGENT];
            sample_counter = props->track_indentifiers.id[TRACK_ID_COUNTER];
            if(operation == kRocProfVisDmMultipleOperations)
            {
                event_types  = { PH_EVENT_TYPE_KERNEL_DISPATCH, PH_EVENT_TYPE_MEMORY_ALLOCATE,
                                 PH_EVENT_TYPE_MEMORY_COPY, PH_EVENT_TYPE_HIP_EVENT };
                operations   = { kRocProfVisDmOperationDispatch,
                                 kRocProfVisDmOperationMemoryAllocate,
                                 kRocProfVisDmOperationMemoryCopy,
                                 kRocProfVisDmOperationHipEvent };
            }
            else if(operation == kRocProfVisDmOperationNoOp)
            {
                // One counter track draws from whichever of the three sample
                // tables matches its kind. The other two reads come back empty.
                one_type_per_job = true;
                event_types      = { PH_EVENT_TYPE_PMC_COUNTER, PH_EVENT_TYPE_PMC_SAMPLE,
                                     PH_EVENT_TYPE_MEMORY_ACTIVITY };
                operations       = { kRocProfVisDmOperationNoOp, kRocProfVisDmOperationNoOp,
                                     kRocProfVisDmOperationNoOp };
            }
            else
            {
                uint32_t event_type = 0;
                if(hub_event_type(operation, event_type))
                {
                    event_types.push_back(event_type);
                    operations.push_back((int64_t) operation);
                }
            }
        }
        if(event_types.empty())
        {
            continue;
        }

        ph_event_query_t event_query = nullptr;
        rocprofvis_dm_result_t opened = open_event_query(
            m_ph_trace, has_time, start, end, has_node, node, has_agent, agent, has_process,
            process_id, has_thread, thread_id, has_track ? &hub_track : nullptr,
            has_track ? 1u : 0u, event_types.data(), (uint32_t) event_types.size(), filters,
            include_substring, include_category, partial_matching, event_query);
        if(opened != kRocProfVisDmResultSuccess)
        {
            free_event_queries(event_queries);
            return opened;
        }
        event_queries.push_back(event_query);

        hub_table_job job;
        job.query        = event_query;
        job.db_instance  = query.first;
        job.record_count = record_count;
        job.after_id       = 0;
        job.last_id        = UINT64_MAX;
        job.page_size      = kHubTablePage;
        job.sample_node    = sample_node;
        job.sample_agent   = sample_agent;
        job.sample_counter = sample_counter;
        if(one_type_per_job)
        {
            for(size_t index = 0; index < event_types.size(); ++index)
            {
                job.event_types   = { event_types[index] };
                job.operations    = { operations[index] };
                job.table_indices = { index == 0 ? (uint32_t) query_index
                                                 : table_processor->AddPackedTable() };
                base_jobs.push_back(job);
            }
        }
        else
        {
            job.event_types = event_types;
            job.operations  = operations;
            job.table_indices.push_back((uint32_t) query_index);
            for(size_t index = 1; index < event_types.size(); ++index)
            {
                job.table_indices.push_back(table_processor->AddPackedTable());
            }
            base_jobs.push_back(job);
        }
    }

    // Split large single-table reads into row-id ranges so one track loads on
    // several pooled connections, the way QueryManager splits by time.
    uint32_t workers = std::min<uint32_t>(
        std::max<uint32_t>(std::thread::hardware_concurrency(), 1u), kHubTableMaxWorkers);
    uint32_t divider = base_jobs.empty() ? 1u : workers / (uint32_t) base_jobs.size();
    if(divider == 0) divider = 1;
    std::vector<hub_table_job> jobs;
    for(hub_table_job& job : base_jobs)
    {
        uint64_t first_id = 0;
        uint64_t last_id  = 0;
        if(divider > 1 && hub_job_splittable(job) && job.record_count >= kHubTableSplitRows &&
           ph_event_query_id_span(job.query, job.event_types[0], &first_id, &last_id,
                                  nullptr) == PH_RESULT_SUCCESS &&
           last_id > first_id && last_id - first_id >= kHubTableSplitRows)
        {
            const uint64_t base = first_id > 0 ? first_id - 1 : 0;
            const uint64_t step = (last_id - base) / divider;
            const uint32_t page = std::max<uint32_t>(kHubTablePage / divider, kHubTableMinPage);
            for(uint32_t slice = 0; slice < divider; ++slice)
            {
                hub_table_job part = job;
                part.after_id      = base + step * slice;
                part.last_id       = slice + 1 == divider ? last_id : base + step * (slice + 1);
                part.page_size     = page;
                if(slice > 0)
                {
                    part.table_indices = { table_processor->AddPackedTable() };
                }
                jobs.push_back(part);
            }
        }
        else
        {
            jobs.push_back(job);
        }
    }

    std::vector<Future*>   futures;
    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
    for(const hub_table_job& job : jobs)
    {
        Future* query_future = future->AddSubFuture();
        if(query_future == nullptr)
        {
            result = kRocProfVisDmResultAllocFailure;
            break;
        }
        ph_future_t hub_future = query_future->AddProfilerHubFuture(Path());
        if(hub_future == nullptr)
        {
            future->DeleteSubFuture(query_future);
            result = kRocProfVisDmResultAllocFailure;
            break;
        }

        rocprofvis_db_query_callback_parameters params;
        params.db          = this;
        params.future      = query_future;
        params.handle      = handle;
        params.callback    = callback;
        params.track_id    = (rocprofvis_dm_track_id_t) job.table_indices[0];
        params.operation   = kRocProfVisDmOperationNoOp;
        params.db_instance = job.db_instance;

        try
        {
            query_future->SetWorker(std::thread(
                [query_future, hub_future, params, job, callback]() {
                    rocprofvis_dm_result_t worker_result = kRocProfVisDmResultSuccess;
                    for(size_t index = 0; index < job.event_types.size(); ++index)
                    {
                        rocprofvis_db_query_callback_parameters table_params = params;
                        table_params.track_id = job.table_indices[index];
                        worker_result         = feed_event_query(
                            job.query, job.event_types[index], job.operations[index],
                            job.after_id, job.last_id, job.page_size, job.sample_node,
                            job.sample_agent, job.sample_counter, &table_params, callback,
                            query_future, hub_future);
                        if(worker_result != kRocProfVisDmResultSuccess)
                        {
                            if(hub_sample_event(job.event_types[index]))
                            {
                                worker_result = kRocProfVisDmResultSuccess;
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    query_future->DeleteProfilerHubFuture(hub_future);
                    return query_future->SetPromise(worker_result);
                }));
            futures.push_back(query_future);
        }
        catch(const std::exception&)
        {
            query_future->DeleteProfilerHubFuture(hub_future);
            future->DeleteSubFuture(query_future);
            result = kRocProfVisDmResultUnknownError;
            break;
        }
    }

    for(Future* query_future : futures)
    {
        const rocprofvis_dm_result_t waited =
            future->WaitAndDeleteSubFuture(query_future);
        if(result == kRocProfVisDmResultSuccess)
        {
            result = waited;
        }
    }
    free_event_queries(event_queries);
    return result;
}

struct string_catalog_import_t
{
    ProfilerHub*                hub;
    uint32_t                    node;
    rocprofvis_db_string_type_t type;
    bool                        failed;
};

void
ProfilerHub::ImportCatalogText(uint64_t id, const char* text, void* user_data)
{
    string_catalog_import_t* import = static_cast<string_catalog_import_t*>(user_data);
    if(import == nullptr || import->hub == nullptr || text == nullptr)
    {
        return;
    }
    const rocprofvis_dm_index_t index = import->hub->BindObject()->FuncAddString(
        import->hub->BindObject()->trace_object, text);
    if(index == INVALID_INDEX)
    {
        import->failed = true;
        return;
    }
    rocprofvis_db_string_id_t key = { id, import->node, import->type };
    import->hub->m_string_index_map[key] = index;
}

rocprofvis_dm_result_t
ProfilerHub::LoadStringCatalog(uint32_t node)
{
    if(m_ph_trace == nullptr || BindObject() == nullptr ||
       BindObject()->FuncAddString == nullptr)
    {
        return kRocProfVisDmResultNotSupported;
    }

    if(BindObject()->FuncAddString(BindObject()->trace_object, "") == INVALID_INDEX)
    {
        return kRocProfVisDmResultAllocFailure;
    }

    string_catalog_import_t import = { this, node,
                                       rocprofvis_db_string_type_t::kRPVStringTypeNameOrCategory,
                                       false };
    ph_result_t visited = ph_visit_text(m_ph_trace, PH_TEXT_KIND_STRING, ImportCatalogText,
                                        &import);
    if(visited != PH_RESULT_SUCCESS || import.failed)
    {
        return kRocProfVisDmResultDbAccessFailed;
    }

    import.type = rocprofvis_db_string_type_t::kRPVStringTypeKernelSymbol;
    visited     = ph_visit_text(m_ph_trace, PH_TEXT_KIND_KERNEL_SYMBOL, ImportCatalogText,
                                &import);
    if(visited != PH_RESULT_SUCCESS || import.failed)
    {
        return kRocProfVisDmResultDbAccessFailed;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t
ProfilerHub::RemapStringId(uint64_t id, rocprofvis_db_string_type_t type, uint32_t node,
                           uint64_t& result)
{
    rocprofvis_db_string_id_t key = { id, node, type };
    string_index_map_t::const_iterator found = m_string_index_map.find(key);
    if(found == m_string_index_map.end())
    {
        return kRocProfVisDmResultNotLoaded;
    }
    result = found->second;
    return kRocProfVisDmResultSuccess;
}

void
ProfilerHub::GetTrackIdentifierIndices(
    int column_index, char** azColName,
    rocprofvis_db_track_descriptor_t& track_ids_indices)
{
    std::string column_name = azColName[column_index];

    if(column_name == Builder::NODE_ID_SERVICE_NAME)
    {
        track_ids_indices.nid_index = column_index;
    }
    else if(column_name == Builder::AGENT_ID_SERVICE_NAME)
    {
        track_ids_indices.process_index = column_index;
    }
    else if(column_name == Builder::QUEUE_ID_SERVICE_NAME)
    {
        track_ids_indices.sub_process_index = column_index;
    }
    else if(column_name == Builder::STREAM_ID_SERVICE_NAME)
    {
        track_ids_indices.stream_index = column_index;
    }
    else if(column_name == Builder::PROCESS_ID_SERVICE_NAME)
    {
        track_ids_indices.process_index = column_index;
    }
    else if(column_name == Builder::THREAD_ID_SERVICE_NAME)
    {
        track_ids_indices.sub_process_index = column_index;
    }
    else if(column_name == Builder::COUNTER_ID_SERVICE_NAME)
    {
        track_ids_indices.is_pmc_identifier = true;
        track_ids_indices.sub_process_index = column_index;
    }
    else if(column_name == Builder::COUNTER_NAME_SERVICE_NAME)
    {
        track_ids_indices.is_pmc_identifier = true;
        track_ids_indices.is_rocpd_pmc      = true;
        track_ids_indices.sub_process_index = column_index;
    }
    else if(column_name == Builder::PROCESS_ID_PUBLIC_NAME)
    {
        track_ids_indices.pid_index = column_index;
    }
}

bool
ProfilerHub::FindTrack(rocprofvis_dm_track_category_t category, uint64_t id_process,
                       uint64_t id_subprocess, uint32_t db_instance, uint32_t& out_track)
{
    return TrackTracker()->FindTrack(category, id_process, id_subprocess, db_instance,
                                     out_track);
}


}  // namespace DataModel
}  // namespace RocProfVis

#endif  // ROCPROFVIS_PROFILER_HUB_ENABLED
