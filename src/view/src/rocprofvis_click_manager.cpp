// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

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

}  // namespace View
}  // namespace RocProfVis
