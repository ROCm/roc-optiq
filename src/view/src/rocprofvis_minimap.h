// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_widget.h"
#include <vector>

namespace RocProfVis
{
namespace View
{

class TimelineView;
class DataProvider;
struct TrackInfo;

class Minimap : public RocWidget
{
public:
    Minimap(DataProvider& data_provider, TimelineView* timeline_view);
    ~Minimap() override = default;
    void UpdateData();  
    void Render() override;

    void SetData(const std::vector<std::vector<double>>& data);

#ifdef IMGUI_ENABLE_TEST_ENGINE
    bool GetShowEventsForTest() const { return m_show_events; }
    bool GetEventsCheckboxScreenCenterForTest(ImVec2& out) const
    {
        if(!m_events_checkbox_rect_valid_for_test) return false;
        out = ImVec2((m_events_checkbox_min_for_test.x + m_events_checkbox_max_for_test.x) * 0.5f,
                     (m_events_checkbox_min_for_test.y + m_events_checkbox_max_for_test.y) * 0.5f);
        return true;
    }
#endif

private:
    static constexpr size_t MINIMAP_SIZE = 500;

    void  BinData(const std::vector<std::vector<double>>& input_data);
    void  NormalizeRawData();
    void  UpdateColorCache();
    ImU32 GetColor(double normalized_value, rocprofvis_controller_track_type_t type) const;
    void  RenderLegend(float width, float height);
    void  RenderMinimapData(ImDrawList* dl, ImVec2 pos, ImVec2 size);
    void  RenderViewport(ImDrawList* dl, ImVec2 pos, ImVec2 size);
    void  HandleNavigation(ImVec2 pos, ImVec2 size);

    std::vector<std::vector<double>> m_downsampled_data;
    std::vector<const TrackInfo*>    m_visible_tracks;
    size_t              m_data_width;
    size_t              m_data_height;
    bool                m_data_valid;
    bool                m_show_events;
    bool                m_show_counters;
    double              m_raw_min_value;
    double              m_raw_max_value;

    // Cached color arrays for performance
    std::vector<ImU32>  m_event_color_bins;
    std::vector<ImU32>  m_counter_color_bins;

    DataProvider& m_data_provider;
    TimelineView* m_timeline_view;
    double        m_event_global_max;
    bool          m_last_normalize_global;

#ifdef IMGUI_ENABLE_TEST_ENGINE
    bool   m_events_checkbox_rect_valid_for_test = false;
    ImVec2 m_events_checkbox_min_for_test{ 0.0f, 0.0f };
    ImVec2 m_events_checkbox_max_for_test{ 0.0f, 0.0f };
#endif
};

}  // namespace View
}  // namespace RocProfVis
