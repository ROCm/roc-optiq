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
    kTabSelected,
    kTimelineTrackSelectionChanged,
    kTimelineEventSelectionChanged,
    kHandleUserGraphNavigationEvent,
    kTrackMetadataChanged,
    kStickyNoteEdited,
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
    kTimelineTrackSelectionChangedEvent,
    kTimelineEventSelectionChangedEvent,
    kScrollToTrackEvent,
    kStickyNoteEvent,
#ifdef COMPUTE_UI_SUPPORT
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
    TrackDataEvent(int event_id, uint64_t track_id, const std::string& trace_path,
                   uint64_t request_id, uint64_t response_code);
    uint64_t           GetTrackID() const;
    const std::string& GetTracePath() const;
    uint64_t           GetRequestID() const;
    uint64_t           GetResponseCode() const;

private:
    uint64_t    m_track_id;
    std::string m_trace_path;
    uint64_t    m_request_id;
    uint64_t    m_response_code;
};

class StickyNoteEvent : public RocEvent
{
public:
    StickyNoteEvent(int id);
    const int GetID();

private:
    int m_id;
};

class ScrollToTrackEvent : public RocEvent
{
public:
    ScrollToTrackEvent(int event_id, const uint64_t& track_id);
    const uint64_t GetTrackID() const;

private:
    uint64_t m_track_id;
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
class TabEvent : public RocEvent
{
public:
    TabEvent(int event_id, const std::string& tab_id, const std::string& tab_source);
    const std::string& GetTabId() const;
    const std::string& GetTabSource() const;

private:
    std::string m_tab_id;
    std::string m_tab_source;
};

class TrackSelectionChangedEvent : public RocEvent
{
public:
    TrackSelectionChangedEvent(std::vector<uint64_t> selected_tracks, double start_ns,
                               double end_ns, const std::string& trace_path);
    const std::vector<uint64_t>& GetSelectedTracks() const;
    double                       GetStartNs() const;
    double                       GetEndNs() const;
    const std::string&           GetTracePath();

private:
    std::vector<uint64_t> m_selected_tracks;  // IDs of selected tracks
    double                m_start_ns;         // start time in nanoseconds
    double                m_end_ns;           // end time in nanoseconds
    std::string           m_trace_path;
};

class EventSelectionChangedEvent : public RocEvent
{
public:
    EventSelectionChangedEvent(uint64_t event_id, uint64_t track_id, bool selected,
                               const std::string& trace_path);
    uint64_t           GetEventID() const;
    uint64_t           GetEventTrackID() const;
    bool               EventSelected() const;
    const std::string& GetTracePath();

private:
    uint64_t    m_event_id;
    uint16_t    m_event_track_id;
    bool        m_selected;
    std::string m_trace_path;
};

}  // namespace View
}  // namespace RocProfVis
