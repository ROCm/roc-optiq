// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <functional>
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
    static constexpr uint32_t NO_SCROLL_TARGET = 0;

    typedef IsaDataBuilder::SamplingRow          SamplingRow;
    typedef IsaDataBuilder::SampleCounts         SampleCounts;
    typedef IsaDataBuilder::StallReason          StallReason;
    typedef IsaDataBuilder::SamplingAvailability SamplingAvailability;

    void               SetupSamplingColumn();
    void               RenderSamplingHeader(int column, bool source_code = false);
    static const char* GetUnavailableSamplingTooltip(const SamplingRow& row);
    void               RenderSamplingStateCell(const SamplingRow& row, bool& row_clicked);
    void DrawStallReasonSegments(const SamplingRow& row, const ImVec2& start,
                                 const ImVec2& end);
    void RenderSamplingStateTooltip(const SamplingRow& row);
    void RenderSamplingSummary(const SamplingRow& row);
    void RenderStallReasonFooter(const SamplingRow& row);
    struct StallReasonTableLayout
    {
        ImVec2 size;
        float  reason_width;
        float  color_width;
        float  count_width;
        float  share_width;
    };

    StallReasonTableLayout CalculateStallReasonTableLayout(const SamplingRow& row);
    void                   RenderStallReasonTable(const SamplingRow&            row,
                                                  const StallReasonTableLayout& layout, float mouse_wheel);

    uint64_t m_hottest_samples = 0;

    void CalculateLineNumberWidth(size_t count);
    void PushStyles();
    bool BeginCodeRow(uint64_t row_id, uint64_t source_line_id);

    // Drives the shared clipper loop: finds the row whose id matches scroll_line
    // (clearing it once consumed), renders visible rows via render_line, and scrolls to
    // the target. id_of(index) returns a row's id; render_line(index) draws it.
    void RenderClippedRows(size_t row_count, uint64_t& scroll_line,
                           const std::function<uint64_t(uint32_t)>& id_of,
                           const std::function<void(uint32_t)>&     render_line);

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

    void Load(const PcSamplingData& data, uint64_t source_file_uuid,
              const IsaDataBuilder::PreparedSamplingData& sampling);
    void UpdateSampling(const PcSamplingData&                       data,
                        const IsaDataBuilder::PreparedSamplingData& sampling);

    uint64_t GetSelectedLine() const { return m_line_selection.selected_line; }
    uint64_t GetHoveredLine() const { return m_line_selection.hovered_line; }

private:
    void RenderLine(uint32_t index);

    typedef IsaDataBuilder::SourceRow SourceRow;
    uint64_t                          m_source_file_uuid    = 0;
    uint64_t                          m_largest_line_number = 0;

    std::vector<SourceRow> m_lines;
};

class IsaCodeWidget : public BaseCodeWidget
{
public:
    IsaCodeWidget(LineSelection& selection);
    void Render() override;

    void Load(const PcSamplingData& data, uint64_t code_object_uuid,
              const IsaDataBuilder::PreparedSamplingData& sampling);
    void UpdateSourceLocations(const PcSamplingData& data);
    void UpdateSampling(const IsaDataBuilder::PreparedSamplingData& sampling);

private:
    typedef IsaDataBuilder::IsaRow IsaRow;

    enum class IsaColumn : int32_t
    {
        kLineNumber,
        kSamplingState,
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

    void RenderLine(uint32_t index);

    std::vector<IsaRow> m_entries;
    uint64_t            m_largest_code_object_offset = 0;
};

}  // namespace View
}  // namespace RocProfVis
