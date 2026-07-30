// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_tables_model.h"
#include "rocprofvis_trace_data_model.h"

#include "json.h"

#include <cstdint>
#include <string>

namespace RocProfVis
{
namespace View
{

/**
 * @brief Translates the trace data model into JSON.
 *
 * The DataProvider fetches from the controller and lands everything in a
 * TraceDataModel; the UI then reads that model back. This class is the same
 * read, expressed as JSON, so a web frontend can consume what the C++ UI sees
 * without linking against any of it.
 *
 * The accessors are named after the model calls the UI makes
 * (TraceDataModel::GetTopology, TimelineModel::GetTrackData, ...) and the keys
 * reuse the model's own field names, so both frontends share one vocabulary.
 *
 * This class only reads. It never fetches, never caches, and holds no state of
 * its own, so drive the DataProvider first and translate whatever it has
 * already loaded. The referenced model must outlive the translator.
 *
 * Lookup keys (node/device/process/thread/queue/stream/counter/track/event ids)
 * are emitted as decimal strings: several are bit-packed 64-bit values that lose
 * precision once a JavaScript client parses them as doubles. Timestamps, values,
 * counts and enums are emitted as numbers, except the track type, which is a
 * string because it decides whether a track carries events or samples.
 */
class DataModelJson
{
public:
    explicit DataModelJson(const TraceDataModel& model);
    ~DataModelJson() = default;

    /* Mirrors TraceDataModel::GetTopology(): the node/device/process tree,
     * walked from the node list the way the UI walks it. */
    jt::Json GetTopology() const;

    /* Mirrors TraceDataModel::GetTimeline(): the trace bounds plus every
     * TrackInfo, ordered by track index. Track contents are not included;
     * ask for them per track with GetTrackData(). */
    jt::Json GetTimeline() const;

    /* Mirrors TimelineModel::GetTrack(). Null when the id is unknown. */
    jt::Json GetTrack(uint64_t track_id) const;

    /* Mirrors TimelineModel::GetTrackData(): the events or samples fetched for
     * one track. Null when the provider has not fetched that track. */
    jt::Json GetTrackData(uint64_t track_id) const;

    /* Mirrors TablesModel::GetTable(): header, rows and total row count. */
    jt::Json GetTable(TableType type) const;

    /* Mirrors EventModel::GetEvent(): one event with its arguments, extended
     * data, flow and call stack. Null when the id is unknown. */
    jt::Json GetEvent(uint64_t event_id) const;

    /* Mirrors SummaryModel::GetSummaryData(). */
    jt::Json GetSummary() const;

    /* Everything above in one document, keyed by sub-model name. Only tracks
     * and tables the provider has already populated are included, so the size
     * of the result tracks how much the UI has loaded rather than how big the
     * trace is. */
    jt::Json ToJson() const;

    std::string ToString(bool pretty = false) const;

private:
    const TraceDataModel& m_model;
};

}  // namespace View
}  // namespace RocProfVis
