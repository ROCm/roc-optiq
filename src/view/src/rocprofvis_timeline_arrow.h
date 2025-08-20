// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_event_manager.h"
#include <memory>

namespace RocProfVis
{
namespace View
{

enum class FlowDisplayMode
{
    ShowAll,
    ShowFirstAndLast,
    Hide
};

class DataProvider;
class TimelineSelection;
struct event_info_t;

class TimelineArrow
{
public:
    void            SetFlowDisplayMode(FlowDisplayMode mode);
    FlowDisplayMode GetFlowDisplayMode() const;
    TimelineArrow(DataProvider&                      data_provider,
                  std::shared_ptr<TimelineSelection> selection);
    ~TimelineArrow();
    // Draws an arrow from (start_time, y_start) to (end_time, y_end) using the given
    // mapping
    void Render(ImDrawList* draw_list, double v_min_x, double pixels_per_ns,
                ImVec2 window, std::map<uint64_t, float>& track_height_total);

private:
    void HandleEventSelectionChanged(std::shared_ptr<RocEvent> e);

    DataProvider&                      m_data_provider;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    EventManager::SubscriptionToken    m_selection_changed_token;
    std::vector<const event_info_t*>   m_selected_event_data;
    FlowDisplayMode                    m_flow_display_mode = FlowDisplayMode::ShowAll;
};

}  // namespace View
}  // namespace RocProfVis