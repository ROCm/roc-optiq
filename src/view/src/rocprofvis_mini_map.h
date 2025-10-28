// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_timeline_view.h"
#include "widgets/rocprofvis_widget.h"
#include <map>
#include <memory>
#include <vector>

namespace RocProfVis
{
namespace View
{
class DataProvider;

class MiniMap : public RocWidget
{
public:
    MiniMap(DataProvider& dp, std::shared_ptr<TimelineView> timeline);
    ~MiniMap();
    void Render();
    void Normalize();
    void RebinMiniMap(size_t max_bins);

private:
    DataProvider&                           m_data_provider;
    std::shared_ptr<TimelineView>           m_timeline;
    float                                   m_height;
    std::map<uint64_t, std::vector<double>> m_rebinned_mini_map;
    int                                     m_histogram_bins;
};

}  // namespace View
}  // namespace RocProfVis