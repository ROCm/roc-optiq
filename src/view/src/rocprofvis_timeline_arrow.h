// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include <memory>
namespace RocProfVis
{
namespace View
{

struct TimelineArrowData
{
    double start_time_ns;  // in ns
    int    start_track_px;
    double end_time_ns;  // in ns
    int    end_track_px;
};

class TimelineArrow
{
public:
    TimelineArrow(DataProvider& data_provider);
    ~TimelineArrow();
    // Draws an arrow from (start_time, y_start) to (end_time, y_end) using the given
    // mapping
    void Render(ImDrawList* draw_list, double v_min_x, double pixels_per_ns,
                ImVec2 window, std::map<uint64_t, float> track_height_total);

    // Set all arrows at once
    void SetArrows(const std::vector<TimelineArrowData>& arrows);

    // Optionally, add a method to add a single arrow
    void AddArrow(const TimelineArrowData& arrow);
    void AddArrows();

private:
    DataProvider&                   m_data_provider;
    std::vector<TimelineArrowData>  m_arrows_to_render;
    EventManager::SubscriptionToken m_add_arrow_token;
};

}  // namespace View
}  // namespace RocProfVis