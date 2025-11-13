// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#include "rocprofvis_timeline_arrow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_track_item.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

void
TimelineArrow::SetFlowDisplayMode(FlowDisplayMode mode)
{
    m_flow_display_mode = mode;
}

FlowDisplayMode
TimelineArrow::GetFlowDisplayMode() const
{
    return m_flow_display_mode;
}

TimelineArrow::RenderStyle
TimelineArrow::GetRenderStyle() const
{
    return m_render_style;
}

void
TimelineArrow::SetRenderStyle(RenderStyle style)
{
    m_render_style = style;
}

void
TimelineArrow::Render(ImDrawList* draw_list, const double v_min_x,
                      const double pixels_per_ns, const ImVec2 window,
                      const std::unordered_map<uint64_t, float>& track_position_y,
                      const std::shared_ptr<std::vector<rocprofvis_graph_t>>     graphs) const
{
    if(m_flow_display_mode == FlowDisplayMode::kHide)
        return;

    SettingsManager& settings     = SettingsManager::GetInstance();
    ImU32            color        = settings.GetColor(Colors::kArrowColor);
    float            thickness    = 2.0f;
    float            head_size    = 8.0f;
    float            level_height = settings.GetEventLevelHeight();
    for(const event_info_t* event : m_selected_event_data)
    {
        if(!event || event->flow_info.size() < 2)
            continue;

        const std::vector<event_flow_data_t>& flows = event->flow_info;

        if(m_render_style == RenderStyle::kFan)
        {
            // True view: origin + multiple targets
            const event_flow_data_t& origin = flows[0];
            const track_info_t*      origin_track_info =
                m_data_provider.GetTrackInfo(origin.track_id);
            if(!origin_track_info)
                continue;
            const rocprofvis_graph_t& origin_track = (*graphs)[origin_track_info->index];
            if(origin_track.chart->IsCompactMode())
            {
                level_height = settings.GetEventLevelCompactHeight();
            }

            float origin_x = (origin.end_timestamp - v_min_x) * pixels_per_ns;
            float origin_y = track_position_y.at(origin.track_id) +
                             std::min(level_height * origin.level - level_height / 2,
                                      origin_track.chart->GetTrackHeight());
            ImVec2 p_origin = ImVec2(window.x + origin_x, window.y + origin_y);

            for(size_t i = 1; i < flows.size(); ++i)
            {
                const event_flow_data_t& target = flows[i];
                const track_info_t*      target_track_info =
                    m_data_provider.GetTrackInfo(target.track_id);
                if(!target_track_info)
                    continue;
                const rocprofvis_graph_t& target_track = (*graphs)[target_track_info->index];
                if(!target_track.display)
                    continue;

                float target_x = (target.start_timestamp - v_min_x) * pixels_per_ns;
                float target_y = track_position_y.at(target.track_id) +
                                 std::min(level_height * target.level - level_height / 2,
                                          target_track.chart->GetTrackHeight());
                ImVec2 p_target = ImVec2(window.x + target_x, window.y + target_y);

                if(p_origin.x == p_target.x && p_origin.y == p_target.y)
                    continue;

                float  curve_offset = 0.25f * (p_target.x - p_origin.x);
                ImVec2 p_ctrl1      = ImVec2(p_origin.x + curve_offset, p_origin.y);
                ImVec2 p_ctrl2      = ImVec2(p_target.x - curve_offset, p_target.y);

                draw_list->AddBezierCubic(p_origin, p_ctrl1, p_ctrl2, p_target, color,
                                          thickness, 32); //TODO: 32 - magic value should be a constant

                ImVec2 dir = ImVec2(p_target.x - p_ctrl2.x, p_target.y - p_ctrl2.y);
                float  len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                if(len > 0.0f)
                {
                    dir.x /= len;
                    dir.y /= len;
                }
                else
                {
                    dir = ImVec2(p_target.x - p_origin.x, p_target.y - p_origin.y);
                    len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                    if(len > 0.0f)
                    {
                        dir.x /= len;
                        dir.y /= len;
                    }
                    else
                    {
                        dir = ImVec2(1.0f, 0.0f);
                    }
                }
                ImVec2 ortho(-dir.y, dir.x);

                ImVec2 p1 = p_target;
                ImVec2 p2 =
                    ImVec2(p_target.x - dir.x * head_size - ortho.x * head_size * 0.5f,
                           p_target.y - dir.y * head_size - ortho.y * head_size * 0.5f);
                ImVec2 p3 =
                    ImVec2(p_target.x - dir.x * head_size + ortho.x * head_size * 0.5f,
                           p_target.y - dir.y * head_size + ortho.y * head_size * 0.5f);
                draw_list->AddTriangleFilled(p1, p2, p3, color);
            }
        }
        else
        {
            // Legacy view: consecutive pairs
            for(size_t i = 0; i + 1 < flows.size(); ++i)
            {
                const event_flow_data_t& from = flows[i];
                const event_flow_data_t& to   = flows[i + 1];

                const track_info_t* from_track_info =
                    m_data_provider.GetTrackInfo(from.track_id);
                const track_info_t* to_track_info =
                    m_data_provider.GetTrackInfo(to.track_id);
                if(!from_track_info || !to_track_info)
                    continue;

                const rocprofvis_graph_t& from_track = (*graphs)[from_track_info->index];
                const rocprofvis_graph_t& to_track   = (*graphs)[to_track_info->index];

                if(!from_track.display || !to_track.display)
                    continue;

                float from_x = (from.end_timestamp - v_min_x) * pixels_per_ns;
                float from_y = track_position_y.at(from.track_id) +
                               std::min(level_height * from.level,
                                        from_track.chart->GetTrackHeight());
                ImVec2 p_from = ImVec2(window.x + from_x, window.y + from_y);

                float to_x = (to.start_timestamp - v_min_x) * pixels_per_ns;
                float to_y =
                    track_position_y.at(to.track_id) +
                    std::min(level_height * to.level, to_track.chart->GetTrackHeight());
                ImVec2 p_to = ImVec2(window.x + to_x, window.y + to_y);

                if(p_from.x == p_to.x && p_from.y == p_to.y)
                    continue;

                float  curve_offset = 0.25f * (p_to.x - p_from.x);
                ImVec2 p_ctrl1      = ImVec2(p_from.x + curve_offset, p_from.y);
                ImVec2 p_ctrl2      = ImVec2(p_to.x - curve_offset, p_to.y);

                draw_list->AddBezierCubic(p_from, p_ctrl1, p_ctrl2, p_to, color,
                                          thickness, 32);

                ImVec2 dir = ImVec2(p_to.x - p_ctrl2.x, p_to.y - p_ctrl2.y);
                float  len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                if(len > 0.0f)
                {
                    dir.x /= len;
                    dir.y /= len;
                }
                else
                {
                    dir = ImVec2(p_to.x - p_from.x, p_to.y - p_from.y);
                    len = sqrtf(dir.x * dir.x + dir.y * dir.y);
                    if(len > 0.0f)
                    {
                        dir.x /= len;
                        dir.y /= len;
                    }
                    else
                    {
                        dir = ImVec2(1.0f, 0.0f);
                    }
                }
                ImVec2 ortho(-dir.y, dir.x);

                ImVec2 p1 = p_to;
                ImVec2 p2 =
                    ImVec2(p_to.x - dir.x * head_size - ortho.x * head_size * 0.5f,
                           p_to.y - dir.y * head_size - ortho.y * head_size * 0.5f);
                ImVec2 p3 =
                    ImVec2(p_to.x - dir.x * head_size + ortho.x * head_size * 0.5f,
                           p_to.y - dir.y * head_size + ortho.y * head_size * 0.5f);
                draw_list->AddTriangleFilled(p1, p2, p3, color);
            }
        }
    }
}

TimelineArrow::TimelineArrow(DataProvider&                      data_provider,
                             std::shared_ptr<TimelineSelection> selection)
: m_data_provider(data_provider)
, m_timeline_selection(selection)
, m_selection_changed_token(-1)
, m_flow_display_mode(FlowDisplayMode::kShowAll)
, m_render_style(RenderStyle::kFan)
{
    auto scroll_to_arrow_handler = [this](std::shared_ptr<RocEvent> e) {
        this->HandleEventSelectionChanged(e);
    };

    m_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        scroll_to_arrow_handler);
}

TimelineArrow::~TimelineArrow()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        m_selection_changed_token);
}

void
TimelineArrow::HandleEventSelectionChanged(std::shared_ptr<RocEvent> e)
{
    std::shared_ptr<EventSelectionChangedEvent> selection_changed_event =
        std::static_pointer_cast<EventSelectionChangedEvent>(e);
    if(selection_changed_event &&
       selection_changed_event->GetSourceId() == m_data_provider.GetTraceFilePath())
    {
        m_selected_event_data.clear();
        std::vector<uint64_t> selected_event_ids;
        m_timeline_selection->GetSelectedEvents(selected_event_ids);
        m_selected_event_data.resize(selected_event_ids.size());
        for(int i = 0; i < selected_event_ids.size(); i++)
        {
            m_selected_event_data[i] =
                m_data_provider.GetEventInfo(selected_event_ids[i]);
        }
    }
}

}  // namespace View
}  // namespace RocProfVis