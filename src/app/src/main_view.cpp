#include "main_view.h"
#include "flame_chart.h"
#include "graph_view_metadata.h"
#include "grid.h"
#include "imgui.h"
#include "line_chart.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

template <typename T>
T
clamp(const T& value, const T& lower, const T& upper)
{
    if(value < lower)
    {
        return lower;
    }
    else if(value > upper)
    {
        return upper;
    }
    else
    {
        return value;
    }
}
MainView::MainView()

{
    this->min_value         = 0.0f;
    this->max_value         = 0.0f;
    this->zoom              = 2.0f;
    this->movement          = 0.0f;
    this->has_zoom_happened = false;
    this->min_x             = 0.0f;
    this->max_x             = 0.0f;
    this->min_y             = 0.0f;
    this->max_y             = 0.0f;
    this->scroll_position   = 0.0f;
    data_arr;
    this->ran_once = false;

    flame_event = {
        { "Event A", 0.0, 20.0 },
        { "Event B", 20.0, 30.0 },
        { "Event C", 50.0, 25.0 },
    };
}

MainView::~MainView() {}

void
MainView::MakeGrid()
{
    /*This section makes the grid for the charts*/

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2      display_size = ImGui::GetWindowSize();
    ImDrawList* draw_list    = ImGui::GetWindowDrawList();

    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y), ImGuiCond_Always);

    ImGui::SetCursorPos(ImVec2(0, 0));

    ImGui::BeginChild("ScrollableArea", ImVec2(0, 0), true, window_flags);

    Grid main_grid = Grid();

    main_grid.RenderGrid(min_x, max_x, movement, zoom, draw_list);

    ImGui::EndChild();
}

void
MainView::MakeGraphMetadataView(
    std::map<std::string, rocprofvis_trace_process_t>& trace_data)
{
    /*This section makes the charts both line and flamechart are constructed here*/

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse |
                                    ImGuiWindowFlags_NoScrollbar;

    ImVec2 display_size = ImGui::GetWindowSize();

    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - 60.0f),
                             ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(0, 0));

    ImGui::BeginChild("ScrollableArea2", ImVec2(0, 0), true, window_flags);

    if(scroll_position != ImGui::GetScrollY())
    {
        ImGui::SetScrollY(scroll_position);
    }

    int count2 = 0;
    for(auto& process : trace_data)
    {
        for(auto& thread : process.second.m_threads)
        {
            auto& events   = thread.second.m_events;
            auto& counters = thread.second.m_counters;

            if(events.size())
            {
                // Create FlameChart
                rocprofvis_metadata_visualization meta_data = {};
                meta_data.chart_name                        = thread.first;

                RenderGraphMetadata(count2, 50, "Flame", meta_data);

                count2 = count2 + 1;
            }

            else if(counters.size())
            {
                // Linechart
                rocprofvis_metadata_visualization meta_data = {};
                meta_data.chart_name                        = thread.first;

                RenderGraphMetadata(count2, 300, "Line", meta_data);

                count2 = count2 + 1;
            }
        }
    }

    ImGui::EndChild();
}

void
MainView::MakeGraphView(std::map<std::string, rocprofvis_trace_process_t>& trace_data)
{
    /*This section makes the charts both line and flamechart are constructed here*/

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2 display_size = ImGui::GetWindowSize();

    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - 60.0f),
                             ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(0, 0));

    ImGui::BeginChild("ScrollableArea2", ImVec2(0, 0), true, window_flags);

    // Prevent choppy behavior by preventing constant rerender.
    if(scroll_position != ImGui::GetScrollY())
    {
        ImGui::SetScrollY(scroll_position);
    }
    else
    {
        scroll_position = ImGui::GetScrollY();
    }
    int count2 = 0;
    for(auto& process : trace_data)
    {
        for(auto& thread : process.second.m_threads)
        {
            auto& events   = thread.second.m_events;
            auto& counters = thread.second.m_counters;
            if(events.size())
            {
                // Create FlameChart
                flame_event = ExtractFlamePoints(events);
                FindMaxMinFlame();
                RenderFlameCharts(count2);

                // Create FlameChart title and info panel

                count2 = count2 + 1;
            }

            else if(counters.size())
            {
                // Linechart
                void* datap = (void*) &thread.second.m_counters;
                int   count = counters.size();

                std::vector<dataPoint> points = ExtractPointsFromData(datap);
                data_arr                      = points;

                FindMaxMin();
                RenderLineCharts(count2);
                count2 = count2 + 1;
            }
        }
    }

    ImGui::EndChild();
}

void
MainView::FindMaxMin()
{
    if(ran_once == false)
    {
        min_x    = data_arr[0].xValue;
        max_x    = data_arr[0].xValue;
        ran_once = true;
    }

    min_y = data_arr[0].yValue;
    max_y = data_arr[0].yValue;

    for(const auto& point : data_arr)
    {
        if(point.xValue < min_x)
        {
            min_x = point.xValue;
        }
        if(point.xValue > max_x)
        {
            max_x = point.xValue;
        }
        if(point.yValue < min_y)
        {
            min_y = point.yValue;
        }
        if(point.yValue > max_y)
        {
            max_y = point.yValue;
        }
    }
}

void
MainView::FindMaxMinFlame()
{
    if(ran_once == false)
    {
        min_x    = flame_event[0].m_start_ts;
        max_x    = flame_event[0].m_start_ts + flame_event[0].m_duration;
        ran_once = true;
    }

    for(const auto& point : flame_event)
    {
        if(point.m_start_ts < min_x)
        {
            min_x = point.m_start_ts;
        }
        if(point.m_start_ts + point.m_duration > max_x)
        {
            max_x = point.m_start_ts + point.m_duration;
        }
    }
}

std::vector<dataPoint>
MainView::ExtractPointsFromData(void* data)

{
    auto* counters_vector = static_cast<std::vector<rocprofvis_trace_counter_t>*>(data);

    std::vector<dataPoint> aggregated_points;

    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    int    screen_width = static_cast<int>(display_size.x);

    float effectiveWidth = screen_width / zoom;
    float bin_size       = (max_x - min_x) / effectiveWidth;

    double bin_sum_x         = 0.0;
    double bin_sum_y         = 0.0;
    int    bin_count         = 0;
    double current_bin_start = counters_vector->at(0).m_start_ts;

    for(const auto& counter : *counters_vector)
    {
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
                dataPoint binned_point;
                binned_point.xValue = bin_sum_x / bin_count;
                binned_point.yValue = bin_sum_y / bin_count;
                aggregated_points.push_back(binned_point);
            }
            current_bin_start += bin_size;
            bin_sum_x = counter.m_start_ts;
            bin_sum_y = counter.m_value;
            bin_count = 1;
        }
    }

    if(bin_count > 0)
    {
        dataPoint binned_point;
        binned_point.xValue = bin_sum_x / bin_count;
        binned_point.yValue = bin_sum_y / bin_count;
        aggregated_points.push_back(binned_point);
    }

    return aggregated_points;
}

std::vector<rocprofvis_trace_event_t>
MainView::ExtractFlamePoints(const std::vector<rocprofvis_trace_event_t>& traceEvents)
{
    std::vector<rocprofvis_trace_event_t> entries;

    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    int    screen_width = static_cast<int>(display_size.x);

    float effective_width = screen_width / zoom;
    float bin_size        = ((max_x - min_x) / effective_width);

    double bin_sum_x         = 0.0;
    int    bin_count         = 0;
    double current_bin_start = traceEvents[0].m_start_ts;
    float  largest_duration  = 0;
    for(const auto& counter : traceEvents)
    {
        if(counter.m_start_ts < current_bin_start + bin_size)
        {
            if(counter.m_duration > largest_duration)
            {
                largest_duration =
                    counter.m_duration;  // Use the largest duration per bin.
            }
            bin_sum_x += counter.m_start_ts;
            bin_count++;
        }
        else
        {
            if(bin_count > 0)
            {
                rocprofvis_trace_event_t binned_point;
                binned_point.m_start_ts = bin_sum_x / bin_count;
                binned_point.m_duration = largest_duration;
                binned_point.m_name     = counter.m_name;
                entries.push_back(binned_point);
            }

            // Prepare next bin.
            current_bin_start =
                current_bin_start +
                bin_size *
                    static_cast<int>((counter.m_start_ts - current_bin_start) / bin_size);
            bin_sum_x        = counter.m_start_ts;
            largest_duration = counter.m_duration;
            bin_count        = 1;
        }
    }

    if(bin_count > 0)
    {
        rocprofvis_trace_event_t binned_point;
        binned_point.m_start_ts = bin_sum_x / bin_count;
        binned_point.m_duration = largest_duration;
        binned_point.m_name     = traceEvents.back().m_name;

        entries.push_back(binned_point);
    }

    return entries;
}

void
MainView::GenerateGraphPoints(
    std::map<std::string, rocprofvis_trace_process_t>& trace_data)

{
    ImVec2      screen_pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list  = ImGui::GetWindowDrawList();

    ImVec2 display_size_main = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display_size_main.x * 0.2f, 00));
    ImGui::SetNextWindowSize(
        ImVec2(display_size_main.x * 0.8f, display_size_main.y * 0.8f), ImGuiCond_Always);

    if(ImGui::Begin("Main Graphs", nullptr,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_HorizontalScrollbar |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        HandleTopSurfaceTouch();

        MakeGrid();

        MakeGraphView(trace_data);

        //// Scrubber Line
        // if(ImGui::IsMouseHoveringRect(ImVec2(display_size_main.x * 0.2f, 00),
        //        ImVec2(display_size_main.x + display_size_main.x * 0.8f,
        //               00 + display_size_main.y * 0.8f)))
        //{
        //     std::cout << "hoverd" << std::endl;
        //     ImVec2 mPos = ImGui::GetMousePos();
        //     draw_list->AddLine(ImVec2(mPos.x, screen_pos.y),
        //                        ImVec2(mPos.x, screen_pos.y + display_size_main.y *
        //                        0.8f),
        //                       IM_COL32(0, 0, 0, 255), 2.0f);
        // }
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(
        ImVec2(display_size_main.x * 0.2f, display_size_main.y * 0.8f), ImGuiCond_Always);

    if(ImGui::Begin("Graph MetaData", nullptr,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_HorizontalScrollbar |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        MakeGraphMetadataView(trace_data);
    }
    ImGui::End();
}

void
MainView::HandleTopSurfaceTouch()
{
    // Handle Zoom
    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
    {
        float scroll_wheel = ImGui::GetIO().MouseWheel;
        if(scroll_wheel != 0.0f)
        {
            float       view_width = (max_x - min_x) / zoom;
            const float zoom_speed = 0.1f;
            zoom *= (scroll_wheel > 0) ? (1.0f + zoom_speed) : (1.0f - zoom_speed);
            zoom = clamp(zoom, 1.0f, 1000.0f);
        }
    }

    // Handle Panning
    if(ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        float drag       = ImGui::GetIO().MouseDelta.x;
        float view_width = (max_x - min_x) / zoom;
        movement -= (drag / ImGui::GetContentRegionAvail().x) * view_width;
        float drag_y    = ImGui::GetIO().MouseDelta.y;
        scroll_position = static_cast<int>(scroll_position - drag_y);
    }
}

void
MainView::RenderLineCharts(int count2)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::BeginChild((std::to_string(count2)).c_str(), ImVec2(0, 300), false,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoScrollbar);

    ImGui::Text((std::to_string(max_y)).c_str());

    LineChart line = LineChart(count2, min_value, max_value, zoom, movement,
                               has_zoom_happened, min_x, max_x, min_y, max_y, data_arr);
    line.Render();

    ImVec2 child_window_size = ImGui::GetWindowSize();
    ImVec2 text_size         = ImGui::CalcTextSize("Bottom Left Text");
    ImGui::SetCursorPos(
        ImVec2(0, child_window_size.y - text_size.y - ImGui::GetStyle().WindowPadding.y));

    // Add text to the bottom left of the child component
    ImGui::Text((std::to_string(min_y)).c_str());

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::Spacing();
    ImGui::Separator();
}

void
MainView::RenderFlameCharts(int count2)
{
    FlameChart flame = FlameChart(count2, min_value, max_value, zoom, movement, min_x,
                                  max_x, flame_event);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::BeginChild((std::to_string(count2)).c_str(), ImVec2(0, 50), false,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoScrollbar);

    flame.render();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::Spacing();
    ImGui::Separator();
}

void
MainView::RenderGraphMetadata(int graph_id, float size, std::string type,
                              rocprofvis_metadata_visualization data)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::BeginChild((std::to_string(graph_id)).c_str(), ImVec2(0, size), false,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoScrollbar);
    GraphViewMetadata metaData = GraphViewMetadata(graph_id, size, type, data);
    metaData.renderData();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::Spacing();
    ImGui::Separator();
}