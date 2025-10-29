// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_click_manager.h"

namespace RocProfVis
{
namespace View
{

ClickManager&
ClickManager::GetInstance()
{
    static ClickManager instance;
    return instance;
}

ClickManager::ClickManager()
: m_layer_clicked(Layer::kNone)
, m_layers_clicked()

{}

void
ClickManager::SetLayerClicked(Layer layer)
{
    m_layers_clicked[layer] = m_layers_clicked[layer] || true;
}
Layer
ClickManager::EvaluateClickedLayers()
{
    if(m_layers_clicked.empty())
    {
        m_layer_clicked = Layer::kNone;
    }
    else
    {
        // Take the last entry in the map (highest Layer value)
        m_layer_clicked = m_layers_clicked.rbegin()->first;
    }
    m_layers_clicked.clear();
    return m_layer_clicked;
}

Layer
ClickManager::GetLayerClicked() const
{
    return m_layer_clicked;
}

}  // namespace View
}  // namespace RocProfVis
