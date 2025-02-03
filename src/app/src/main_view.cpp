#include "imgui.h"
#include "main_view.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
 #include "line_chart.h"
#include "flame_chart.h"
 

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
main_view::~main_view() {}

std::vector<dataPoint>
main_view::extractPointsFromData(void* data)
{
    // Cast the void* pointer back to the original type
    auto* counters_vector = static_cast<std::vector<rocprofvis_trace_counter_t>*>(data);

    std::vector<dataPoint> aggregatedPoints;

    // Get screen width from ImGui
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
     int    screenWidth = static_cast<int>(displaySize.x);  // Screen width in px

    // Adjust bin size based on zoom factor
    float effectiveWidth = screenWidth / zoom;
    float binSize        = (maxX - minX) / effectiveWidth;

    double binSumX         = 0.0;
    double binSumY         = 0.0;
    int    binCount        = 0;
    double currentBinStart = counters_vector->at(0).m_start_ts;
 

    for(const auto& counter : *counters_vector)
    {
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

    // Handle the last bin
    if(binCount > 0)
    {
        dataPoint binnedPoint;
        binnedPoint.xValue = binSumX / binCount;
        binnedPoint.yValue = binSumY / binCount;
        aggregatedPoints.push_back(binnedPoint);
    }

    return aggregatedPoints;
    }

std::vector<rocprofvis_trace_event_t> extractFlamePoints(const std::vector<rocprofvis_trace_event_t>& traceEvents) {

    std::vector<rocprofvis_trace_event_t> entries; 
    int                                   count = 0;
    for (const auto& event : traceEvents){



                    entries.push_back({ event.m_name, event.m_start_ts , event.m_duration });

            
                     
     
    }
    return entries; 
}


void
main_view::generate_graph_points(std::map<std::string, rocprofvis_trace_process_t>& trace_data)

{
    int count2 = 0;
    for(auto& process : trace_data)
    {
         
            for(auto& thread : process.second.m_threads)
            {
                 
                    auto& events   = thread.second.m_events;
                    auto& counters = thread.second.m_counters;
                    if(events.size()){
                    
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
                                    double Gap =
                                        (event.m_start_ts -
                                         (new_event.m_duration + new_event.m_start_ts));
                                    double duration =
                                        ((event.m_start_ts + event.m_duration) -
                                         new_event.m_start_ts);
                                    if(!is_first && Gap < 1000.0 && duration < 1000.0)
                                    {
                                        new_event.m_name.clear();
                                        new_event.m_duration = duration;
                                    }
                                    else
                                    {
                                        if(!is_first)
                                            thread.second.m_events_l1.push_back(
                                                new_event);

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
                       
                               
                            //put flamechart code here 
                            flameEvent = extractFlamePoints(events);



                                count2 = count2 + 1;

                                renderMain3(count2);

                    }
                    
                    

                    else if(counters.size())
                    {
                     void* datap = (void*) &thread.second.m_counters;
                        int   count = counters.size();
                        ;
                         std::vector<dataPoint> points = extractPointsFromData(datap);
                        data_arr                      = points;
                        findMaxMin();
                        count2 = count2 + 1;

                        renderMain2(count2);
                    }
                
            }
        
    }
}

void
main_view::handleTouch() {
    // Handle Zoom
    if(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
    {
        float scrollWheel = ImGui::GetIO().MouseWheel;
        if(scrollWheel != 0.0f)
        {
            // Use a factor to control the zoom speed more gradually
            const float zoomSpeed = 0.01f;
            zoom *= (scrollWheel > 0) ? (1.0f + zoomSpeed) : (1.0f - zoomSpeed);
            zoom            = clamp(zoom, 0.001f, 1000.0f);
            hasZoomHappened = true;
         }
    }

    // Handle Panning
    if(ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        float drag      = ImGui::GetIO().MouseDelta.x/100;
        float viewWidth = (maxX - minX) / zoom;
        movement -= (drag / ImGui::GetContentRegionAvail().x) * viewWidth;
        movement = clamp(movement, 0.0f, (maxX - minX) - viewWidth);
    }

 

               
}
void
main_view::renderMain2( int count2)
{



       ImVec2 displaySize = ImGui::GetIO().DisplaySize;

        ImGui::SetNextWindowPos(ImVec2(displaySize.x, 0), ImGuiCond_Always,
                                ImVec2(1.0f, 0.0f));

        ImGui::SetNextWindowSize(ImVec2(displaySize.x * 0.8f, displaySize.y * 0.8f),
                                 ImGuiCond_Always);

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollWithMouse ;
      ImGui::BeginChild("ScrollableArea", ImVec2(0, 0), true, windowFlags);


      ImDrawList* drawList = ImGui::GetWindowDrawList();
      ImVec2      sPos     = ImGui::GetCursorScreenPos();
      ImVec2      subComponentSize = ImGui::GetContentRegionAvail();
 
      
      if(ImGui::IsWindowHovered())
      {
          ImVec2 mPos = ImGui::GetMousePos();
          drawList->AddLine(ImVec2(mPos.x, sPos.y),
                            ImVec2(mPos.x, sPos.y + subComponentSize.y),
                            IM_COL32(0, 0, 0, 255), 2.0f);
      } 
    handleTouch(); 


 
  
        ImGui::BeginChild((std::to_string(count2)).c_str(), ImVec2(0, 150), true,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoScrollbar);     


 




         line_chart line = line_chart(count2, minValue, maxValue, zoom, movement,
                                      hasZoomHappened, minX, maxX, minY, maxY, data_arr);
        line.render();
        ImGui::EndChild();
        ImGui::Spacing();  // Add some spacing between boxes
 
   
    ImGui::SetItemAllowOverlap();  // Allow input to pass through

    ImGui::EndChild();
}


void
main_view::renderMain3(int count2)
{
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(displaySize.x, 0), ImGuiCond_Always,
                            ImVec2(1.0f, 0.0f));

    ImGui::SetNextWindowSize(ImVec2(displaySize.x * 0.8f, displaySize.y * 0.8f),
                             ImGuiCond_Always);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::BeginChild("ScrollableArea", ImVec2(0, 0), true, windowFlags);

    ImDrawList* drawList         = ImGui::GetWindowDrawList();
    ImVec2      sPos             = ImGui::GetCursorScreenPos();
    ImVec2      subComponentSize = ImGui::GetContentRegionAvail();

    if(ImGui::IsWindowHovered())
    {
        ImVec2 mPos = ImGui::GetMousePos();
        drawList->AddLine(ImVec2(mPos.x, sPos.y),
                          ImVec2(mPos.x, sPos.y + subComponentSize.y),
                          IM_COL32(0, 0, 0, 255), 2.0f);
    }
    handleTouch();

    ImGui::BeginChild((std::to_string(count2)).c_str(), ImVec2(0, 150), true,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollWithMouse |
                          ImGuiWindowFlags_NoScrollbar);
 
     

    

    FlameChart flame =
        FlameChart(count2, minValue, maxValue, zoom, movement, minX, maxX, flameEvent);
  
    flame.render();





    ImGui::EndChild();
    ImGui::Spacing();  // Add some spacing between boxes

    ImGui::SetItemAllowOverlap();  // Allow input to pass through

    ImGui::EndChild();
}