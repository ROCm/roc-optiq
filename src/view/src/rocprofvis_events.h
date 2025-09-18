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
    kNewTableData,
    kTabClosed,
    kTabSelected,
    kTimelineTrackSelectionChanged,
    kTimelineEventSelectionChanged,
    kHandleUserGraphNavigationEvent,
    kTrackMetadataChanged,
    kStickyNoteEdited,
    kFontSizeChanged,
    kSetViewRange,
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
    kTableDataEvent,
    kTabEvent,
    kTimelineTrackSelectionChangedEvent,
    kTimelineEventSelectionChangedEvent,
    kScrollToTrackEvent,
    kStickyNoteEvent,
    kRangeEvent,
#ifdef COMPUTE_UI_SUPPORT
    kComputeTableSearchEvent,
#endif
};

class RocEvent
{
public:
    RocEvent();
    RocEvent(int event_id);
    RocEvent(int event_id, const std::string& src_id);
    virtual ~RocEvent();

    /**
     * @brief Get the event ID, see the RocEvents enum for standard event IDs.
     * @return int Event ID
     */
    int GetId() const;

    /**
     * @brief Set the event ID
     * @param id Event ID
     */
    void SetId(int id);

    /**
     * @brief Get the event type, see the RocEventType enum for standard event types.
     *        Use this to determine the actual derived type of the event object.
     * @return RocEventType Event Type
     */
    RocEventType GetType() const;

    /**
     * @brief Set whether the event can propagate to other listeners.
     *        If set to false, the event will not be propagated further.
     * @param allow True to allow propagation, false to stop propagation.
     */
    void SetAllowPropagate(bool allow);

    /**
     * @brief Stop the propagation of the event to other listeners.
     */
    void StopPropagation();

    /**
     * @brief Check if the event can propagate to other listeners.
     * @return true if the event can propagate, false otherwise.
     */
    bool CanPropagate() const;

    /**
     * @brief Set the source ID of the event.
     *        Set this so that event listeners can filter events based on source.
     * @param source_id Source ID
     */
    void SetSourceId(const std::string& source_id);

    /**
     * @brief Get the source ID of the event.
     * @return std::string Source ID
     */
    const std::string GetSourceId() const;

protected:
    void SetType(RocEventType type);

    int          m_event_id;
    RocEventType m_event_type;
    std::string  m_source_id;

private:
    bool m_allow_propagate;
};

class TrackDataEvent : public RocEvent
{
public:
    TrackDataEvent(int event_id, uint64_t track_id, const std::string& source_id,
                   uint64_t request_id, uint64_t response_code);
    uint64_t           GetTrackID() const;
    uint64_t           GetRequestID() const;
    uint64_t           GetResponseCode() const;

private:
    uint64_t    m_track_id;
    uint64_t    m_request_id;
    uint64_t    m_response_code;
};

class TableDataEvent : public RocEvent
{
public:
    TableDataEvent(const std::string& source_id, uint64_t request_id);
    uint64_t           GetRequestID() const;

private:
    uint64_t    m_request_id;
};

class StickyNoteEvent : public RocEvent
{
public:
    StickyNoteEvent(int id, const std::string& source_id);
    const int GetNoteId() const;

private:
    int m_id;
};

class ScrollToTrackEvent : public RocEvent
{
public:
    ScrollToTrackEvent(int event_id, const uint64_t& track_id,
                       const std::string& source_id);
    const uint64_t     GetTrackID() const;

private:
    uint64_t    m_track_id;
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
    TabEvent(int event_id, const std::string& tab_id, const std::string& source_id);
    const std::string& GetTabId() const;

private:
    std::string m_tab_id;
};

class TrackSelectionChangedEvent : public RocEvent
{
public:
    TrackSelectionChangedEvent(uint64_t track_id, bool selected, const std::string& source_id);
    uint64_t GetTrackID() const;
    bool     TrackSelected() const;

private:
    uint64_t m_track_id;
    bool     m_selected;
};

class EventSelectionChangedEvent : public RocEvent
{
public:
    EventSelectionChangedEvent(uint64_t event_id, uint64_t track_id, bool selected,
                               const std::string& source_id, bool batch = false);
    uint64_t           GetEventID() const;
    uint64_t           GetEventTrackID() const;
    bool               EventSelected() const;
    bool               IsBatch() const;
    
private:
    uint64_t    m_event_id;
    uint16_t    m_event_track_id;
    bool        m_selected;
    bool        m_is_batch;
};

class RangeEvent : public RocEvent
{
public:
    RangeEvent(int event_id, double start_ns, double end_ns,
                      const std::string& source_id);
    double             GetStartNs() const;
    double             GetEndNs() const;

private:
    double      m_start_ns;
    double      m_end_ns;
};

}  // namespace View
}  // namespace RocProfVis
