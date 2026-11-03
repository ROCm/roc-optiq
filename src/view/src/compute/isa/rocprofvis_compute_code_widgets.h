// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "rocprofvis_compute_isa_data.h"
#include "rocprofvis_compute_sampling_state_renderer.h"
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

enum class CodeLoadState : uint32_t
{
    kNotLoaded,
    kLoading,
    kLoaded,
    kFailed
};

class BaseCodeWidget : public RocWidget
{
public:
    explicit BaseCodeWidget(LineSelection& selection);
    ~BaseCodeWidget() override = default;

    virtual void Render() override = 0;

    void               SetLoadState(CodeLoadState state);
    void               SetSamplingLoadState(SamplingStateRenderer::LoadState state);
    [[nodiscard]] bool TakeCodeRetryRequest();
    [[nodiscard]] bool TakeSamplingRetryRequest();

protected:
    uint64_t m_hottest_samples = 0;

    void               CalculateLineNumberWidth(size_t count);
    void               RenderEmptyState(const char* data_name, const char* empty_message);
    void               RenderTableContextMenu();
    [[nodiscard]] bool BeginCodeRow(uint64_t row_id, uint64_t source_line_id);

    // Rebuilt only when rows or source correlations change; keep the first matching row.
    void RebuildRowIndex(size_t                                   row_count,
                         const std::function<uint64_t(uint32_t)>& source_line_of);
    // Consume unmatched requests as well, once the destination data is loaded.
    [[nodiscard]] std::optional<uint32_t> TakeScrollTarget(uint64_t& scroll_line) const;
    void RenderClippedRows(size_t row_count, std::optional<uint32_t> scroll_target,
                           const std::function<void(uint32_t)>& render_line);

    LineSelection& m_line_selection;
    float          m_line_num_width  = 0.0f;
    uint32_t       m_line_num_digits = 1;

    SettingsManager&      m_settings;
    SamplingStateRenderer m_sampling_renderer;
    ImGuiTableFlags       m_table_flags;

    ImVec4 m_line_num_color;

private:
    CodeLoadState m_load_state     = CodeLoadState::kNotLoaded;
    bool          m_retry_code     = false;
    bool          m_retry_sampling = false;

    std::unordered_map<uint64_t, uint32_t> m_row_index_by_source_line;
};

class SourceCodeWidget : public BaseCodeWidget
{
public:
    explicit SourceCodeWidget(LineSelection& selection);
    void Render() override;

    void Load(const PcSamplingData& data, uint64_t source_file_uuid,
              const IsaDataBuilder::PreparedSamplingData& sampling);
    void UpdateSampling(const PcSamplingData&                       data,
                        const IsaDataBuilder::PreparedSamplingData& sampling);

private:
    enum class SourceColumn : int32_t
    {
        kLineNumber,
        kSamplingState,
        kSourceText,
        kCount
    };

    static constexpr int ColumnIndex(SourceColumn column)
    {
        return static_cast<int>(column);
    }

    void SetupColumns();
    void RenderHeaders();
    void RenderLine(uint32_t index);

    using SourceRow = IsaDataBuilder::SourceRow;

    uint64_t m_source_file_uuid    = 0;
    uint64_t m_largest_line_number = 0;

    std::vector<SourceRow> m_lines;
};

class IsaCodeWidget : public BaseCodeWidget
{
public:
    explicit IsaCodeWidget(LineSelection& selection);
    void Render() override;

    void Load(const PcSamplingData& data, uint64_t code_object_uuid,
              const IsaDataBuilder::PreparedSamplingData& sampling);
    void UpdateSourceLocations(const PcSamplingData& data);
    void UpdateSampling(const IsaDataBuilder::PreparedSamplingData& sampling);

private:
    using IsaRow = IsaDataBuilder::IsaRow;

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
