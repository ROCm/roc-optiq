// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface.h"
#include <vector>
#include <set>
#include <map>
#include <mutex>

namespace RocProfVis
{
namespace Controller
{

class Array;
class Future;
class Track;
class Graph;
class Timeline;
class Event;
class Segment;

#define TRACE_MEMORY_USAGE_LIMIT 10000000
struct LRUMember
{
    uint64_t  timestamp;
    Track* track;
    Segment* reference;

    // Logic for descending sort
    bool operator<(const LRUMember& other) const { return timestamp > other.timestamp; }
};

class Trace : public Handle
{
public:
    Trace();

    virtual ~Trace();

    rocprofvis_result_t Load(char const* const filename, Future& future);

    rocprofvis_result_t AsyncFetch(Track& track, Future& future, Array& array,
                                   double start, double end);

    rocprofvis_result_t AsyncFetch(Graph& graph, Future& future, Array& array,
                                   double start, double end, uint32_t pixels);

    rocprofvis_result_t AsyncFetch(Event& event, Future& future, Array& array,
                                   rocprofvis_property_t property);

    rocprofvis_result_t AddLRUReference(Track* owner, Segment* reference);
    rocprofvis_result_t ManageLRU(Track* requestor);

    rocprofvis_controller_object_type_t GetType(void) final;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final;

private:
    std::vector<Track*> m_tracks;
    uint64_t m_id;
    Timeline* m_timeline;
    rocprofvis_dm_trace_t m_dm_handle;
    std::set<LRUMember> m_lru_array;
    std::map<Segment*, std::set<LRUMember>::iterator> m_lru_lookup;
    std::mutex m_lru_mutex;

private:
#ifdef JSON_SUPPORT
    rocprofvis_result_t LoadJson(char const* const filename);
#endif
    rocprofvis_result_t LoadRocpd(char const* const filename);
};

}
}
