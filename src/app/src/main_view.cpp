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

void
main_view::make_grid()
{
    /*This section makes the grid for the charts*/

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2      displaySize = ImGui::GetWindowSize();
    ImDrawList* drawList    = ImGui::GetWindowDrawList();

    ImVec2 sPos             = ImGui::GetCursorScreenPos();
    ImVec2 subComponentSize = ImGui::GetContentRegionAvail();
    ImGui::SetNextWindowSize(ImVec2(displaySize.x, displaySize.y), ImGuiCond_Always);

    ImGui::SetCursorPos(ImVec2(0, 0));

    ImGui::BeginChild("ScrollableArea", ImVec2(0, 0), true, windowFlags);

    grid g = grid();

    g.renderGrid(minX, maxX, movement, zoom, drawList);

    ImGui::EndChild();
}
void
main_view::make_graph_metadata_view(
    std::map<std::string, rocprofvis_trace_process_t>& trace_data)
{
    /*This section makes the charts both line and flamechart are constructed here*/

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoScrollbar;

    ImVec2      displaySize = ImGui::GetWindowSize();
    ImDrawList* drawList    = ImGui::GetWindowDrawList();

    ImVec2 sPos             = ImGui::GetCursorScreenPos();
    ImVec2 subComponentSize = ImGui::GetContentRegionAvail();

    ImGui::SetNextWindowSize(ImVec2(displaySize.x, displaySize.y - 60.0f),
                             ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(0, 0));

    ImGui::BeginChild("ScrollableArea2", ImVec2(0, 0), true, windowFlags);

  

    if(scrollPosition != ImGui::GetScrollY())
    {
        ImGui::SetScrollY(scrollPosition);
    }

    std::map<int, std::vector<dataPoint>> pointMap;
    int                                   count2 = 0;
    for(auto& process : trace_data)
    {
        for(auto& thread : process.second.m_threads)
        {
            auto& events   = thread.second.m_events;
            auto& counters = thread.second.m_counters;

            if(events.size())
            {
                // Create FlameChart
                rocprofvis_metadata_visualization metaData = {};
                metaData.chart_name                        = thread.first;

                renderGraphMetadata(count2, 50, "Flame", metaData);

                count2 = count2 + 1;
            }

            else if(counters.size())
            {
                // Linechart
                rocprofvis_metadata_visualization metaData = {};
                metaData.chart_name                        = thread.first;

                renderGraphMetadata(count2, 300, "Line", metaData);

                count2 = count2 + 1;
            }
        }
    }



 

    ImGui::EndChild();
}

void
main_view::make_graph_view(std::map<std::string, rocprofvis_trace_process_t>& trace_data)
{
    /*This section makes the charts both line and flamechart are constructed here*/

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoScrollWithMouse;

    ImVec2      displaySize = ImGui::GetWindowSize();
    ImDrawList* drawList    = ImGui::GetWindowDrawList();

    ImVec2 sPos             = ImGui::GetCursorScreenPos();
    ImVec2 subComponentSize = ImGui::GetContentRegionAvail();

    ImGui::SetNextWindowSize(ImVec2(displaySize.x, displaySize.y - 60.0f),
                             ImGuiCond_Always);
    ImGui::SetCursorPos(ImVec2(0, 0));

    ImGui::BeginChild("ScrollableArea2", ImVec2(0, 0), true, windowFlags);

    // Prevent choppy behavior by preventing constant rerender.
    if(scrollPosition != ImGui::GetScrollY())
    {
        ImGui::SetScrollY(scrollPosition);
    }
    else
    {
        scrollPosition = ImGui::GetScrollY();
    }
    std::map<int, std::vector<dataPoint>> pointMap;
    int                                   count2 = 0;
    for(auto& process : trace_data)
    {
        for(auto& thread : process.second.m_threads)
        {
            auto& events   = thread.second.m_events;
            auto& counters = thread.second.m_counters;
            if(events.size())
            {
                // Create FlameChart
                flameEvent = extractFlamePoints(events);
                findMaxMinFlame();
                renderMain3(count2);

                // Create FlameChart title and info panel

                count2 = count2 + 1;
            }

            else if(counters.size())
            {
                // Linechart
                void* datap = (void*) &thread.second.m_counters;
                int   count = counters.size();

                std::vector<dataPoint> points = extractPointsFromData(datap);
                data_arr                      = points;

                findMaxMin();
                renderMain2(count2);
                pointMap[count2] = points;
                count2           = count2 + 1;
            }
        }
    }
 
    ImGui::EndChild();
}

main_view::main_view()

{
    this->minValue        = 0.0f;
    this->maxValue        = 0.0f;
    this->zoom            = 1.0f;
    this->movement        = 0.0f;
    this->hasZoomHappened = false;
    this->minX            = 0.0f;
    this->maxX            = 0.0f;
    this->minY            = 0.0f;
    this->maxY            = 0.0f;
    this->scrollPosition  = 0.0f;
    data_arr;
    this->ranOnce             = false;
    renderedOnce              = false;
    this->fullyRenderedPoints = false;
    flameEvent                = {
        { "Event A", 0.0, 20.0 },
        { "Event B", 20.0, 30.0 },
        { "Event C", 50.0, 25.0 },
    };
}

void
main_view::findMaxMin()
{
    if(ranOnce == false)
    {
        minX    = data_arr[0].xValue;
        maxX    = data_arr[0].xValue;
        ranOnce = true;
    }

    minY = data_arr[0].yValue;
    maxY = data_arr[0].yValue;

    for(const auto& point : data_arr)
    {
        if(point.xValue < minX)
        {
            minX = point.xValue;
        }
        if(point.xValue > maxX)
        {
            maxX = point.xValue;
        }
        if(point.yValue < minY)
        {
            minY = point.yValue;
        }
        if(point.yValue > maxY)
        {
            maxY = point.yValue;
        }
    }
}

void
main_view::findMaxMinFlame()
{
    if(ranOnce == false)
    {
        minX    = flameEvent[0].m_start_ts;
        maxX    = flameEvent[0].m_start_ts + flameEvent[0].m_duration;
        ranOnce = true;
    }

    for(const auto& point : flameEvent)
    {
        if(point.m_start_ts < minX)
        {
            minX = point.m_start_ts;
        }
        if(point.m_start_ts + point.m_duration > maxX)
        {
            maxX = point.m_start_ts + point.m_duration;
        }
    }
}

main_view::~main_view() {}

std::vector<dataPoint>
main_view::extractPointsFromData(void* data)

{
    auto* counters_vector = static_cast<std::vector<rocprofvis_trace_counter_t>*>(data);

    std::vector<dataPoint> aggregatedPoints;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    int    screenWidth = static_cast<int>(displaySize.x);

    float effectiveWidth = screenWidth / zoom;
    float binSize        = (maxX - minX) / effectiveWidth;

    double binSumX         = 0.0;
    double binSumY         = 0.0;
    int    binCount        = 0;
    double currentBinStart = counters_vector->at(0).m_start_ts;
    int    count           = 0;

    for(const auto& counter : *counters_vector)
    {
        if(count == 0)
        {
            count = 10;
        }
        if(counter.m_start_ts < currentBinStart + binSize)
        {
            binSumX += counter.m_start_ts;
            binSumY += counter.m_value;
            binCount++;
        }
        else
        {
            if(binCount > 0)
            {
                dataPoint binnedPoint;
                binnedPoint.xValue = binSumX / binCount;
                binnedPoint.yValue = binSumY / binCount;
                aggregatedPoints.push_back(binnedPoint);
            }
            currentBinStart += binSize;
            binSumX  = counter.m_start_ts;
            binSumY  = counter.m_value;
            binCount = 1;
        }
    }

    if(binCount > 0)
    {
        dataPoint binnedPoint;
        binnedPoint.xValue = binSumX / binCount;
        binnedPoint.yValue = binSumY / binCount;
        aggregatedPoints.push_back(binnedPoint);
    }

    return aggregatedPoints;
}

std::vector<rocprofvis_trace_event_t>
main_view::extractFlamePoints(const std::vector<rocprofvis_trace_event_t>& traceEvents)
{
    std::vector<rocprofvis_trace_event_t> entries;

    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    int    screenWidth = static_cast<int>(displaySize.x);

    float effectiveWidth = screenWidth / zoom;
    float binSize        = ((maxX - minX) / effectiveWidth);

    double binSumX         = 0.0;
    int    binCount        = 0;
    double currentBinStart = traceEvents[0].m_start_ts;
    float  largestDuration = 0;
    for(const auto& counter : traceEvents)
    {
        if(counter.m_start_ts < currentBinStart + binSize)
        {
            if(counter.m_duration > largestDuration)
            {
                largestDuration =
                    counter.m_duration;  // Use the largest duration per bin.
            }
            binSumX += counter.m_start_ts;
            binCount++;
        }
        else
        {
            if(binCount > 0)
            {
                rocprofvis_trace_event_t binnedPoint;
                binnedPoint.m_start_ts = binSumX / binCount;
                binnedPoint.m_duration = largestDuration;
                binnedPoint.m_name     = counter.m_name;
                entries.push_back(binnedPoint);
            }

            // Prepare next bin.
            currentBinStart =
                currentBinStart +
                binSize *
                    static_cast<int>((counter.m_start_ts - currentBinStart) / binSize);
            binSumX         = counter.m_start_ts;
            largestDuration = counter.m_duration;
            binCount        = 1;
        }
    }

    if(binCount > 0)
    {
        rocprofvis_trace_event_t binnedPoint;
        binnedPoint.m_start_ts = binSumX / binCount;
        binnedPoint.m_duration = largestDuration;
        binnedPoint.m_name     = traceEvents.back().m_name;

        entries.push_back(binnedPoint);
    }

    return entries;
}

void
main_view::generate_graph_points(
    std::map<std::string, rocprofvis_trace_process_t>& trace_data)

{
    ImVec2      sPos             = ImGui::GetCursorScreenPos();
    ImVec2      subComponentSize = ImGui::GetContentRegionAvail();
    ImDrawList* drawList         = ImGui::GetWindowDrawList();

    ImVec2 displaySizeMain = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(displaySizeMain.x * 0.2f, 00));
    ImGui::SetNextWindowSize(ImVec2(displaySizeMain.x * 0.8f, displaySizeMain.y * 0.8f),
                             ImGuiCond_Always);
    ImGui::GetIO().FontGlobalScale = 1.2f;

    if(ImGui::Begin("Main Graphs", nullptr,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_HorizontalScrollbar |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
      
        handleTouch();

        make_grid();

        make_graph_view(trace_data);

            // Scrubber Line
        if(ImGui::IsMouseHoveringRect(
               ImVec2(displaySizeMain.x * 0.2f, 00),
                                      ImVec2(displaySizeMain.x + displaySizeMain.x * 0.8f,
                                             00 + displaySizeMain.y * 0.8f)))
        {
            std::cout << "hoverd" << std::endl;
            ImVec2 mPos = ImGui::GetMousePos();
            drawList->AddLine(ImVec2(mPos.x, sPos.y),
                              ImVec2(mPos.x, sPos.y + displaySizeMain.y * 0.8f),
                              IM_COL32(0, 0, 0, 255), 2.0f);
        }
   
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(displaySizeMain.x * 0.2f, displaySizeMain.y * 0.8f),
                             ImGuiCond_Always);

    if(ImGui::Begin("Graph MetaData", nullptr,
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_HorizontalScrollbar |
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        make_graph_metadata_view(trace_data);
    }
    ImGui::End();

    renderedOnce = true;
}

void
main_view::handleTouch()
{
    // Handle Zoom
    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
    {
        float scrollWheel = ImGui::GetIO().MouseWheel;
        if(scrollWheel != 0.0f)
        {
            float       viewWidth = (maxX - minX) / zoom;
            const float zoomSpeed = 0.1f;
            zoom *= (scrollWheel > 0) ? (1.0f + zoomSpeed) : (1.0f - zoomSpeed);
            zoom = clamp(zoom, 1.0f, 1000.0f);
        }
    }

    // Handle Panning
    if(ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        float drag      = ImGui::GetIO().MouseDelta.x;
        float viewWidth = (maxX - minX) / zoom;
        movement -= (drag / ImGui::GetContentRegionAvail().x) * viewWidth;

        float dragY    = ImGui::GetIO().MouseDelta.y;
        scrollPosition = static_cast<int>(scrollPosition - dragY);
    }
}

void
main_view::renderMain2(int count2)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::BeginChild((std::to_string(count2)).c_str(), ImVec2(0, 300), false,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoScrollbar);

    ImGui::Text((std::to_string(maxY)).c_str());

    line_chart line = line_chart(count2, minValue, maxValue, zoom, movement,
                                 hasZoomHappened, minX, maxX, minY, maxY, data_arr);
    line.render();

    ImVec2 child_window_size = ImGui::GetWindowSize();
    ImVec2 text_size         = ImGui::CalcTextSize("Bottom Left Text");
    ImGui::SetCursorPos(
        ImVec2(0, child_window_size.y - text_size.y - ImGui::GetStyle().WindowPadding.y));

    // Add text to the bottom left of the child component
    ImGui::Text((std::to_string(minY)).c_str());

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::Spacing();
    ImGui::Separator();
}

void
main_view::renderMain3(int count2)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::BeginChild((std::to_string(count2)).c_str(), ImVec2(0, 50), false,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoScrollbar);

    FlameChart flame =
        FlameChart(count2, minValue, maxValue, zoom, movement, minX, maxX, flameEvent);
    flame.render();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::Spacing();
    ImGui::Separator();
}

void
main_view::renderGraphMetadata(int graphID, float size, std::string type,
                               rocprofvis_metadata_visualization data)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::BeginChild((std::to_string(graphID)).c_str(), ImVec2(0, size), false,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoScrollbar);
    GraphViewMetadata metaData = GraphViewMetadata(graphID, size, type, data);
    metaData.renderData();

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::Spacing();
    ImGui::Separator();
}