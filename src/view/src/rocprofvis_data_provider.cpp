#include "rocprofvis_data_provider.h"
#include "rocprofvis_controller.h"

#include "spdlog/spdlog.h"
#include <cassert>

using namespace RocProfVis::View;

DataProvider::DataProvider()
: m_state(ProviderState::kInit)
, m_trace_future(nullptr)
, m_trace_controller(nullptr)
, m_trace_timeline(nullptr)
{}

DataProvider::~DataProvider() { CloseController(); }

void
DataProvider::CloseController()
{
    if(m_trace_controller)
    {
        rocprofvis_controller_free(m_trace_controller);
        m_trace_controller = nullptr;
    }
    if(m_trace_future)
    {
        rocprofvis_controller_future_free(m_trace_future);
        m_trace_future = nullptr;
    }
    if(m_trace_timeline)
    {
        m_trace_timeline = nullptr;
    }

    FreeAllTracks();
    FreeRequests();
    m_state      = ProviderState::kInit;
    m_num_graphs = 0;
    m_min_ts     = 0;
    m_max_ts     = 0;
}

void
DataProvider::FreeRequests()
{
    for(auto item : m_requests)
    {
        data_req_info_t& req = item.second;
        if(req.graph_array)
        {
            rocprofvis_controller_array_free(req.graph_array);
            req.graph_array = nullptr;
        }
        if(req.graph_future)
        {
            rocprofvis_controller_future_free(req.graph_future);
            req.graph_future = nullptr;
        }
        if(req.graph_obj)
        {
            req.graph_obj = nullptr;
        }
    }

    m_requests.clear();
}

void
DataProvider::FreeAllTracks()
{
    for(RawTrackData* item : m_raw_trackdata)
    {
        if(item)
        {
            delete item;
        }
    }
    m_raw_trackdata.clear();
}

double
DataProvider::GetStartTime()
{
    return m_min_ts;
}

double
DataProvider::GetEndTime()
{
    return m_max_ts;
}

uint64_t
DataProvider::GetTrackCount()
{
    return m_num_graphs;
}

const std::string&
DataProvider::GetTraceFilePath()
{
    return m_trace_file_path;
}

ProviderState DataProvider::GetState() {
    return m_state;
}

bool
DataProvider::FetchTrace(const std::string& file_path)
{
    if(m_state == ProviderState::kLoading || m_state == ProviderState::kError)
    {
        spdlog::debug("Cannot fetch, provider busy or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    // free any previously acquired resources
    CloseController();

    m_trace_controller = rocprofvis_controller_alloc();
    if(m_trace_controller)
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        m_trace_future             = rocprofvis_controller_future_alloc();
        if(m_trace_future)
        {
            result = rocprofvis_controller_load_async(m_trace_controller,
                                                      file_path.c_str(), m_trace_future);
            assert(result == kRocProfVisResultSuccess);

            if(result != kRocProfVisResultSuccess)
            {
                rocprofvis_controller_future_free(m_trace_future);
                m_trace_future = nullptr;
            }
        }
        if(result != kRocProfVisResultSuccess)
        {
            rocprofvis_controller_free(m_trace_controller);
            m_trace_controller = nullptr;
            return false;
        }

        m_state = ProviderState::kLoading;
        spdlog::debug("Provider load started, state: {}", static_cast<int>(m_state));
    }

    return true;
}

void
DataProvider::Update()
{
    // process initial controller loading state
    if(m_state == ProviderState::kLoading)
    {
        HandleLoadTrace();
    }
    else if(m_state == ProviderState::kReady)
    {
        HandleLoadGraphs();
    }
}

void
DataProvider::HandleLoadTrace()
{
    if(m_trace_future)
    {
        rocprofvis_result_t result = rocprofvis_controller_future_wait(m_trace_future, 0);
        assert(result == kRocProfVisResultSuccess || result == kRocProfVisResultTimeout);
        if(result == kRocProfVisResultSuccess)
        {
            uint64_t uint64_result = 0;
            result                 = rocprofvis_controller_get_uint64(
                m_trace_future, kRPVControllerFutureResult, 0, &uint64_result);
            assert(result == kRocProfVisResultSuccess &&
                   uint64_result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_get_object(
                m_trace_controller, kRPVControllerTimeline, 0, &m_trace_timeline);

            // store timeline meta data
            if(result == kRocProfVisResultSuccess && m_trace_timeline)
            {
                m_num_graphs = 0;
                result       = rocprofvis_controller_get_uint64(
                    m_trace_timeline, kRPVControllerTimelineNumGraphs, 0, &m_num_graphs);

                m_min_ts = 0;
                result   = rocprofvis_controller_get_double(
                    m_trace_timeline, kRPVControllerTimelineMinTimestamp, 0, &m_min_ts);

                m_max_ts = 0;
                result   = rocprofvis_controller_get_double(
                    m_trace_timeline, kRPVControllerTimelineMaxTimestamp, 0, &m_max_ts);

                spdlog::debug("Timeline parameters: tracks {} min ts {} max ts {}",
                              m_num_graphs, m_min_ts, m_max_ts);

                // get the per track meta data
                HandleLoadTrackMetaData();
            }

            // trace loaded successfully, free the future pointer
            rocprofvis_controller_future_free(m_trace_future);
            m_trace_future = nullptr;

            m_state = ProviderState::kReady;
        }
        else
        {
            // timed out, do nothing and try again later
        }
    }
}

void
DataProvider::HandleLoadTrackMetaData()
{
    m_track_metadata.clear();
    FreeAllTracks();

    for(uint64_t i = 0; i < m_num_graphs; i++)
    {
        rocprofvis_handle_t* graph  = nullptr;
        rocprofvis_result_t  result = rocprofvis_controller_get_object(
            m_trace_timeline, kRPVControllerTimelineGraphIndexed, i, &graph);
        assert(result == kRocProfVisResultSuccess && graph);

        rocprofvis_handle_t* track = nullptr;
        result =
            rocprofvis_controller_get_object(graph, kRPVControllerGraphTrack, 0, &track);
        assert(result == kRocProfVisResultSuccess && track);

        if(result == kRocProfVisResultSuccess)
        {
            track_info_t track_info;

            track_info.index = i;
            result = rocprofvis_controller_get_uint64(track, kRPVControllerTrackId, 0,
                                                      &track_info.id);
            assert(result == kRocProfVisResultSuccess);

            uint64_t track_type = 0;
            result = rocprofvis_controller_get_uint64(track, kRPVControllerTrackType, 0,
                                                      &track_type);
            assert(result == kRocProfVisResultSuccess &&
                   (track_type == kRPVControllerTrackTypeEvents ||
                    track_type == kRPVControllerTrackTypeSamples));
            track_info.track_type = track_type == kRPVControllerTrackTypeSamples
                                        ? kRPVControllerTrackTypeSamples
                                        : kRPVControllerTrackTypeEvents;

            // get track name
            uint32_t length = 0;
            result = rocprofvis_controller_get_string(track, kRPVControllerTrackName, 0,
                                                      nullptr, &length);
            assert(result == kRocProfVisResultSuccess);

            char* buffer = (char*) alloca(length + 1);
            length += 1;
            result = rocprofvis_controller_get_string(track, kRPVControllerTrackName, 0,
                                                      buffer, &length);
            assert(result == kRocProfVisResultSuccess);
            track_info.name = std::string(buffer);

            result = rocprofvis_controller_get_double(
                track, kRPVControllerTrackMinTimestamp, 0, &track_info.min_ts);
            assert(result == kRocProfVisResultSuccess);

            result = rocprofvis_controller_get_double(
                track, kRPVControllerTrackMaxTimestamp, 0, &track_info.max_ts);
            assert(result == kRocProfVisResultSuccess);

            // Todo:
            // kRPVControllerTrackDescription = 0x30000007,
            // kRPVControllerTrackMinValue = 0x30000008,
            // kRPVControllerTrackMaxValue = 0x30000009,
            // kRPVControllerTrackNode = 0x3000000A,
            // kRPVControllerTrackProcessor = 0x3000000B,

            m_track_metadata.push_back(track_info);
            m_raw_trackdata.push_back(nullptr);
        }
        else
        {
            spdlog::debug("Error getting track meta data for track at index: {}", i);
        }
    }

    spdlog::info("Track meta data loaded");
}

bool
DataProvider::FetchTrack(uint64_t index, double start_ts, double end_ts,
                         int horz_pixel_range, int lod)
{
    if(m_state != ProviderState::kReady)
    {
        spdlog::debug("Cannot fetch, provider not ready or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }

    if(index < m_track_metadata.size())
    {
        auto it = m_requests.find(index);
        // only allow load if a request for this index (track) is not pending
        if(it == m_requests.end())
        {
            rocprofvis_handle_t* graph_future = rocprofvis_controller_future_alloc();
            rocprofvis_controller_array_t* graph_array =
                rocprofvis_controller_array_alloc(32);
            rocprofvis_handle_t* graph_obj = nullptr;

            rocprofvis_result_t result = rocprofvis_controller_get_object(
                m_trace_timeline, kRPVControllerTimelineGraphIndexed, index, &graph_obj);

            if(result == kRocProfVisResultSuccess && graph_obj && graph_future &&
               graph_array)
            {
                result = rocprofvis_controller_graph_fetch_async(
                    m_trace_controller, graph_obj, start_ts, end_ts, horz_pixel_range,
                    graph_future, graph_array);

                assert(result == kRocProfVisResultSuccess);
            }

            data_req_info_t request_info;
            request_info.graph_array   = graph_array;
            request_info.graph_future  = graph_future;
            request_info.graph_obj     = graph_obj;
            request_info.index         = index;
            request_info.loading_state = ProviderState::kLoading;

            m_requests.emplace(request_info.index, request_info);

            spdlog::debug("Fetching track data {}", index);
            return true;
        }
        else
        {
            // request for item already exists
            spdlog::debug("Request for this track, index {}, is already pending", index);
            return false;
        }
    }
    else
    {
        spdlog::debug("Cannot fetch Track index {} is out of range", index);
        return false;
    }
}

const RawTrackData*
DataProvider::GetRawTrackData(uint64_t index)
{
    return m_raw_trackdata[index];
}

const track_info_t*
DataProvider::GetTrackInfo(uint64_t index)
{
    if(index < m_track_metadata.size())
    {
        return &m_track_metadata[index];
    }
    return nullptr;
}

bool
DataProvider::FreeTrack(uint64_t index)
{
    if(index < m_raw_trackdata.size())
    {
        if(m_raw_trackdata[index])
        {
            delete m_raw_trackdata[index];
            m_raw_trackdata[index] = nullptr;
            spdlog::debug("Deleted track data at index: {}", index);
            return true;
        }
        else
        {
            spdlog::debug("No track data to delete at index: {}", index);
        }
    }
    else
    {
        spdlog::debug("Cannot delete track data, index out range at index: {}", index);
    }
    return false;
}

void
DataProvider::DumpMetaData()
{
    for(track_info_t& track_info : m_track_metadata)
    {
        spdlog::debug("Track index {}, id {}, name {}, min ts {}, max ts {}, type {}",
                      track_info.index, track_info.id, track_info.name, track_info.min_ts,
                      track_info.max_ts, static_cast<int>(track_info.track_type));
    }
}

bool
DataProvider::DumpTrack(uint64_t index)
{
    if(index < m_raw_trackdata.size())
    {
        if(m_raw_trackdata[index])
        {
            bool result = false;
            switch(m_raw_trackdata[index]->GetType())
            {
                case kRPVControllerTrackTypeSamples:
                {
                    RawTrackSampleData* track =
                        dynamic_cast<RawTrackSampleData*>(m_raw_trackdata[index]);
                    if(track)
                    {
                        const std::vector<rocprofvis_trace_counter_t>& buffer =
                            track->GetData();
                        int64_t i = 0;
                        for(const auto item : buffer)
                        {
                            spdlog::debug("{}, start_ts {}, value {}", i, item.m_start_ts,
                                          item.m_value);
                            ++i;
                        }
                        result = true;
                    }
                    else
                    {
                        spdlog::debug("Cannot dump track data, Type Error");
                    }
                    break;
                }
                case kRPVControllerTrackTypeEvents:
                {
                    RawTrackEventData* track =
                        dynamic_cast<RawTrackEventData*>(m_raw_trackdata[index]);
                    if(track)
                    {
                        const std::vector<rocprofvis_trace_event_t>& buffer =
                            track->GetData();
                        int64_t i = 0;
                        for(const auto item : buffer)
                        {
                            spdlog::debug(
                                "{}, name: {}, start_ts {}, duration {}, end_ts {}", i,
                                item.m_name, item.m_start_ts, item.m_duration,
                                item.m_duration + item.m_start_ts);
                            ++i;
                        }
                        result = true;
                    }
                    else
                    {
                        spdlog::debug("Cannot dump track data, Type Error");
                    }
                    break;
                }
                default:
                {
                    spdlog::debug("Cannot dump track data, unknown type");
                    break;
                }
            }
            return result;
        }
        else
        {
            spdlog::debug("No track data at index: {}", index);
        }
    }
    else
    {
        spdlog::debug("Cannot show track data, index out range at index: {}", index);
    }

    return false;
}

void
DataProvider::HandleLoadGraphs()
{
    if(m_requests.size() > 0)
    {
        for(auto it = m_requests.begin(); it != m_requests.end();)
        {
            data_req_info_t& req = it->second;

            rocprofvis_result_t result =
                rocprofvis_controller_future_wait(req.graph_future, 0);
            assert(result == kRocProfVisResultSuccess ||
                   result == kRocProfVisResultTimeout);

            // this graph is ready
            if(result == kRocProfVisResultSuccess)
            {
                rocprofvis_controller_future_free(req.graph_future);
                req.graph_future  = nullptr;
                req.loading_state = ProviderState::kReady;
                ProcessRequest(req);
                // remove request from processing container
                it = m_requests.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

void
DataProvider::ProcessRequest(data_req_info_t& req)
{
    spdlog::debug("Processing track data {}", req.index);

    uint64_t graph_type = 0;
    auto     graph      = req.graph_obj;

    rocprofvis_result_t result =
        rocprofvis_controller_get_uint64(graph, kRPVControllerGraphType, 0, &graph_type);
    assert(result == kRocProfVisResultSuccess);

    double min_ts = 0;
    result = rocprofvis_controller_get_double(graph, kRPVControllerGraphStartTimestamp, 0,
                                              &min_ts);
    assert(result == kRocProfVisResultSuccess);

    double max_ts = 0;
    result = rocprofvis_controller_get_double(graph, kRPVControllerGraphEndTimestamp, 0,
                                              &max_ts);
    assert(result == kRocProfVisResultSuccess);

    uint64_t item_count = 0;
    result = rocprofvis_controller_get_uint64(graph, kRPVControllerGraphNumEntries, 0,
                                              &item_count);
    assert(result == kRocProfVisResultSuccess);

    spdlog::debug("{} Graph item count {} min ts {} max ts {}", req.index, item_count,
                  min_ts, max_ts);

    // use the track type to determine what type of data is present in the graph array
    assert(req.index < m_track_metadata.size());
    switch(m_track_metadata[req.index].track_type)
    {
        case kRPVControllerTrackTypeEvents:
        {
            CreateRawEventData(req.index, req.graph_array, min_ts, max_ts);
            break;
        }
        case kRPVControllerTrackTypeSamples:
        {
            CreateRawSampleData(req.index, req.graph_array, min_ts, max_ts);
            break;
        }
        default:
        {
            break;
        }
    }

    // free the array
    if(req.graph_array)
    {
        rocprofvis_controller_array_free(req.graph_array);
        req.graph_array = nullptr;
    }
}

void
DataProvider::CreateRawSampleData(uint64_t                       index,
                                  rocprofvis_controller_array_t* track_data,
                                  double min_ts, double max_ts)
{
    uint64_t            count  = 0;
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(
        track_data, kRPVControllerArrayNumEntries, 0, &count);
    assert(result == kRocProfVisResultSuccess);

    RawTrackSampleData* raw_sample_data = new RawTrackSampleData(index, min_ts, max_ts);
    spdlog::debug("Create sample track data at index {} with {} entries", index, count);

    if(m_raw_trackdata[index])
    {
        delete m_raw_trackdata[index];
        m_raw_trackdata[index] = nullptr;
        spdlog::debug("replacing existing track data at index {}", index);
    }
    m_raw_trackdata[index] = raw_sample_data;

    std::vector<rocprofvis_trace_counter_t> buffer;
    buffer.reserve(count);

    rocprofvis_trace_counter_t trace_counter;

    for(uint64_t i = 0; i < count; i++)
    {
        rocprofvis_controller_sample_t* sample = nullptr;
        result                                 = rocprofvis_controller_get_object(
            track_data, kRPVControllerArrayEntryIndexed, i, &sample);
        assert(result == kRocProfVisResultSuccess && sample);

        double start_ts = 0;
        result = rocprofvis_controller_get_double(sample, kRPVControllerSampleTimestamp,
                                                  0, &start_ts);
        assert(result == kRocProfVisResultSuccess);

        double value = 0;
        result = rocprofvis_controller_get_double(sample, kRPVControllerSampleValue, 0,
                                                  &value);
        assert(result == kRocProfVisResultSuccess);

        trace_counter.m_start_ts = start_ts;
        trace_counter.m_value    = value;

        buffer.push_back(trace_counter);
    }

    raw_sample_data->SetData(std::move(buffer));
    m_raw_trackdata[index] = raw_sample_data;
}

void
DataProvider::CreateRawEventData(uint64_t                       index,
                                 rocprofvis_controller_array_t* track_data, double min_ts,
                                 double max_ts)
{
    uint64_t            count  = 0;
    rocprofvis_result_t result = rocprofvis_controller_get_uint64(
        track_data, kRPVControllerArrayNumEntries, 0, &count);
    assert(result == kRocProfVisResultSuccess);

    RawTrackEventData* raw_event_data = new RawTrackEventData(index, min_ts, max_ts);
    spdlog::debug("Create event track data at index {} with {} entries", index, count);

    if(m_raw_trackdata[index])
    {
        delete m_raw_trackdata[index];
        m_raw_trackdata[index] = nullptr;
        spdlog::debug("replacing existing track data at index {}", index);
    }

    std::vector<rocprofvis_trace_event_t> buffer;
    buffer.reserve(count);

    rocprofvis_trace_event_t trace_event;

    for(uint64_t i = 0; i < count; i++)
    {
        rocprofvis_controller_event_t* event = nullptr;
        result                               = rocprofvis_controller_get_object(
            track_data, kRPVControllerArrayEntryIndexed, i, &event);
        assert(result == kRocProfVisResultSuccess && event);

        double start_ts = 0;
        result          = rocprofvis_controller_get_double(
            event, kRPVControllerEventStartTimestamp, 0, &start_ts);
        assert(result == kRocProfVisResultSuccess);
        trace_event.m_start_ts = start_ts;

        double end_ts = 0;
        result = rocprofvis_controller_get_double(event, kRPVControllerEventEndTimestamp,
                                                  0, &end_ts);
        assert(result == kRocProfVisResultSuccess);
        trace_event.m_duration = end_ts - start_ts;

        // get event name
        uint32_t length = 0;
        result = rocprofvis_controller_get_string(event, kRPVControllerEventName, 0,
                                                  nullptr, &length);
        assert(result == kRocProfVisResultSuccess);

        char* str_buffer = (char*) alloca(length + 1);
        length += 1;
        result = rocprofvis_controller_get_string(event, kRPVControllerEventName, 0,
                                                  str_buffer, &length);
        assert(result == kRocProfVisResultSuccess);
        trace_event.m_name = std::string(str_buffer);

        buffer.push_back(trace_event);
    }

    raw_event_data->SetData(std::move(buffer));
    m_raw_trackdata[index] = raw_event_data;
}