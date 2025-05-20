// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <cstdint>
#include <string>

namespace RocProfVis
{
namespace View
{

enum class RocEvents
{
    kInvalidEvent = -1,
    kNewTrackData,
    kComputeBlockNavigationChanged,
    kTabClosed
};

enum class RocEventType
{
    kRocEvent,
    kTrackDataEvent,
    kComputeBlockNavigationEvent,
    kTabEvent
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
    const std::string& TrackDataEvent::GetTracePath();

private:
    uint64_t    m_track_index;
    std::string m_trace_path;
};

class ComputeBlockNavigationEvent : public RocEvent
{
public:
    ComputeBlockNavigationEvent(int event_id, int level, int block);
    const int GetLevel();
    const int GetBlock();

private:
    int m_level;
    int m_block;
};

class TabClosedEvent : public RocEvent
{
public:
    TabClosedEvent(int event_id, const std::string& tab_id);
    const std::string& GetTabId();  
private:
    std::string m_tab_id;
};

}  // namespace View
}  // namespace RocProfVis
