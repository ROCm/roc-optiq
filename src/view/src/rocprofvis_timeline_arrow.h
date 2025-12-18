// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_view_structs.h"
#include <memory>
#include "rocprofvis_time_to_pixel.h"

namespace RocProfVis
{
namespace View
{

enum class FlowDisplayMode
{
    kShowAll,
    kHide,
    __kLastMode = kHide
};

class DataProvider;
class TimelineSelection;
struct event_info_t;
class TimePixelTransform;

class TimelineArrow
{
public:
    enum class RenderStyle
    {
        kFan,
        kChain
    };

    void            SetFlowDisplayMode(FlowDisplayMode mode);
    FlowDisplayMode GetFlowDisplayMode() const;
    RenderStyle     GetRenderStyle() const;
    void            SetRenderStyle(RenderStyle style);
    TimelineArrow(DataProvider&                      data_provider,
                  std::shared_ptr<TimelineSelection> selection);
    ~TimelineArrow();
    // Draws an arrow from (start_time, y_start) to (end_time, y_end) using the given
    // mapping
    void Render(ImDrawList* draw_list, const ImVec2 window,
                const std::unordered_map<uint64_t, float>&             track_position_y,
                const std::shared_ptr<std::vector<rocprofvis_graph_t>> graphs,
                std::shared_ptr<TimePixelTransform>                    tpt) const;

private:
    void HandleEventSelectionChanged(std::shared_ptr<RocEvent> e);

    DataProvider&                      m_data_provider;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    EventManager::SubscriptionToken    m_selection_changed_token;
    std::vector<const event_info_t*>   m_selected_event_data;
    FlowDisplayMode                    m_flow_display_mode = FlowDisplayMode::kShowAll;
    std::unordered_map<uint64_t, std::vector<event_flow_data_t>*> m_arrow_dictionary;
    RenderStyle                                                   m_render_style;
};

}  // namespace View
}  // namespace RocProfVis