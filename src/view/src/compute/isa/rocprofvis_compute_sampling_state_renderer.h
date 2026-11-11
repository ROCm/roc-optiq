// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "rocprofvis_compute_isa_data.h"
#include "rocprofvis_compute_sampling_load_state.h"
#include "rocprofvis_settings_manager.h"

namespace RocProfVis
{
namespace View
{

struct HeaderTooltipText
{
    const char* user_description;
    const char* developer_information;
};

struct ColorLegendEntry
{
    const char* name;
    Colors      color;
};

// Renders sampling-state, issue-state counts, and percentage columns for code lines.
class SamplingStateRenderer
{
public:
    enum class CodePane : uint32_t
    {
        kIsa,
        kSource
    };

    enum class Metric : uint32_t
    {
        kSamplingState,
        kWaveOccupancy,
        kActiveThreads,
        kIssueStateCounts,
        kCount
    };

    explicit SamplingStateRenderer(SettingsManager& settings);

    static void RenderHeaderWithTooltip(int column, const char* label,
                                        const HeaderTooltipText& tooltip,
                                        void (*render_user_content)() = nullptr);
    static void RenderColorLegend(const char* id, const ColorLegendEntry* entries,
                                  size_t count);

    void SetupColumns();
    void SetLoadStates(const SamplingLoadStates& states) { m_load_states = states; }
    // Extends the native column menu; returns whether sampling should be retried.
    [[nodiscard]] bool RenderTableContextMenu();
    void               RenderHeaders(int first_column, CodePane pane = CodePane::kIsa);
    // Returns whether any sampling cell was clicked, for the widget's row selection.
    [[nodiscard]] bool RenderCells(const IsaDataBuilder::SamplingRow& row,
                                   uint64_t hottest_samples, int first_column);

private:
    using SamplingRow          = IsaDataBuilder::SamplingRow;
    using SampleCounts         = IsaDataBuilder::SampleCounts;
    using StallReason          = IsaDataBuilder::StallReason;
    using SamplingAvailability = IsaDataBuilder::SamplingAvailability;

    struct BarPresentation
    {
        double percent    = 0.0;
        Colors color      = Colors::kTextDim;
        bool   available  = false;
        bool   show_value = false;
        // Borrow the row only while drawing an optional stacked breakdown.
        const SamplingRow* breakdown = nullptr;
    };

    static constexpr size_t SHARE_TEXT_CAPACITY = 32;
    struct TooltipRow
    {
        // Labels borrow the input row or static strings for this tooltip invocation.
        const char* label;
        Colors      color;
        std::string count_text;
        char        share_text[SHARE_TEXT_CAPACITY];
    };
    using TooltipRows = std::vector<TooltipRow>;

    struct TooltipTableLayout
    {
        ImVec2 size;
        float  state_width;
        float  color_width;
        float  count_width;
        float  share_width;
    };

    static const char* GetUnavailableSamplingTooltip(const SamplingRow& row);
    static void        RenderHeaderTooltip(const HeaderTooltipText& tooltip,
                                           void (*render_user_content)());
    void               DrawBar(const BarPresentation& bar, const ImVec2& cell_start,
                               const ImVec2& cell_size);
    void               DrawBarValue(const BarPresentation& bar, const ImVec2& cell_start,
                                    const ImVec2& cell_size, float bar_width);
    [[nodiscard]] bool IsDisabled(Metric metric = Metric::kSamplingState) const;
    static TooltipRows BuildTooltipRows(const SamplingRow& row);
    TooltipTableLayout CalculateTooltipLayout(const TooltipRows& rows,
                                              Metric metric = Metric::kSamplingState);
    void RenderTooltipTable(const TooltipRows& rows, const TooltipTableLayout& layout,
                            float mouse_wheel, Metric metric = Metric::kSamplingState);
    const char*        GetMetricStatusText(const SamplingRow& row, Metric metric) const;
    void               RenderHeader(int column, Metric metric, CodePane pane);
    [[nodiscard]] bool RenderCell(const SamplingRow& row, uint64_t hottest_samples,
                                  Metric metric);
    [[nodiscard]] bool RenderIssueStateCell(const SamplingRow& row);
    void               RenderIssueStateTooltip(const SamplingRow& row);
    void               RenderMetricValue(const SamplingRow& row, Metric metric);
    void               RenderMetricTooltip(const SamplingRow& row, Metric metric);
    void               RenderMetricsSummary(const SamplingRow& row);
    void               RenderTooltip(const SamplingRow& row, double bar_percent);
    void               RenderSummary(const SamplingRow& row);
    void               RenderFooter(const SamplingRow& row);
    void DrawStallReasonSegments(const SamplingRow& row, const ImVec2& start,
                                 const ImVec2& end);

    SettingsManager& m_settings;
    SamplingLoadStates m_load_states;
};

}  // namespace View
}  // namespace RocProfVis
