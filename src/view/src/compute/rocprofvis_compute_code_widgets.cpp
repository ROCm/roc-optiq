// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_code_widgets.h"

#include <algorithm>
#include <cstdio>

#include "widgets/rocprofvis_gui_helpers.h"

namespace RocProfVis
{
namespace View
{

constexpr uint64_t INVALID_SOURCE_LINE_NUMBER = 0;
constexpr uint32_t NO_SCROLL_TARGET           = 0;

constexpr uint64_t    LOW_CONFIDENCE_SAMPLE_COUNT               = 10;
constexpr float       SAMPLING_STATE_BAR_HEIGHT_FRACTION        = 0.5f;
constexpr float       SAMPLING_STATE_COLUMN_WIDTH_IN_CHARACTERS = 14.0f;
constexpr const char* SAMPLING_STATE_COLUMN_LABEL               = "Sampling State";
constexpr const char* STALL_REASONS_COLUMN_LABEL                = "Stall Categories";
constexpr float       STALL_REASON_TOOLTIP_DESCRIPTION_WIDTH_IN_CHARACTERS = 42.0f;
constexpr float       STALL_REASON_TOOLTIP_MAX_HEIGHT_FRACTION             = 0.5f;
constexpr float       STALL_REASON_TOOLTIP_SCROLL_LINES                    = 3.0f;

struct StallReasonPresentation
{
    IsaDataBuilder::StallReasonKind kind;
    const char*                     description;
    Colors                          color;
};

// Meanings follow ROCprofiler-SDK's CDNA3/CDNA4 PC-sampling stall reasons.
constexpr StallReasonPresentation STALL_REASON_PRESENTATIONS[] = {
    { IsaDataBuilder::StallReasonKind::kInstructionFetch,
      "Instruction fetch is not ready, for example after a branch or instruction-cache "
      "miss.",
      Colors::kPcSamplingInstructionFetch },
    { IsaDataBuilder::StallReasonKind::kAluDependency,
      "A hardware or data dependency prevents issue, including dependencies between "
      "pipelines.",
      Colors::kPcSamplingAluDependency },
    { IsaDataBuilder::StallReasonKind::kWaitcnt,
      "The wave is waiting for memory operations required by a wait-count instruction.",
      Colors::kPcSamplingWaitcnt },
    { IsaDataBuilder::StallReasonKind::kInternalInstruction,
      "The wave is processing an internal instruction, such as a NOP.",
      Colors::kPcSamplingInternalInstruction },
    { IsaDataBuilder::StallReasonKind::kBarrier,
      "The wave is waiting for other waves in its workgroup to reach a synchronization "
      "barrier.",
      Colors::kPcSamplingBarrier },
    { IsaDataBuilder::StallReasonKind::kArbiter,
      "The scheduler did not select this wave to issue to the requested execution "
      "pipeline.",
      Colors::kPcSamplingArbiter },
    { IsaDataBuilder::StallReasonKind::kPipelineStall,
      "The scheduler selected this wave, but the execution pipeline could not accept its "
      "instruction.",
      Colors::kPcSamplingPipelineStall },
    { IsaDataBuilder::StallReasonKind::kOtherWait,
      "Another wait condition was recorded; the sample does not identify a more specific "
      "cause.",
      Colors::kPcSamplingOtherWait },
    { IsaDataBuilder::StallReasonKind::kSleeping,
      "The sample reports that the wave is sleeping.", Colors::kPcSamplingSleeping }
};
constexpr StallReasonPresentation UNKNOWN_STALL_REASON_PRESENTATION{
    IsaDataBuilder::StallReasonKind::kUnknown,
    "This version does not recognize the recorded reason. Its database name is preserved "
    "above.",
    Colors::kTextDim
};

constexpr ImVec4 HEATMAP_LOW_COLOR{ 0.24f, 0.70f, 0.28f, 0.5f };
constexpr ImVec4 HEATMAP_MID_COLOR{ 0.90f, 0.78f, 0.18f, 0.5f };
constexpr ImVec4 HEATMAP_HIGH_COLOR{ 0.82f, 0.24f, 0.24f, 0.5f };

struct HeaderTooltipText
{
    const char* user_description;
    const char* developer_information;
};

constexpr const char* DEVELOPER_INFORMATION_LABEL      = "Developer information";
constexpr const char* CODE_OBJECT_OFFSET_FORMAT        = "0x%llX";
constexpr size_t      CODE_OBJECT_OFFSET_TEXT_CAPACITY = 19;

constexpr HeaderTooltipText SOURCE_CODE_HEADER_TOOLTIP{
    "Source text for this line from the selected source file.",
    "DB field: compute_source_line.content\n"
    "Row key: compute_source_line.source_line_uuid\n"
    "Filter: source_file_uuid = selected source file"
};

constexpr HeaderTooltipText SAMPLES_HEADER_TOOLTIP{
    "How often PC sampling observed the GPU at this instruction.\n"
    "Larger values identify hotter instructions worth investigating.\n"
    "The bar compares each row with the hottest displayed instruction.\n"
    "Samples are observations, not elapsed time.",
    "DB field: compute_pc_sample_state.total_count\n"
    "Group key: compute_pc_sample_state.instruction_uuid\n"
    "Value: SUM(total_count) per instruction_uuid\n"
    "Bar: instruction samples / MAX(displayed instruction samples)\n"
    "Kernel share: instruction samples / SUM(kernel total_count)"
};

constexpr HeaderTooltipText ISA_INSTRUCTION_HEADER_TOOLTIP{
    "GPU machine instruction for this row.\n"
    "It shows the operation and operands from the kernel disassembly.",
    "DB field: compute_instruction_line.instruction\n"
    "Row key: compute_instruction_line.instruction_uuid\n"
    "DB filter: compute_kernel_symbol.kernel_uuid = selected kernel\n"
    "View filter: selected code object UUID"
};

constexpr HeaderTooltipText SAMPLING_STATE_HEADER_TOOLTIP{
    "PC-sampling observations split into stalled (red) and issued (green) samples.\n"
    "Bar length compares the sample count with the hottest displayed instruction.\n"
    "Gray indicates unavailable or unclassified progress data.\n"
    "Hover for exact counts and percentages. Samples are observations, not elapsed time.",
    "DB fields: compute_pc_sample_state.total_count, stall_count, issue_count\n"
    "Group key: compute_pc_sample_state.instruction_uuid\n"
    "Bar length: SUM(total_count) / MAX(displayed instruction samples)\n"
    "Segment widths: SUM(stall_count) or SUM(issue_count) / MAX(displayed instruction "
    "samples)\n"
    "Breakdown requires non-NULL counts for every sample state of the instruction.\n"
    "Counts exceeding total_count disable the breakdown; a remainder is unclassified."
};

constexpr HeaderTooltipText CODE_OBJECT_OFFSET_HEADER_TOOLTIP{
    "Byte offset of this instruction inside the selected GPU code object.\n"
    "Use it to match sampled instructions with disassembly or other PC data.\n"
    "The value is hexadecimal and is not an absolute runtime address.",
    "DB field: compute_instruction_line.code_object_offset\n"
    "Row key: compute_instruction_line.instruction_uuid\n"
    "Display: 0x followed by the uppercase hexadecimal offset\n"
    "Missing DB values are returned as 0 by the instruction-line query."
};

constexpr HeaderTooltipText STALL_PERCENT_HEADER_TOOLTIP{
    "How often this instruction was unable to issue and was waiting when sampled.\n"
    "Higher values identify where to investigate, but not the cause of the wait.\n"
    "Hover a value to see every recorded stall reason, ordered by sample count.\n"
    "Instructions with missing progress data display N/A.\n"
    "Inconsistent issued/stalled counts also display N/A.\n"
    "Host-trap PC sampling has no progress data, so the column displays disabled N/A "
    "values.",
    "DB fields: compute_pc_sample_state.issue_count, stall_count, total_count\n"
    "Group key: compute_pc_sample_state.instruction_uuid\n"
    "Value: 100 * SUM(stall_count) / SUM(total_count)\n"
    "Availability: issue_count and stall_count must be non-NULL for every sample state "
    "of this instruction.\n"
    "If SUM(total_count) is zero, the displayed value is 0%.\n"
    "Reason fields: compute_pc_sample_stall_reason.pc_sample_state_uuid, "
    "pc_sample_stall_reason_lookup_uuid, count\n"
    "Reason text: compute_pc_sample_stall_reason_lookup.text\n"
    "Issued-sample normalization: subtract issue_count from OTHER_WAIT when raw "
    "reason totals include total_count\n"
    "Reason count: SUM(normalized count) by instruction_uuid and reason lookup UUID\n"
    "Reason share: 100 * normalized reason count / SUM(stall_count)\n"
    "Order: reason count descending, then reason text ascending."
};

constexpr HeaderTooltipText STALL_REASONS_HEADER_TOOLTIP{
    "Distribution of the reasons this instruction was stalled when sampled.\n"
    "Full width represents 100% of this instruction's samples; bar length matches "
    "Stall %.\n"
    "Each color identifies the same reason across instructions and kernels.\n"
    "Hover for a color legend, reason explanations, counts, and shares of stalled "
    "samples.\n"
    "Gray represents stalled samples without a recorded reason. Host-trap data displays "
    "N/A. Inconsistent issued/stalled counts also display N/A.",
    "DB fields: compute_pc_sample_stall_reason.count and pc_sample_state_uuid\n"
    "Reason names: compute_pc_sample_stall_reason_lookup.text\n"
    "Grouping and issued-sample normalization match the Stall % tooltip.\n"
    "Bar length: SUM(stall_count) / SUM(total_count), clamped to [0, 1].\n"
    "Segment width: bar length * normalized reason count / SUM(stall_count).\n"
    "If reason counts exceed stall_count, segment shares use the recorded reason total; "
    "tooltip shares still use stall_count and report the mismatch."
};

constexpr const char* LOW_CONFIDENCE_SAMPLES_CELL_TOOLTIP_FORMAT =
    "%s samples\n%.1f%% of kernel samples\n"
    "%.1f%% relative to the hottest instruction\n\n"
    "Low-confidence estimate: percentages based on %llu or fewer samples may be "
    "unstable.";
constexpr const char* SAMPLES_CELL_TOOLTIP_FORMAT =
    "%s samples\n%.1f%% of kernel samples\n"
    "%.1f%% relative to the hottest instruction";
constexpr const char* SAMPLING_STATE_UNAVAILABLE_TOOLTIP =
    "Issue/stall breakdown is unavailable because progress data is missing for this "
    "instruction.\nThis is expected for host-trap PC sampling.";
constexpr const char* SAMPLING_STATE_INVALID_COUNTS_TOOLTIP =
    "Issue/stall breakdown is unavailable because the recorded counts exceed the total "
    "sample count.";

constexpr const char* STALL_REASON_TOOLTIP_TITLE = "Stall-reason distribution";
constexpr const char* STALL_REASON_TOOLTIP_SUMMARY =
    "%s of %s samples were stalled (%.1f%%).";
constexpr const char* STALL_REASON_TOOLTIP_ISSUED = "%s samples issued the instruction.";
constexpr const char* STALL_REASON_TOOLTIP_NO_STALLS =
    "No stalled samples were recorded for this instruction.";
constexpr const char* STALL_REASON_TOOLTIP_UNAVAILABLE =
    "No stall-reason details were recorded for this instruction.";
constexpr const char* STALL_DATA_UNAVAILABLE_CELL_TEXT = "N/A";
constexpr const char* STALL_DATA_UNAVAILABLE_CELL_TOOLTIP =
    "Stall percentage is unavailable because progress data is missing for this "
    "instruction.\n"
    "This is expected for host-trap PC sampling.";
constexpr const char* STALL_REASON_TOOLTIP_SHARE_DESCRIPTION =
    "Shares use all %s stalled samples as the denominator.";
constexpr const char* STALL_REASON_TOOLTIP_COUNT_MISMATCH =
    "Reason data classifies %s samples; expected %s stalled samples.";
constexpr const char* STALL_REASON_BAR_NORMALIZED_TOOLTIP =
    "Colors are scaled to fit the stalled portion because the recorded reason total "
    "exceeds the stalled count.";
constexpr const char* STALL_REASON_DATA_UNAVAILABLE_TOOLTIP =
    "Stall reasons are unavailable because progress data is missing for this "
    "instruction.\n"
    "This is expected for host-trap PC sampling.";
constexpr const char* STALL_REASON_UNCLASSIFIED_TEXT = "Unclassified";
constexpr const char* STALL_REASON_UNCLASSIFIED_DESCRIPTION =
    "These stalled samples have no recorded stall-reason classification.";
constexpr const char*     STALL_REASON_TOOLTIP_COLUMN_HEADERS[] = { "Color",
                                                                    "Reason / Meaning",
                                                                    "Samples", "Share" };
constexpr ImGuiTableFlags STALL_REASON_TOOLTIP_TABLE_FLAGS =
    ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
    ImGuiTableFlags_BordersInnerV;

struct CodeCellBounds
{
    ImVec2 start;
    ImVec2 size;

    ImVec2 End() const { return ImVec2(start.x + size.x, start.y + size.y); }
};

struct CodeCellInteraction
{
    bool clicked;
    bool hovered;
};

static CodeCellBounds
get_code_cell_bounds()
{
    return { ImGui::GetCursorScreenPos(),
             ImVec2(std::max(0.0f, ImGui::GetContentRegionAvail().x),
                    ImGui::GetTextLineHeight()) };
}

static CodeCellInteraction
finish_code_cell(const CodeCellBounds& cell, const char* interaction_id = nullptr)
{
    ImGui::SetCursorScreenPos(cell.start);
    bool clicked = false;
    if(interaction_id)
    {
        clicked = ImGui::InvisibleButton(interaction_id, cell.size);
    }
    else
    {
        ImGui::Dummy(cell.size);
    }
    return { clicked, ImGui::IsItemHovered() };
}

static void
draw_code_cell_fill(const CodeCellBounds& cell, double percent, ImU32 color)
{
    const float fraction = std::clamp(static_cast<float>(percent) / 100.0f, 0.0f, 1.0f);
    if(fraction > 0.0f)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(cell.start, cell.End(), true);
        draw_list->AddRectFilled(
            cell.start, ImVec2(cell.start.x + cell.size.x * fraction, cell.End().y),
            color);
        draw_list->PopClipRect();
    }
}

static const StallReasonPresentation&
get_stall_reason_presentation(IsaDataBuilder::StallReasonKind kind)
{
    for(const StallReasonPresentation& presentation : STALL_REASON_PRESENTATIONS)
    {
        if(kind == presentation.kind)
        {
            return presentation;
        }
    }
    return UNKNOWN_STALL_REASON_PRESENTATION;
}

static void
render_stall_reason_legend_row(const char* text, const char* description, ImU32 color,
                               const char* count_text, double share)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::PushID(text);
    const float swatch_size = ImGui::GetTextLineHeight();
    ImGui::ColorButton("##reason_color", ImGui::ColorConvertU32ToFloat4(color),
                       ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs |
                           ImGuiColorEditFlags_NoDragDrop,
                       ImVec2(swatch_size, swatch_size));
    ImGui::PopID();
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped("%s", text);
    ImGui::TextWrapped("%s", description);
    ImGui::TableSetColumnIndex(2);
    ImGui::TextUnformatted(count_text);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%.1f%%", share);
}

static void
render_centered_table_header_label(int column, const char* label)
{
    ImGui::TableSetColumnIndex(column);
    CenterNextTextItem(label);
    ImGui::TableHeader(label);
}

static void
render_table_header_with_tooltip(int column, const char* label,
                                 const HeaderTooltipText& tooltip, bool disabled = false)
{
    if(!ImGui::TableSetColumnIndex(column))
    {
        return;
    }
    if(disabled)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    }
    ImGui::TableHeader(label);
    if(disabled)
    {
        ImGui::PopStyleColor();
    }
    if(!ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        return;
    }

    BeginTooltipStyled();
    ImGui::TextUnformatted(tooltip.user_description);
#ifdef ROCPROFVIS_DEVELOPER_MODE
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled(DEVELOPER_INFORMATION_LABEL);
    ImGui::TextUnformatted(tooltip.developer_information);
#endif
    EndTooltipStyled();
}

BaseCodeWidget::BaseCodeWidget(LineSelection& selection)
: m_line_selection(selection)
, m_settings(SettingsManager::GetInstance())
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

void
BaseCodeWidget::PushStyles()
{
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                        ImVec2(ImGui::GetStyle().CellPadding.x, 0.0f));

    ImGui::PushStyleColor(ImGuiCol_Header, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                          m_settings.GetColor(Colors::kTransparent));
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

SourceCodeWidget::SourceCodeWidget(LineSelection& selection)
: BaseCodeWidget(selection)
{}

void
SourceCodeWidget::Load(const PcSamplingData& data, uint64_t source_file_uuid)
{
    m_lines.clear();

    const SourceFile* source_file = nullptr;
    for(const auto& file : data.source_files)
    {
        if(file.source_file_uuid == source_file_uuid)
        {
            source_file = &file;
            break;
        }
    }
    if(!source_file)
    {
        return;
    }

    uint64_t max_line_number = 0;
    for(const auto& source_line : source_file->source_lines)
    {
        if(source_line.line_number == INVALID_SOURCE_LINE_NUMBER)
        {
            continue;
        }

        m_lines.push_back({ source_line.content, source_line.source_line_uuid,
                            source_line.line_number });
        max_line_number = std::max(max_line_number, source_line.line_number);
    }

    CalculateLineNumberWidth(static_cast<size_t>(max_line_number));
}

void
SourceCodeWidget::Render()
{
    if(m_lines.empty())
    {
        ImGui::TextDisabled("No file loaded");
        return;
    }

    const int columns_count = 2;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        m_settings.GetDefaultStyle().WindowPadding);
    if(!ImGui::BeginTable("SourceCode", columns_count, m_table_flags))
    {
        ImGui::PopStyleVar();
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);

    ImGui::TableSetupColumn("#",
                            ImGuiTableColumnFlags_NoResize |
                                ImGuiTableColumnFlags_NoHide |
                                ImGuiTableColumnFlags_WidthFixed,
                            m_line_num_width);

    ImGui::TableSetupColumn("Source code", ImGuiTableColumnFlags_NoHide |
                                               ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    render_centered_table_header_label(0, "#");
    render_table_header_with_tooltip(1, "Source code", SOURCE_CODE_HEADER_TOOLTIP);
    PushStyles();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_lines.size()));
    const uint32_t scroll_target = GetScrollTarget(clipper);

    while(clipper.Step())
    {
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            RenderLine(static_cast<uint32_t>(i));
            if(scroll_target != NO_SCROLL_TARGET &&
               static_cast<uint32_t>(i) + 1 == scroll_target)
            {
                ImGui::SetScrollHereY(0.0f);
            }
        }
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndTable();
    ImGui::PopStyleVar();
}

uint32_t
SourceCodeWidget::GetScrollTarget(ImGuiListClipper& clipper)
{
    uint32_t scroll_target = NO_SCROLL_TARGET;
    if(m_line_selection.source_scroll_line != LineSelection::UNSELECTED)
    {
        for(uint32_t i = 0; i < m_lines.size(); ++i)
        {
            if(m_lines[i].id == m_line_selection.source_scroll_line)
            {
                scroll_target                       = i + 1;
                m_line_selection.source_scroll_line = LineSelection::UNSELECTED;
                clipper.IncludeItemByIndex(static_cast<int>(i));
                break;
            }
        }
    }
    return scroll_target;
}

void
SourceCodeWidget::RenderLine(uint32_t index)
{
    const SourceRow& source_row  = m_lines[index];
    const uint64_t   display_num = source_row.line_number;
    bool             row_clicked = BeginCodeRow(source_row.id, source_row.id);

    ImGui::TextColored(m_line_num_color, "%*llu", static_cast<int>(m_line_num_digits),
                       static_cast<unsigned long long>(display_num));

    ImGui::TableSetColumnIndex(1);
    ImGui::PushID(static_cast<int>(index));
    row_clicked |= CopyableTextUnformatted(source_row.content.c_str(), "source",
                                           COPY_DATA_NOTIFICATION, false, true);
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
IsaCodeWidget::Load(const PcSamplingData& data, uint64_t code_object_uuid)
{
    m_entries = IsaDataBuilder::BuildInstructions(data, code_object_uuid);
    m_largest_code_object_offset = 0;
    for(const IsaRow& row : m_entries)
    {
        m_largest_code_object_offset =
            std::max(m_largest_code_object_offset, row.code_object_offset);
    }
    UpdateSourceLocations(data);
    UpdateSampling(data);
}

void
IsaCodeWidget::UpdateSourceLocations(const PcSamplingData& data)
{
    IsaDataBuilder::UpdateSourceLocations(m_entries, data);
}

void
IsaCodeWidget::UpdateSampling(const PcSamplingData& data)
{
    const IsaDataBuilder::SamplingTotals totals =
        IsaDataBuilder::UpdateSampling(m_entries, data);
    m_kernel_total_samples        = totals.kernel_total_samples;
    m_hottest_instruction_samples = totals.hottest_instruction_samples;
    m_stall_data_available        = totals.stall_data_available;
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
    static_assert(columns_count == 7);
    CalculateLineNumberWidth(m_entries.size());

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        m_settings.GetDefaultStyle().WindowPadding);
    // Use a new table ID so the previous column layout does not override these defaults.
    if(!ImGui::BeginTable("IsaCodeStallCategories", columns_count,
                          m_table_flags | ImGuiTableFlags_Reorderable))
    {
        ImGui::PopStyleVar();
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);

    SetupColumns();
    RenderHeaders();
    PushStyles();

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_entries.size()));
    const uint32_t scroll_target = GetScrollTarget(clipper);
    while(clipper.Step())
    {
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            RenderLine(static_cast<uint32_t>(i));
            if(scroll_target != NO_SCROLL_TARGET &&
               static_cast<uint32_t>(i) + 1 == scroll_target)
            {
                ImGui::SetScrollHereY(0.0f);
            }
        }
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndTable();
    ImGui::PopStyleVar();
}

void
IsaCodeWidget::SetupColumns()
{
    // Disabled hides sampling columns without discarding their saved layout choices.
    const ImGuiTableColumnFlags sampling_visibility_flags =
        m_show_sampling ? ImGuiTableColumnFlags_None : ImGuiTableColumnFlags_Disabled;
    // NoResize makes ImGui reapply the calculated widths after data or font changes.
    const ImGuiTableColumnFlags numeric_column_flags =
        ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed;
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_NoHide | numeric_column_flags,
                            m_line_num_width);

    const float sampling_state_column_width =
        std::max(ImGui::CalcTextSize(SAMPLING_STATE_COLUMN_LABEL).x,
                 ImGui::CalcTextSize("0").x * SAMPLING_STATE_COLUMN_WIDTH_IN_CHARACTERS);
    ImGui::TableSetupColumn(SAMPLING_STATE_COLUMN_LABEL,
                            sampling_visibility_flags | ImGuiTableColumnFlags_WidthFixed |
                                ImGuiTableColumnFlags_DefaultHide,
                            sampling_state_column_width);
    std::string widest_sample_count_text =
        IsaDataBuilder::FormatSampleCount(m_hottest_instruction_samples);
    if(m_hottest_instruction_samples > 0 &&
       m_hottest_instruction_samples <= LOW_CONFIDENCE_SAMPLE_COUNT)
    {
        widest_sample_count_text += "*";
    }

    const float samples_header_width = ImGui::CalcTextSize("Samples").x;
    const float sample_count_width =
        ImGui::CalcTextSize(widest_sample_count_text.c_str()).x;
    const float samples_column_width = std::max(samples_header_width, sample_count_width);
    ImGui::TableSetupColumn("Samples",
                            numeric_column_flags | sampling_visibility_flags |
                                ImGuiTableColumnFlags_DefaultHide,
                            samples_column_width);
    const float stall_column_width = ImGui::CalcTextSize("Stall %").x;
    ImGui::TableSetupColumn("Stall %",
                            sampling_visibility_flags | ImGuiTableColumnFlags_WidthFixed |
                                ImGuiTableColumnFlags_DefaultHide,
                            stall_column_width);
    const float reasons_column_width =
        std::max(ImGui::CalcTextSize(STALL_REASONS_COLUMN_LABEL).x,
                 ImGui::CalcTextSize("0").x * SAMPLING_STATE_COLUMN_WIDTH_IN_CHARACTERS);
    ImGui::TableSetupColumn(STALL_REASONS_COLUMN_LABEL,
                            sampling_visibility_flags | ImGuiTableColumnFlags_WidthFixed,
                            reasons_column_width);

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
    render_centered_table_header_label(ColumnIndex(IsaColumn::kLineNumber), "#");
    render_table_header_with_tooltip(ColumnIndex(IsaColumn::kSamplingState),
                                     SAMPLING_STATE_COLUMN_LABEL,
                                     SAMPLING_STATE_HEADER_TOOLTIP);
    render_table_header_with_tooltip(ColumnIndex(IsaColumn::kSamples), "Samples",
                                     SAMPLES_HEADER_TOOLTIP);
    render_table_header_with_tooltip(ColumnIndex(IsaColumn::kStallPercent), "Stall %",
                                     STALL_PERCENT_HEADER_TOOLTIP,
                                     !m_stall_data_available);
    render_table_header_with_tooltip(
        ColumnIndex(IsaColumn::kStallCategories), STALL_REASONS_COLUMN_LABEL,
        STALL_REASONS_HEADER_TOOLTIP, !m_stall_data_available);
    render_table_header_with_tooltip(ColumnIndex(IsaColumn::kOffset), "Offset",
                                     CODE_OBJECT_OFFSET_HEADER_TOOLTIP);
    render_table_header_with_tooltip(ColumnIndex(IsaColumn::kInstruction), "ISA",
                                     ISA_INSTRUCTION_HEADER_TOOLTIP);
}

uint32_t
IsaCodeWidget::GetScrollTarget(ImGuiListClipper& clipper)
{
    uint32_t scroll_target = NO_SCROLL_TARGET;
    if(m_line_selection.isa_scroll_line != LineSelection::UNSELECTED)
    {
        for(uint32_t i = 0; i < m_entries.size(); ++i)
        {
            if(m_entries[i].source_line_id == m_line_selection.isa_scroll_line)
            {
                scroll_target                    = i + 1;
                m_line_selection.isa_scroll_line = LineSelection::UNSELECTED;
                clipper.IncludeItemByIndex(static_cast<int>(i));
                break;
            }
        }
    }
    return scroll_target;
}

ImU32
IsaCodeWidget::HeatmapColor(double percent)
{
    const float fraction = std::clamp(static_cast<float>(percent) / 100.0f, 0.0f, 1.0f);

    const auto interpolate_color = [](const ImVec4& from, const ImVec4& to,
                                      float amount) {
        return ImVec4(
            from.x + (to.x - from.x) * amount, from.y + (to.y - from.y) * amount,
            from.z + (to.z - from.z) * amount, from.w + (to.w - from.w) * amount);
    };

    constexpr float COLOR_MIDPOINT = 0.5f;
    const ImVec4    color =
        fraction < COLOR_MIDPOINT
               ? interpolate_color(HEATMAP_LOW_COLOR, HEATMAP_MID_COLOR,
                                   fraction / COLOR_MIDPOINT)
               : interpolate_color(HEATMAP_MID_COLOR, HEATMAP_HIGH_COLOR,
                                   (fraction - COLOR_MIDPOINT) / COLOR_MIDPOINT);
    return ImGui::ColorConvertFloat4ToU32(color);
}

void
IsaCodeWidget::RenderSamplesCell(uint64_t sample_count)
{
    const CodeCellBounds cell = get_code_cell_bounds();
    const double         relative_hotness =
        IsaDataBuilder::CalculatePercentage(sample_count, m_hottest_instruction_samples);
    draw_code_cell_fill(cell, relative_hotness, HeatmapColor(relative_hotness));

    const std::string count_text = IsaDataBuilder::FormatSampleCount(sample_count);
    const bool        is_low_confidence =
        sample_count > 0 && sample_count <= LOW_CONFIDENCE_SAMPLE_COUNT;
    const std::string display_text = is_low_confidence ? count_text + "*" : count_text;
    const float       text_width   = ImGui::CalcTextSize(display_text.c_str()).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         std::max(0.0f, cell.size.x - text_width));
    ImGui::TextUnformatted(display_text.c_str());
    if(finish_code_cell(cell).hovered)
    {
        BeginTooltipStyled();
        RenderSampleSummary(sample_count);
        EndTooltipStyled();
    }
}

void
IsaCodeWidget::RenderSampleSummary(uint64_t sample_count)
{
    const std::string count_text = IsaDataBuilder::FormatSampleCount(sample_count);
    const double      kernel_sample_share =
        IsaDataBuilder::CalculatePercentage(sample_count, m_kernel_total_samples);
    const double relative_hotness =
        IsaDataBuilder::CalculatePercentage(sample_count, m_hottest_instruction_samples);
    if(sample_count > 0 && sample_count <= LOW_CONFIDENCE_SAMPLE_COUNT)
    {
        ImGui::Text(LOW_CONFIDENCE_SAMPLES_CELL_TOOLTIP_FORMAT, count_text.c_str(),
                    kernel_sample_share, relative_hotness,
                    static_cast<unsigned long long>(LOW_CONFIDENCE_SAMPLE_COUNT));
    }
    else
    {
        ImGui::Text(SAMPLES_CELL_TOOLTIP_FORMAT, count_text.c_str(), kernel_sample_share,
                    relative_hotness);
    }
}

void
IsaCodeWidget::RenderSamplingStateCell(const IsaRow& row)
{
    const SampleCounts&  counts     = row.sample_counts;
    const CodeCellBounds cell       = get_code_cell_bounds();
    const float          bar_height = cell.size.y * SAMPLING_STATE_BAR_HEIGHT_FRACTION;
    const ImVec2         bar_start(cell.start.x,
                                   cell.start.y + (cell.size.y - bar_height) * 0.5f);
    const float          bar_width =
        cell.size.x *
        static_cast<float>(IsaDataBuilder::CalculatePercentage(
                               counts.total_count, m_hottest_instruction_samples) /
                           100.0);
    const ImVec2 bar_end(bar_start.x + bar_width, bar_start.y + bar_height);

    if(bar_width > 0.0f)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(
            cell.start, ImVec2(cell.start.x + cell.size.x, cell.start.y + cell.size.y),
            true);
        draw_list->AddRectFilled(bar_start, bar_end,
                                 m_settings.GetColor(Colors::kTextDim));
        if(row.sampling.availability == SamplingAvailability::kValid)
        {
            const float stall_width =
                bar_width * static_cast<float>(row.sampling.stall_percent / 100.0);
            const float issue_width =
                bar_width * static_cast<float>(row.sampling.issue_percent / 100.0);
            const ImVec2 stall_end(bar_start.x + stall_width, bar_end.y);
            const ImVec2 issue_start(stall_end.x, bar_start.y);
            const ImVec2 issue_end(issue_start.x + issue_width, bar_end.y);
            if(stall_width > 0.0f)
            {
                draw_list->AddRectFilled(bar_start, stall_end,
                                         m_settings.GetColor(Colors::kTextError));
            }
            if(issue_width > 0.0f)
            {
                draw_list->AddRectFilled(issue_start, issue_end,
                                         m_settings.GetColor(Colors::kTextSuccess));
            }
        }
        draw_list->AddRect(bar_start, bar_end, m_settings.GetColor(Colors::kBorderGray));
        draw_list->PopClipRect();
    }

    if(finish_code_cell(cell).hovered)
    {
        RenderSamplingStateTooltip(row);
    }
}

void
IsaCodeWidget::RenderSamplingStateTooltip(const IsaRow& row)
{
    const SampleCounts& counts = row.sample_counts;
    BeginTooltipStyled();
    RenderSampleSummary(counts.total_count);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if(row.sampling.availability == SamplingAvailability::kInvalid)
    {
        ImGui::TextUnformatted(SAMPLING_STATE_INVALID_COUNTS_TOOLTIP);
    }
    else if(counts.total_count == 0)
    {
        ImGui::TextDisabled("No PC samples were recorded for this instruction.");
    }
    else if(row.sampling.availability == SamplingAvailability::kMissing)
    {
        ImGui::TextUnformatted(SAMPLING_STATE_UNAVAILABLE_TOOLTIP);
    }
    else
    {
        const std::string stalled =
            IsaDataBuilder::FormatSampleCount(*counts.stall_count);
        const std::string issued = IsaDataBuilder::FormatSampleCount(*counts.issue_count);
        ImGui::TextColored(
            ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kTextError)),
            "Stalled: %s samples (%.1f%%)", stalled.c_str(), row.sampling.stall_percent);
        ImGui::TextColored(
            ImGui::ColorConvertU32ToFloat4(m_settings.GetColor(Colors::kTextSuccess)),
            "Issued: %s samples (%.1f%%)", issued.c_str(), row.sampling.issue_percent);
        const uint64_t unclassified_count = row.sampling.unclassified_count;
        if(unclassified_count > 0)
        {
            const std::string unclassified =
                IsaDataBuilder::FormatSampleCount(unclassified_count);
            ImGui::TextDisabled("Unclassified: %s samples (%.1f%%)", unclassified.c_str(),
                                IsaDataBuilder::CalculatePercentage(unclassified_count,
                                                                    counts.total_count));
        }
    }
    EndTooltipStyled();
}

void
IsaCodeWidget::RenderStallPercentCell(const IsaRow& row, bool& row_clicked)
{
    if(row.sampling.availability != SamplingAvailability::kValid)
    {
        RenderUnavailableStallCell(row.sampling.availability ==
                                           SamplingAvailability::kInvalid
                                       ? SAMPLING_STATE_INVALID_COUNTS_TOOLTIP
                                       : STALL_DATA_UNAVAILABLE_CELL_TOOLTIP);
        return;
    }
    const CodeCellBounds cell = get_code_cell_bounds();
    draw_code_cell_fill(cell, row.sampling.stall_percent,
                        HeatmapColor(row.sampling.stall_percent));
    ImGui::TextDisabled("%.1f%%", row.sampling.stall_percent);
    const CodeCellInteraction interaction =
        finish_code_cell(cell, "##stall_percent_cell");
    row_clicked |= interaction.clicked;
    if(interaction.hovered)
    {
        RenderStallReasonsTooltip(row);
    }
}

void
IsaCodeWidget::RenderUnavailableStallCell(const char* tooltip)
{
    const CodeCellBounds cell = get_code_cell_bounds();

    ImGui::BeginDisabled();
    ImGui::TextUnformatted(STALL_DATA_UNAVAILABLE_CELL_TEXT);
    ImGui::EndDisabled();
    if(finish_code_cell(cell).hovered)
    {
        SetTooltipStyled("%s", tooltip);
    }
}

void
IsaCodeWidget::RenderStallReasonBarCell(const IsaRow& row, bool& row_clicked)
{
    if(row.sampling.availability == SamplingAvailability::kMissing)
    {
        RenderUnavailableStallCell(STALL_REASON_DATA_UNAVAILABLE_TOOLTIP);
        return;
    }
    if(row.sampling.availability == SamplingAvailability::kInvalid)
    {
        RenderUnavailableStallCell(SAMPLING_STATE_INVALID_COUNTS_TOOLTIP);
        return;
    }

    const CodeCellBounds cell       = get_code_cell_bounds();
    const float          bar_height = cell.size.y * SAMPLING_STATE_BAR_HEIGHT_FRACTION;
    const float          stall_fraction =
        std::clamp(static_cast<float>(row.sampling.stall_percent) / 100.0f, 0.0f, 1.0f);
    const float  bar_width = cell.size.x * stall_fraction;
    const ImVec2 bar_start(cell.start.x,
                           cell.start.y + (cell.size.y - bar_height) * 0.5f);
    const ImVec2 bar_end(bar_start.x + bar_width, bar_start.y + bar_height);
    if(bar_width > 0.0f)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(
            cell.start, ImVec2(cell.start.x + cell.size.x, cell.start.y + cell.size.y),
            true);
        draw_list->AddRectFilled(bar_start, bar_end,
                                 m_settings.GetColor(Colors::kTextDim));
        double accumulated_share = 0.0;
        for(const StallReason& reason : row.stall_reasons)
        {
            const float segment_start_x =
                bar_start.x + bar_width * static_cast<float>(accumulated_share);
            accumulated_share += reason.bar_share;
            const float segment_end_x =
                bar_start.x +
                bar_width * static_cast<float>(std::min(accumulated_share, 1.0));
            draw_list->AddRectFilled(
                ImVec2(segment_start_x, bar_start.y), ImVec2(segment_end_x, bar_end.y),
                m_settings.GetColor(get_stall_reason_presentation(reason.kind).color));
        }
        draw_list->AddRect(bar_start, bar_end, m_settings.GetColor(Colors::kBorderGray));
        draw_list->PopClipRect();
    }
    const CodeCellInteraction interaction =
        finish_code_cell(cell, "##stall_categories_cell");
    row_clicked |= interaction.clicked;
    if(interaction.hovered)
    {
        RenderStallReasonsTooltip(row);
    }
}

void
IsaCodeWidget::RenderStallReasonsTooltip(const IsaRow& row)
{
    if(row.sampling.availability == SamplingAvailability::kMissing)
    {
        return;
    }
    if(row.sampling.availability == SamplingAvailability::kInvalid)
    {
        SetTooltipStyled("%s", SAMPLING_STATE_INVALID_COUNTS_TOOLTIP);
        return;
    }

    const StallReasonTableLayout layout = CalculateStallReasonTableLayout(row);
    const float mouse_wheel = layout.size.y > 0.0f && !ImGui::GetIO().KeyCtrl &&
                                      ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY)
                                  ? ImGui::GetIO().MouseWheel
                                  : 0.0f;
    BeginTooltipStyled();
    ImGui::PushID(static_cast<int>(row.id));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + layout.size.x);
    RenderStallReasonSummary(row);
    if(!row.stall_reasons.empty() || row.unclassified_stall_count > 0)
    {
        if(layout.size.y > 0.0f)
        {
            ImGui::TextDisabled("Scroll while hovering the cell to see all reasons.");
        }
        RenderStallReasonTable(row, layout, mouse_wheel);
        RenderStallReasonFooter(row);
    }
    ImGui::PopTextWrapPos();
    ImGui::PopID();
    EndTooltipStyled();
}

void
IsaCodeWidget::RenderStallReasonSummary(const IsaRow& row)
{
    const uint64_t    stall_count_value = *row.sample_counts.stall_count;
    const double      stall_percent     = row.sampling.stall_percent;
    const std::string stall_count = IsaDataBuilder::FormatSampleCount(stall_count_value);
    const std::string issue_count =
        IsaDataBuilder::FormatSampleCount(*row.sample_counts.issue_count);
    const std::string total_count =
        IsaDataBuilder::FormatSampleCount(row.sample_counts.total_count);

    ImGui::TextUnformatted(STALL_REASON_TOOLTIP_TITLE);
    ImGui::Text(STALL_REASON_TOOLTIP_SUMMARY, stall_count.c_str(), total_count.c_str(),
                stall_percent);
    ImGui::Text(STALL_REASON_TOOLTIP_ISSUED, issue_count.c_str());

    if(row.stall_reasons.empty())
    {
        ImGui::TextDisabled(stall_count_value == 0 ? STALL_REASON_TOOLTIP_NO_STALLS
                                                   : STALL_REASON_TOOLTIP_UNAVAILABLE);
    }
}

void
IsaCodeWidget::RenderStallReasonFooter(const IsaRow& row)
{
    const uint64_t    stall_count_value = *row.sample_counts.stall_count;
    const std::string stall_count = IsaDataBuilder::FormatSampleCount(stall_count_value);
    ImGui::TextDisabled(STALL_REASON_TOOLTIP_SHARE_DESCRIPTION, stall_count.c_str());
    if(row.stall_reason_sample_count != stall_count_value)
    {
        const std::string classified_count =
            IsaDataBuilder::FormatSampleCount(row.stall_reason_sample_count);
        ImGui::TextDisabled(STALL_REASON_TOOLTIP_COUNT_MISMATCH, classified_count.c_str(),
                            stall_count.c_str());
    }
    if(row.stall_reason_sample_count > stall_count_value)
    {
        ImGui::TextDisabled("%s", STALL_REASON_BAR_NORMALIZED_TOOLTIP);
    }
}

IsaCodeWidget::StallReasonTableLayout
IsaCodeWidget::CalculateStallReasonTableLayout(const IsaRow& row)
{
    const ImGuiStyle& style         = ImGui::GetStyle();
    const ImVec2      viewport_size = ImGui::GetWindowViewport()->WorkSize;
    float             count_width   = ImGui::CalcTextSize("Samples").x;
    for(const StallReason& reason : row.stall_reasons)
    {
        count_width =
            std::max(count_width, ImGui::CalcTextSize(reason.count_text.c_str()).x);
    }
    if(row.unclassified_stall_count > 0)
    {
        count_width =
            std::max(count_width,
                     ImGui::CalcTextSize(row.unclassified_stall_count_text.c_str()).x);
    }
    const float other_columns_width = ImGui::CalcTextSize("Color").x + count_width +
                                      ImGui::CalcTextSize("100.0%").x +
                                      style.CellPadding.x * 8.0f + style.ScrollbarSize;
    const float description_width = std::min(
        ImGui::CalcTextSize("0").x * STALL_REASON_TOOLTIP_DESCRIPTION_WIDTH_IN_CHARACTERS,
        std::max(ImGui::GetTextLineHeight(),
                 viewport_size.x - style.WindowPadding.x * 2.0f -
                     style.DisplaySafeAreaPadding.x * 2.0f - other_columns_width));
    const auto row_height = [description_width, &style](const char* name,
                                                        const char* description) {
        return ImGui::CalcTextSize(name, nullptr, false, description_width).y +
               ImGui::CalcTextSize(description, nullptr, false, description_width).y +
               style.ItemSpacing.y + style.CellPadding.y * 2.0f;
    };
    float height = ImGui::GetTextLineHeight() + style.CellPadding.y * 2.0f;
    for(const StallReason& reason : row.stall_reasons)
    {
        height += row_height(reason.text.c_str(),
                             get_stall_reason_presentation(reason.kind).description);
    }
    if(row.unclassified_stall_count > 0)
    {
        height += row_height(STALL_REASON_UNCLASSIFIED_TEXT,
                             STALL_REASON_UNCLASSIFIED_DESCRIPTION);
    }
    const float max_height = viewport_size.y * STALL_REASON_TOOLTIP_MAX_HEIGHT_FRACTION;
    return { ImVec2(description_width + other_columns_width,
                    height > max_height ? max_height : 0.0f),
             description_width };
}

void
IsaCodeWidget::RenderStallReasonTable(const IsaRow&                 row,
                                      const StallReasonTableLayout& layout,
                                      float                         mouse_wheel)
{
    if(row.sampling.availability != SamplingAvailability::kValid)
    {
        return;
    }

    const uint64_t stall_count_value = *row.sample_counts.stall_count;
    ImGui::Spacing();
    const ImGuiTableFlags flags = STALL_REASON_TOOLTIP_TABLE_FLAGS |
                                  (layout.size.y > 0.0f ? ImGuiTableFlags_ScrollY : 0);
    if(ImGui::BeginTable("##StallReasonDistribution", 4, flags, layout.size))
    {
        if(layout.size.y > 0.0f)
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            if(mouse_wheel != 0.0f)
            {
                ImGui::SetScrollY(ImGui::GetScrollY() -
                                  mouse_wheel * ImGui::GetTextLineHeightWithSpacing() *
                                      STALL_REASON_TOOLTIP_SCROLL_LINES);
            }
        }
        ImGui::TableSetupColumn(STALL_REASON_TOOLTIP_COLUMN_HEADERS[0]);
        ImGui::TableSetupColumn(STALL_REASON_TOOLTIP_COLUMN_HEADERS[1],
                                ImGuiTableColumnFlags_WidthFixed,
                                layout.description_width);
        ImGui::TableSetupColumn(STALL_REASON_TOOLTIP_COLUMN_HEADERS[2]);
        ImGui::TableSetupColumn(STALL_REASON_TOOLTIP_COLUMN_HEADERS[3]);
        ImGui::TableHeadersRow();

        for(const StallReason& reason : row.stall_reasons)
        {
            const StallReasonPresentation& presentation =
                get_stall_reason_presentation(reason.kind);
            render_stall_reason_legend_row(reason.text.c_str(), presentation.description,
                                           m_settings.GetColor(presentation.color),
                                           reason.count_text.c_str(), reason.share);
        }
        if(row.unclassified_stall_count > 0)
        {
            const uint64_t unclassified = row.unclassified_stall_count;
            render_stall_reason_legend_row(
                STALL_REASON_UNCLASSIFIED_TEXT, STALL_REASON_UNCLASSIFIED_DESCRIPTION,
                m_settings.GetColor(Colors::kTextDim),
                row.unclassified_stall_count_text.c_str(),
                IsaDataBuilder::CalculatePercentage(unclassified, stall_count_value));
        }
        ImGui::EndTable();
    }
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
        RenderSamplingStateCell(isa_row);
    }
    if(ImGui::TableSetColumnIndex(ColumnIndex(IsaColumn::kSamples)))
    {
        RenderSamplesCell(isa_row.sample_counts.total_count);
    }
    if(ImGui::TableSetColumnIndex(ColumnIndex(IsaColumn::kStallPercent)))
    {
        RenderStallPercentCell(isa_row, row_clicked);
    }
    if(ImGui::TableSetColumnIndex(ColumnIndex(IsaColumn::kStallCategories)))
    {
        RenderStallReasonBarCell(isa_row, row_clicked);
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
