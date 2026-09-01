// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_memory_chart_model.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct ImDrawList;
struct ImVec2;

namespace RocProfVis
{
namespace View
{

struct MetricValue;
class DataProvider;
class ComputeSelection;

// Renders the compute kernel-details "Memory Chart" from a relational layout
// (MemChartLayout): generic blocks arranged in columns, connected by arrows.
// Metric ids/names referenced by the layout are resolved against the compute
// metric table for the selected kernel.
class ComputeMemoryChartView
{
public:
    ComputeMemoryChartView(DataProvider&                     data_provider,
                           std::shared_ptr<ComputeSelection> compute_selection);
    ~ComputeMemoryChartView();

    void Render();

    // Load the layout for a workload: prefer the JSON stored in the DB
    // (WorkloadInfo::memory_chart_layout); fall back to the embedded default.
    void LoadWorkloadLayout(uint32_t workload_id);

    // Fetch every metric in the layout's source category for the selected kernel.
    void FetchMemChartMetrics();

    void UpdateMetrics();

    uint64_t GetClientId() const { return m_client_id; }

private:
    // A fully computed arrow, ready to draw. Produced by BuildArrowRoutes so
    // labels can be de-overlapped before anything is drawn. Coordinates are in
    // local canvas space.
    struct ArrowRoute
    {
        std::vector<std::pair<float, float>> points;  // Polyline (x, y).
        bool              head_at_first = false;
        bool              head_at_last  = false;
        uint32_t          color         = 0;
        std::string       label;
        float             label_x       = 0.0f;
        float             label_y       = 0.0f;
        float             label_w       = 0.0f;
        float             label_h       = 0.0f;
        MemChartMetricRef metric;
    };

    void LoadLayout();
    void ComputeLayout(float available_width);
    void MeasureBlock(MemChartBlock& block) const;
    // Recursively assign geometry: `conn_left`/`conn_right` are the top-level
    // ancestor's box edges (passed unchanged into children) so arrows terminate
    // at the outer box; `column` is propagated so routing sees nested blocks.
    void PositionBlock(MemChartBlock& block, float x, float y, float w, float h,
                       float conn_l, float conn_r, int32_t column);

    // Recursively draw a block: container -> recurse into children; leaf -> card.
    void DrawBlock(ImDrawList* draw_list, ImVec2 origin, const MemChartBlock& block);
    void DrawLeaf(ImDrawList* draw_list, ImVec2 origin, const MemChartBlock& block);
    void BuildArrowRoutes(std::vector<ArrowRoute>& routes) const;
    void ResolveLabelOverlaps(std::vector<ArrowRoute>& routes) const;
    void DrawArrowRoutes(ImDrawList* draw_list, ImVec2 origin,
                         const std::vector<ArrowRoute>& routes);

    // Metric resolution (by id or name) against the fetched table.
    const MetricValue* ResolveMetric(const MemChartMetricRef& ref) const;
    std::string        MetricLabel(const MemChartMetricRef& ref,
                                   const std::string&       title_override) const;
    std::string        MetricValueText(const MemChartMetricRef& ref,
                                       bool include_unit = true) const;

    void ShowMetricTooltip(ImVec2 hover_min, ImVec2 hover_max,
                           const MemChartMetricRef& ref, bool show_description,
                           bool show_raw_value);
    void DrawTextWithTooltip(ImDrawList* draw_list, ImVec2 pos, uint32_t color,
                             const char* text, const MemChartMetricRef& ref,
                             bool show_description, bool show_raw_value);

    DataProvider&                     m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;

    uint64_t m_client_id;

    MemChartLayout m_layout;

    // Group boxes (title + rect) computed during layout, drawn behind blocks.
    std::vector<MemChartGroupBox> m_group_boxes;

    // Resolved after each fetch; keyed by the metric's full dotted id
    // ("category.table.entry", e.g. "3.1.0").
    std::unordered_map<std::string, const MetricValue*> m_ptr_by_metric_id;
};

}  // namespace View
}  // namespace RocProfVis
