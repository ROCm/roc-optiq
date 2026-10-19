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

// Renders the same sampling-state column for ISA instructions and source lines.
class SamplingStateRenderer
{
public:
    enum class CodePane : uint32_t
    {
        kIsa,
        kSource
    };

    explicit SamplingStateRenderer(SettingsManager& settings);

    void SetupColumn();
    void RenderHeader(int column, CodePane pane = CodePane::kIsa);
    // Returns whether the cell was clicked, for the owning widget's row selection.
    [[nodiscard]] bool RenderCell(const IsaDataBuilder::SamplingRow& row,
                                  uint64_t                           hottest_samples);

private:
    typedef IsaDataBuilder::SamplingRow          SamplingRow;
    typedef IsaDataBuilder::SampleCounts         SampleCounts;
    typedef IsaDataBuilder::StallReason          StallReason;
    typedef IsaDataBuilder::SamplingAvailability SamplingAvailability;

    static constexpr size_t SHARE_TEXT_CAPACITY = 32;
    struct TooltipRow
    {
        // Labels borrow the input row or static strings for this tooltip invocation.
        const char* label;
        Colors      color;
        std::string count_text;
        char        share_text[SHARE_TEXT_CAPACITY];
    };
    typedef std::vector<TooltipRow> TooltipRows;

    struct TooltipTableLayout
    {
        ImVec2 size;
        float  state_width;
        float  color_width;
        float  count_width;
        float  share_width;
    };

    static const char* GetUnavailableSamplingTooltip(const SamplingRow& row);
    static TooltipRows BuildTooltipRows(const SamplingRow& row);
    TooltipTableLayout CalculateTooltipLayout(const TooltipRows& rows);
    void RenderTooltipTable(const TooltipRows& rows, const TooltipTableLayout& layout,
                            float mouse_wheel);
    void RenderTooltip(const SamplingRow& row);
    void RenderSummary(const SamplingRow& row);
    void RenderFooter(const SamplingRow& row);
    void DrawStallReasonSegments(const SamplingRow& row, const ImVec2& start,
                                 const ImVec2& end);

    SettingsManager& m_settings;
};

}  // namespace View
}  // namespace RocProfVis
