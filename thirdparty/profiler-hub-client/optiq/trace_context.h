// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "c_interface/profiler_hub.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace optiq
{

// Thrown when ph_ctx_create() fails to open/parse a trace file.
class TraceOpenError : public std::runtime_error
{
public:
    explicit TraceOpenError(const std::string& file_path) :
        std::runtime_error("profiler-hub failed to open trace: " + file_path)
    {
    }
};

// RAII wrapper around a profiler-hub ph_ctx_t.
class TraceContext
{
public:
    // @throws TraceOpenError if the trace cannot be opened/parsed.
    explicit TraceContext(const std::string& file_path);
    ~TraceContext();

    TraceContext(const TraceContext&)            = delete;
    TraceContext& operator=(const TraceContext&) = delete;
    TraceContext(TraceContext&&)                 = delete;
    TraceContext& operator=(TraceContext&&)      = delete;

    // Copies the current track list out of the context (the C ABI's
    // ph_track_list_t points into memory owned by ctx, invalidated by the
    // next call or by destruction, so callers get an owned copy instead).
    std::vector<ph_track_t> GetTrackList() const;

    // Copies duration events (region/kernel-dispatch/memory-copy/memory-
    // allocate) for a track within [start_ts, end_ts]. Empty on failure
    // (e.g. track_id from a different context, or a PMC/sample track).
    std::vector<ph_event_t> GetTrackEvents(uint32_t track_id, uint64_t start_ts,
                                            uint64_t end_ts) const;

    // Copies PMC/counter samples for a track within [start_ts, end_ts].
    // Empty on failure (e.g. track_id from a different context, or a
    // duration-event track).
    std::vector<ph_sample_t> GetTrackSamples(uint32_t track_id, uint64_t start_ts,
                                              uint64_t end_ts) const;

private:
    ph_ctx_t m_ctx;
};

}  // namespace optiq
