// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
// Standalone port of the app's ComputeMemoryChartView drawing/layout code. It
// takes a relational MemChartLayout (parsed from JSON) plus an optional
// MetricStore (values read from a trace DB) and renders the memory chart with
// Dear ImGui. All dependencies on SettingsManager / DataProvider / gui_helpers
// have been inlined so this can build outside the main application.

#pragma once

#include "rocprofvis_memory_chart_model.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct ImDrawList;
struct ImVec2;

namespace mcv
{

class MetricStore;
struct ResolvedMetric;

class MemChartRenderer
{
public:
    // Parse and adopt a layout. Returns false and sets *error on parse failure
    // (the previous layout is kept).
    bool SetLayoutJson(const std::string& json_text, std::string* error);

    void SetMetricStore(const MetricStore* store) { m_store = store; }

    bool HasLayout() const { return !m_layout.blocks.empty(); }

    // Draw the chart into the current ImGui window.
    void Render();

    const RocProfVis::View::MemChartLayout& Layout() const { return m_layout; }

private:
    struct ArrowRoute
    {
        std::vector<std::pair<float, float>>  points;
        bool                                  head_at_first = false;
        bool                                  head_at_last  = false;
        uint32_t                              color         = 0;
        std::string                           label;
        float                                 label_x = 0.0f;
        float                                 label_y = 0.0f;
        float                                 label_w = 0.0f;
        float                                 label_h = 0.0f;
        RocProfVis::View::MemChartMetricRef   metric;
    };

    void ComputeLayout(float available_width);
    void MeasureBlock(RocProfVis::View::MemChartBlock& block) const;
    void PositionBlock(RocProfVis::View::MemChartBlock& block, float x, float y, float w,
                       float h, float conn_l, float conn_r, int32_t column);

    void DrawBlock(ImDrawList* draw_list, ImVec2 origin,
                   const RocProfVis::View::MemChartBlock& block);
    void DrawLeaf(ImDrawList* draw_list, ImVec2 origin,
                  const RocProfVis::View::MemChartBlock& block);
    void BuildArrowRoutes(std::vector<ArrowRoute>& routes) const;
    void ResolveLabelOverlaps(std::vector<ArrowRoute>& routes) const;
    void DrawArrowRoutes(ImDrawList* draw_list, ImVec2 origin,
                         const std::vector<ArrowRoute>& routes);

    const ResolvedMetric* ResolveMetric(const RocProfVis::View::MemChartMetricRef& ref) const;
    std::string           MetricLabel(const RocProfVis::View::MemChartMetricRef& ref,
                                      const std::string& title_override) const;
    std::string           MetricValueText(const RocProfVis::View::MemChartMetricRef& ref,
                                          bool include_unit = true) const;

    void ShowMetricTooltip(ImVec2 hover_min, ImVec2 hover_max,
                           const RocProfVis::View::MemChartMetricRef& ref,
                           bool show_description, bool show_raw_value);
    void DrawTextWithTooltip(ImDrawList* draw_list, ImVec2 pos, uint32_t color,
                             const char* text,
                             const RocProfVis::View::MemChartMetricRef& ref,
                             bool show_description, bool show_raw_value);

    RocProfVis::View::MemChartLayout            m_layout;
    std::vector<RocProfVis::View::MemChartGroupBox> m_group_boxes;
    const MetricStore*                          m_store = nullptr;
};

}  // namespace mcv
