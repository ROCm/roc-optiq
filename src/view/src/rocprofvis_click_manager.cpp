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
    m_right_click_event_name.clear();
}

void
TimelineFocusManager::SetRightClickEventName(const std::string& name)
{
    m_right_click_event_name = name;
}

const std::string&
TimelineFocusManager::GetRightClickEventName() const
{
    return m_right_click_event_name;
}

}  // namespace View
}  // namespace RocProfVis
