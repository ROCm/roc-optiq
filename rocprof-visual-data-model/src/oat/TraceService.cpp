
#include "TraceService.hpp"

#include "../database/sqlite/rocpd/RocpdDatabase.h"
#include "../database/DbInterface.h"

#include <sstream>
#include <iostream>

int g_progress=0;
char* g_action;

void progressfn(const int progress, const char* action)
{
    g_progress = progress;
    g_action = (char*)action;
}

oatpp::Object<TraceOpenDto> TraceService::openTrace(const oatpp::String& path)
{
    std::chrono::duration<double> diff;
    TraceHandler trace = create_trace();
    if (trace != nullptr)
    {
        DatabaseHandler db = open_rocpd_database(path.get()->c_str());
        if (db != nullptr)
        {
            if (bind_trace_to_database(trace, db))
            {
                auto t0 = std::chrono::steady_clock::now();
                if (read_trace_properties_async(db, progressfn))
                {
                    time = std::chrono::steady_clock::now();
                    diff = time - t0;
                } else
                {
                    destroy_trace(trace);
                    OATPP_ASSERT_HTTP(false, Status::CODE_500, "Failed to read trace properties");
                }
            } else{
                destroy_trace(trace);
                OATPP_ASSERT_HTTP(false, Status::CODE_500, "Failed to bind trace to database");
            }
        } else{
            destroy_trace(trace);
            OATPP_ASSERT_HTTP(false, Status::CODE_404, "Database not found");
        }
    } else
    {
        OATPP_ASSERT_HTTP(false, Status::CODE_500, "Failed to create trace");
    }
    auto traceDto = TraceOpenDto::createShared();
    traceDto->timeSpent = diff.count();
    traceDto->traceHandler = (unsigned long long)trace;
    m_traces.push_back((unsigned long long)trace);
    return traceDto;
}

oatpp::Object<ProgressDto> TraceService::getProgress(const oatpp::UInt64& traceHandler)
{
    OATPP_ASSERT_HTTP(std::find(m_traces.begin(), m_traces.end(), traceHandler) != m_traces.end(), Status::CODE_404, "Trace not found");
    DatabaseHandler db = get_database_handler(*(TraceHandler*)traceHandler.get());
    OATPP_ASSERT_HTTP(db!=nullptr, Status::CODE_404, "Database not found");
    auto progressInfo = ProgressDto::createShared();
    progressInfo->completed = !is_database_read_in_progress(db);
    auto t = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = t - time;
    progressInfo->timeSpent = diff.count();
    time = t;
    progressInfo->progress = g_progress;
    progressInfo->message = g_action;
                   
    return progressInfo;
}

oatpp::Object<TraceInfoDto> TraceService::readTraceInfo(const oatpp::UInt64& traceHandler)
{
    OATPP_ASSERT_HTTP(std::find(m_traces.begin(), m_traces.end(), traceHandler) != m_traces.end(), Status::CODE_404, "Trace not found");
    auto traceInfo = TraceInfoDto::createShared();

    traceInfo->numTracks = get_number_of_tracks(*(TraceHandler*)traceHandler.get());
    traceInfo->minTime = get_trace_min_time(*(TraceHandler*)traceHandler.get());
    traceInfo->maxTime = get_trace_max_time(*(TraceHandler*)traceHandler.get());
                   
    return traceInfo;
}

oatpp::Object<StatusDto> TraceService::readTraceChunk(const oatpp::Object<ReadTraceDto>& dto) {

    OATPP_ASSERT_HTTP(std::find(m_traces.begin(), m_traces.end(), dto->traceHandler) != m_traces.end(), Status::CODE_404, "Trace not found");
    DatabaseHandler db = get_database_handler(*(TraceHandler*)dto->traceHandler.get());
    OATPP_ASSERT_HTTP(db!=nullptr, Status::CODE_404, "Database not found");
    configure_trace_read_time_period(db, dto->startTime, dto->endTime);

    for (int i=0; i < dto->trackIds->size(); i++)
        add_track_to_trace_read_config(db, dto->trackIds[i]);
    auto t0 = std::chrono::steady_clock::now();
    if (dto->readOptions == DtoReadOptions::READ_ALL_TRACKS)
        read_trace_chunk_all_tracks_async(db,  progressfn); else
            read_trace_chunk_track_by_track_async(db, progressfn);
    auto t1 = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = t1 - t0;
    auto status = StatusDto::createShared();
    status->timeSpent = diff.count();
    status->message = "Trace chunk read started";
    return status;
}

oatpp::Object<PageDto<oatpp::Object<TrackInfoDto>>> TraceService::getAllTracks(const oatpp::UInt64& traceHandler)
{
    auto page = PageDto<oatpp::Object<TrackInfoDto>>::createShared();

    OATPP_ASSERT_HTTP(std::find(m_traces.begin(), m_traces.end(), traceHandler) != m_traces.end(), Status::CODE_404, "Trace not found");
    auto t0 = std::chrono::steady_clock::now();
    page->count = get_number_of_tracks(*(TraceHandler*)traceHandler.get());
    page->items = {};
    for (int i = 0; i < page->count; i++)
    {
        auto trackInfo = TrackInfoDto::createShared();
        TrackHandler track = get_track_at(*(TraceHandler*)traceHandler.get(),i);
        trackInfo->trackHandler = (unsigned long long)track;
        trackInfo->groupName = get_track_group_name(track);
        trackInfo->trackName = get_track_name(track);
        if (is_data_type_kernel_launch(track))
            trackInfo->trackType =  DtoTrackType::CPU_TRACK; else
        if (is_data_type_kernel_execute(track))
            trackInfo->trackType =  DtoTrackType::GPU_TRACK; else
        if (is_data_type_metric(track))
            trackInfo->trackType =  DtoTrackType::METRIC_TRACK; else
            continue;
        page->items->push_back(trackInfo);
    }
    auto t1 = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = t1 - t0;
    page->timeSpent = diff.count();
    return page;
}


oatpp::Object<PageDto<oatpp::Object<EventTrackDataDto>>> TraceService::getEventTrackLatestData(const oatpp::UInt64& trackHandler)
{
    OATPP_ASSERT_HTTP(std::find_if(m_traces.begin(), m_traces.end(), [trackHandler](unsigned long long traceHandler) { return validate_track_handler_for_trace((TraceHandler)traceHandler, *(TrackHandler*)trackHandler.get()); }) != m_traces.end(), Status::CODE_404, "Track not found");
    OATPP_ASSERT_HTTP(is_data_type_kernel_execute(*(TrackHandler*)trackHandler.get()) || is_data_type_kernel_launch(*(TrackHandler*)trackHandler.get()), Status::CODE_404, "Not event track");
    auto t0 = std::chrono::steady_clock::now();
    auto page = PageDto<oatpp::Object<EventTrackDataDto>>::createShared();
    RecordArrayHandler  chunk = get_track_last_acquaired_chunk(*(TrackHandler*)trackHandler.get());
    OATPP_ASSERT_HTTP(chunk!=nullptr, Status::CODE_404, "Data not found");
    page->count = get_chunk_number_of_records(chunk);
    page->items = {};
    for (int i = 0; i < page->count; i++)
    {
        auto data = EventTrackDataDto::createShared();
        data->id = get_kernel_id_at(chunk,i);
        data->timestamp = get_timestamp_at(chunk,i);
        data->duration = get_kernel_duration_at(chunk,i);
        data->eventType = get_kernel_type_at(chunk,i);
        data->eventDescription = get_kernel_description_at(chunk,i);
        page->items->push_back(data);
    }
    auto t1 = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = t1 - t0;
    page->timeSpent = diff.count();
    return page;
}

oatpp::Object<EventTrackArrayDto> TraceService::getEventTrackLatestDataArray(const oatpp::UInt64& trackHandler)
{
    OATPP_ASSERT_HTTP(std::find_if(m_traces.begin(), m_traces.end(), [trackHandler](unsigned long long traceHandler) { return validate_track_handler_for_trace((TraceHandler)traceHandler, *(TrackHandler*)trackHandler.get()); }) != m_traces.end(), Status::CODE_404, "Track not found");
    OATPP_ASSERT_HTTP(is_data_type_kernel_execute(*(TrackHandler*)trackHandler.get()) || is_data_type_kernel_launch(*(TrackHandler*)trackHandler.get()), Status::CODE_404, "Not event track");
    auto t0 = std::chrono::steady_clock::now();
    RecordArrayHandler  chunk = get_track_last_acquaired_chunk(*(TrackHandler*)trackHandler.get());
    OATPP_ASSERT_HTTP(chunk!=nullptr, Status::CODE_404, "Data not found");
    size_t count = get_chunk_number_of_records(chunk);
    auto data = EventTrackArrayDto::createShared();
    data->ids = {};
    data->timestamps = {};
    data->durations = {};
    data->eventTypes = {};
    data->eventDescriptions = {};
    for (int i = 0; i < count; i++)
    {
        data->ids->push_back(get_kernel_id_at(chunk,i));
        data->timestamps->push_back(get_timestamp_at(chunk,i));
        data->durations->push_back(get_kernel_duration_at(chunk,i));
        data->eventTypes->push_back(get_kernel_type_at(chunk, i));
        data->eventDescriptions->push_back(get_kernel_description_at(chunk, i));
    }
    auto t1 = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = t1 - t0;
    data->timeSpent = diff.count();
    return data;
}

oatpp::Object<PageDto<oatpp::Object<MetricTrackDataDto>>> TraceService::getMetricTrackLatestData(const oatpp::UInt64& trackHandler)
{
    OATPP_ASSERT_HTTP(std::find_if(m_traces.begin(), m_traces.end(), [trackHandler](unsigned long long traceHandler) { return validate_track_handler_for_trace((TraceHandler)traceHandler, *(TrackHandler*)trackHandler.get()); }) != m_traces.end(), Status::CODE_404, "Track not found");
    OATPP_ASSERT_HTTP(is_data_type_metric(*(TrackHandler*)trackHandler.get()), Status::CODE_404, "Not metric track");
    auto t0 = std::chrono::steady_clock::now();
    auto page = PageDto<oatpp::Object<MetricTrackDataDto>>::createShared();
    RecordArrayHandler  chunk = get_track_last_acquaired_chunk(*(TrackHandler*)trackHandler.get());
    OATPP_ASSERT_HTTP(chunk!=nullptr, Status::CODE_404, "Data not found");
    page->count = get_chunk_number_of_records(chunk);
    page->items = {};
    for (int i = 0; i < page->count; i++)
    {
        auto data = MetricTrackDataDto::createShared();
        data->timestamp = get_timestamp_at(chunk,i);
        data->value = get_metric_value_at(chunk,i);
        page->items->push_back(data);
    }
    auto t1 = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = t1 - t0;
    page->timeSpent = diff.count();
    return page;
}

oatpp::Object<MetricTrackArrayDto> TraceService::getMetricTrackLatestDataArray(const oatpp::UInt64& trackHandler)
{
    OATPP_ASSERT_HTTP(std::find_if(m_traces.begin(), m_traces.end(), [trackHandler](unsigned long long traceHandler) { return validate_track_handler_for_trace((TraceHandler)traceHandler, *(TrackHandler*)trackHandler.get()); }) != m_traces.end(), Status::CODE_404, "Track not found");
    OATPP_ASSERT_HTTP(is_data_type_metric(*(TrackHandler*)trackHandler.get()), Status::CODE_404, "Not metric track");
    auto t0 = std::chrono::steady_clock::now();
    RecordArrayHandler  chunk = get_track_last_acquaired_chunk(*(TrackHandler*)trackHandler.get());
    OATPP_ASSERT_HTTP(chunk!=nullptr, Status::CODE_404, "Data not found");
    size_t count = get_chunk_number_of_records(chunk);
    auto data = MetricTrackArrayDto::createShared();
    data->timestamps = {};
    data->values = {};
    for (int i = 0; i < count; i++)
    {     
        data->timestamps->push_back(get_timestamp_at(chunk,i));
        data->values->push_back(get_metric_value_at(chunk,i));
    }
    auto t1 = std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = t1 - t0;
    data->timeSpent = diff.count();
    return data;
}


oatpp::Object<StatusDto> TraceService::deleteTraceChunksAtTime(const oatpp::UInt64& traceHandler, const oatpp::UInt64& time) {
  OATPP_ASSERT_HTTP(std::find(m_traces.begin(), m_traces.end(), traceHandler) != m_traces.end(), Status::CODE_404, "Trace not found");
  delete_trace_chunks_at(*(TraceHandler*)traceHandler.get(), time);
  auto status = StatusDto::createShared();
  status->message = "Chunks were successfully deleted";
  return status;
}

oatpp::Object<StatusDto> TraceService::deleteTrace(const oatpp::UInt64& traceHandler) {
  OATPP_ASSERT_HTTP(std::find(m_traces.begin(), m_traces.end(), traceHandler) != m_traces.end(), Status::CODE_404, "Trace not found");
  destroy_trace(*(TraceHandler*)traceHandler.get());
  auto status = StatusDto::createShared();
  status->message = "Trace was successfully deleted";
  return status;
}