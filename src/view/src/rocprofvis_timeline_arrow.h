// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_data_provider.h"
#include <memory>

namespace RocProfVis
{
namespace View
{

struct TimelineArrowData
{
    double start_time;  // in ns
    int    start_track;
    double end_time;  // in ns
    int    end_track;
};

class TimelineArrow
{
public:
    TimelineArrow(DataProvider& data_provider);
    ~TimelineArrow();
    // Draws an arrow from (start_time, y_start) to (end_time, y_end) using the given
    // mapping
    void Draw(ImDrawList* draw_list, double v_min_x, double pixels_per_ns, ImVec2 window,
              std::map<uint64_t, float> track_height_total,
              ImU32 color = IM_COL32(255, 0, 0, 255), float thickness = 4.0f,
              float head_size = 14.0f );
    void        Update();
    void        Render();

    // Set all arrows at once
    void SetArrows(const std::vector<TimelineArrowData>& arrows);

    // Optionally, add a method to add a single arrow
    void AddArrow(const TimelineArrowData& arrow);

private:
    DataProvider&                  m_data_provider;
    std::vector<TimelineArrowData> m_arrows_to_render;
};

}  // namespace View
}  // namespace RocProfVis