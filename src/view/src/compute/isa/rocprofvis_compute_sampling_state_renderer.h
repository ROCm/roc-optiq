// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "rocprofvis_compute_isa_data.h"
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

// Renders the same sampling-state column for ISA instructions and source lines.
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
        kWaveOccupancy,
        kActiveThreads
    };

    enum class LoadState : uint32_t
    {
        kReady,
        kLoading,
        kLoadingDetails,
        kTotalsOnly,
        kFailed
    };

    explicit SamplingStateRenderer(SettingsManager& settings);

    static void RenderHeaderWithTooltip(int column, const char* label,
                                        const HeaderTooltipText& tooltip,
                                        void (*render_user_content)() = nullptr);
    static void RenderColorLegend(const char* id, const ColorLegendEntry* entries,
                                  size_t count);

    void SetupColumn();
    void SetupMetricColumn(Metric metric);
    void SetLoadState(LoadState state) { m_load_state = state; }
    // Extends the native column menu; returns whether sampling should be retried.
    [[nodiscard]] bool RenderTableContextMenu();
    void RenderHeader(int column, CodePane pane = CodePane::kIsa);
    void RenderMetricHeader(int column, Metric metric, CodePane pane = CodePane::kIsa);
    // Returns whether the cell was clicked, for the owning widget's row selection.
    [[nodiscard]] bool RenderCell(const IsaDataBuilder::SamplingRow& row,
                                  uint64_t                           hottest_samples,
                                  CodePane pane = CodePane::kIsa);
    [[nodiscard]] bool RenderMetricCell(const IsaDataBuilder::SamplingRow& row,
                                        Metric metric, CodePane pane = CodePane::kIsa);

private:
    using SamplingRow          = IsaDataBuilder::SamplingRow;
    using SampleCounts         = IsaDataBuilder::SampleCounts;
    using StallReason          = IsaDataBuilder::StallReason;
    using SamplingAvailability = IsaDataBuilder::SamplingAvailability;

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

    struct MetricTooltipText
    {
        const char* status;
        const char* description;
    };

    struct TooltipTableLayout
    {
        ImVec2 size;
        float  state_width;
        float  color_width;
        float  count_width;
        float  share_width;
    };

    static const char* GetUnavailableSamplingTooltip(const SamplingRow& row);
    [[nodiscard]] bool IsDisabled() const;
    static TooltipRows BuildTooltipRows(const SamplingRow& row);
    TooltipTableLayout CalculateTooltipLayout(const TooltipRows& rows);
    void RenderTooltipTable(const TooltipRows& rows, const TooltipTableLayout& layout,
                            float mouse_wheel);
    MetricTooltipText GetMetricTooltipText(const SamplingRow& row, Metric metric) const;
    void              RenderMetricValue(const SamplingRow& row, Metric metric);
    void RenderMetricTooltip(const SamplingRow& row, Metric metric, CodePane pane);
    void RenderMetricsSummary(const SamplingRow& row, CodePane pane);
    void RenderMetricAggregation(CodePane pane);
    void RenderTooltip(const SamplingRow& row, double bar_percent, CodePane pane);
    void RenderSummary(const SamplingRow& row);
    void RenderFooter(const SamplingRow& row);
    void DrawStallReasonSegments(const SamplingRow& row, const ImVec2& start,
                                 const ImVec2& end);

    SettingsManager& m_settings;
    LoadState        m_load_state = LoadState::kReady;
};

}  // namespace View
}  // namespace RocProfVis
