// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include <map>
#include <string>
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
    kScrubberLayer,
    kCount
};

class TimelineFocusManager
{
public:
    static TimelineFocusManager& GetInstance();

    void  RequestLayerFocus(Layer layer);
    Layer GetFocusedLayer() const;
    Layer EvaluateFocusedLayer();

    // Right-click context menu channel - persists until explicitly cleared
    void  SetRightClickLayer(Layer layer);
    Layer GetRightClickLayer() const;
    void  ClearRightClickLayer();

    void               SetRightClickEventName(const std::string& name);
    const std::string& GetRightClickEventName() const;

private:
    TimelineFocusManager();
    ~TimelineFocusManager()                              = default;
    TimelineFocusManager(const TimelineFocusManager&)            = delete;
    TimelineFocusManager& operator=(const TimelineFocusManager&) = delete;

    Layer                 m_layer_focused;
    std::map<Layer, bool> m_all_layers_focused;
    Layer                 m_right_click_layer;
    std::string           m_right_click_event_name;
};

}  // namespace View
}  // namespace RocProfVis
