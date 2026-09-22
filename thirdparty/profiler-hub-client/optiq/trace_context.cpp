// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "trace_context.h"

namespace optiq {

TraceContext::TraceContext(const std::string &file_path) : m_ctx(nullptr) {
  if (ph_ctx_create(&m_ctx, file_path.c_str()) != PH_RESULT_SUCCESS)
    throw TraceOpenError(file_path);
}

TraceContext::~TraceContext() {
  if (m_ctx != nullptr)
    ph_ctx_free(m_ctx);
}

std::vector<ph_track_t> TraceContext::GetTrackList() const {
  ph_track_list_t track_list{};
  if (ph_get_track_list(m_ctx, &track_list) != PH_RESULT_SUCCESS)
    return {};

  return std::vector<ph_track_t>(track_list.tracks,
                                 track_list.tracks + track_list.list_size);
}

std::vector<ph_event_t> TraceContext::GetTrackEvents(uint32_t track_id,
                                                     uint64_t start_ts,
                                                     uint64_t end_ts) const {
  printf("------> Get track event called. Track id %d\n", track_id);
  ph_event_list_t events{};
  if (ph_get_track_events(m_ctx, track_id, start_ts, end_ts, &events) !=
      PH_RESULT_SUCCESS)
    return {};

  return std::vector<ph_event_t>(events.events,
                                 events.events + events.list_size);
}

std::vector<ph_sample_t> TraceContext::GetTrackSamples(uint32_t track_id,
                                                       uint64_t start_ts,
                                                       uint64_t end_ts) const {
  ph_sample_list_t samples{};
  if (ph_get_track_samples(m_ctx, track_id, start_ts, end_ts, &samples) !=
      PH_RESULT_SUCCESS)
    return {};

  return std::vector<ph_sample_t>(samples.samples,
                                  samples.samples + samples.list_size);
}

} // namespace optiq
