#include "rocprofvis_controller_api.h"

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "system/rocprofvis_controller_summary_metrics.h"

#include <cfloat>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace Controller
{
namespace
{
Result ConvertResult(rocprofvis_result_t result)
{
    switch(result)
    {
        case kRocProfVisResultSuccess:
            return Result::Success;
        case kRocProfVisResultTimeout:
            return Result::Timeout;
        case kRocProfVisResultNotLoaded:
            return Result::NotLoaded;
        case kRocProfVisResultInvalidArgument:
        case kRocProfVisResultInvalidType:
        case kRocProfVisResultOutOfRange:
            return Result::InvalidArgument;
        case kRocProfVisResultNotSupported:
            return Result::NotSupported;
        case kRocProfVisResultCancelled:
            return Result::Cancelled;
        case kRocProfVisResultMemoryAllocError:
            return Result::OutOfMemory;
        default:
            return Result::UnknownError;
    }
}

std::string ReadString(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                       uint64_t index)
{
    uint32_t length = 0;
    if(rocprofvis_controller_get_string(handle, property, index, nullptr, &length) !=
       kRocProfVisResultSuccess)
    {
        return std::string();
    }

    std::vector<char> buffer(length + 1, 0);
    if(rocprofvis_controller_get_string(handle, property, index, buffer.data(), &length) !=
       kRocProfVisResultSuccess)
    {
        return std::string();
    }

    return std::string(buffer.data());
}

uint64_t ReadUInt64(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                    uint64_t index = 0)
{
    uint64_t value = 0;
    rocprofvis_controller_get_uint64(handle, property, index, &value);
    return value;
}

double ReadDouble(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                  uint64_t index = 0)
{
    double value = 0.0;
    rocprofvis_controller_get_double(handle, property, index, &value);
    return value;
}

class LegacyControllerAdapter;

class LegacyTrackAdapter : public ITrack
{
public:
    LegacyTrackAdapter(LegacyControllerAdapter* owner, rocprofvis_controller_track_t* track_handle,
                       rocprofvis_controller_graph_t* graph_handle,
                       const ApiStringTable* string_table);

    uint64_t GetId() const override { return m_id; }
    uint32_t GetCategoryIndex() const override { return m_category_index; }
    uint32_t GetMainNameIndex() const override { return m_main_name_index; }
    uint32_t GetSubNameIndex() const override { return m_sub_name_index; }
    Type GetType() const override { return m_type; }
    double GetMinTimestamp() const override { return m_min_timestamp; }
    double GetMaxTimestamp() const override { return m_max_timestamp; }
    uint32_t GetEntryCount() const override { return m_entry_count; }
    uint64_t GetOwnerProcessOrAgentId() const override { return m_owner_process_or_agent_id; }
    uint64_t GetOwnerQueueOrThreadId() const override { return m_owner_queue_or_thread_id; }
    double GetMinValue() const override { return m_min_value; }
    double GetMaxValue() const override { return m_max_value; }
    TrackTopologyInfo GetTopology() const override { return m_topology; }

    Result BeginFetch(double start_time, double end_time,
                      RequestHandle* out_request) override;
    Result BeginFetchLod(uint32_t pixel_range, double start_time, double end_time,
                         RequestHandle* out_request) override;

    Result GetEventResult(RequestHandle request, const EventRecord** out_events,
                          uint32_t* out_count,
                          const ApiStringTable** out_strings) override;
    Result GetSampleResult(RequestHandle request, const SampleRecord** out_samples,
                           uint32_t* out_count) override;

private:
    friend class LegacyControllerAdapter;

    void LoadMetadata(const ApiStringTable* string_table);

    LegacyControllerAdapter*        m_owner;
    rocprofvis_controller_track_t*  m_track_handle;
    rocprofvis_controller_graph_t*  m_graph_handle;
    uint64_t                        m_id;
    uint32_t                        m_category_index;
    uint32_t                        m_main_name_index;
    uint32_t                        m_sub_name_index;
    Type                            m_type;
    double                          m_min_timestamp;
    double                          m_max_timestamp;
    uint32_t                        m_entry_count;
    uint64_t                        m_owner_process_or_agent_id;
    uint64_t                        m_owner_queue_or_thread_id;
    double                          m_min_value;
    double                          m_max_value;
    TrackTopologyInfo               m_topology;
};

enum class RequestKind : uint8_t
{
    Load = 0,
    TrackFetch,
    SummaryFetch
};

struct RequestRecord
{
    RequestKind                        kind;
    rocprofvis_controller_future_t*    future;
    rocprofvis_controller_array_t*     array;
    rocprofvis_handle_t*               result_object;
    rocprofvis_controller_arguments_t* args;
    ITrack::Type                       track_type;
    rocprofvis_result_t                response_code;
    bool                               converted;
    ApiStringTable                     string_table;
    std::vector<EventRecord>           event_records;
    std::vector<SampleRecord>          sample_records;
    std::vector<KernelMetrics>         kernel_metrics;
    ControllerSummaryMetrics           summary_metrics;

    RequestRecord()
    : kind(RequestKind::Load)
    , future(nullptr)
    , array(nullptr)
    , result_object(nullptr)
    , args(nullptr)
    , track_type(ITrack::Type::Events)
    , response_code(kRocProfVisResultUnknownError)
    , converted(false)
    , summary_metrics{ -1.0f, -1.0f, 0, 0, 0.0 }
    {}
};

class LegacyControllerAdapter : public IController
{
public:
    LegacyControllerAdapter(rocprofvis_controller_t* controller, bool owns_controller,
                            const char* trace_file_path)
    : m_controller(controller)
    , m_owns_controller(owns_controller)
    , m_controller_type(kRPVControllerObjectTypeControllerSystem)
    , m_tracks_valid(false)
    , m_next_request_handle(1)
    , m_trace_file_path(trace_file_path ? trace_file_path : "")
    {
        if(m_controller)
        {
            rocprofvis_controller_get_object_type(reinterpret_cast<rocprofvis_handle_t*>(m_controller),
                                                  &m_controller_type);
        }
    }

    ~LegacyControllerAdapter() override
    {
        ReleaseAllRequests();
        if(m_owns_controller && m_controller)
        {
            rocprofvis_controller_free(m_controller);
            m_controller = nullptr;
        }
    }

    Result BeginLoad(RequestHandle* out_request) override
    {
        if(!out_request || !m_controller)
        {
            return Result::InvalidArgument;
        }

        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        if(!future)
        {
            return Result::OutOfMemory;
        }

        rocprofvis_result_t result = rocprofvis_controller_load_async(m_controller, future);
        if(result != kRocProfVisResultSuccess)
        {
            rocprofvis_controller_future_free(future);
            return ConvertResult(result);
        }

        RequestHandle handle = AllocateRequestHandle();
        RequestRecord record;
        record.kind = RequestKind::Load;
        record.future = future;
        m_requests.emplace(handle, std::move(record));
        *out_request = handle;
        return Result::Success;
    }

    Result BeginFetchSummary(double start_time, double end_time,
                             RequestHandle* out_request) override
    {
        if(!out_request || !m_controller ||
           m_controller_type != kRPVControllerObjectTypeControllerSystem)
        {
            return Result::InvalidArgument;
        }

        rocprofvis_controller_arguments_t* args = rocprofvis_controller_arguments_alloc();
        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        rocprofvis_controller_summary_metrics_t* metrics =
            rocprofvis_controller_summary_metrics_alloc();
        if(!args || !future || !metrics)
        {
            if(args)
            {
                rocprofvis_controller_arguments_free(args);
            }
            if(future)
            {
                rocprofvis_controller_future_free(future);
            }
            if(metrics)
            {
                delete reinterpret_cast<SummaryMetrics*>(metrics);
            }
            return Result::OutOfMemory;
        }

        rocprofvis_result_t result = rocprofvis_controller_set_double(
            args, kRPVControllerSummaryArgsStartTimestamp, 0, start_time);
        if(result == kRocProfVisResultSuccess)
        {
            result = rocprofvis_controller_set_double(
                args, kRPVControllerSummaryArgsEndTimestamp, 0, end_time);
        }

        rocprofvis_handle_t* summary_handle = nullptr;
        if(result == kRocProfVisResultSuccess)
        {
            result = rocprofvis_controller_get_object(
                m_controller, kRPVControllerSystemSummary, 0, &summary_handle);
        }

        if(result == kRocProfVisResultSuccess)
        {
            result = rocprofvis_controller_summary_fetch_async(
                m_controller, summary_handle, args, future, metrics);
        }

        if(result != kRocProfVisResultSuccess)
        {
            rocprofvis_controller_arguments_free(args);
            rocprofvis_controller_future_free(future);
            delete reinterpret_cast<SummaryMetrics*>(metrics);
            return ConvertResult(result);
        }

        RequestHandle handle = AllocateRequestHandle();
        RequestRecord record;
        record.kind = RequestKind::SummaryFetch;
        record.future = future;
        record.args = args;
        record.result_object = reinterpret_cast<rocprofvis_handle_t*>(metrics);
        m_requests.emplace(handle, std::move(record));
        *out_request = handle;
        return Result::Success;
    }

    Result GetRequestStatus(RequestHandle request,
                            RequestStatus* out_status) override
    {
        if(!out_status)
        {
            return Result::InvalidArgument;
        }

        auto it = m_requests.find(request);
        if(it == m_requests.end())
        {
            out_status->state = RequestState::Invalid;
            out_status->result = Result::InvalidArgument;
            std::memset(out_status->reserved, 0, sizeof(out_status->reserved));
            return Result::InvalidArgument;
        }

        RequestRecord& record = it->second;
        if(!record.future)
        {
            out_status->state =
                record.response_code == kRocProfVisResultCancelled ? RequestState::Cancelled :
                record.response_code == kRocProfVisResultSuccess ? RequestState::Ready :
                                                                   RequestState::Error;
            out_status->result = ConvertResult(record.response_code);
            std::memset(out_status->reserved, 0, sizeof(out_status->reserved));
            return Result::Success;
        }

        rocprofvis_result_t wait_result = rocprofvis_controller_future_wait(record.future, 0);
        if(wait_result == kRocProfVisResultTimeout)
        {
            out_status->state = RequestState::Pending;
            out_status->result = Result::Timeout;
            std::memset(out_status->reserved, 0, sizeof(out_status->reserved));
            return Result::Success;
        }

        if(wait_result == kRocProfVisResultSuccess)
        {
            uint64_t raw_result = kRocProfVisResultSuccess;
            rocprofvis_controller_get_uint64(record.future, kRPVControllerFutureResult, 0,
                                             &raw_result);
            record.response_code = static_cast<rocprofvis_result_t>(raw_result);
            rocprofvis_controller_future_free(record.future);
            record.future = nullptr;

            out_status->state =
                record.response_code == kRocProfVisResultCancelled ? RequestState::Cancelled :
                record.response_code == kRocProfVisResultSuccess ? RequestState::Ready :
                                                                   RequestState::Error;
            out_status->result = ConvertResult(record.response_code);
            std::memset(out_status->reserved, 0, sizeof(out_status->reserved));
            return Result::Success;
        }

        out_status->state = RequestState::Error;
        out_status->result = ConvertResult(wait_result);
        std::memset(out_status->reserved, 0, sizeof(out_status->reserved));
        return Result::Success;
    }

    Result CancelRequest(RequestHandle request) override
    {
        auto it = m_requests.find(request);
        if(it == m_requests.end() || !it->second.future)
        {
            return Result::InvalidArgument;
        }

        return ConvertResult(rocprofvis_controller_future_cancel(it->second.future));
    }

    Result ReleaseRequest(RequestHandle request) override
    {
        auto it = m_requests.find(request);
        if(it == m_requests.end())
        {
            return Result::InvalidArgument;
        }

        FreeRequest(it->second);
        m_requests.erase(it);
        return Result::Success;
    }

    Result GetLoadResult(RequestHandle request) override
    {
        auto it = m_requests.find(request);
        if(it == m_requests.end() || it->second.kind != RequestKind::Load)
        {
            return Result::InvalidArgument;
        }

        RequestStatus status = {};
        Result poll_result = GetRequestStatus(request, &status);
        if(poll_result != Result::Success)
        {
            return poll_result;
        }

        if(status.state != RequestState::Ready)
        {
            return status.result;
        }

        m_tracks_valid = false;
        return ConvertResult(it->second.response_code);
    }

    Result GetSummaryResult(RequestHandle request,
                            ControllerSummaryMetrics* out_metrics,
                            const KernelMetrics** out_kernels,
                            uint32_t* out_count,
                            const ApiStringTable** out_strings) override
    {
        if(!out_metrics || !out_kernels || !out_count || !out_strings)
        {
            return Result::InvalidArgument;
        }

        auto it = m_requests.find(request);
        if(it == m_requests.end() || it->second.kind != RequestKind::SummaryFetch)
        {
            return Result::InvalidArgument;
        }

        RequestStatus status = {};
        Result poll_result = GetRequestStatus(request, &status);
        if(poll_result != Result::Success)
        {
            return poll_result;
        }
        if(status.state != RequestState::Ready)
        {
            return status.result;
        }
        if(status.result != Result::Success)
        {
            return status.result;
        }

        RequestRecord& record = it->second;
        if(!record.converted)
        {
            Result convert_result = ConvertSummaryRequest(record);
            if(convert_result != Result::Success)
            {
                return convert_result;
            }
        }

        *out_metrics = record.summary_metrics;
        *out_kernels = record.kernel_metrics.empty() ? nullptr : record.kernel_metrics.data();
        *out_count = static_cast<uint32_t>(record.kernel_metrics.size());
        *out_strings = &record.string_table;
        return Result::Success;
    }

    Result GetTrackCount(uint32_t* out_count) const override
    {
        if(!out_count)
        {
            return Result::InvalidArgument;
        }

        Result result = RefreshTrackCache();
        if(result != Result::Success)
        {
            return result;
        }

        *out_count = static_cast<uint32_t>(m_track_cache.size());
        return Result::Success;
    }

    Result GetTrack(uint32_t index, ITrack** out_track) const override
    {
        if(!out_track)
        {
            return Result::InvalidArgument;
        }

        Result result = RefreshTrackCache();
        if(result != Result::Success)
        {
            return result;
        }

        if(index >= m_track_cache.size())
        {
            *out_track = nullptr;
            return Result::InvalidArgument;
        }

        *out_track = m_track_cache[index].get();
        return Result::Success;
    }

    Result GetTrackById(uint64_t id, ITrack** out_track) const override
    {
        if(!out_track)
        {
            return Result::InvalidArgument;
        }

        Result result = RefreshTrackCache();
        if(result != Result::Success)
        {
            return result;
        }

        for(const std::unique_ptr<LegacyTrackAdapter>& track : m_track_cache)
        {
            if(track->GetId() == id)
            {
                *out_track = track.get();
                return Result::Success;
            }
        }

        *out_track = nullptr;
        return Result::InvalidArgument;
    }

    Result GetStringTable(const ApiStringTable** out_strings) const override
    {
        if(!out_strings)
        {
            return Result::InvalidArgument;
        }

        Result result = RefreshTrackCache();
        if(result != Result::Success &&
           result != Result::NotSupported &&
           result != Result::NotLoaded)
        {
            return result;
        }

        *out_strings = &m_track_string_table;
        return Result::Success;
    }

    Result GetTimeRange(double* out_min_timestamp,
                        double* out_max_timestamp) const override
    {
        if(!out_min_timestamp || !out_max_timestamp || !m_controller)
        {
            return Result::InvalidArgument;
        }

        rocprofvis_handle_t* timeline_handle = nullptr;
        rocprofvis_result_t result = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSystemTimeline, 0, &timeline_handle);
        if(result != kRocProfVisResultSuccess || !timeline_handle)
        {
            return ConvertResult(result);
        }

        result = rocprofvis_controller_get_double(
            timeline_handle, kRPVControllerTimelineMinTimestamp, 0, out_min_timestamp);
        rocprofvis_result_t max_result = rocprofvis_controller_get_double(
            timeline_handle, kRPVControllerTimelineMaxTimestamp, 0, out_max_timestamp);

        if(result != kRocProfVisResultSuccess || max_result != kRocProfVisResultSuccess)
        {
            return Result::InvalidArgument;
        }

        return Result::Success;
    }

    const char* GetTraceFilePath() const override
    {
        return m_trace_file_path.c_str();
    }

    Result BeginTrackRequest(const LegacyTrackAdapter& track, bool use_lod,
                             uint32_t pixel_range, double start_time,
                             double end_time, RequestHandle* out_request)
    {
        if(!out_request || !m_controller)
        {
            return Result::InvalidArgument;
        }

        rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
        rocprofvis_controller_array_t* array = rocprofvis_controller_array_alloc(32);
        if(!future || !array)
        {
            if(future)
            {
                rocprofvis_controller_future_free(future);
            }
            if(array)
            {
                rocprofvis_controller_array_free(array);
            }
            return Result::OutOfMemory;
        }

        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if(use_lod)
        {
            result = rocprofvis_controller_graph_fetch_async(
                m_controller, track.m_graph_handle, start_time, end_time, pixel_range,
                future, array);
        }
        else
        {
            result = rocprofvis_controller_track_fetch_async(
                m_controller, track.m_track_handle, start_time, end_time, future, array);
        }

        if(result != kRocProfVisResultSuccess)
        {
            rocprofvis_controller_future_free(future);
            rocprofvis_controller_array_free(array);
            return ConvertResult(result);
        }

        RequestHandle handle = AllocateRequestHandle();
        RequestRecord record;
        record.kind = RequestKind::TrackFetch;
        record.future = future;
        record.array = array;
        record.track_type = track.m_type;
        m_requests.emplace(handle, std::move(record));
        *out_request = handle;
        return Result::Success;
    }

    Result GetTrackEventResult(RequestHandle request,
                               const EventRecord** out_events,
                               uint32_t* out_count,
                               const ApiStringTable** out_strings)
    {
        if(!out_events || !out_count || !out_strings)
        {
            return Result::InvalidArgument;
        }

        auto it = m_requests.find(request);
        if(it == m_requests.end() || it->second.kind != RequestKind::TrackFetch)
        {
            return Result::InvalidArgument;
        }

        RequestStatus status = {};
        Result poll_result = GetRequestStatus(request, &status);
        if(poll_result != Result::Success)
        {
            return poll_result;
        }
        if(status.state != RequestState::Ready)
        {
            return status.result;
        }
        if(status.result != Result::Success)
        {
            return status.result;
        }

        RequestRecord& record = it->second;
        if(record.track_type != ITrack::Type::Events)
        {
            return Result::InvalidArgument;
        }

        if(!record.converted)
        {
            Result convert_result = ConvertTrackRequest(record);
            if(convert_result != Result::Success)
            {
                return convert_result;
            }
        }

        *out_events = record.event_records.empty() ? nullptr : record.event_records.data();
        *out_count = static_cast<uint32_t>(record.event_records.size());
        *out_strings = &record.string_table;
        return Result::Success;
    }

    Result GetTrackSampleResult(RequestHandle request,
                                const SampleRecord** out_samples,
                                uint32_t* out_count)
    {
        if(!out_samples || !out_count)
        {
            return Result::InvalidArgument;
        }

        auto it = m_requests.find(request);
        if(it == m_requests.end() || it->second.kind != RequestKind::TrackFetch)
        {
            return Result::InvalidArgument;
        }

        RequestStatus status = {};
        Result poll_result = GetRequestStatus(request, &status);
        if(poll_result != Result::Success)
        {
            return poll_result;
        }
        if(status.state != RequestState::Ready)
        {
            return status.result;
        }
        if(status.result != Result::Success)
        {
            return status.result;
        }

        RequestRecord& record = it->second;
        if(record.track_type != ITrack::Type::Samples)
        {
            return Result::InvalidArgument;
        }

        if(!record.converted)
        {
            Result convert_result = ConvertTrackRequest(record);
            if(convert_result != Result::Success)
            {
                return convert_result;
            }
        }

        *out_samples = record.sample_records.empty() ? nullptr : record.sample_records.data();
        *out_count = static_cast<uint32_t>(record.sample_records.size());
        return Result::Success;
    }

private:
    Result RefreshTrackCache() const
    {
        if(m_tracks_valid)
        {
            return Result::Success;
        }

        if(!m_controller || m_controller_type != kRPVControllerObjectTypeControllerSystem)
        {
            return m_controller ? Result::NotSupported : Result::NotLoaded;
        }

        rocprofvis_handle_t* timeline_handle = nullptr;
        rocprofvis_result_t result = rocprofvis_controller_get_object(
            m_controller, kRPVControllerSystemTimeline, 0, &timeline_handle);
        if(result != kRocProfVisResultSuccess || !timeline_handle)
        {
            return ConvertResult(result);
        }

        uint64_t graph_count = 0;
        result = rocprofvis_controller_get_uint64(
            timeline_handle, kRPVControllerTimelineNumGraphs, 0, &graph_count);
        if(result != kRocProfVisResultSuccess)
        {
            return ConvertResult(result);
        }

        m_track_cache.clear();
        m_track_string_table.Clear();
        for(uint64_t i = 0; i < graph_count; ++i)
        {
            rocprofvis_controller_graph_t* graph_handle = nullptr;
            rocprofvis_handle_t* track_handle = nullptr;
            result = rocprofvis_controller_get_object(
                timeline_handle, kRPVControllerTimelineGraphIndexed, i,
                reinterpret_cast<rocprofvis_handle_t**>(&graph_handle));
            if(result != kRocProfVisResultSuccess || !graph_handle)
            {
                continue;
            }

            result = rocprofvis_controller_get_object(
                reinterpret_cast<rocprofvis_handle_t*>(graph_handle),
                kRPVControllerGraphTrack, 0, &track_handle);
            if(result != kRocProfVisResultSuccess || !track_handle)
            {
                continue;
            }

            m_track_cache.emplace_back(std::make_unique<LegacyTrackAdapter>(
                const_cast<LegacyControllerAdapter*>(this),
                reinterpret_cast<rocprofvis_controller_track_t*>(track_handle),
                graph_handle, &m_track_string_table));
        }

        m_tracks_valid = true;
        return Result::Success;
    }

    Result ConvertTrackRequest(RequestRecord& record)
    {
        if(!record.array)
        {
            return Result::InvalidArgument;
        }

        uint64_t entry_count = 0;
        rocprofvis_result_t result = rocprofvis_controller_get_uint64(
            record.array, kRPVControllerArrayNumEntries, 0, &entry_count);
        if(result != kRocProfVisResultSuccess)
        {
            return ConvertResult(result);
        }

        if(record.track_type == ITrack::Type::Events)
        {
            record.event_records.clear();
            record.event_records.reserve(static_cast<size_t>(entry_count));
            for(uint64_t i = 0; i < entry_count; ++i)
            {
                rocprofvis_handle_t* event_handle = nullptr;
                result = rocprofvis_controller_get_object(
                    record.array, kRPVControllerArrayEntryIndexed, i, &event_handle);
                if(result != kRocProfVisResultSuccess || !event_handle)
                {
                    continue;
                }

                EventRecord event = {};
                double start_ts = 0.0;
                double end_ts = 0.0;
                rocprofvis_controller_get_uint64(event_handle, kRPVControllerEventId, 0,
                                                 &event.id);
                rocprofvis_controller_get_double(event_handle,
                                                 kRPVControllerEventStartTimestamp, 0,
                                                 &start_ts);
                rocprofvis_controller_get_double(event_handle,
                                                 kRPVControllerEventEndTimestamp, 0,
                                                 &end_ts);
                event.timestamp = start_ts;
                event.duration = end_ts - start_ts;
                event.category_index = record.string_table.AddString(
                    ReadString(event_handle, kRPVControllerEventCategory, 0).c_str());
                event.name_index = record.string_table.AddString(
                    ReadString(event_handle, kRPVControllerEventName, 0).c_str());
                event.child_count = static_cast<uint32_t>(
                    ReadUInt64(event_handle, kRPVControllerEventNumChildren, 0));
                event.top_combined_name_index = event.child_count > 1
                    ? record.string_table.AddString(
                          ReadString(event_handle, kRPVControllerEventTopCombinedName, 0)
                              .c_str())
                    : 0;
                event.level = static_cast<uint8_t>(
                    ReadUInt64(event_handle, kRPVControllerEventLevel, 0));
                event.operation = 0;
                std::memset(event.reserved, 0, sizeof(event.reserved));
                record.event_records.push_back(event);
            }
        }
        else
        {
            record.sample_records.clear();
            record.sample_records.reserve(static_cast<size_t>(entry_count));
            for(uint64_t i = 0; i < entry_count; ++i)
            {
                rocprofvis_handle_t* sample_handle = nullptr;
                result = rocprofvis_controller_get_object(
                    record.array, kRPVControllerArrayEntryIndexed, i, &sample_handle);
                if(result != kRocProfVisResultSuccess || !sample_handle)
                {
                    continue;
                }

                SampleRecord sample = {};
                rocprofvis_controller_get_double(sample_handle,
                                                 kRPVControllerSampleTimestamp, 0,
                                                 &sample.timestamp);
                rocprofvis_controller_get_double(sample_handle,
                                                 kRPVControllerSampleValue, 0,
                                                 &sample.value);
                rocprofvis_controller_get_double(sample_handle,
                                                 kRPVControllerSampleEndTimestamp, 0,
                                                 &sample.end_timestamp);
                std::memset(sample.reserved, 0, sizeof(sample.reserved));
                record.sample_records.push_back(sample);
            }
        }

        record.converted = true;
        return Result::Success;
    }

    Result ConvertSummaryRequest(RequestRecord& record)
    {
        if(!record.result_object)
        {
            return Result::InvalidArgument;
        }

        rocprofvis_handle_t* metrics_handle = record.result_object;
        double double_value = 0.0;
        uint64_t uint64_value = 0;

        record.summary_metrics.gpu_utilization = -1.0f;
        record.summary_metrics.memory_utilization = -1.0f;
        record.summary_metrics.top_kernel_count = 0;
        record.summary_metrics.kernel_exec_time_total_ns = 0.0;
        record.kernel_metrics.clear();
        record.string_table.Clear();

        if(rocprofvis_controller_get_double(
               metrics_handle, kRPVControllerSummaryMetricPropertyGpuGfxUtil, 0,
               &double_value) == kRocProfVisResultSuccess)
        {
            record.summary_metrics.gpu_utilization = static_cast<float>(double_value);
        }
        if(rocprofvis_controller_get_double(
               metrics_handle, kRPVControllerSummaryMetricPropertyGpuMemUtil, 0,
               &double_value) == kRocProfVisResultSuccess)
        {
            record.summary_metrics.memory_utilization = static_cast<float>(double_value);
        }
        if(rocprofvis_controller_get_double(
               metrics_handle, kRPVControllerSummaryMetricPropertyKernelsExecTimeTotal, 0,
               &double_value) == kRocProfVisResultSuccess)
        {
            record.summary_metrics.kernel_exec_time_total_ns = double_value;
        }
        if(rocprofvis_controller_get_uint64(
               metrics_handle, kRPVControllerSummaryMetricPropertyNumKernels, 0,
               &uint64_value) == kRocProfVisResultSuccess)
        {
            record.summary_metrics.top_kernel_count = static_cast<uint32_t>(uint64_value);
        }

        record.kernel_metrics.reserve(record.summary_metrics.top_kernel_count);
        for(uint32_t i = 0; i < record.summary_metrics.top_kernel_count; ++i)
        {
            KernelMetrics metrics = {};
            metrics.id = i;
            metrics.name_index = record.string_table.AddString(
                ReadString(metrics_handle,
                           kRPVControllerSummaryMetricPropertyKernelNameIndexed, i)
                    .c_str());
            metrics.invocation_count = static_cast<uint32_t>(ReadUInt64(
                metrics_handle,
                kRPVControllerSummaryMetricPropertyKernelInvocationsIndexed, i));
            metrics.duration_total_ns = static_cast<uint64_t>(ReadDouble(
                metrics_handle,
                kRPVControllerSummaryMetricPropertyKernelExecTimeSumIndexed, i));
            metrics.duration_min_ns = static_cast<uint32_t>(ReadDouble(
                metrics_handle,
                kRPVControllerSummaryMetricPropertyKernelExecTimeMinIndexed, i));
            metrics.duration_max_ns = static_cast<uint32_t>(ReadDouble(
                metrics_handle,
                kRPVControllerSummaryMetricPropertyKernelExecTimeMaxIndexed, i));
            metrics.duration_median_ns = 0;
            metrics.duration_mean_ns = metrics.invocation_count > 0
                ? static_cast<uint32_t>(
                      static_cast<double>(metrics.duration_total_ns) /
                      static_cast<double>(metrics.invocation_count))
                : 0;
            record.kernel_metrics.push_back(metrics);
        }

        record.converted = true;
        return Result::Success;
    }

    void FreeRequest(RequestRecord& record)
    {
        if(record.future)
        {
            rocprofvis_controller_future_free(record.future);
            record.future = nullptr;
        }
        if(record.array)
        {
            rocprofvis_controller_array_free(record.array);
            record.array = nullptr;
        }
        if(record.args)
        {
            rocprofvis_controller_arguments_free(record.args);
            record.args = nullptr;
        }
        if(record.result_object)
        {
            delete reinterpret_cast<SummaryMetrics*>(record.result_object);
            record.result_object = nullptr;
        }
    }

    void ReleaseAllRequests()
    {
        for(auto& request_pair : m_requests)
        {
            RequestRecord& record = request_pair.second;
            if(record.future)
            {
                rocprofvis_controller_future_cancel(record.future);
                rocprofvis_controller_future_wait(record.future, FLT_MAX);
            }
            FreeRequest(record);
        }
        m_requests.clear();
    }

    RequestHandle AllocateRequestHandle()
    {
        return m_next_request_handle++;
    }

    rocprofvis_controller_t*                               m_controller;
    bool                                                   m_owns_controller;
    rocprofvis_controller_object_type_t                    m_controller_type;
    mutable bool                                           m_tracks_valid;
    mutable ApiStringTable                                 m_track_string_table;
    mutable std::vector<std::unique_ptr<LegacyTrackAdapter>> m_track_cache;
    std::unordered_map<RequestHandle, RequestRecord>       m_requests;
    RequestHandle                                          m_next_request_handle;
    std::string                                            m_trace_file_path;
};

LegacyTrackAdapter::LegacyTrackAdapter(LegacyControllerAdapter* owner,
                                       rocprofvis_controller_track_t* track_handle,
                                       rocprofvis_controller_graph_t* graph_handle,
                                       const ApiStringTable* string_table)
: m_owner(owner)
, m_track_handle(track_handle)
, m_graph_handle(graph_handle)
, m_id(0)
, m_category_index(0)
, m_main_name_index(0)
, m_sub_name_index(0)
, m_type(Type::Events)
, m_min_timestamp(0.0)
, m_max_timestamp(0.0)
, m_entry_count(0)
, m_owner_process_or_agent_id(0)
, m_owner_queue_or_thread_id(0)
, m_min_value(0.0)
, m_max_value(0.0)
, m_topology{ TrackTopologyType::Unknown, { 0 }, 0, 0, 0, 0 }
{
    LoadMetadata(string_table);
}

void LegacyTrackAdapter::LoadMetadata(const ApiStringTable* string_table)
{
    ApiStringTable* writable_strings = const_cast<ApiStringTable*>(string_table);

    m_id = ReadUInt64(reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                      kRPVControllerTrackId, 0);
    uint64_t raw_type = ReadUInt64(reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                                   kRPVControllerTrackType, 0);
    m_type = raw_type == kRPVControllerTrackTypeSamples ? Type::Samples : Type::Events;
    m_min_timestamp = ReadDouble(reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                                 kRPVControllerTrackMinTimestamp, 0);
    m_max_timestamp = ReadDouble(reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                                 kRPVControllerTrackMaxTimestamp, 0);
    m_entry_count = static_cast<uint32_t>(
        ReadUInt64(reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                   kRPVControllerTrackNumberOfEntries, 0));
    m_owner_process_or_agent_id = ReadUInt64(
        reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
        kRPVControllerTrackAgentIdOrPid, 0);
    m_owner_queue_or_thread_id = ReadUInt64(
        reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
        kRPVControllerTrackQueueIdOrTid, 0);
    m_min_value = ReadDouble(reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                             kRPVControllerTrackMinValue, 0);
    m_max_value = ReadDouble(reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                             kRPVControllerTrackMaxValue, 0);

    m_category_index = writable_strings->AddString(
        ReadString(reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                   kRPVControllerCategory, 0).c_str());
    m_main_name_index = writable_strings->AddString(
        ReadString(reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                   kRPVControllerMainName, 0).c_str());
    m_sub_name_index = writable_strings->AddString(
        ReadString(reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                   kRPVControllerSubName, 0).c_str());

    rocprofvis_handle_t* topology_object = nullptr;
    rocprofvis_handle_t* process_object = nullptr;
    rocprofvis_handle_t* node_object = nullptr;
    rocprofvis_handle_t* processor_object = nullptr;

    if(rocprofvis_controller_get_object(reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                                        kRPVControllerTrackQueue, 0,
                                        &topology_object) == kRocProfVisResultSuccess &&
       topology_object)
    {
        m_topology.type = TrackTopologyType::Queue;
        m_topology.id = ReadUInt64(topology_object, kRPVControllerQueueId, 0);
        rocprofvis_controller_get_object(topology_object, kRPVControllerQueueProcess, 0,
                                         &process_object);
        rocprofvis_controller_get_object(topology_object, kRPVControllerQueueNode, 0,
                                         &node_object);
        rocprofvis_controller_get_object(topology_object, kRPVControllerQueueProcessor, 0,
                                         &processor_object);
    }
    else if(rocprofvis_controller_get_object(
                reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                kRPVControllerTrackStream, 0,
                &topology_object) == kRocProfVisResultSuccess &&
            topology_object)
    {
        m_topology.type = TrackTopologyType::Stream;
        m_topology.id = ReadUInt64(topology_object, kRPVControllerStreamId, 0);
        rocprofvis_controller_get_object(topology_object, kRPVControllerStreamProcess, 0,
                                         &process_object);
        rocprofvis_controller_get_object(topology_object, kRPVControllerStreamNode, 0,
                                         &node_object);
        rocprofvis_controller_get_object(topology_object, kRPVControllerStreamProcessor, 0,
                                         &processor_object);
    }
    else if(rocprofvis_controller_get_object(
                reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                kRPVControllerTrackThread, 0,
                &topology_object) == kRocProfVisResultSuccess &&
            topology_object)
    {
        uint64_t thread_type = 0;
        m_topology.id = ReadUInt64(topology_object, kRPVControllerThreadId, 0);
        rocprofvis_controller_get_object(topology_object, kRPVControllerThreadProcess, 0,
                                         &process_object);
        rocprofvis_controller_get_object(topology_object, kRPVControllerThreadNode, 0,
                                         &node_object);
        rocprofvis_controller_get_uint64(topology_object, kRPVControllerThreadType, 0,
                                         &thread_type);
        m_topology.type = thread_type == kRPVControllerThreadTypeInstrumented
            ? TrackTopologyType::InstrumentedThread
            : TrackTopologyType::SampledThread;
    }
    else if(rocprofvis_controller_get_object(
                reinterpret_cast<rocprofvis_handle_t*>(m_track_handle),
                kRPVControllerTrackCounter, 0,
                &topology_object) == kRocProfVisResultSuccess &&
            topology_object)
    {
        m_topology.type = TrackTopologyType::Counter;
        m_topology.id = ReadUInt64(topology_object, kRPVControllerCounterId, 0);
        rocprofvis_controller_get_object(topology_object, kRPVControllerCounterProcess, 0,
                                         &process_object);
        rocprofvis_controller_get_object(topology_object, kRPVControllerCounterNode, 0,
                                         &node_object);
        rocprofvis_controller_get_object(topology_object, kRPVControllerCounterProcessor, 0,
                                         &processor_object);
    }

    if(process_object)
    {
        m_topology.process_id = ReadUInt64(process_object, kRPVControllerProcessId, 0);
    }
    if(node_object)
    {
        m_topology.node_id = ReadUInt64(node_object, kRPVControllerNodeId, 0);
    }
    if(processor_object)
    {
        m_topology.device_id = ReadUInt64(processor_object, kRPVControllerProcessorId, 0);
    }
}

Result LegacyTrackAdapter::BeginFetch(double start_time, double end_time,
                                      RequestHandle* out_request)
{
    return m_owner->BeginTrackRequest(*this, false, 0, start_time, end_time, out_request);
}

Result LegacyTrackAdapter::BeginFetchLod(uint32_t pixel_range, double start_time,
                                         double end_time, RequestHandle* out_request)
{
    return m_owner->BeginTrackRequest(*this, true, pixel_range, start_time, end_time,
                                      out_request);
}

Result LegacyTrackAdapter::GetEventResult(RequestHandle request,
                                          const EventRecord** out_events,
                                          uint32_t* out_count,
                                          const ApiStringTable** out_strings)
{
    return m_owner->GetTrackEventResult(request, out_events, out_count, out_strings);
}

Result LegacyTrackAdapter::GetSampleResult(RequestHandle request,
                                           const SampleRecord** out_samples,
                                           uint32_t* out_count)
{
    return m_owner->GetTrackSampleResult(request, out_samples, out_count);
}

}  // namespace

IController* IController::Create(const char* filename)
{
    rocprofvis_controller_t* controller = rocprofvis_controller_alloc(filename);
    if(!controller)
    {
        return nullptr;
    }

    return new LegacyControllerAdapter(controller, true, filename);
}

IController* IController::Attach(void* controller_handle)
{
    if(!controller_handle)
    {
        return nullptr;
    }

    return new LegacyControllerAdapter(
        reinterpret_cast<rocprofvis_controller_t*>(controller_handle), false, nullptr);
}

void IController::Destroy(IController* controller)
{
    delete controller;
}

}  // namespace Controller
}  // namespace RocProfVis
