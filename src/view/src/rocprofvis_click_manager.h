// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <map>
#include <vector>
namespace RocProfVis
{
namespace View
{

enum class Layer
{
    kNone = -1,
    kGraphLayer,
    kInteractiveLayer,
    kCount
};

class ClickManager
{
public:
    static ClickManager& GetInstance();

    void  SetLayerClicked(Layer layer);
    Layer GetLayerClicked() const;
    Layer EvaluateClickedLayers();

private:
    ClickManager();
    ~ClickManager()                              = default;
    ClickManager(const ClickManager&)            = delete;
    ClickManager& operator=(const ClickManager&) = delete;

    Layer                 m_layer_clicked;
    std::map<Layer, bool> m_layers_clicked;
};

}  // namespace View
}  // namespace RocProfVis
