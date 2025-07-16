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
    m_track_height          = 90.0f;
    m_meta_area_scale_width = 70.0f;
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
}

bool
LineTrackItem::HandleTrackDataChanged()
{
    m_request_state = TrackDataRequestState::kIdle;
    bool result     = false;
    result          = ExtractPointsFromData();
    if(result)
    {
        FindMaxMin();

        // This statement is needed to prevent render errors when all y values are the
        // same.
        if(m_max_y == m_min_y)
        {
            if(m_max_y == 0)
            {
                m_max_y = 1.0;
                m_min_y = -1.0;
            }
            else
            {
                m_max_y = m_min_y + 1.0;
            }
        }
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

    std::vector<rocprofvis_data_point_t> aggregated_points;

    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    int    screen_width = static_cast<int>(display_size.x);

    double effective_width = screen_width / m_zoom;
    double bin_size        = (m_max_x - m_min_x) / effective_width;

    double bin_sum_x         = 0.0;
    double bin_sum_y         = 0.0;
    int    bin_count         = 0;
    double current_bin_start = DBL_MAX;

    const std::vector<rocprofvis_trace_counter_t> track_data = sample_track->GetData();
    uint64_t                                      count      = track_data.size();

    rocprofvis_trace_counter_t counter;
    bool                       enable_binning = false;
    for(uint64_t i = 0; i < count; i++)
    {
        if(enable_binning)
        {
            counter.m_start_ts = track_data[i].m_start_ts;
            counter.m_value    = track_data[i].m_value;

            if(i == 0)
            {
                current_bin_start = counter.m_start_ts;
            }

            if(counter.m_start_ts < current_bin_start + bin_size)
            {
                bin_sum_x += counter.m_start_ts;
                bin_sum_y += counter.m_value;
                bin_count++;
            }
            else
            {
                if(bin_count > 0)
                {
                    rocprofvis_data_point_t binned_point;
                    binned_point.x_value = bin_sum_x / bin_count;
                    binned_point.y_value = bin_sum_y / bin_count;
                    aggregated_points.push_back(binned_point);
                }
                current_bin_start += bin_size;
                bin_sum_x = counter.m_start_ts;
                bin_sum_y = counter.m_value;
                bin_count = 1;
            }
        }
        else
        {
            rocprofvis_data_point_t binned_point;
            binned_point.x_value = track_data[i].m_start_ts;
            binned_point.y_value = track_data[i].m_value;
            aggregated_points.push_back(binned_point);
        }
    }

    if(bin_count > 0)
    {
        rocprofvis_data_point_t binned_point;
        binned_point.x_value = bin_sum_x / bin_count;
        binned_point.y_value = bin_sum_y / bin_count;
        aggregated_points.push_back(binned_point);
    }

    m_data = aggregated_points;
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
LineTrackItem::RenderMetaAreaScale(ImVec2& container_size)
{
    ImGui::BeginChild("MetaData Scale", ImVec2(m_meta_area_scale_width, container_size.y),
                      ImGuiChildFlags_None);

    ImDrawList* draw_list         = ImGui::GetWindowDrawList();
    ImVec2      child_window_size = ImGui::GetWindowSize();

    char text_buffer[32];
    snprintf(text_buffer, 32, "%29.1f", m_max_y);
    ImVec2 text_size = ImGui::CalcTextSize(text_buffer);

    ImGui::SetCursorPos(ImVec2(child_window_size.x - (text_size.x + m_metadata_padding.x),
                               ImGui::GetStyle().WindowPadding.y));
    ImGui::Text("%s", text_buffer);

    if(ImGui::IsItemVisible())
    {
        m_is_in_view_vertical = true;
    }
    else
    {
        m_is_in_view_vertical = false;
    }

    snprintf(text_buffer, 32, "%29.1f", m_min_y);
    text_size = ImGui::CalcTextSize(text_buffer);
    
    ImGui::SetCursorPos(
        ImVec2(child_window_size.x - (text_size.x + m_metadata_padding.x),
               child_window_size.y - text_size.y - ImGui::GetStyle().WindowPadding.y));
    
    ImGui::Text("%s", text_buffer);

    ImGui::SetCursorPos(ImVec2(0, 0));
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();

    draw_list->AddLine(
        ImVec2(cursor_position.x + m_metadata_padding.x, cursor_position.y),
        ImVec2(cursor_position.x + m_metadata_padding.x,
               cursor_position.y + child_window_size.y),
        m_settings.GetColor(Colors::kMetaDataSeparator), 2.0f);

    ImGui::EndChild();
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
