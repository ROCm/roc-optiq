// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "rocprofvis_compute_isa_data.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

struct LineSelection
{
    static constexpr uint64_t UNSELECTED         = 0;
    uint64_t                  hovered_line       = UNSELECTED;
    uint64_t                  selected_line      = UNSELECTED;
    uint64_t                  source_scroll_line = UNSELECTED;
    uint64_t                  source_scroll_file = UNSELECTED;
    uint64_t                  isa_scroll_line    = UNSELECTED;
    bool                      hovered_this_frame = false;
};

class BaseCodeWidget : public RocWidget
{
public:
    BaseCodeWidget(LineSelection& selection);
    ~BaseCodeWidget() = default;

    virtual void Render() override = 0;

protected:
    void CalculateLineNumberWidth(size_t count);
    void PushStyles();
    bool BeginCodeRow(uint64_t row_id, uint64_t source_line_id);

    LineSelection& m_line_selection;
    float          m_line_num_width  = 0.0f;
    uint32_t       m_line_num_digits = 1;

    SettingsManager& m_settings;
    ImGuiTableFlags  m_table_flags;

    ImVec4 m_line_num_color;
};

class SourceCodeWidget : public BaseCodeWidget
{
public:
    SourceCodeWidget(LineSelection& selection);
    void Render() override;

    void Load(const PcSamplingData& data, uint64_t source_file_uuid);

    uint64_t GetSelectedLine() const { return m_line_selection.selected_line; }
    uint64_t GetHoveredLine() const { return m_line_selection.hovered_line; }

private:
    uint32_t GetScrollTarget(ImGuiListClipper& clipper);
    void     RenderLine(uint32_t index);

    struct SourceRow
    {
        std::string content;
        uint64_t    id          = 0;
        uint64_t    line_number = 0;
    };

    std::vector<SourceRow> m_lines;
};

class IsaCodeWidget : public BaseCodeWidget
{
public:
    IsaCodeWidget(LineSelection& selection);
    void Render() override;

    void Load(const PcSamplingData& data, uint64_t code_object_uuid);
    void UpdateSourceLocations(const PcSamplingData& data);
    void UpdateSampling(const PcSamplingData& data);
    void ChangeSamplingVisibility(bool show) { m_show_sampling = show; }

private:
    typedef IsaDataBuilder::IsaRow               IsaRow;
    typedef IsaDataBuilder::SampleCounts         SampleCounts;
    typedef IsaDataBuilder::StallReason          StallReason;
    typedef IsaDataBuilder::SamplingAvailability SamplingAvailability;

    enum class IsaColumn : int32_t
    {
        kLineNumber,
        kSamplingState,
        kSamples,
        kStallPercent,
        kStallCategories,
        kOffset,
        kInstruction,
        kCount
    };

    static constexpr int ColumnIndex(IsaColumn column)
    {
        return static_cast<int>(column);
    }

    void SetupColumns();
    void RenderHeaders();

    uint32_t GetScrollTarget(ImGuiListClipper& clipper);
    void     RenderLine(uint32_t index);

    static ImU32 HeatmapColor(double percent);
    void         RenderStallPercentCell(const IsaRow& row, bool& row_clicked);
    void         RenderSamplesCell(uint64_t sample_count);
    void         RenderSampleSummary(uint64_t sample_count);
    void         RenderSamplingStateCell(const IsaRow& row);
    void         RenderSamplingStateTooltip(const IsaRow& row);
    void         RenderUnavailableStallCell(const char* tooltip);
    void         RenderStallReasonBarCell(const IsaRow& row, bool& row_clicked);
    void         RenderStallReasonsTooltip(const IsaRow& row);
    void         RenderStallReasonSummary(const IsaRow& row);
    void         RenderStallReasonFooter(const IsaRow& row);
    struct StallReasonTableLayout
    {
        ImVec2 size;
        float  description_width;
    };

    StallReasonTableLayout CalculateStallReasonTableLayout(const IsaRow& row);
    void RenderStallReasonTable(const IsaRow& row, const StallReasonTableLayout& layout,
                                float mouse_wheel);

    bool                m_show_sampling = false;
    std::vector<IsaRow> m_entries;
    uint64_t            m_kernel_total_samples        = 0;
    uint64_t            m_hottest_instruction_samples = 0;
    uint64_t            m_largest_code_object_offset  = 0;
    bool                m_stall_data_available        = false;
};

}  // namespace View
}  // namespace RocProfVis
