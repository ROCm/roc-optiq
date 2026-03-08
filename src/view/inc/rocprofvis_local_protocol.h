// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_data_protocol.h"
#include "rocprofvis_controller_api.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{

// Local (in-process) protocol implementation
// Directly wraps the Controller API for desktop application
// Even though in-process, uses DLL-safe patterns for consistency with remote protocols
class LocalProtocol : public IDataProtocol
{
public:
    LocalProtocol();
    virtual ~LocalProtocol();

    Result Initialize(const char* trace_path) override;
    void SetController(void* controller_handle) override;
    void Shutdown() override;

    Result BeginLoad(ProtocolRequestHandle* out_request) override;
    Result BeginFetchTrack(uint64_t track_id, double start_time, double end_time,
                           ProtocolRequestHandle* out_request) override;
    Result BeginFetchTrackLod(uint64_t track_id, double start_time, double end_time,
                              uint32_t pixel_range,
                              ProtocolRequestHandle* out_request) override;
    Result BeginFetchSummary(double start_time, double end_time,
                             ProtocolRequestHandle* out_request) override;

    Result GetRequestStatus(ProtocolRequestHandle request, ProtocolRequestStatus* out_status) override;
    Result CancelRequest(ProtocolRequestHandle request) override;
    Result ReleaseRequest(ProtocolRequestHandle request) override;

    Result GetTrackList(
        const ProtocolTrackInfo** out_tracks,
        uint32_t* out_count,
        const StringTable** out_strings) override;

    Result GetEventResult(
        ProtocolRequestHandle request,
        const EventData** out_events,
        uint32_t* out_count,
        const StringTable** out_strings) override;
    Result GetSampleResult(
        ProtocolRequestHandle request,
        const SampleData** out_samples,
        uint32_t* out_count) override;
    Result GetSummaryResult(
        ProtocolRequestHandle request,
        SummaryData* out_summary,
        const KernelMetricsData** out_top_kernels,
        uint32_t* out_top_kernel_count,
        const StringTable** out_strings) override;

    const char* ResolveString(uint32_t string_index) const override;
    Result GetTimeRange(double* out_min_timestamp,
                        double* out_max_timestamp) const override;
    const char* GetTraceFilePath() const override;

private:
    struct RequestCache
    {
        enum class Kind : uint8_t
        {
            kNone = 0,
            kLoad,
            kTrack,
            kSummary
        };

        StringTable                    strings;
        std::vector<EventData>         events;
        std::vector<SampleData>        samples;
        std::vector<KernelMetricsData> kernels;
        SummaryData                    summary;
        Controller::ITrack*            track;
        Kind                           kind;
        bool                           converted;

        RequestCache()
        : summary{ -1.0f, -1.0f, 0, 0, 0.0 }
        , track(nullptr)
        , kind(Kind::kNone)
        , converted(false)
        {}
    };

    Controller::ITrack* FindTrack(uint64_t track_id) const;
    void ConvertControllerStrings(const Controller::ApiStringTable* controller_strings,
                                  StringTable& output_strings);

    Controller::IController* m_controller;
    bool                     m_owns_controller;

    std::vector<ProtocolTrackInfo> m_track_cache;
    StringTable                    m_track_strings;
    std::unordered_map<ProtocolRequestHandle, RequestCache> m_request_cache;

    std::string m_trace_file_path;
};

}  // namespace View
}  // namespace RocProfVis
