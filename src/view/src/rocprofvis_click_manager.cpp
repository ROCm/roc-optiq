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
        m_measurement_point1 = { timestamp, duration, track_id, level, name, true };
        m_measurement_state  = MeasurementState::kWaitingForSecond;
    }
    else if(m_measurement_state == MeasurementState::kWaitingForSecond ||
            m_measurement_state == MeasurementState::kComplete)
    {
        m_measurement_point2 = { timestamp, duration, track_id, level, name, true };
        m_measurement_state  = MeasurementState::kComplete;
    }
}

void
TimelineFocusManager::ClearMeasurement()
{
    m_measurement_point1 = {};
    m_measurement_point2 = {};
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

}  // namespace View
}  // namespace RocProfVis
