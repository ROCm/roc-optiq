// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_code_widgets.h"

#include <algorithm>
#include <cstdio>

#include "rocprofvis_compute_code_table.h"
#include "widgets/rocprofvis_gui_helpers.h"

namespace RocProfVis
{
namespace View
{

constexpr const char* CODE_OBJECT_OFFSET_FORMAT        = "0x%llX";
constexpr size_t      CODE_OBJECT_OFFSET_TEXT_CAPACITY = 19;

constexpr ComputeCodeTable::HeaderTooltipText SOURCE_CODE_HEADER_TOOLTIP{
    "Source text for this line from the selected source file.",
    "DB field: compute_source_line.content\n"
    "Row key: compute_source_line.source_line_uuid\n"
    "Filter: source_file_uuid = selected source file"
};

constexpr ComputeCodeTable::HeaderTooltipText ISA_INSTRUCTION_HEADER_TOOLTIP{
    "GPU instruction and its operands.",
    "DB field: compute_instruction_line.instruction\n"
    "Row key: compute_instruction_line.instruction_uuid\n"
    "DB filter: compute_kernel_symbol.kernel_uuid = selected kernel\n"
    "View filter: selected code object UUID"
};

constexpr ComputeCodeTable::HeaderTooltipText CODE_OBJECT_OFFSET_HEADER_TOOLTIP{
    "Instruction's byte offset within the GPU code object, in hexadecimal.",
    "DB field: compute_instruction_line.code_object_offset\n"
    "Row key: compute_instruction_line.instruction_uuid\n"
    "Display: 0x followed by the uppercase hexadecimal offset\n"
    "Missing DB values are returned as 0 by the instruction-line query."
};

BaseCodeWidget::BaseCodeWidget(LineSelection& selection)
: m_line_selection(selection)
, m_settings(SettingsManager::GetInstance())
, m_sampling_renderer(m_settings)
{
    m_line_num_color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);

    m_table_flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable |
                    ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_ScrollY;
}

void
BaseCodeWidget::CalculateLineNumberWidth(size_t count)
{
    m_line_num_digits = 1;
    for(size_t number = count; number >= 10; number /= 10)
    {
        m_line_num_digits++;
    }

    m_line_num_width =
        ImGui::CalcTextSize("0").x * static_cast<float>(m_line_num_digits + 1);
}

bool
BaseCodeWidget::BeginCodeRow(uint64_t row_id, uint64_t source_line_id)
{
    const bool row_selected =
        source_line_id != 0 && source_line_id == m_line_selection.selected_line;
    const bool row_hovered =
        source_line_id != 0 && source_line_id == m_line_selection.hovered_line;

    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(static_cast<int>(row_id));
    const bool row_clicked = ImGui::Selectable("##row", row_selected,
                                               ImGuiSelectableFlags_SpanAllColumns |
                                                   ImGuiSelectableFlags_AllowOverlap,
                                               ImVec2(0.0f, ImGui::GetTextLineHeight()));
    const bool item_hovered =
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                             ImGuiHoveredFlags_AllowWhenOverlappedByItem);
    if(item_hovered)
    {
        m_line_selection.hovered_line       = source_line_id;
        m_line_selection.hovered_this_frame = true;
    }

    if(row_selected)
    {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               m_settings.GetColor(Colors::kSelection));
    }
    else if(item_hovered || row_hovered)
    {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               m_settings.GetColor(Colors::kHighlightChart));
    }

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PopID();

    return row_clicked;
}

void
BaseCodeWidget::RebuildRowIndex(size_t                                   row_count,
                                const std::function<uint64_t(uint32_t)>& source_line_of)
{
    m_row_index_by_source_line.clear();
    for(uint32_t i = 0; i < row_count; ++i)
    {
        const uint64_t source_line_id = source_line_of(i);
        if(source_line_id != LineSelection::UNSELECTED)
        {
            m_row_index_by_source_line.emplace(source_line_id, i);
        }
    }
}

std::optional<uint32_t>
BaseCodeWidget::TakeScrollTarget(uint64_t& scroll_line) const
{
    std::optional<uint32_t> scroll_target;
    if(scroll_line != LineSelection::UNSELECTED)
    {
        const auto target = m_row_index_by_source_line.find(scroll_line);
        if(target != m_row_index_by_source_line.end())
        {
            scroll_target = target->second;
        }
        scroll_line = LineSelection::UNSELECTED;
    }
    return scroll_target;
}

void
BaseCodeWidget::RenderClippedRows(size_t row_count, std::optional<uint32_t> scroll_target,
                                  const std::function<void(uint32_t)>& render_line)
{
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(row_count));
    if(scroll_target)
    {
        clipper.IncludeItemByIndex(static_cast<int>(*scroll_target));
    }

    while(clipper.Step())
    {
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            render_line(static_cast<uint32_t>(i));
            if(scroll_target && static_cast<uint32_t>(i) == *scroll_target)
            {
                ImGui::SetScrollHereY(0.0f);
            }
        }
    }
}

SourceCodeWidget::SourceCodeWidget(LineSelection& selection)
: BaseCodeWidget(selection)
{}

void
SourceCodeWidget::Load(const PcSamplingData& data, uint64_t source_file_uuid,
                       const IsaDataBuilder::PreparedSamplingData& sampling)
{
    m_source_file_uuid = source_file_uuid;
    m_lines            = IsaDataBuilder::BuildSourceLines(data, source_file_uuid);
    RebuildRowIndex(m_lines.size(), [this](uint32_t i) { return m_lines[i].id; });
    m_largest_line_number = 0;
    for(const SourceRow& row : m_lines)
    {
        m_largest_line_number = std::max(m_largest_line_number, row.line_number);
    }
    UpdateSampling(data, sampling);
}

void
SourceCodeWidget::UpdateSampling(const PcSamplingData&                       data,
                                 const IsaDataBuilder::PreparedSamplingData& sampling)
{
    m_hottest_samples =
        IsaDataBuilder::UpdateSourceSampling(m_lines, data, m_source_file_uuid, sampling);
}

void
SourceCodeWidget::Render()
{
    if(m_lines.empty())
    {
        ImGui::TextDisabled("No file loaded");
        return;
    }

    constexpr int columns_count = ColumnIndex(SourceColumn::kCount);
    CalculateLineNumberWidth(static_cast<size_t>(m_largest_line_number));
    ComputeCodeTable table("SourceCodeSamplingState", columns_count,
                           m_table_flags | ImGuiTableFlags_Reorderable,
                           m_settings.GetDefaultStyle().WindowPadding);
    if(!table.IsOpen())
    {
        return;
    }
    SetupColumns();
    RenderHeaders();
    table.BeginRows(m_settings.GetColor(Colors::kTransparent));

    std::optional<uint32_t> scroll_target;
    // The ISA pane can request another file before the view processes the switch.
    if(m_line_selection.source_scroll_file == LineSelection::UNSELECTED ||
       m_line_selection.source_scroll_file == m_source_file_uuid)
    {
        scroll_target = TakeScrollTarget(m_line_selection.source_scroll_line);
    }
    RenderClippedRows(m_lines.size(), scroll_target,
                      [this](uint32_t i) { RenderLine(i); });
}

void
SourceCodeWidget::SetupColumns()
{
    ImGui::TableSetupColumn("#",
                            ImGuiTableColumnFlags_NoResize |
                                ImGuiTableColumnFlags_NoHide |
                                ImGuiTableColumnFlags_WidthFixed,
                            m_line_num_width);

    m_sampling_renderer.SetupColumn();
    ImGui::TableSetupColumn("Source code", ImGuiTableColumnFlags_NoHide |
                                               ImGuiTableColumnFlags_WidthStretch);
}

void
SourceCodeWidget::RenderHeaders()
{
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    ComputeCodeTable::RenderCenteredHeader(ColumnIndex(SourceColumn::kLineNumber), "#");
    m_sampling_renderer.RenderHeader(ColumnIndex(SourceColumn::kSamplingState),
                                     SamplingStateRenderer::CodePane::kSource);
    ComputeCodeTable::RenderHeaderWithTooltip(ColumnIndex(SourceColumn::kSourceText),
                                              "Source code", SOURCE_CODE_HEADER_TOOLTIP);
}

void
SourceCodeWidget::RenderLine(uint32_t index)
{
    const SourceRow& source_row  = m_lines[index];
    const uint64_t   display_num = source_row.line_number;
    bool             row_clicked = BeginCodeRow(source_row.id, source_row.id);

    ImGui::TextColored(m_line_num_color, "%*llu", static_cast<int>(m_line_num_digits),
                       static_cast<unsigned long long>(display_num));

    ImGui::PushID(static_cast<int>(index));
    if(ImGui::TableSetColumnIndex(ColumnIndex(SourceColumn::kSamplingState)))
    {
        row_clicked |= m_sampling_renderer.RenderCell(source_row, m_hottest_samples);
    }
    if(ImGui::TableSetColumnIndex(ColumnIndex(SourceColumn::kSourceText)))
    {
        row_clicked |= CopyableTextUnformatted(source_row.content.c_str(), "source",
                                               COPY_DATA_NOTIFICATION, false, true);
    }
    ImGui::PopID();

    if(row_clicked)
    {
        m_line_selection.selected_line   = source_row.id;
        m_line_selection.isa_scroll_line = source_row.id;
    }
}

IsaCodeWidget::IsaCodeWidget(LineSelection& selection)
: BaseCodeWidget(selection)
{}

void
IsaCodeWidget::Load(const PcSamplingData& data, uint64_t code_object_uuid,
                    const IsaDataBuilder::PreparedSamplingData& sampling)
{
    m_entries = IsaDataBuilder::BuildInstructions(data, code_object_uuid);
    m_largest_code_object_offset = 0;
    for(const IsaRow& row : m_entries)
    {
        m_largest_code_object_offset =
            std::max(m_largest_code_object_offset, row.code_object_offset);
    }
    UpdateSourceLocations(data);
    UpdateSampling(sampling);
}

void
IsaCodeWidget::UpdateSourceLocations(const PcSamplingData& data)
{
    IsaDataBuilder::UpdateSourceLocations(m_entries, data);
    RebuildRowIndex(m_entries.size(),
                    [this](uint32_t i) { return m_entries[i].source_line_id; });
}

void
IsaCodeWidget::UpdateSampling(const IsaDataBuilder::PreparedSamplingData& sampling)
{
    m_hottest_samples = IsaDataBuilder::UpdateSampling(m_entries, sampling);
}

void
IsaCodeWidget::Render()
{
    if(m_entries.empty())
    {
        ImGui::TextDisabled("No ISA loaded");
        return;
    }

    constexpr int columns_count = ColumnIndex(IsaColumn::kCount);
    static_assert(columns_count == 4);
    CalculateLineNumberWidth(m_entries.size());

    // Keep the table ID stable to preserve users' column preferences.
    ComputeCodeTable table("IsaCodeSamplingState", columns_count,
                           m_table_flags | ImGuiTableFlags_Reorderable,
                           m_settings.GetDefaultStyle().WindowPadding);
    if(!table.IsOpen())
    {
        return;
    }

    SetupColumns();
    RenderHeaders();
    table.BeginRows(m_settings.GetColor(Colors::kTransparent));

    RenderClippedRows(m_entries.size(),
                      TakeScrollTarget(m_line_selection.isa_scroll_line),
                      [this](uint32_t i) { RenderLine(i); });
}

void
IsaCodeWidget::SetupColumns()
{
    // NoResize makes ImGui reapply the calculated widths after data or font changes.
    const ImGuiTableColumnFlags numeric_column_flags =
        ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed;
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_NoHide | numeric_column_flags,
                            m_line_num_width);

    m_sampling_renderer.SetupColumn();
    char largest_offset_text[CODE_OBJECT_OFFSET_TEXT_CAPACITY] = {};
    std::snprintf(largest_offset_text, sizeof(largest_offset_text),
                  CODE_OBJECT_OFFSET_FORMAT,
                  static_cast<unsigned long long>(m_largest_code_object_offset));
    const float offset_column_width = std::max(
        ImGui::CalcTextSize("Offset").x, ImGui::CalcTextSize(largest_offset_text).x);
    ImGui::TableSetupColumn("Offset", numeric_column_flags, offset_column_width);
    ImGui::TableSetupColumn("ISA", ImGuiTableColumnFlags_NoHide |
                                       ImGuiTableColumnFlags_WidthStretch);
}

void
IsaCodeWidget::RenderHeaders()
{
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    ComputeCodeTable::RenderCenteredHeader(ColumnIndex(IsaColumn::kLineNumber), "#");
    m_sampling_renderer.RenderHeader(ColumnIndex(IsaColumn::kSamplingState));
    ComputeCodeTable::RenderHeaderWithTooltip(ColumnIndex(IsaColumn::kOffset), "Offset",
                                              CODE_OBJECT_OFFSET_HEADER_TOOLTIP);
    ComputeCodeTable::RenderHeaderWithTooltip(ColumnIndex(IsaColumn::kInstruction), "ISA",
                                              ISA_INSTRUCTION_HEADER_TOOLTIP);
}

void
IsaCodeWidget::RenderLine(uint32_t index)
{
    const IsaRow& isa_row     = m_entries[index];
    bool          row_clicked = BeginCodeRow(isa_row.id, isa_row.source_line_id);

    ImGui::TextColored(m_line_num_color, "%*u", static_cast<int>(m_line_num_digits),
                       index + 1);

    ImGui::PushID(static_cast<int>(index));
    if(ImGui::TableSetColumnIndex(ColumnIndex(IsaColumn::kSamplingState)))
    {
        row_clicked |= m_sampling_renderer.RenderCell(isa_row, m_hottest_samples);
    }
    ImGui::PopID();

    if(ImGui::TableSetColumnIndex(ColumnIndex(IsaColumn::kOffset)))
    {
        char offset_text[CODE_OBJECT_OFFSET_TEXT_CAPACITY] = {};
        std::snprintf(offset_text, sizeof(offset_text), CODE_OBJECT_OFFSET_FORMAT,
                      static_cast<unsigned long long>(isa_row.code_object_offset));
        ImGui::PushID(static_cast<int>(index));
        row_clicked |= CopyableTextUnformatted(offset_text, "offset",
                                               COPY_DATA_NOTIFICATION, false, true);
        ImGui::PopID();
    }

    if(ImGui::TableSetColumnIndex(ColumnIndex(IsaColumn::kInstruction)))
    {
        ImGui::PushID(static_cast<int>(index));
        row_clicked |= CopyableTextUnformatted(isa_row.instruction.c_str(), "instruction",
                                               COPY_DATA_NOTIFICATION, false, true);
        ImGui::PopID();
    }

    if(row_clicked && isa_row.source_line_id != LineSelection::UNSELECTED)
    {
        m_line_selection.selected_line      = isa_row.source_line_id;
        m_line_selection.source_scroll_line = isa_row.source_line_id;
        m_line_selection.source_scroll_file = isa_row.source_file_id;
    }
}

}  // namespace View
}  // namespace RocProfVis
