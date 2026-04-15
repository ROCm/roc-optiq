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
, m_measure_edge1(MeasureEdge::kStart)
, m_measure_edge2(MeasureEdge::kStart)
, m_freehand_mode(false)
, m_freehand_offset1(0.0)
, m_freehand_offset2(0.0)
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
    m_measurement_state  = MeasurementState::kWaitingForFirst;
    m_measurement_point1 = {};
    m_measurement_point2 = {};
    m_freehand_offset1   = 0.0;
    m_freehand_offset2   = 0.0;
}

void
TimelineFocusManager::ExitMeasurementMode()
{
    m_measurement_state  = MeasurementState::kInactive;
    m_measurement_point1 = {};
    m_measurement_point2 = {};
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
                                          const std::string& name)
{
    if(m_measurement_state == MeasurementState::kWaitingForFirst)
    {
        m_measurement_point1 = { timestamp, duration, track_id, level, name, true, false };
        m_measurement_state  = MeasurementState::kWaitingForSecond;
    }
    else if(m_measurement_state == MeasurementState::kWaitingForSecond ||
            m_measurement_state == MeasurementState::kComplete)
    {
        m_measurement_point2 = { timestamp, duration, track_id, level, name, true, false };
        m_measurement_state  = MeasurementState::kComplete;
    }
}

void
TimelineFocusManager::SetFreehandMeasurementPoint(double timestamp)
{
    if(m_measurement_state == MeasurementState::kWaitingForFirst)
    {
        m_measurement_point1 = { timestamp, 0.0, 0, 0, {}, true, true };
        m_freehand_offset1   = 0.0;
        m_measurement_state  = MeasurementState::kWaitingForSecond;
    }
    else if(m_measurement_state == MeasurementState::kWaitingForSecond ||
            m_measurement_state == MeasurementState::kComplete)
    {
        m_measurement_point2 = { timestamp, 0.0, 0, 0, {}, true, true };
        m_freehand_offset2   = 0.0;
        m_measurement_state  = MeasurementState::kComplete;
    }
}

void
TimelineFocusManager::ClearMeasurement()
{
    m_measurement_point1 = {};
    m_measurement_point2 = {};
    m_freehand_offset1   = 0.0;
    m_freehand_offset2   = 0.0;
    if(m_measurement_state == MeasurementState::kComplete)
    {
        m_measurement_state = MeasurementState::kWaitingForFirst;
    }
}

const MeasurementPoint&
TimelineFocusManager::GetMeasurementPoint1() const
{
    return m_measurement_point1;
}

const MeasurementPoint&
TimelineFocusManager::GetMeasurementPoint2() const
{
    return m_measurement_point2;
}

void TimelineFocusManager::SetMeasureEdge1(MeasureEdge edge)
{ m_measure_edge1 = edge; m_freehand_offset1 = 0.0; }

void TimelineFocusManager::SetMeasureEdge2(MeasureEdge edge)
{ m_measure_edge2 = edge; m_freehand_offset2 = 0.0; }

MeasureEdge TimelineFocusManager::GetMeasureEdge1() const { return m_measure_edge1; }
MeasureEdge TimelineFocusManager::GetMeasureEdge2() const { return m_measure_edge2; }

double
TimelineFocusManager::GetEffectiveTimestamp1() const
{
    if(!m_measurement_point1.valid) return 0.0;
    if(m_measurement_point1.freehand)
        return m_measurement_point1.timestamp + m_freehand_offset1;
    double base = (m_measure_edge1 == MeasureEdge::kStart)
                      ? m_measurement_point1.timestamp
                      : m_measurement_point1.timestamp + m_measurement_point1.duration;
    return base + m_freehand_offset1;
}

double
TimelineFocusManager::GetEffectiveTimestamp2() const
{
    if(!m_measurement_point2.valid) return 0.0;
    if(m_measurement_point2.freehand)
        return m_measurement_point2.timestamp + m_freehand_offset2;
    double base = (m_measure_edge2 == MeasureEdge::kStart)
                      ? m_measurement_point2.timestamp
                      : m_measurement_point2.timestamp + m_measurement_point2.duration;
    return base + m_freehand_offset2;
}

void  TimelineFocusManager::SetFreehandMode(bool enabled) { m_freehand_mode = enabled; }
bool  TimelineFocusManager::IsFreehandMode() const { return m_freehand_mode; }
void  TimelineFocusManager::SetFreehandOffset1(double o) { m_freehand_offset1 = o; }
void  TimelineFocusManager::SetFreehandOffset2(double o) { m_freehand_offset2 = o; }
double TimelineFocusManager::GetFreehandOffset1() const { return m_freehand_offset1; }
double TimelineFocusManager::GetFreehandOffset2() const { return m_freehand_offset2; }

}  // namespace View
}  // namespace RocProfVis
