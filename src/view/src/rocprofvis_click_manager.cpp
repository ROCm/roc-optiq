// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_click_manager.h"

namespace RocProfVis
{
namespace View
{

TimelineFocusManager&
TimelineFocusManager::GetInstance()
{
    static TimelineFocusManager instance;
    return instance;
}

TimelineFocusManager::TimelineFocusManager()
: m_layer_focused(Layer::kNone)
, m_all_layers_focused()
, m_right_click_layer(Layer::kNone)
, m_measurement_state(MeasurementState::kInactive)
, m_points{}
, m_edges{ MeasureEdge::kStart, MeasureEdge::kStart }
, m_freehand_mode(false)
, m_freehand_offsets{ 0.0, 0.0 }
{}

void
TimelineFocusManager::RequestLayerFocus(Layer layer)
{
    m_all_layers_focused[layer] = true;
}
Layer
TimelineFocusManager::EvaluateFocusedLayer()
{
    if(m_all_layers_focused.empty())
    {
        m_layer_focused = Layer::kNone;
    }
    else
    {
        // Take the last entry in the map (highest Layer value)
        m_layer_focused = m_all_layers_focused.rbegin()->first;
    }
    m_all_layers_focused.clear();
    return m_layer_focused;
}

Layer
TimelineFocusManager::GetFocusedLayer() const
{
    return m_layer_focused;
}

void
TimelineFocusManager::SetRightClickLayer(Layer layer)
{
    m_right_click_layer = layer;
}

Layer
TimelineFocusManager::GetRightClickLayer() const
{
    return m_right_click_layer;
}

void
TimelineFocusManager::ClearRightClickLayer()
{
    m_right_click_layer = Layer::kNone;
}

void
TimelineFocusManager::EnterMeasurementMode()
{
    if(m_points[0].valid && m_points[1].valid)
        m_measurement_state = MeasurementState::kComplete;
    else if(m_points[0].valid)
        m_measurement_state = MeasurementState::kWaitingForSecond;
    else
        m_measurement_state = MeasurementState::kWaitingForFirst;
}

void
TimelineFocusManager::ExitMeasurementMode()
{
    m_measurement_state = MeasurementState::kInactive;
}

bool
TimelineFocusManager::IsMeasurementMode() const
{
    return m_measurement_state != MeasurementState::kInactive;
}

MeasurementState
TimelineFocusManager::GetMeasurementState() const
{
    return m_measurement_state;
}

void
TimelineFocusManager::SetMeasurementPoint(double timestamp, double duration,
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
TimelineFocusManager::SetFreehandMeasurementPoint(double timestamp)
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
TimelineFocusManager::ClearMeasurement()
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
TimelineFocusManager::GetPoint(int index) const
{
    return m_points[index & 1];
}

MeasureEdge
TimelineFocusManager::GetEdge(int index) const
{
    return m_edges[index & 1];
}

void
TimelineFocusManager::SetEdge(int index, MeasureEdge edge)
{
    m_edges[index & 1]           = edge;
    m_freehand_offsets[index & 1] = 0.0;
}

double
TimelineFocusManager::GetEffectiveTimestamp(int index) const
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

void   TimelineFocusManager::SetFreehandMode(bool enabled)    { m_freehand_mode = enabled; }
bool   TimelineFocusManager::IsFreehandMode() const           { return m_freehand_mode; }

void
TimelineFocusManager::SetFreehandOffset(int index, double offset)
{
    m_freehand_offsets[index & 1] = offset;
}

double
TimelineFocusManager::GetFreehandOffset(int index) const
{
    return m_freehand_offsets[index & 1];
}

}  // namespace View
}  // namespace RocProfVis
