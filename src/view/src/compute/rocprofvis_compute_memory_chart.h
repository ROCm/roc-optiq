// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "model/compute/rocprofvis_memory_chart_model.h"
#include "rocprofvis_event_manager.h"

#include <cstdint>
#include <map>
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
        // Flow direction (-1/0/+1 per axis) drawn as a label marker so same-color
        // arrows are told apart by which way they point; bidir = double-headed.
        float             label_dir_x   = 0.0f;
        float             label_dir_y   = 0.0f;
        bool              label_bidir   = false;
        MemChartMetricRef metric;
    };

    // Horizontal extent a skip-column arrow occupies along its row's highway.
    struct SkipSpan
    {
        size_t index;  // into m_layout.arrows
        float  lo;
        float  hi;
        int    lane;
    };

    void LoadLayout();
    // Load the dev override file (<config-dir>/memory_chart.json) into m_layout.
    // Returns true if a valid override was found and applied.
    bool TryLoadOverrideFile();

    // Called once whenever m_layout is (re)assigned (workload change): sorts
    // blocks by column/`order`, resolves each arrow's endpoint ids to block
    // pointers, and primes the per-item/arrow render strings.
    void OnLayoutLoaded();
    // Recompute the cached label/value strings for every content item and arrow
    // from the currently-resolved metrics (on layout load and on metric fetch).
    void RefreshMetricStrings();
    // Snapshot the memory-chart palette from SettingsManager. Called on
    // construction and on kThemeChanged.
    void RefreshPalette();
    // Map each item/arrow's cached_color_kind through the current palette into
    // cached_color. Called after RefreshMetricStrings and on theme change.
    void RefreshCachedColors();
    uint32_t ColorFromKind(MemChartColorKind kind) const;

    void DrawBlockRect(ImDrawList* draw_list, ImVec2 top_left, ImVec2 bottom_right);
    float DrawBlockHeader(ImDrawList* draw_list, const char* title, float block_x,
                          float block_y, float block_w);
    // Draws a floating label. When (dir_x, dir_y) is non-zero, a small flow
    // marker is drawn to the left of the text (double-headed when `bidir`).
    void DrawFloatingLabel(ImDrawList* draw_list, ImVec2 pos, const char* text,
                           uint32_t accent_color, float dir_x = 0.0f, float dir_y = 0.0f,
                           bool bidir = false);
    void DrawGroupBox(ImDrawList* draw_list, ImVec2 top_left, float w, float h,
                      const char* title);
    void DrawLegend(ImDrawList* draw_list, ImVec2 origin, float y);

    // Precompute which inter-column gaps an arrow crosses, and whether any of
    // those arrows is labeled (m_gap_kinds). Only depends on block columns,
    // arrow endpoints and label presence, so it runs on layout load rather than
    // every frame.
    void RebuildColumnGaps();

    void ComputeLayout(float available_width);
    void MeasureBlock(MemChartBlock& block) const;
    // Recursively assign geometry: `conn_left`/`conn_right` are the top-level
    // ancestor's box edges (passed unchanged into children) so arrows terminate
    // at the outer box; `column`/`row` are propagated so routing sees nested blocks.
    void PositionBlock(MemChartBlock& block, float x, float y, float w, float h,
                       float conn_l, float conn_r, int32_t column, int32_t row);

    // Recursively draw a block: container -> recurse into children; leaf -> card.
    void DrawBlock(ImDrawList* draw_list, ImVec2 origin, const MemChartBlock& block);
    void DrawLeaf(ImDrawList* draw_list, ImVec2 origin, const MemChartBlock& block);
    // Group skip-column arrows by row band, spanning column centres (or the left
    // margin for left-going arrows). Shared by ComputeLayout, which reserves the
    // lanes' height under each band, and BuildArrowRoutes, which draws them.
    void CollectSkipSpans(const std::map<int32_t, float>&           col_mid_x,
                          std::map<int32_t, std::vector<SkipSpan>>& spans_by_row) const;
    // First-fit lane packing (shortest spans innermost); returns the lane count.
    static int PackSkipLanes(std::vector<SkipSpan>& spans);
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
    void DrawTextWithTooltip(ImDrawList* draw_list, ImVec2 pos, uint32_t color,
                             const char* text, const MemChartMetricRef& ref,
                             bool show_description, bool show_raw_value);

    DataProvider&                     m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;

    uint64_t m_client_id;

    struct ChartColors
    {
        uint32_t bg         = 0;
        uint32_t panel      = 0;
        uint32_t panel_alt  = 0;
        uint32_t border     = 0;
        uint32_t border_hot = 0;
        uint32_t text_main  = 0;
        uint32_t text_dim   = 0;
        uint32_t read       = 0;
        uint32_t write      = 0;
        uint32_t atomic     = 0;
        uint32_t util       = 0;
        uint32_t hit        = 0;
        uint32_t stall      = 0;
        uint32_t shadow     = 0;
    };
    ChartColors m_colors;

    EventManager::SubscriptionToken m_theme_changed_token =
        EventManager::InvalidSubscriptionToken;

    MemChartLayout m_layout;

    // Group boxes (title + rect) computed during layout, drawn behind blocks.
    std::vector<MemChartGroupBox> m_group_boxes;

    // Resolved after each fetch; keyed by the metric's full dotted id
    // ("category.table.entry", e.g. "3.1.0").
    std::unordered_map<std::string, const MetricValue*> m_ptr_by_metric_id;

    // What crosses each inter-column gap (between ascending distinct columns),
    // ordered by how much room the gap needs. Precomputed on layout load so
    // ComputeLayout avoids an arrow-by-gap scan every frame.
    enum class GapKind : uint8_t
    {
        kEmpty,
        kUnlabeledArrow,
        kLabeledArrow,
    };
    std::vector<GapKind> m_gap_kinds;

    // Cached geometry so the (fairly heavy) layout + arrow routing only runs when
    // something that affects it changes - the panel width, the font, or the
    // metric strings/colors (which flip m_layout_dirty). Drawing still happens
    // every frame from these cached results.
    std::vector<ArrowRoute> m_routes;
    float                   m_cached_layout_width = -1.0f;
    float                   m_cached_font_size    = -1.0f;
    float                   m_canvas_w            = 0.0f;
    float                   m_canvas_h            = 0.0f;
    bool                    m_layout_dirty        = true;
};

}  // namespace View
}  // namespace RocProfVis
