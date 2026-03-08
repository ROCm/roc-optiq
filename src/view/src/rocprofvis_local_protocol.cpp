#include "rocprofvis_local_protocol.h"

#include "spdlog/spdlog.h"

#include <cstring>

namespace RocProfVis
{
namespace View
{

LocalProtocol::LocalProtocol()
    : m_controller(nullptr)
    , m_owns_controller(false)
{
}

LocalProtocol::~LocalProtocol()
{
    Shutdown();
}

Result LocalProtocol::Initialize(const char* trace_path)
{
    m_trace_file_path = trace_path ? trace_path : "";

    if(!m_controller && !m_trace_file_path.empty())
    {
        m_controller = Controller::IController::Create(m_trace_file_path.c_str());
        m_owns_controller = m_controller != nullptr;
        if(!m_controller)
        {
            return Result::InvalidArgument;
        }
    }

    return Result::Success;
}

void LocalProtocol::SetController(void* controller_handle)
{
    if(m_owns_controller && m_controller)
    {
        Controller::IController::Destroy(m_controller);
    }

    m_controller = nullptr;
    m_owns_controller = false;

    if(controller_handle)
    {
        m_controller = Controller::IController::Attach(controller_handle);
    }
}

void LocalProtocol::Shutdown()
{
    m_request_cache.clear();
    m_track_cache.clear();
    m_track_strings.Clear();

    if(m_owns_controller && m_controller)
    {
        Controller::IController::Destroy(m_controller);
    }

    m_controller = nullptr;
    m_owns_controller = false;
}

Result LocalProtocol::BeginLoad(ProtocolRequestHandle* out_request)
{
    if(!m_controller)
    {
        return Result::NotLoaded;
    }

    Result result = m_controller->BeginLoad(out_request);
    if(result == Result::Success)
    {
        RequestCache cache;
        cache.kind = RequestCache::Kind::kLoad;
        m_request_cache[*out_request] = std::move(cache);
    }

    return result;
}

Controller::ITrack* LocalProtocol::FindTrack(uint64_t track_id) const
{
    if(!m_controller)
    {
        return nullptr;
    }

    Controller::ITrack* track = nullptr;
    if(m_controller->GetTrackById(track_id, &track) != Result::Success)
    {
        return nullptr;
    }

    return track;
}

Result LocalProtocol::BeginFetchTrack(uint64_t track_id, double start_time,
                                      double end_time, ProtocolRequestHandle* out_request)
{
    if(!out_request)
    {
        return Result::InvalidArgument;
    }

    Controller::ITrack* track = FindTrack(track_id);
    if(!track)
    {
        return Result::InvalidArgument;
    }

    Result result = track->BeginFetch(start_time, end_time, out_request);
    if(result == Result::Success)
    {
        RequestCache cache;
        cache.kind = RequestCache::Kind::kTrack;
        cache.track = track;
        m_request_cache[*out_request] = std::move(cache);
    }

    return result;
}

Result LocalProtocol::BeginFetchTrackLod(uint64_t track_id, double start_time,
                                         double end_time, uint32_t pixel_range,
                                         ProtocolRequestHandle* out_request)
{
    if(!out_request)
    {
        return Result::InvalidArgument;
    }

    Controller::ITrack* track = FindTrack(track_id);
    if(!track)
    {
        return Result::InvalidArgument;
    }

    Result result = track->BeginFetchLod(pixel_range, start_time, end_time, out_request);
    if(result == Result::Success)
    {
        RequestCache cache;
        cache.kind = RequestCache::Kind::kTrack;
        cache.track = track;
        m_request_cache[*out_request] = std::move(cache);
    }

    return result;
}

Result LocalProtocol::BeginFetchSummary(double start_time, double end_time,
                                        ProtocolRequestHandle* out_request)
{
    if(!m_controller)
    {
        return Result::NotLoaded;
    }

    Result result = m_controller->BeginFetchSummary(start_time, end_time, out_request);
    if(result == Result::Success)
    {
        RequestCache cache;
        cache.kind = RequestCache::Kind::kSummary;
        m_request_cache[*out_request] = std::move(cache);
    }

    return result;
}

Result LocalProtocol::GetRequestStatus(ProtocolRequestHandle request, ProtocolRequestStatus* out_status)
{
    if(!m_controller)
    {
        return Result::NotLoaded;
    }

    return m_controller->GetRequestStatus(request, out_status);
}

Result LocalProtocol::CancelRequest(ProtocolRequestHandle request)
{
    if(!m_controller)
    {
        return Result::NotLoaded;
    }

    return m_controller->CancelRequest(request);
}

Result LocalProtocol::ReleaseRequest(ProtocolRequestHandle request)
{
    if(!m_controller)
    {
        return Result::NotLoaded;
    }

    m_request_cache.erase(request);
    return m_controller->ReleaseRequest(request);
}

void LocalProtocol::ConvertControllerStrings(const Controller::ApiStringTable* controller_strings,
                                             StringTable& output_strings)
{
    output_strings.Clear();
    if(!controller_strings)
    {
        return;
    }

    for(uint32_t i = 0; i < controller_strings->GetCount(); ++i)
    {
        output_strings.AddString(controller_strings->GetString(i));
    }
}

Result LocalProtocol::GetTrackList(const ProtocolTrackInfo** out_tracks,
                                   uint32_t* out_count,
                                   const StringTable** out_strings)
{
    if(!out_tracks || !out_count || !out_strings || !m_controller)
    {
        return Result::InvalidArgument;
    }

    uint32_t track_count = 0;
    Result result = m_controller->GetTrackCount(&track_count);
    if(result != Result::Success)
    {
        *out_tracks = nullptr;
        *out_count = 0;
        *out_strings = &m_track_strings;
        return result;
    }

    const Controller::ApiStringTable* controller_strings = nullptr;
    m_controller->GetStringTable(&controller_strings);
    ConvertControllerStrings(controller_strings, m_track_strings);

    m_track_cache.clear();
    m_track_cache.reserve(track_count);

    for(uint32_t i = 0; i < track_count; ++i)
    {
        Controller::ITrack* track = nullptr;
        result = m_controller->GetTrack(i, &track);
        if(result != Result::Success || !track)
        {
            continue;
        }

        Controller::TrackTopologyInfo topology = track->GetTopology();

        ProtocolTrackInfo info = {};
        info.index = i;
        info.id = track->GetId();
        info.category_index = track->GetCategoryIndex();
        info.main_name_index = track->GetMainNameIndex();
        info.sub_name_index = track->GetSubNameIndex();
        info.entry_count = track->GetEntryCount();
        info.min_timestamp = track->GetMinTimestamp();
        info.max_timestamp = track->GetMaxTimestamp();
        info.owner_process_or_agent_id = track->GetOwnerProcessOrAgentId();
        info.owner_queue_or_thread_id = track->GetOwnerQueueOrThreadId();
        info.min_value = track->GetMinValue();
        info.max_value = track->GetMaxValue();
        info.topology_type = topology.type;
        info.topology_id = topology.id;
        info.topology_process_id = topology.process_id;
        info.topology_node_id = topology.node_id;
        info.topology_device_id = topology.device_id;
        info.track_type = static_cast<uint8_t>(track->GetType());
        std::memset(info.reserved, 0, sizeof(info.reserved));
        std::memset(info.reserved1, 0, sizeof(info.reserved1));

        m_track_cache.push_back(info);
    }

    *out_tracks = m_track_cache.empty() ? nullptr : m_track_cache.data();
    *out_count = static_cast<uint32_t>(m_track_cache.size());
    *out_strings = &m_track_strings;
    return Result::Success;
}

Result LocalProtocol::GetEventResult(ProtocolRequestHandle request,
                                     const EventData** out_events,
                                     uint32_t* out_count,
                                     const StringTable** out_strings)
{
    if(!out_events || !out_count || !out_strings || !m_controller)
    {
        return Result::InvalidArgument;
    }

    auto it = m_request_cache.find(request);
    if(it == m_request_cache.end() || !it->second.track)
    {
        return Result::InvalidArgument;
    }

    RequestCache& cache = it->second;
    if(!cache.converted)
    {
        const Controller::EventRecord* controller_events = nullptr;
        uint32_t controller_count = 0;
        const Controller::ApiStringTable* controller_strings = nullptr;

        Result result = cache.track->GetEventResult(
            request, &controller_events, &controller_count, &controller_strings);
        if(result != Result::Success)
        {
            return result;
        }

        ConvertControllerStrings(controller_strings, cache.strings);
        cache.events.clear();
        cache.events.reserve(controller_count);

        for(uint32_t i = 0; i < controller_count; ++i)
        {
            const Controller::EventRecord& controller_event = controller_events[i];
            EventData event = {};
            event.id = controller_event.id;
            event.timestamp = controller_event.timestamp;
            event.duration = controller_event.duration;
            event.category_index = controller_event.category_index;
            event.name_index = controller_event.name_index;
            event.child_count = controller_event.child_count;
            event.top_combined_name_index = controller_event.top_combined_name_index;
            event.level = controller_event.level;
            event.operation = controller_event.operation;
            std::memset(event.reserved, 0, sizeof(event.reserved));
            cache.events.push_back(event);
        }

        cache.converted = true;
    }

    *out_events = cache.events.empty() ? nullptr : cache.events.data();
    *out_count = static_cast<uint32_t>(cache.events.size());
    *out_strings = &cache.strings;
    return Result::Success;
}

Result LocalProtocol::GetSampleResult(ProtocolRequestHandle request,
                                      const SampleData** out_samples,
                                      uint32_t* out_count)
{
    if(!out_samples || !out_count || !m_controller)
    {
        return Result::InvalidArgument;
    }

    auto it = m_request_cache.find(request);
    if(it == m_request_cache.end() || !it->second.track)
    {
        return Result::InvalidArgument;
    }

    RequestCache& cache = it->second;
    if(!cache.converted)
    {
        const Controller::SampleRecord* controller_samples = nullptr;
        uint32_t controller_count = 0;

        Result result = cache.track->GetSampleResult(request, &controller_samples,
                                                     &controller_count);
        if(result != Result::Success)
        {
            return result;
        }

        cache.samples.clear();
        cache.samples.reserve(controller_count);

        for(uint32_t i = 0; i < controller_count; ++i)
        {
            const Controller::SampleRecord& controller_sample = controller_samples[i];
            SampleData sample = {};
            sample.timestamp = controller_sample.timestamp;
            sample.value = controller_sample.value;
            sample.end_timestamp = controller_sample.end_timestamp;
            std::memset(sample.reserved, 0, sizeof(sample.reserved));
            cache.samples.push_back(sample);
        }

        cache.converted = true;
    }

    *out_samples = cache.samples.empty() ? nullptr : cache.samples.data();
    *out_count = static_cast<uint32_t>(cache.samples.size());
    return Result::Success;
}

Result LocalProtocol::GetSummaryResult(ProtocolRequestHandle request,
                                       SummaryData* out_summary,
                                       const KernelMetricsData** out_top_kernels,
                                       uint32_t* out_top_kernel_count,
                                       const StringTable** out_strings)
{
    if(!out_summary || !out_top_kernels || !out_top_kernel_count || !out_strings ||
       !m_controller)
    {
        return Result::InvalidArgument;
    }

    auto it = m_request_cache.find(request);
    if(it == m_request_cache.end())
    {
        return Result::InvalidArgument;
    }

    RequestCache& cache = it->second;
    if(!cache.converted)
    {
        Controller::ControllerSummaryMetrics controller_summary = {};
        const Controller::KernelMetrics* controller_kernels = nullptr;
        uint32_t kernel_count = 0;
        const Controller::ApiStringTable* controller_strings = nullptr;

        Result result = m_controller->GetSummaryResult(
            request, &controller_summary, &controller_kernels, &kernel_count,
            &controller_strings);
        if(result != Result::Success)
        {
            return result;
        }

        ConvertControllerStrings(controller_strings, cache.strings);
        cache.kernels.clear();
        cache.kernels.reserve(kernel_count);

        cache.summary.gpu_utilization = controller_summary.gpu_utilization;
        cache.summary.memory_utilization = controller_summary.memory_utilization;
        cache.summary.top_kernel_count = controller_summary.top_kernel_count;
        cache.summary.kernel_exec_time_total_ns =
            controller_summary.kernel_exec_time_total_ns;

        for(uint32_t i = 0; i < kernel_count; ++i)
        {
            const Controller::KernelMetrics& controller_kernel = controller_kernels[i];
            KernelMetricsData kernel = {};
            kernel.id = controller_kernel.id;
            kernel.name_index = controller_kernel.name_index;
            kernel.invocation_count = controller_kernel.invocation_count;
            kernel.reserved0 = 0;
            kernel.duration_total_ns = controller_kernel.duration_total_ns;
            kernel.duration_min_ns = controller_kernel.duration_min_ns;
            kernel.duration_max_ns = controller_kernel.duration_max_ns;
            kernel.duration_median_ns = controller_kernel.duration_median_ns;
            kernel.duration_mean_ns = controller_kernel.duration_mean_ns;
            cache.kernels.push_back(kernel);
        }

        cache.converted = true;
    }

    *out_summary = cache.summary;
    *out_top_kernels = cache.kernels.empty() ? nullptr : cache.kernels.data();
    *out_top_kernel_count = static_cast<uint32_t>(cache.kernels.size());
    *out_strings = &cache.strings;
    return Result::Success;
}

const char* LocalProtocol::ResolveString(uint32_t string_index) const
{
    return m_track_strings.GetString(string_index);
}

Result LocalProtocol::GetTimeRange(double* out_min_timestamp,
                                   double* out_max_timestamp) const
{
    if(!m_controller)
    {
        return Result::NotLoaded;
    }

    return m_controller->GetTimeRange(out_min_timestamp, out_max_timestamp);
}

const char* LocalProtocol::GetTraceFilePath() const
{
    if(m_controller)
    {
        return m_controller->GetTraceFilePath();
    }

    return m_trace_file_path.c_str();
}

}  // namespace View
}  // namespace RocProfVis
