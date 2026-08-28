// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_mem_mgmt.h"
#include <bitset>
#include <condition_variable>
#include <map>
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <shared_mutex>

namespace RocProfVis
{
namespace Controller
{

class Array;
class Event;
class Sample;
class SystemTrace;
class SegmentTimeline;
class Future;

constexpr double kSegmentDuration = 1000000000.0;
constexpr double kScalableSegmentDuration   = 10000.0;
constexpr uint32_t kSegmentBitSetSize = 64;
struct SegmentLRUParams
{
    SystemTrace* m_ctx;
    SegmentTimeline* m_owner;
    uint32_t m_lod;
};

struct SegmentItemKey
{
    double  m_timestamp;
    uint8_t m_level;

    SegmentItemKey(double timestamp, uint8_t level)
    : m_timestamp(timestamp)
    , m_level(level) {};

    bool operator<(const SegmentItemKey& other) const
    {
        if(m_timestamp != other.m_timestamp) return m_timestamp < other.m_timestamp;
        return m_level < other.m_level;
    }
};

class Segment
{
    using rocprofvis_timeline_iterator_t = std::map<double, std::unique_ptr<Segment>>::iterator;
    using rocprofvis_lru_iterator_t = std::unordered_map<Segment*, std::unique_ptr<LRUMember>>::iterator;
public:
    Segment() = delete;

    Segment(rocprofvis_controller_track_type_t type, SegmentTimeline* ctx);

    ~Segment();

    double GetStartTimestamp();
    double GetEndTimestamp();
    double GetMinTimestamp();
    double GetMaxTimestamp();

    void SetStartEndTimestamps(double start, double end);

    void SetMinTimestamp(double value);

    void SetMaxTimestamp(double value);

    void Insert(double timestamp, uint8_t level, Handle* event);

    rocprofvis_result_t Fetch(double start, double end, std::vector<Data>& array, uint64_t& index, std::unordered_set<uint64_t>* event_id_set, SegmentLRUParams* lru_params);

    rocprofvis_result_t GetMemoryUsage(uint64_t* value, rocprofvis_common_property_t property);

    size_t              GetNumEntries();
    void                SetTimelineIterator(rocprofvis_timeline_iterator_t timeline_iterator);
    rocprofvis_timeline_iterator_t& GetTimelineIterator(void);


private:
    SegmentTimeline* m_ctx;
    std::map<uint8_t, std::map<double, Handle*>>  m_entries;
    double m_start_timestamp;
    double m_end_timestamp;
    double m_min_timestamp;
    double m_max_timestamp;
    rocprofvis_controller_track_type_t m_type;
    rocprofvis_timeline_iterator_t  m_timeline_iterator;
    std::shared_mutex               m_mutex;
};

typedef rocprofvis_result_t (*FetchSegmentsFunc)(double start, double end, Segment& segment, void* user_ptr, SegmentTimeline* owner); 

class SegmentTimeline
{
    SegmentTimeline(SegmentTimeline const& other) = delete;
    SegmentTimeline& operator=(SegmentTimeline const& other) = delete;

public:
    SegmentTimeline();
    ~SegmentTimeline();
    SegmentTimeline(SegmentTimeline&& other);
    SegmentTimeline& operator=(SegmentTimeline&& other);

    void Init(double start_time, double segment_duration, uint32_t num_segments, size_t num_items);
    void SetContext(Handle* ctx);
    Handle * GetContext();

    rocprofvis_result_t FetchSegments(double start, double end, void* user_ptr, Future* future, FetchSegmentsFunc func);
    rocprofvis_result_t Remove(Segment* segment);
    rocprofvis_result_t Insert(double segment_start, std::unique_ptr<Segment>&& segment);
    std::map<double, std::unique_ptr<Segment>>& GetSegments();
    bool IsValid(uint32_t segment_index) const;
    void SetValid(uint32_t segment_index, bool state);
    bool IsProcessed(uint32_t segment_index) const;
    void SetProcessed(uint32_t segment_index, bool state);
    std::shared_mutex* GetMutex();
    double GetSegmentDuration() const;
    size_t GetMaxNumItems() const;

private:
    std::map<double, std::unique_ptr<Segment>> m_segments;
    BitSet                                     m_valid_segments;
    BitSet                                     m_processed_segments;
    double                                     m_segment_start_time;
    double                                     m_segment_duration;
    uint32_t                                   m_num_segments;
    size_t                                     m_max_num_items;
    Handle*                                    m_ctx;
    mutable std::shared_mutex                  m_mutex;

};

// How long a fetch path sleeps between re-checks while another request holds the
// claim on a segment it needs.
constexpr uint32_t kSegmentClaimPollMs = 50;

// Blocks until no segment in [first_segment, last_segment) is claimed by another
// request. Returns false if `future` was cancelled while waiting. The caller must
// own `lock`; it is released while waiting and re-acquired before returning.
bool wait_for_segment_claims(SegmentTimeline& timeline, std::condition_variable_any& cv,
                             std::unique_lock<std::shared_mutex>& lock,
                             uint32_t first_segment, uint32_t last_segment,
                             Future* future);

// Owns the "processed" claim a fetch path takes on a set of segment ranges.
//
// Track::FetchSegments and Graph::GenerateLOD mark segments processed to reserve
// them, read from the model with the timeline unlocked, then clear the claim. If a
// claim is ever dropped without being cleared - an early return, or an allocation
// failure unwinding the stack - every later request for those segments waits on the
// condition variable forever. Making the claim an object means that cannot happen.
class SegmentClaim
{
    SegmentClaim(SegmentClaim const& other) = delete;
    SegmentClaim& operator=(SegmentClaim const& other) = delete;

public:
    SegmentClaim(SegmentTimeline& timeline, std::condition_variable_any& cv,
                 std::vector<std::pair<uint32_t, uint32_t>> const& ranges);
    ~SegmentClaim();

    // Clears the next claimed range and publishes whether its data loaded. Takes
    // the timeline lock and wakes anything waiting on the claim.
    void ReleaseNext(bool valid);

private:
    SegmentTimeline&                                  m_timeline;
    std::condition_variable_any&                      m_cv;
    std::vector<std::pair<uint32_t, uint32_t>> const& m_ranges;
    size_t                                            m_released;
};

}
}
