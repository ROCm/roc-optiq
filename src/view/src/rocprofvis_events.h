// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
namespace RocProfVis
{
namespace View
{

enum class RocEvents
{
    kInvalidEvent = -1,
    kNewTrackData,
    kTabClosed,
    kTimelineSelectionChanged,
    kHandleUserGraphNavigationEvent,
#ifdef COMPUTE_UI_SUPPORT
    kComputeDataDirty,
    kComputeBlockNavigationChanged,
    kComputeTableSearchChanged,
#endif
};

enum class RocEventType
{
    kRocEvent,
    kTrackDataEvent,
    kTabEvent,
    kTimelineSelectionChangedEvent,
    kScrollToTrackByNameEvent,
#ifdef  COMPUTE_UI_SUPPORT
    kComputeTableSearchEvent,
#endif
};

class RocEvent
{
public:
    RocEvent();
    RocEvent(int event_id);
    virtual ~RocEvent();

    virtual int          GetId();
    virtual void         SetId(int id);
    virtual RocEventType GetType();
    virtual void         SetType(RocEventType type);
    virtual void         SetAllowPropagate(bool allow);
    void                 StopPropagation();
    bool                 CanPropagate() const;

protected:
    int          m_event_id;
    RocEventType m_event_type;

private:
    bool m_allow_propagate;
};

class TrackDataEvent : public RocEvent
{
public:
    TrackDataEvent(int event_id, uint64_t track_index, const std::string& trace_path);
    uint64_t           GetTrackIndex();
    const std::string& GetTracePath();

private:
    uint64_t    m_track_index;
    std::string m_trace_path;
};

class ScrollToTrackByNameEvent : public RocEvent
{
public:
    ScrollToTrackByNameEvent(int event_id, const std::string& track_name)
    : RocEvent(event_id)
    , m_track_name(track_name)
    {
        m_event_type = RocEventType::kScrollToTrackByNameEvent;
    }
    const std::string& GetTrackName() const { return m_track_name; }

private:
    std::string m_track_name;
};
#ifdef COMPUTE_UI_SUPPORT
class ComputeTableSearchEvent : public RocEvent
{
public:
    ComputeTableSearchEvent(int event_id, std::string& term);
    const std::string GetSearchTerm();

private:
    std::string m_search_term;
};
#endif
class TabClosedEvent : public RocEvent
{
public:
    TabClosedEvent(int event_id, const std::string& tab_id);
    const std::string& GetTabId();

private:
    std::string m_tab_id;
};

class TrackSelectionChangedEvent : public RocEvent
{
public:
    TrackSelectionChangedEvent(int event_id, std::vector<uint64_t> selected_tracks,
                               double start_ns, double end_ns);
    const std::vector<uint64_t>& GetSelectedTracks() const;
    double                       GetStartNs() const;
    double                       GetEndNs() const;

private:
    std::vector<uint64_t> m_selected_tracks;  // indices of selected tracks
    double                m_start_ns;         // start time in nanoseconds
    double                m_end_ns;           // end time in nanoseconds
};

}  // namespace View
}  // namespace RocProfVis
