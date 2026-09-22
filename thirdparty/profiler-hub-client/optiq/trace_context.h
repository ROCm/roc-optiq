// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "c_interface/profiler_hub.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace optiq
{

class TraceOpenError : public std::runtime_error
{
public:
    explicit TraceOpenError(const std::string& file_path) :
        std::runtime_error("profiler-hub failed to open trace: " + file_path)
    {
    }
};

class TraceContext
{
public:
    explicit TraceContext(const std::string& file_path);
    ~TraceContext();

    TraceContext(const TraceContext&)            = delete;
    TraceContext& operator=(const TraceContext&) = delete;
    TraceContext(TraceContext&&)                 = delete;
    TraceContext& operator=(TraceContext&&)      = delete;

    std::vector<ph_track_t> GetTrackList() const;

    std::vector<ph_event_t> GetTrackEvents(uint32_t track_id, uint64_t start_ts,
                                            uint64_t end_ts) const;

    std::vector<ph_sample_t> GetTrackSamples(uint32_t track_id, uint64_t start_ts,
                                              uint64_t end_ts) const;

private:
    ph_ctx_t m_ctx;
};

}
