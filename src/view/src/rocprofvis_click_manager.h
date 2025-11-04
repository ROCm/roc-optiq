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
    kPopOut,
    kCount
};

class TimelineFocusManager
{
public:
    static TimelineFocusManager& GetInstance();

    void  RequestLayerFocus(Layer layer);
    Layer GetFocusedLayer() const;
    Layer EvaluateFocusedLayer();

private:
    TimelineFocusManager();
    ~TimelineFocusManager()                                      = default;
    TimelineFocusManager(const TimelineFocusManager&)            = delete;
    TimelineFocusManager& operator=(const TimelineFocusManager&) = delete;

    Layer                 m_layer_focused;
    std::map<Layer, bool> m_all_layers_focused;
};

}  // namespace View
}  // namespace RocProfVis
