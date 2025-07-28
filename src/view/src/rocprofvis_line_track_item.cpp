// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_line_track_item.h"
#include "imgui.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_assert.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

LineTrackItem::LineTrackItem(DataProvider& dp, int id, std::string name, float zoom,
                             double time_offset_ns, double& min_x, double& max_x, double scale_x)
: TrackItem(dp, id, name, zoom, time_offset_ns, min_x, max_x, scale_x)
, m_min_y(0)
, m_max_y(0)
, m_data({})
, m_color_by_value_digits()
, m_is_color_value_existant(false)
, m_dp(dp)
, m_show_boxplot(false)
{
    m_track_height = 90.0f;
}

LineTrackItem::~LineTrackItem() {}

void
LineTrackItem::SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits)
{
    m_color_by_value_digits   = color_by_value_digits;
    m_is_color_value_existant = true;
}

bool
LineTrackItem::HasData()
{
    return m_data_provider.GetRawTrackData(m_id) != nullptr;
}

void
LineTrackItem::SetShowBoxplot(bool show_boxplot)
{
    m_show_boxplot = show_boxplot;
}

void
LineTrackItem::LineTrackRender(float graph_width)
{
    ImGui::BeginChild("LV", ImVec2(graph_width, m_track_content_height), false);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();
    ImVec2 container_pos   = ImGui::GetWindowPos();

    float scale_y = content_size.y / (m_max_y - m_min_y);

    float tooltip_x     = 0;
    float tooltip_y     = 0;
    bool  show_tooltip  = false;
    ImU32 generic_black = m_settings.GetColor(static_cast<int>(Colors::kGridColor));
    ImU32 generic_red   = m_settings.GetColor(static_cast<int>(Colors::kGridRed));

    for(int i = 1; i < m_data.size(); i++)
    {
        ImVec2 point_1 =
            MapToUI(m_data[i - 1], cursor_position, content_size, m_scale_x, scale_y);
        if(ImGui::IsMouseHoveringRect(ImVec2(point_1.x - 10, point_1.y - 10),
                                      ImVec2(point_1.x + 10, point_1.y + 10)))
        {
            tooltip_x    = m_data[i - 1].x_value - m_min_x;
            tooltip_y    = m_data[i - 1].y_value - m_min_y;
            show_tooltip = true;
        }

        ImVec2 point_2 =
            MapToUI(m_data[i], cursor_position, content_size, m_scale_x, scale_y);
        ImU32 LineColor = generic_black;

        if(point_2.x < container_pos.x || point_1.x > container_pos.x + content_size.x)
        {
            // Skip rendering if the points are outside the visible area.
            continue;
        }

        if(m_is_color_value_existant)
        {
            // Code below enables user to define problematic regions in LineChart.
            // Add to struct if more regions needed.

            bool point_1_in =
                (m_color_by_value_digits.interest_1_max > m_data[i - 1].y_value &&
                 m_color_by_value_digits.interest_1_min < m_data[i - 1].y_value);
            bool point_2_in =
                (m_color_by_value_digits.interest_1_max > m_data[i].y_value &&
                 m_color_by_value_digits.interest_1_min < m_data[i].y_value);

            if(point_1_in && point_2_in)
            {
                LineColor = generic_red;
            }

            else if(!point_1_in && point_2_in)
            {
                if(m_color_by_value_digits.interest_1_max < m_data[i - 1].y_value)
                {
                    double new_y =
                        cursor_position.y + content_size.y -
                        (m_color_by_value_digits.interest_1_max - m_min_y) * scale_y;
                    double new_x = CalculateMissingX(point_1.x, point_1.y, point_2.x,
                                                     point_2.y, new_y);

                    ImVec2 new_point = ImVec2(new_x, new_y);
                    LineColor        = generic_black;
                    draw_list->AddLine(point_1, new_point, LineColor, 2.0f);
                    LineColor = generic_red;
                    point_1   = new_point;
                }
                else if(m_color_by_value_digits.interest_1_min > m_data[i - 1].y_value)
                {
                    double new_y =
                        cursor_position.y + content_size.y -
                        (m_color_by_value_digits.interest_1_min - m_min_y) * scale_y;
                    double new_x = CalculateMissingX(point_1.x, point_1.y, point_2.x,
                                                     point_2.y, new_y);

                    ImVec2 new_point = ImVec2(new_x, new_y);
                    LineColor        = generic_black;
                    draw_list->AddLine(point_1, new_point, LineColor, 2.0f);
                    LineColor = generic_red;
                    point_1   = new_point;
                }
            }
            else if(point_1_in && !point_2_in)
            {
                if(m_color_by_value_digits.interest_1_max < m_data[i].y_value)
                {
                    // if greater than upper max.

                    double new_y =
                        cursor_position.y + content_size.y -
                        (m_color_by_value_digits.interest_1_max - m_min_y) * scale_y;
                    double new_x = CalculateMissingX(point_1.x, point_1.y, point_2.x,
                                                     point_2.y, new_y);

                    ImVec2 new_point = ImVec2(new_x, new_y);
                    LineColor        = generic_red;
                    draw_list->AddLine(point_1, new_point, LineColor, 2.0f);
                    LineColor = generic_black;
                    point_1   = new_point;
                }
                else if(m_color_by_value_digits.interest_1_min > m_data[i].y_value)
                {
                    double new_y =
                        cursor_position.y + content_size.y -
                        (m_color_by_value_digits.interest_1_min - m_min_y) * scale_y;
                    double new_x = CalculateMissingX(point_1.x, point_1.y, point_2.x,
                                                     point_2.y, new_y);

                    ImVec2 new_point = ImVec2(new_x, new_y);
                    LineColor        = generic_red;
                    draw_list->AddLine(point_1, new_point, LineColor, 2.0f);
                    LineColor = generic_black;
                    point_1   = new_point;
                }
            }
        }
        draw_list->AddLine(point_1, point_2, LineColor, 2.0f);
    }
    if(show_tooltip == true)
    {
        ImGui::BeginTooltip();
        ImGui::Text("X Value: %f", tooltip_x);
        ImGui::Text("Y Value: %f", tooltip_y);
        ImGui::EndTooltip();
    }
    ImGui::EndChild();
}

void
LineTrackItem::BoxPlotRender(float graph_width)
{
    ImGui::BeginChild("BV", ImVec2(graph_width, m_track_content_height), false);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_size    = ImGui::GetContentRegionAvail();
    ImVec2 container_pos   = ImGui::GetWindowPos();

    float scale_y = content_size.y / (m_max_y - m_min_y);

    float tooltip_x     = 0;
    float tooltip_y     = 0;
    bool  show_tooltip  = false;
    ImU32 generic_black = m_settings.GetColor(static_cast<int>(Colors::kGridColor));
    ImU32 generic_red   = m_settings.GetColor(static_cast<int>(Colors::kGridRed));

    for(int i = 1; i < m_data.size(); i++)
    {
        ImVec2 point_1 =
            MapToUI(m_data[i - 1], cursor_position, content_size, m_scale_x, scale_y);
        if(ImGui::IsMouseHoveringRect(ImVec2(point_1.x - 10, point_1.y - 10),
                                      ImVec2(point_1.x + 10, point_1.y + 10)))
        {
            tooltip_x    = m_data[i - 1].x_value - m_min_x;
            tooltip_y    = m_data[i - 1].y_value - m_min_y;
            show_tooltip = true;
        }

        ImVec2 point_2 =
            MapToUI(m_data[i], cursor_position, content_size, m_scale_x, scale_y);
        ImU32 LineColor = generic_black;

        if(point_2.x < container_pos.x || point_1.x > container_pos.x + content_size.x)
        {
            // Skip rendering if the points are outside the visible area.
            continue;
        }

        float bottom_of_chart =
            cursor_position.y + content_size.y - (m_min_y - m_min_y) * scale_y;

        draw_list->AddRectFilled(
            point_1, ImVec2(point_1.x + (point_2.x - point_1.x), bottom_of_chart),
            m_settings.GetColor(static_cast<int>(Colors::kGridColor)), 2.0f);
    }
    if(show_tooltip == true)
    {
        ImGui::BeginTooltip();
        ImGui::Text("X Value: %f", tooltip_x);
        ImGui::Text("Y Value: %f", tooltip_y);
        ImGui::EndTooltip();
    }
    ImGui::EndChild();
}

void
LineTrackItem::ReleaseData()
{
    m_data.clear();
    m_data  = {};
    m_min_y = 0;
    m_max_y = 0;
    m_min_y_str.clear();
    m_max_y_str.clear();
}

bool
LineTrackItem::HandleTrackDataChanged()
{
    m_request_state = TrackDataRequestState::kIdle;
    bool result     = false;
    result          = ExtractPointsFromData();
    if(result)
    {
        const track_info_t* track_info = m_data_provider.GetTrackInfo(m_id);
        if(track_info)
        {
            m_min_y = track_info->min_value;
            m_max_y = track_info->max_value;
        }
        else
        {
            spdlog::warn("Track info not found for track ID: {}", m_id);
        }
        // Ensure that min and max are not equal to allow rendering
        if(m_max_y == m_min_y)
        {
            m_max_y = m_min_y + 1.0;
        }
        std::string flt = std::to_string(m_min_y);
        m_min_y_str     = flt.substr(0, flt.find('.') + 2);
        flt             = std::to_string(m_max_y);
        m_max_y_str     = flt.substr(0, flt.find('.') + 2);
    }
    return result;
}

bool
LineTrackItem::ExtractPointsFromData()
{
    const RawTrackData*       rtd          = m_data_provider.GetRawTrackData(m_id);
    const RawTrackSampleData* sample_track = dynamic_cast<const RawTrackSampleData*>(rtd);
    if(!sample_track)
    {
        spdlog::debug("Invalid track data type for track {}", m_id);
        return false;
    }
    if(sample_track->GetData().empty())
    {
        spdlog::debug("No data for track {}", m_id);
        return false;
    }
    const std::vector<rocprofvis_trace_counter_t> track_data = sample_track->GetData();
    uint64_t                                      count      = track_data.size();

    m_data.clear();
    m_data.reserve(count);
    for(uint64_t i = 0; i < count; i++)
    {
        m_data.emplace_back(rocprofvis_data_point_t{track_data[i].m_start_ts, track_data[i].m_value});
    }
    return true;
}

std::tuple<double, double>
LineTrackItem::FindMaxMin()
{
    if(m_data.size() > 0)
    {
        m_min_y = m_data[0].y_value;
        m_max_y = m_data[0].y_value;

        for(const auto& point : m_data)
        {
            if(point.y_value < m_min_y)
            {
                m_min_y = point.y_value;
            }
            if(point.y_value > m_max_y)
            {
                m_max_y = point.y_value;
            }
        }
    }
    return std::make_tuple(m_min_x, m_max_x);
}

float
LineTrackItem::CalculateMissingX(float x_1, float y_1, float x_2, float y_2,
                                 float known_y)
{
    // Calculate slope (m)
    double m = (y_2 - y_1) / (x_2 - x_1);

    // Calculate y-intercept (b)
    double b = y_1 - m * x_1;

    // Calculate x for the given y
    double x = (known_y - b) / m;

    return x;
}

void
LineTrackItem::RenderMetaAreaScale()
{
    ImVec2 max_size =
        ImGui::CalcTextSize(m_max_y_str.c_str()) + ImGui::CalcTextSize("Max: ");
    ImVec2 min_size         = ImGui::CalcTextSize(m_min_y_str.c_str());
    m_meta_area_scale_width = max_size.x + 2 * m_metadata_padding.x;
    ImVec2 content_region   = ImGui::GetContentRegionMax();
    ImVec2 window_pos       = ImGui::GetWindowPos();

    ImGui::SetCursorPos(ImVec2(content_region.x - (max_size.x + m_metadata_padding.x),
                               m_metadata_padding.y));
    ImGui::TextUnformatted("Max: ");
    ImGui::SameLine();
    ImGui::TextUnformatted(m_max_y_str.c_str());

    ImGui::SetCursorPos(ImVec2(content_region.x - (max_size.x + m_metadata_padding.x),
                               content_region.y - min_size.y - m_metadata_padding.y));
    ImGui::TextUnformatted("Min: ");

    ImGui::SetCursorPos(ImVec2(content_region.x - (min_size.x + m_metadata_padding.x),
                               content_region.y - min_size.y - m_metadata_padding.y));
    ImGui::TextUnformatted(m_min_y_str.c_str());

    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(window_pos.x + content_region.x - m_meta_area_scale_width, window_pos.y),
        ImVec2(window_pos.x + content_region.x - m_meta_area_scale_width,
               window_pos.y + content_region.y),
        m_settings.GetColor(Colors::kMetaDataSeparator), 2.0f);
}

void
LineTrackItem::RenderChart(float graph_width)
{
    if(m_show_boxplot)
    {
        BoxPlotRender(graph_width);
    }
    else
    {
        LineTrackRender(graph_width);
    }
}

void
LineTrackItem::RenderMetaAreaOptions()
{
    ImGui::Checkbox("Show as Box Plot", &m_show_boxplot);
    ImGui::Checkbox("Highlight Y Range", &m_is_color_value_existant);
    if(m_is_color_value_existant)
    {        
        float width = ImGui::GetItemRectSize().x;
        ImGui::TextUnformatted("Max");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(width - ImGui::CalcTextSize("Max").x);
        ImGui::SliderFloat("##max", &m_color_by_value_digits.interest_1_max, m_color_by_value_digits.interest_1_min, m_max_y, "%.1f");
        ImGui::TextUnformatted("Min");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(width - ImGui::CalcTextSize("Min").x);
        ImGui::SliderFloat("##min", &m_color_by_value_digits.interest_1_min, m_min_y, m_color_by_value_digits.interest_1_max, "%.1f");
    }
}

ImVec2
LineTrackItem::MapToUI(rocprofvis_data_point_t& point, ImVec2& cursor_position,
                       ImVec2& content_size, float scaleX, float scaleY)
{
    ImVec2 container_pos = ImGui::GetWindowPos();

    double x = container_pos.x + (point.x_value - (m_min_x + m_time_offset_ns)) * scaleX;
    double y = cursor_position.y + content_size.y - (point.y_value - m_min_y) * scaleY;

    return ImVec2(x, y);
}

}  // namespace View
}  // namespace RocProfVis
