// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "model/compute/rocprofvis_memory_chart_model.h"

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

    // Fit: scale the chart down (never above actual size) so all of it shows in
    // the space it is given. Off: draw at actual size and scroll.
    void SetFitToView(bool fit) { m_fit_to_view = fit; }
    bool GetFitToView() const { return m_fit_to_view; }
    // Auto-compact: when the space given would show too little of the chart at
    // actual size, fit it (however small) as an overview instead. Overrides
    // SetFitToView and never stretches.
    void SetAutoCompact(bool enabled) { m_auto_compact = enabled; }
    // Whether the last frame was drawn compact.
    bool IsCompact() const { return m_is_compact; }

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
    // Load the dev override file (<config-dir>/memory_chart.json) into m_layout.
    // Returns true if a valid override was found and applied.
    bool TryLoadOverrideFile();

    // Called once whenever m_layout is (re)assigned (workload change): sorts
    // blocks by column/`order`, builds the id -> block index, and primes the
    // per-item/arrow render strings.
    void OnLayoutLoaded();
    // Recompute the cached label/value strings for every content item and arrow
    // from the currently-resolved metrics (on layout load and on metric fetch).
    void RefreshMetricStrings();
    // O(1) block lookup by id (backed by m_block_by_id).
    const MemChartBlock* Block(uint32_t id) const;

    // Precompute which inter-column gaps an arrow crosses (m_gap_has_arrow). Only
    // depends on block columns and arrow endpoints, so it runs on layout load
    // rather than every frame.
    void RebuildColumnGaps();

    // `extra_height` is added to the common column height, stretching every
    // column (used to fill a pane taller than the chart's natural height).
    void ComputeLayout(float available_width, float extra_height = 0.0f);
    // Layout + arrow routing, returning the canvas size they cover.
    void LayoutCanvas(float available_width, float extra_height,
                      std::vector<ArrowRoute>& routes, float& canvas_w, float& canvas_h);
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
                                       bool include_unit = true,
                                       const std::string& unit_override = "") const;

    void ShowMetricTooltip(ImVec2 hover_min, ImVec2 hover_max,
                           const MemChartMetricRef& ref, bool show_description,
                           bool show_raw_value);
    // Blocks and labels are laid out at actual size, then scaled into view about
    // this frame's origin; hit tests map through the same transform.
    ImVec2 ToScreen(ImVec2 unscaled) const;
    bool   IsHoveringChartRect(ImVec2 unscaled_min, ImVec2 unscaled_max) const;
    void DrawTextWithTooltip(ImDrawList* draw_list, ImVec2 pos, uint32_t color,
                             const char* text, const MemChartMetricRef& ref,
                             bool show_description, bool show_raw_value);

    DataProvider&                     m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;

    uint64_t m_client_id;

    bool  m_fit_to_view   = false;
    bool  m_auto_compact  = false;
    bool  m_is_compact    = false;
    float m_view_scale    = 1.0f;
    float m_view_origin_x = 0.0f;
    float m_view_origin_y = 0.0f;

    MemChartLayout m_layout;

    // Group boxes (title + rect) computed during layout, drawn behind blocks.
    std::vector<MemChartGroupBox> m_group_boxes;

    // Resolved after each fetch; keyed by the metric's full dotted id
    // ("category.table.entry", e.g. "3.1.0").
    std::unordered_map<std::string, const MetricValue*> m_ptr_by_metric_id;

    // Block-by-id index into m_layout, rebuilt on layout load; avoids scanning
    // the block tree on every arrow lookup each frame.
    std::unordered_map<uint32_t, const MemChartBlock*> m_block_by_id;

    // For each inter-column gap (between ascending distinct columns), whether an
    // arrow crosses it. Precomputed on layout load so ComputeLayout avoids an
    // arrow-by-gap scan every frame.
    std::vector<bool> m_gap_has_arrow;
};

}  // namespace View
}  // namespace RocProfVis
