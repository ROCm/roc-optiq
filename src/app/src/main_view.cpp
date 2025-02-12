#include "imgui.h"
#include "main_view.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
 #include "line_chart.h"
#include "flame_chart.h"
 #include <map>
#include "grid.h"
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


main_view::main_view()
 
{
    
        this->minValue        = 0.0f;
        this->maxValue        = 0.0f;
        this->zoom            = 1.0f;
        this->movement        = 0.0f;
        this->hasZoomHappened = false;
        this->id              = 0;
        this->minX            = 0.0f;
        this->maxX            = 0.0f;
        this->minY            = 0.0f;
        this->maxY            = 0.0f;
        data_arr;
        this->ranOnce = false; 
        renderedOnce = false;
        this->fullyRenderedPoints = false; 
        flameEvent                = {
            { "Event A", 0.0, 20.0 },
            { "Event B", 20.0, 30.0 },
            { "Event C", 50.0, 25.0 },
        };
        flameChartPointMap; 
        count3 = 0; 
 lineChartPointMap;  
 flameChartPointMap;
 }
 
void
main_view::findMaxMin()
{
    if (ranOnce == false) {

 minX = data_arr[0].xValue;
        maxX = data_arr[0].xValue;
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
            maxX =  point.m_start_ts + point.m_duration;
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
    int count = 0 ; 

    for(const auto& counter : *counters_vector)
    {   
        if (count == 0) {
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

std::vector<rocprofvis_trace_event_t> main_view::extractFlamePoints(const std::vector<rocprofvis_trace_event_t>& traceEvents) {
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
            if (counter.m_duration > largestDuration) {
                largestDuration = counter.m_duration; // Use the largest duration per bin. 
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

            //Prepare next bin. 
            currentBinStart =
                currentBinStart +
                binSize *
                    static_cast<int>((counter.m_start_ts - currentBinStart) / binSize);
            binSumX  = counter.m_start_ts;
            largestDuration = counter.m_duration;
            binCount = 1;
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
main_view::generate_graph_points(std::map<std::string, rocprofvis_trace_process_t>& trace_data)

{
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoScrollWithMouse;

     
 
    ImVec2      displaySizeMain      = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowSize(
        ImVec2(displaySizeMain.x* 0.8f, displaySizeMain.y * 0.8f),
                             ImGuiCond_Always);

     if(ImGui::Begin("Main Graphs", nullptr,
                    ImGuiWindowFlags_NoMove  |
                        ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_HorizontalScrollbar |
                        ImGuiWindowFlags_NoScrollbar))
    {
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2      displaySize = ImGui::GetWindowSize();

        ImVec2 sPos             = ImGui::GetCursorScreenPos();
        ImVec2 subComponentSize = ImGui::GetContentRegionAvail();
    
    
   

    //========================This subsection is for the grid.===================================== 
       
   

    handleTouch();
    
    ImGui::SetNextWindowSize(ImVec2(displaySize.x + 90.0f, displaySize.y + 90.0f),
                             ImGuiCond_Always);
   
    ImGui::SetNextWindowPos(ImVec2(0, 0));


    ImGui::BeginChild("ScrollableArea", ImVec2(0, 0), true, windowFlags);

    if(ImGui::IsWindowHovered())
    {
        ImVec2 mPos = ImGui::GetMousePos();
        drawList->AddLine(ImVec2(mPos.x, sPos.y),
                           ImVec2(mPos.x, sPos.y + subComponentSize.y),
                           IM_COL32(0, 0, 0, 255), 2.0f);
    }
    
    
   
    
      grid g = grid();

    g.renderGrid(minX, maxX, movement, zoom, drawList);

    ImGui::EndChild();
 


    
 //==============================This subsection is for the charts.====================================== 
    
 
 

    ImGui::SetNextWindowSize(ImVec2(displaySize.x + 90.0f, displaySize.y + 60.0f),
                             ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(0, 0));

    
    ImGui::BeginChild("ScrollableArea2", ImVec2(0, 0), true, windowFlags);

    
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
                const void* data          = (const void*) &thread.second.m_events;
                int         values_count  = events.size();
                int         values_offset = 0;
                const char* overlay_text  = "";
                float       scale_min     = FLT_MAX;
                float       scale_max     = FLT_MAX;

                if(!thread.second.m_has_events_l1)
                {
                    rocprofvis_trace_event_t new_event;
                    bool                     is_first = true;
                    for(auto event : thread.second.m_events)
                    {
                        double Gap      = (event.m_start_ts -
                                      (new_event.m_duration + new_event.m_start_ts));
                        double duration = ((event.m_start_ts + event.m_duration) -
                                           new_event.m_start_ts);
                        if(!is_first && Gap < 1000.0 && duration < 1000.0)
                        {
                            new_event.m_name.clear();
                            new_event.m_duration = duration;
                        }
                        else
                        {
                            if(!is_first) thread.second.m_events_l1.push_back(new_event);

                            new_event = event;
                            is_first  = false;
                        }
                    }
                    thread.second.m_events_l1.push_back(new_event);
                    thread.second.m_has_events_l1 = true;
                }
                if(values_count > thread.second.m_events_l1.size())
                {
                    values_count = thread.second.m_events_l1.size();
                    data         = (const void*) &thread.second.m_events_l1;
                }

                ////put flamechart code here
                flameEvent                 = extractFlamePoints(events);
                flameChartPointMap[count2] = flameEvent;

                findMaxMinFlame();
                renderMain3(count2);
                count2 = count2 + 1;
            }

            else if(counters.size())
            {
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
    ImGui::SetItemAllowOverlap();  // Allow input to pass through to sub components. 
 

  
    ImGui::EndChild();

      }
    ImGui::End();
    
    renderedOnce = true; 
}

void
main_view::handleTouch() {
    // Handle Zoom
    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
    {
        float scrollWheel = ImGui::GetIO().MouseWheel;
        if(scrollWheel != 0.0f)
        {
            float       viewWidth = (maxX - minX) / zoom;
            const float zoomSpeed = 0.1f;
            zoom *= (scrollWheel > 0) ? (1.0f + zoomSpeed) : (1.0f - zoomSpeed);
            zoom            = clamp(zoom, 1.0f, 1000.0f);
           }
    }

    // Handle Panning
    if(ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        float drag      = ImGui::GetIO().MouseDelta.x;
        float viewWidth = (maxX - minX) / zoom;
        movement -= (drag / ImGui::GetContentRegionAvail().x) * viewWidth;
       
     }

 

               
}
void
main_view::renderMain2( int count2)
{


     


     ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

  
        ImGui::BeginChild((std::to_string(count2)).c_str(), ImVec2(0, 150), false,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoScrollbar);     


  
 
         line_chart line = line_chart(count2, minValue, maxValue, zoom, movement,
                                      hasZoomHappened, minX, maxX, minY, maxY, data_arr);
        line.render();
        ImGui::EndChild();
            ImGui::PopStyleVar();

        ImGui::Spacing();  // Add some spacing between boxes
 
   ImGui::Spacing();  // Add some spacing

        // Horizontal line at the top
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
    ImGui::Spacing();  // Add some spacing between boxes
    ImGui::Separator();
}