// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <cstdint>

namespace RocProfVis
{
namespace View
{

enum class RocEvents
{
    kInvalidEvent = -1,
    kNewTrackData
};

enum class RocEventType
{
    kRocEvent,
    kTrackDataEvent,
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
    TrackDataEvent(int event_id, uint64_t track_index);
    uint64_t GetTrackIndex();

private:
    uint64_t m_track_index;
};

}  // namespace View
}  // namespace RocProfVis
