// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_mini_map.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_view.h"
#include <algorithm>

namespace RocProfVis
{
namespace View
{

MiniMap::MiniMap(DataProvider& dp, std::shared_ptr<TimelineView> timeline)
: m_data_provider(dp)
, m_timeline(timeline)
, m_height(0)
, m_rebinned_mini_map({})
, m_histogram_bins(0)
{
 }
MiniMap::~MiniMap() {}
void
MiniMap::RebinMiniMap(size_t max_bins)
{
    m_histogram_bins = m_data_provider.GetHistogram().size();
    const auto& mini_map    = m_data_provider.GetMiniMap();
    size_t      track_count = mini_map.size();
    if(track_count <= max_bins)
    {
        m_rebinned_mini_map = mini_map;
        Normalize();
        return;
    }

    // Get all keys
    std::vector<uint64_t> track_ids;
    for(const auto& [track_id, _] : mini_map)
        track_ids.push_back(track_id);

    size_t group_size = (track_count + max_bins - 1) / max_bins;  
    m_rebinned_mini_map.clear();

    for(size_t i = 0; i < track_count; i += group_size)
    {
        // Aggregate bins for this group
        std::vector<double> avg_bins;
        size_t              bins_per_track = mini_map.at(track_ids[i]).size();
        avg_bins.resize(bins_per_track, 0.0);
        size_t tracks_in_group = 0;

        for(size_t j = i; j < i + group_size && j < track_count; ++j)
        {
            const auto& bins = mini_map.at(track_ids[j]);
            for(size_t k = 0; k < bins_per_track; ++k)
                avg_bins[k] += bins[k];
            tracks_in_group++;
        }
        for(size_t k = 0; k < bins_per_track; ++k)
            avg_bins[k] /= tracks_in_group;

        m_rebinned_mini_map[track_ids[i]] = std::move(avg_bins);
    }

    Normalize();
}
void
MiniMap::Normalize()
{
    // Get minimap and histogram from DataProvider
    const auto& histogram = m_data_provider.GetHistogram();

    // Find min and max in histogram
    auto   minmax  = std::minmax_element(histogram.begin(), histogram.end());
    double min_val = *minmax.first;
    double max_val = *minmax.second;
    double range   = (max_val > min_val) ? (max_val - min_val) : 1.0;

    // Normalize all bins in-place to [0,1] using log scale
    for(auto& [track_id, bins] : m_rebinned_mini_map)
    {
        for(double& value : bins)
        {
            double norm = (range > 0.0) ? (value - min_val) / range : 0.0;
            norm        = std::clamp(norm, 0.0, 1.0);
            norm        = std::log10(1.0 + 9.0 * norm) / std::log10(10.0);
            value       = norm;
        }
    }
}
void
MiniMap::Render()
{
    ImGui::BeginChild("MiniMapComponent", ImVec2(0, 0), true,
                      ImGuiWindowFlags_NoScrollWithMouse);

    float mini_map_height = ImGui::GetContentRegionAvail().y;
    float mini_map_width  = ImGui::GetContentRegionAvail().x;

    if(m_height != mini_map_height || m_timeline->GetReorderStatus())
    {
        m_height = mini_map_height;
        RebinMiniMap(m_height);
    }

    SettingsManager& settings = SettingsManager::GetInstance();

    ImU32 colors[7] = {
        settings.GetColor(Colors::kMMBin1), settings.GetColor(Colors::kMMBin2),
        settings.GetColor(Colors::kMMBin3), settings.GetColor(Colors::kMMBin4),
        settings.GetColor(Colors::kMMBin5), settings.GetColor(Colors::kMMBin6),
        settings.GetColor(Colors::kMMBin7)

    };

    double rect_height =
        mini_map_height / static_cast<double>(m_rebinned_mini_map.size());
    double rect_width = mini_map_width / m_histogram_bins;

    int track_idx = 0;
    for(const auto& [track_id, bins] : m_rebinned_mini_map)
    {
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        for(size_t i = 0; i < bins.size(); ++i)
        {
            int color_idx = static_cast<int>(std::round(bins[i] * 6));
            color_idx     = std::clamp(color_idx, 0, 6);

            // Use rounded pixel positions to avoid floating point errors
            float x0 = std::round(cursor.x + static_cast<float>(i) * rect_width);
            float x1 = std::round(cursor.x + static_cast<float>(i + 1) * rect_width);
            float y0 = std::round(cursor.y);
            float y1 = std::round(cursor.y + rect_height);

            ImVec2 p_min(x0, y0);
            ImVec2 p_max(x1, y1);
            ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, colors[color_idx]);
        }
        ImGui::Dummy(ImVec2(static_cast<float>(bins.size()) * rect_width,
                            static_cast<float>(rect_height)));
        track_idx++;
    }

    ImGui::EndChild();

    if(ImGui::IsItemClicked())
    {
        // Get mouse position relative to minimap
        ImVec2 mouse_pos   = ImGui::GetMousePos();
        ImVec2 minimap_pos = ImGui::GetItemRectMin();
        double rel_x       = mouse_pos.x - minimap_pos.x;
        double rel_y       = mouse_pos.y - minimap_pos.y;

        // Get minimap and histogram from DataProvider
        const auto& histogram = m_data_provider.GetHistogram();
        double      min_val   = m_data_provider.GetStartTime();
        double      max_val   = m_data_provider.GetEndTime();

        // Calculate bin index
        size_t bin_count = histogram.size();
        size_t bin_idx   = static_cast<size_t>(rel_x / mini_map_width * bin_count);
        bin_idx          = std::clamp(bin_idx, size_t(0), bin_count - 1);

        // Calculate v_min and v_max based on bin
        double bin_width = (max_val - min_val) / bin_count;
        double v_min     = min_val + bin_idx * bin_width;
        double v_max     = v_min + bin_width;

        // Calculate y_position based on y_scroll_max
        double y_relative_position =
            m_timeline->GetYScrollMax() * (rel_y / mini_map_height);
        // Center the view on the selected bin
        bool center = true;

        // NavigationEvent
        EventManager::GetInstance()->AddEvent(
            std::make_shared<NavigationEvent>(v_min, v_max, y_relative_position, center));
    }
}

}  // namespace View
}  // namespace RocProfVis