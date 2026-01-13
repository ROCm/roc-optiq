// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_widget.h"
#include <vector>

namespace RocProfVis
{
namespace View
{

class TimelineView;
class DataProvider;

class Minimap : public RocWidget
{
public:
    Minimap(DataProvider& data_provider, TimelineView* timeline_view);
    ~Minimap() override = default;
    void UpdateData();  
    void Render() override;

    void SetData(const std::vector<std::vector<double>>& data);

private:
    static constexpr size_t MINIMAP_SIZE = 500;

    void  BinData(const std::vector<std::vector<double>>& input_data);
    void  NormalizeRawData();
    ImU32 GetColor(double normalized_value) const;
    void  RenderLegend(float width, float height);
    void  RenderMinimapData(ImDrawList* dl, ImVec2 pos, ImVec2 size);
    void  RenderViewport(ImDrawList* dl, ImVec2 pos, ImVec2 size);
    void  HandleNavigation(ImVec2 pos, ImVec2 size);

    std::vector<std::vector<double>> m_downsampled_data;
    size_t              m_data_width;
    size_t              m_data_height;
    bool                m_data_valid;
    double              m_raw_min_value;
    double              m_raw_max_value;

    DataProvider& m_data_provider;
    TimelineView* m_timeline_view;
};

}  // namespace View
}  // namespace RocProfVis
