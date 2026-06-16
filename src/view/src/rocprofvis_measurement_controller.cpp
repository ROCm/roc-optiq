// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_measurement_controller.h"

namespace RocProfVis
{
namespace View
{

MeasurementController::MeasurementController()
: m_measurement_state(MeasurementState::kInactive)
, m_points{}
, m_edges{ MeasureEdge::kStart, MeasureEdge::kStart }
, m_freehand_mode(false)
, m_freehand_offsets{ 0.0, 0.0 }
{}

void
MeasurementController::EnterMeasurementMode()
{
    if(m_points[0].valid && m_points[1].valid)
        m_measurement_state = MeasurementState::kComplete;
    else if(m_points[0].valid)
        m_measurement_state = MeasurementState::kWaitingForSecond;
    else
        m_measurement_state = MeasurementState::kWaitingForFirst;
}

void
MeasurementController::ExitMeasurementMode()
{
    m_measurement_state = MeasurementState::kInactive;
}

bool
MeasurementController::IsMeasurementMode() const
{
    return m_measurement_state != MeasurementState::kInactive;
}

MeasurementState
MeasurementController::GetMeasurementState() const
{
    return m_measurement_state;
}

void
MeasurementController::SetMeasurementPoint(double timestamp, double duration,
                                           uint64_t track_id, uint32_t level,
                                           const std::string& name, uint64_t event_uuid)
{
    if(m_measurement_state == MeasurementState::kWaitingForFirst)
    {
        m_points[0]         = { timestamp, duration, track_id, level, name, event_uuid, true, false };
        m_measurement_state = MeasurementState::kWaitingForSecond;
    }
    else if(m_measurement_state == MeasurementState::kWaitingForSecond ||
            m_measurement_state == MeasurementState::kComplete)
    {
        m_points[1]         = { timestamp, duration, track_id, level, name, event_uuid, true, false };
        m_measurement_state = MeasurementState::kComplete;
    }
}

void
MeasurementController::SetFreehandMeasurementPoint(double timestamp)
{
    int slot = 0;
    if(m_measurement_state == MeasurementState::kWaitingForFirst)
    {
        slot                = 0;
        m_measurement_state = MeasurementState::kWaitingForSecond;
    }
    else if(m_measurement_state == MeasurementState::kWaitingForSecond ||
            m_measurement_state == MeasurementState::kComplete)
    {
        slot                = 1;
        m_measurement_state = MeasurementState::kComplete;
    }
    else
    {
        return;
    }

    m_points[slot]           = MeasurementPoint{};
    m_points[slot].timestamp = timestamp;
    m_points[slot].valid     = true;
    m_points[slot].freehand  = true;
    m_freehand_offsets[slot] = 0.0;
}

void
MeasurementController::ClearMeasurement()
{
    bool was_active       = m_measurement_state != MeasurementState::kInactive;
    m_points[0]           = {};
    m_points[1]           = {};
    m_freehand_offsets[0] = 0.0;
    m_freehand_offsets[1] = 0.0;
    m_measurement_state   = was_active ? MeasurementState::kWaitingForFirst
                                       : MeasurementState::kInactive;
}

const MeasurementPoint&
MeasurementController::GetPoint(int index) const
{
    return m_points[index & 1];
}

MeasureEdge
MeasurementController::GetEdge(int index) const
{
    return m_edges[index & 1];
}

void
MeasurementController::SetEdge(int index, MeasureEdge edge)
{
    m_edges[index & 1]            = edge;
    m_freehand_offsets[index & 1] = 0.0;
}

double
MeasurementController::GetEffectiveTimestamp(int index) const
{
    int i = index & 1;
    if(!m_points[i].valid) return 0.0;
    if(m_points[i].freehand)
        return m_points[i].timestamp + m_freehand_offsets[i];

    double base = (m_edges[i] == MeasureEdge::kStart)
                      ? m_points[i].timestamp
                      : m_points[i].timestamp + m_points[i].duration;
    return base + m_freehand_offsets[i];
}

void   MeasurementController::SetFreehandMode(bool enabled) { m_freehand_mode = enabled; }
bool   MeasurementController::IsFreehandMode() const        { return m_freehand_mode; }

void
MeasurementController::SetFreehandOffset(int index, double offset)
{
    m_freehand_offsets[index & 1] = offset;
}

double
MeasurementController::GetFreehandOffset(int index) const
{
    return m_freehand_offsets[index & 1];
}

}  // namespace View
}  // namespace RocProfVis
