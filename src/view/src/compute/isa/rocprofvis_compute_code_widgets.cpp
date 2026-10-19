// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_code_widgets.h"

#include <algorithm>
#include <cstdio>
#include <iterator>

#include "widgets/rocprofvis_gui_helpers.h"

namespace RocProfVis
{
namespace View
{

constexpr uint64_t INVALID_SOURCE_LINE_NUMBER = 0;

constexpr float       SAMPLING_STATE_BAR_HEIGHT_FRACTION              = 0.5f;
constexpr float       SAMPLING_STATE_COLUMN_WIDTH_IN_CHARACTERS       = 14.0f;
constexpr const char* SAMPLING_STATE_COLUMN_LABEL                     = "Sampling State";
constexpr float       STALL_REASON_TOOLTIP_REASON_WIDTH_IN_CHARACTERS = 42.0f;
constexpr float       STALL_REASON_TOOLTIP_MAX_HEIGHT_FRACTION        = 0.5f;
constexpr float       STALL_REASON_TOOLTIP_SCROLL_LINES               = 3.0f;
constexpr size_t      STALL_REASON_SHARE_TEXT_CAPACITY                = 32;

struct StallReasonPresentation
{
    IsaDataBuilder::StallReasonKind kind;
    Colors                          color;
    const char*                     name;
};

constexpr StallReasonPresentation STALL_REASON_PRESENTATIONS[] = {
    { IsaDataBuilder::StallReasonKind::kInstructionFetch,
      Colors::kPcSamplingInstructionFetch, "NO_INSTRUCTION_AVAILABLE" },
    { IsaDataBuilder::StallReasonKind::kAluDependency, Colors::kPcSamplingAluDependency,
      "ALU_DEPENDENCY" },
    { IsaDataBuilder::StallReasonKind::kWaitcnt, Colors::kPcSamplingWaitcnt, "WAITCNT" },
    { IsaDataBuilder::StallReasonKind::kInternalInstruction,
      Colors::kPcSamplingInternalInstruction, "INTERNAL_INSTRUCTION" },
    { IsaDataBuilder::StallReasonKind::kBarrier, Colors::kPcSamplingBarrier,
      "BARRIER_WAIT" },
    { IsaDataBuilder::StallReasonKind::kArbiter, Colors::kPcSamplingArbiter,
      "ARBITER_NOT_WIN" },
    { IsaDataBuilder::StallReasonKind::kPipelineStall, Colors::kPcSamplingPipelineStall,
      "ARBITER_WIN_EX_STALL" },
    { IsaDataBuilder::StallReasonKind::kOtherWait, Colors::kPcSamplingOtherWait,
      "OTHER_WAIT" },
    { IsaDataBuilder::StallReasonKind::kSleeping, Colors::kPcSamplingSleeping, "SLEEP" }
};
constexpr StallReasonPresentation UNKNOWN_STALL_REASON_PRESENTATION{
    IsaDataBuilder::StallReasonKind::kUnknown, Colors::kTextDim, "Unknown"
};

// get_stall_reason_presentation indexes STALL_REASON_PRESENTATIONS by enum value, so the
// array must stay in StallReasonKind order and cover every reason before kUnknown.
static_assert(std::size(STALL_REASON_PRESENTATIONS) ==
              static_cast<size_t>(IsaDataBuilder::StallReasonKind::kUnknown));

constexpr bool
stall_reason_presentations_in_enum_order()
{
    for(size_t i = 0; i < std::size(STALL_REASON_PRESENTATIONS); ++i)
    {
        if(static_cast<size_t>(STALL_REASON_PRESENTATIONS[i].kind) != i)
        {
            return false;
        }
    }
    return true;
}
static_assert(stall_reason_presentations_in_enum_order());

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

constexpr HeaderTooltipText ISA_INSTRUCTION_HEADER_TOOLTIP{
    "GPU instruction and its operands.",
    "DB field: compute_instruction_line.instruction\n"
    "Row key: compute_instruction_line.instruction_uuid\n"
    "DB filter: compute_kernel_symbol.kernel_uuid = selected kernel\n"
    "View filter: selected code object UUID"
};

constexpr HeaderTooltipText SAMPLING_STATE_HEADER_TOOLTIP{
    nullptr,
    "DB: compute_pc_sample_state; group by instruction_uuid.\n"
    "T = SUM(total_count), I = SUM(issue_count), S = SUM(stall_count).\n"
    "H = MAX(T) across displayed instructions; R = SUM(normalized reason counts).\n"
    "Reason DB: compute_pc_sample_stall_reason, joined by pc_sample_state_uuid;\n"
    "names: compute_pc_sample_stall_reason_lookup.text via lookup UUID.\n"
    "Valid: every state has non-NULL progress counts; I <= T and S <= T - I.\n"
    "Cell-width fractions: total = T/H, issued = I/H, stalled = S/H.\n"
    "Each reason = (S/H) * reason_count / max(S, R); no fill for zero denominator.\n"
    "Order: issued first, then reasons by count descending, name ascending.\n"
    "Gray fractions: missing stall reasons = max(S - R, 0)/H; progress = (T - I - S)/H.\n"
    "Per state, OTHER_WAIT/OTHER includes SDK-prefixed aliases. Subtract issued only if\n"
    "raw reasons = total_count, issue_count + stall_count = total_count, and OTHER "
    "suffices.\n"
    "Cell tooltip: 100 * count/T, including Issued; zero T gives 0%.\n"
    "Missing/invalid progress gives a gray total bar; zero H gives no bar."
};

constexpr HeaderTooltipText CODE_OBJECT_OFFSET_HEADER_TOOLTIP{
    "Instruction's byte offset within the GPU code object, in hexadecimal.",
    "DB field: compute_instruction_line.code_object_offset\n"
    "Row key: compute_instruction_line.instruction_uuid\n"
    "Display: 0x followed by the uppercase hexadecimal offset\n"
    "Missing DB values are returned as 0 by the instruction-line query."
};

constexpr const char* SAMPLING_STATE_NO_SAMPLES_TOOLTIP =
    "No PC samples were recorded for this instruction.\n"
    "There are no sampled observations to classify by stall reason.";
constexpr const char* SAMPLING_STATE_MISSING_ISSUED_COUNTS_TOOLTIP =
    "PC samples were recorded for this instruction, but their issued counts were not "
    "fully recorded.\nThe sampling-state breakdown is unavailable.";
constexpr const char* SAMPLING_STATE_MISSING_STALLED_COUNTS_TOOLTIP =
    "PC samples were recorded for this instruction, but their stalled counts were not "
    "fully recorded.\nThe sampling-state breakdown is unavailable.";
constexpr const char* SAMPLING_STATE_MISSING_PROGRESS_COUNTS_TOOLTIP =
    "PC samples were recorded for this instruction, but their issued and stalled counts "
    "were not fully recorded.\nThe sampling-state breakdown is unavailable.";
constexpr const char* SAMPLING_STATE_INVALID_COUNTS_TOOLTIP =
    "Issue/stall breakdown is unavailable because the recorded counts exceed the total "
    "sample count.";

constexpr const char* SAMPLING_STATE_ISSUED_TEXT                = "Issued";
constexpr const char* SAMPLING_STATE_UNCLASSIFIED_STALLS_TEXT   = "Unclassified stalls";
constexpr const char* SAMPLING_STATE_UNCLASSIFIED_PROGRESS_TEXT = "Unclassified progress";
constexpr const char* STALL_REASON_TOOLTIP_SUMMARY = "Stalled: %s/%s (%.1f%%)";
constexpr const char* STALL_REASON_TOOLTIP_NO_STALLS =
    "No stalled samples were recorded for this instruction.";
constexpr const char* STALL_REASON_TOOLTIP_UNAVAILABLE =
    "No stall-reason details were recorded for this instruction.";
constexpr const char* STALL_REASON_TOOLTIP_COUNT_MISMATCH =
    "Reason data classifies %s samples; expected %s stalled samples.";
constexpr const char* STALL_REASON_BAR_NORMALIZED_TOOLTIP =
    "Colors are scaled to fit the stalled portion because the recorded reason total "
    "exceeds the stalled count.";
constexpr const char* SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[] = {
    "Color", SAMPLING_STATE_COLUMN_LABEL, "Samples", "% of Samples"
};
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
finish_code_cell(const CodeCellBounds& cell, const char* interaction_id)
{
    ImGui::SetCursorScreenPos(cell.start);
    const bool clicked = ImGui::InvisibleButton(interaction_id, cell.size);
    return { clicked, ImGui::IsItemHovered() };
}

static const StallReasonPresentation&
get_stall_reason_presentation(IsaDataBuilder::StallReasonKind kind)
{
    const uint32_t index = static_cast<uint32_t>(kind);
    if(index < std::size(STALL_REASON_PRESENTATIONS))
    {
        return STALL_REASON_PRESENTATIONS[index];
    }
    return UNKNOWN_STALL_REASON_PRESENTATION;
}

static void
render_sampling_color_swatch(const char* text, ImU32 color, float size_fraction = 1.0f)
{
    ImGui::PushID(text);
    const float line_height = ImGui::GetTextLineHeight();
    const float swatch_size = line_height * size_fraction;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (line_height - swatch_size) * 0.5f);
    ImGui::ColorButton("##reason_color", ImGui::ColorConvertU32ToFloat4(color),
                       ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoInputs |
                           ImGuiColorEditFlags_NoDragDrop,
                       ImVec2(swatch_size, swatch_size));
    ImGui::PopID();
}

static void
render_sampling_state_color_legend()
{
    SettingsManager& settings                   = SettingsManager::GetInstance();
    constexpr float  SWATCH_SIZE_FRACTION       = 0.7f;
    constexpr float  CELL_PADDING_IN_CHARACTERS = 0.25f;
    ImGui::PushStyleVar(
        ImGuiStyleVar_CellPadding,
        ImVec2(ImGui::CalcTextSize(" ").x * CELL_PADDING_IN_CHARACTERS, 0.0f));
    const ImGuiTableFlags flags =
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX |
        ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoSavedSettings;
    if(ImGui::BeginTable("##SamplingStateColorLegend", 2, flags))
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        render_sampling_color_swatch(SAMPLING_STATE_ISSUED_TEXT,
                                     settings.GetColor(Colors::kTextSuccess),
                                     SWATCH_SIZE_FRACTION);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(SAMPLING_STATE_ISSUED_TEXT);
        for(const StallReasonPresentation& presentation : STALL_REASON_PRESENTATIONS)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            render_sampling_color_swatch(presentation.name,
                                         settings.GetColor(presentation.color),
                                         SWATCH_SIZE_FRACTION);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(presentation.name);
        }
        constexpr const char* GRAY_LABEL = "Unknown / unclassified / N/A";
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        render_sampling_color_swatch(GRAY_LABEL, settings.GetColor(Colors::kTextDim),
                                     SWATCH_SIZE_FRACTION);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(GRAY_LABEL);
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

static void
render_stall_reason_legend_row(const char* text, ImU32 color, const char* count_text,
                               double share)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    render_sampling_color_swatch(text, color);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped("%s", text);
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
                                 const HeaderTooltipText& tooltip,
                                 void (*render_user_content)() = nullptr)
{
    if(!ImGui::TableSetColumnIndex(column))
    {
        return;
    }
    ImGui::TableHeader(label);
    if(!ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
    {
        return;
    }

    BeginTooltipStyled();
    if(tooltip.user_description)
    {
        ImGui::TextUnformatted(tooltip.user_description);
    }
    if(render_user_content)
    {
        render_user_content();
    }
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

void
BaseCodeWidget::RenderClippedRows(size_t row_count, uint64_t& scroll_line,
                                  const std::function<uint64_t(uint32_t)>& id_of,
                                  const std::function<void(uint32_t)>&     render_line)
{
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(row_count));

    uint32_t scroll_target = NO_SCROLL_TARGET;
    if(scroll_line != LineSelection::UNSELECTED)
    {
        for(uint32_t i = 0; i < row_count; ++i)
        {
            if(id_of(i) == scroll_line)
            {
                scroll_target = i + 1;
                scroll_line   = LineSelection::UNSELECTED;
                clipper.IncludeItemByIndex(static_cast<int>(i));
                break;
            }
        }
    }

    while(clipper.Step())
    {
        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
        {
            render_line(static_cast<uint32_t>(i));
            if(scroll_target != NO_SCROLL_TARGET &&
               static_cast<uint32_t>(i) + 1 == scroll_target)
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

    RenderClippedRows(
        m_lines.size(), m_line_selection.source_scroll_line,
        [this](uint32_t i) { return m_lines[i].id; },
        [this](uint32_t i) { RenderLine(i); });

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndTable();
    ImGui::PopStyleVar();
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
    m_hottest_instruction_samples = IsaDataBuilder::UpdateSampling(m_entries, data);
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

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        m_settings.GetDefaultStyle().WindowPadding);
    // Use a new table ID so the previous column layout does not override these defaults.
    if(!ImGui::BeginTable("IsaCodeSamplingState", columns_count,
                          m_table_flags | ImGuiTableFlags_Reorderable))
    {
        ImGui::PopStyleVar();
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);

    SetupColumns();
    RenderHeaders();
    PushStyles();

    RenderClippedRows(
        m_entries.size(), m_line_selection.isa_scroll_line,
        [this](uint32_t i) { return m_entries[i].source_line_id; },
        [this](uint32_t i) { RenderLine(i); });

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    ImGui::EndTable();
    ImGui::PopStyleVar();
}

void
IsaCodeWidget::SetupColumns()
{
    // Disabled temporarily hides Sampling State without discarding its saved layout.
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
                            sampling_visibility_flags | ImGuiTableColumnFlags_WidthFixed,
                            sampling_state_column_width);
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
    render_table_header_with_tooltip(
        ColumnIndex(IsaColumn::kSamplingState), SAMPLING_STATE_COLUMN_LABEL,
        SAMPLING_STATE_HEADER_TOOLTIP, render_sampling_state_color_legend);
    render_table_header_with_tooltip(ColumnIndex(IsaColumn::kOffset), "Offset",
                                     CODE_OBJECT_OFFSET_HEADER_TOOLTIP);
    render_table_header_with_tooltip(ColumnIndex(IsaColumn::kInstruction), "ISA",
                                     ISA_INSTRUCTION_HEADER_TOOLTIP);
}

void
IsaCodeWidget::RenderSamplingStateCell(const IsaRow& row, bool& row_clicked)
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
        draw_list->PushClipRect(cell.start, cell.End(), true);
        draw_list->AddRectFilled(bar_start, bar_end,
                                 m_settings.GetColor(Colors::kTextDim));
        if(row.sampling.availability == SamplingAvailability::kValid)
        {
            const float issue_width =
                bar_width * static_cast<float>(row.sampling.issue_percent / 100.0);
            const float stall_width =
                bar_width * static_cast<float>(row.sampling.stall_percent / 100.0);
            const ImVec2 issue_end(bar_start.x + issue_width, bar_end.y);
            if(issue_width > 0.0f)
            {
                draw_list->AddRectFilled(bar_start, issue_end,
                                         m_settings.GetColor(Colors::kTextSuccess));
            }
            DrawStallReasonSegments(row, ImVec2(issue_end.x, bar_start.y),
                                    ImVec2(issue_end.x + stall_width, bar_end.y));
        }
        draw_list->AddRect(bar_start, bar_end, m_settings.GetColor(Colors::kBorderGray));
        draw_list->PopClipRect();
    }
    const CodeCellInteraction interaction =
        finish_code_cell(cell, "##sampling_state_cell");
    row_clicked |= interaction.clicked;
    if(interaction.hovered)
    {
        RenderSamplingStateTooltip(row);
    }
}

const char*
IsaCodeWidget::GetUnavailableSamplingTooltip(const IsaRow& row)
{
    if(row.sampling.availability == SamplingAvailability::kInvalid)
    {
        return SAMPLING_STATE_INVALID_COUNTS_TOOLTIP;
    }
    if(row.sample_counts.total_count == 0)
    {
        return SAMPLING_STATE_NO_SAMPLES_TOOLTIP;
    }
    if(!row.sample_counts.issue_count && row.sample_counts.stall_count)
    {
        return SAMPLING_STATE_MISSING_ISSUED_COUNTS_TOOLTIP;
    }
    if(row.sample_counts.issue_count && !row.sample_counts.stall_count)
    {
        return SAMPLING_STATE_MISSING_STALLED_COUNTS_TOOLTIP;
    }
    return SAMPLING_STATE_MISSING_PROGRESS_COUNTS_TOOLTIP;
}

void
IsaCodeWidget::DrawStallReasonSegments(const IsaRow& row, const ImVec2& start,
                                       const ImVec2& end)
{
    const float width = end.x - start.x;
    if(width <= 0.0f)
    {
        return;
    }
    ImDrawList* draw_list         = ImGui::GetWindowDrawList();
    double      accumulated_share = 0.0;
    for(const StallReason& reason : row.stall_reasons)
    {
        const float segment_start =
            start.x + width * static_cast<float>(accumulated_share);
        accumulated_share += reason.bar_share;
        const float segment_end =
            start.x + width * static_cast<float>(std::min(accumulated_share, 1.0));
        draw_list->AddRectFilled(
            ImVec2(segment_start, start.y), ImVec2(segment_end, end.y),
            m_settings.GetColor(get_stall_reason_presentation(reason.kind).color));
    }
}

void
IsaCodeWidget::RenderSamplingStateTooltip(const IsaRow& row)
{
    if(row.sampling.availability != SamplingAvailability::kValid)
    {
        SetTooltipStyled("%s", GetUnavailableSamplingTooltip(row));
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
    RenderSamplingSummary(row);
    if(layout.size.y > 0.0f)
    {
        ImGui::TextDisabled("Hover the cell and scroll to see all sampling states.");
    }
    RenderStallReasonTable(row, layout, mouse_wheel);
    RenderStallReasonFooter(row);
    ImGui::PopTextWrapPos();
    ImGui::PopID();
    EndTooltipStyled();
}

void
IsaCodeWidget::RenderSamplingSummary(const IsaRow& row)
{
    const uint64_t    stall_count_value = *row.sample_counts.stall_count;
    const double      stall_percent     = row.sampling.stall_percent;
    const std::string stall_count = IsaDataBuilder::FormatSampleCount(stall_count_value);
    const std::string total_count =
        IsaDataBuilder::FormatSampleCount(row.sample_counts.total_count);

    ImGui::Text(STALL_REASON_TOOLTIP_SUMMARY, stall_count.c_str(), total_count.c_str(),
                stall_percent);

    if(row.stall_reasons.empty())
    {
        ImGui::TextDisabled(stall_count_value == 0 ? STALL_REASON_TOOLTIP_NO_STALLS
                                                   : STALL_REASON_TOOLTIP_UNAVAILABLE);
    }
}

void
IsaCodeWidget::RenderStallReasonFooter(const IsaRow& row)
{
    const uint64_t stall_count_value = *row.sample_counts.stall_count;
    if(row.stall_reason_sample_count != stall_count_value)
    {
        const std::string stall_count =
            IsaDataBuilder::FormatSampleCount(stall_count_value);
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
    float             reason_text_width =
        ImGui::CalcTextSize(SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[1]).x;
    float share_width =
        std::max(ImGui::CalcTextSize(SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[3]).x,
                 ImGui::CalcTextSize("100.0%").x);
    for(const StallReason& reason : row.stall_reasons)
    {
        count_width =
            std::max(count_width, ImGui::CalcTextSize(reason.count_text.c_str()).x);
        reason_text_width =
            std::max(reason_text_width, ImGui::CalcTextSize(reason.text.c_str()).x);
        char share_text[STALL_REASON_SHARE_TEXT_CAPACITY] = {};
        std::snprintf(share_text, sizeof(share_text), "%.1f%%",
                      IsaDataBuilder::CalculatePercentage(reason.count,
                                                          row.sample_counts.total_count));
        share_width = std::max(share_width, ImGui::CalcTextSize(share_text).x);
    }
    if(row.unclassified_stall_count > 0)
    {
        count_width =
            std::max(count_width,
                     ImGui::CalcTextSize(row.unclassified_stall_count_text.c_str()).x);
        reason_text_width =
            std::max(reason_text_width,
                     ImGui::CalcTextSize(SAMPLING_STATE_UNCLASSIFIED_STALLS_TEXT).x);
    }
    count_width = std::max(
        count_width,
        ImGui::CalcTextSize(
            IsaDataBuilder::FormatSampleCount(*row.sample_counts.issue_count).c_str())
            .x);
    reason_text_width =
        std::max(reason_text_width, ImGui::CalcTextSize(SAMPLING_STATE_ISSUED_TEXT).x);
    if(row.sampling.unclassified_count > 0)
    {
        count_width =
            std::max(count_width, ImGui::CalcTextSize(IsaDataBuilder::FormatSampleCount(
                                                          row.sampling.unclassified_count)
                                                          .c_str())
                                      .x);
        reason_text_width =
            std::max(reason_text_width,
                     ImGui::CalcTextSize(SAMPLING_STATE_UNCLASSIFIED_PROGRESS_TEXT).x);
    }
    const float color_width =
        std::max(ImGui::CalcTextSize("Color").x, ImGui::GetTextLineHeight());
    const float other_columns_width =
        color_width + count_width + share_width + style.CellPadding.x * 8.0f;
    const float reason_width = std::min(
        { reason_text_width,
          ImGui::CalcTextSize("0").x * STALL_REASON_TOOLTIP_REASON_WIDTH_IN_CHARACTERS,
          std::max(ImGui::GetTextLineHeight(),
                   viewport_size.x - style.WindowPadding.x * 2.0f -
                       style.DisplaySafeAreaPadding.x * 2.0f - other_columns_width -
                       style.ScrollbarSize) });
    const auto row_height = [reason_width, &style](const char* name) {
        return std::max(ImGui::GetTextLineHeight(),
                        ImGui::CalcTextSize(name, nullptr, false, reason_width).y) +
               style.CellPadding.y * 2.0f;
    };
    float height = ImGui::GetTextLineHeight() + style.CellPadding.y * 2.0f;
    for(const StallReason& reason : row.stall_reasons)
    {
        height += row_height(reason.text.c_str());
    }
    if(row.unclassified_stall_count > 0)
    {
        height += row_height(SAMPLING_STATE_UNCLASSIFIED_STALLS_TEXT);
    }
    height += row_height(SAMPLING_STATE_ISSUED_TEXT);
    if(row.sampling.unclassified_count > 0)
    {
        height += row_height(SAMPLING_STATE_UNCLASSIFIED_PROGRESS_TEXT);
    }
    const float max_height = viewport_size.y * STALL_REASON_TOOLTIP_MAX_HEIGHT_FRACTION;
    const bool  scrollable = height > max_height;
    return { ImVec2(reason_width + other_columns_width +
                        (scrollable ? style.ScrollbarSize : 0.0f),
                    scrollable ? max_height : 0.0f),
             reason_width, color_width, count_width, share_width };
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

    const uint64_t share_total = row.sample_counts.total_count;
    ImGui::Spacing();
    const ImGuiTableFlags flags =
        STALL_REASON_TOOLTIP_TABLE_FLAGS |
        (layout.size.y > 0.0f ? ImGuiTableFlags_ScrollY : ImGuiTableFlags_NoHostExtendX);
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
        // Reapply content widths so no column retains spare space from an earlier layout.
        const ImGuiTableColumnFlags column_flags =
            ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize;
        ImGui::TableSetupColumn(SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[0], column_flags,
                                layout.color_width);
        ImGui::TableSetupColumn(SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[1], column_flags,
                                layout.reason_width);
        ImGui::TableSetupColumn(SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[2], column_flags,
                                layout.count_width);
        ImGui::TableSetupColumn(SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[3], column_flags,
                                layout.share_width);
        ImGui::TableHeadersRow();
        const std::string issued_count =
            IsaDataBuilder::FormatSampleCount(*row.sample_counts.issue_count);
        render_stall_reason_legend_row(SAMPLING_STATE_ISSUED_TEXT,
                                       m_settings.GetColor(Colors::kTextSuccess),
                                       issued_count.c_str(), row.sampling.issue_percent);

        for(const StallReason& reason : row.stall_reasons)
        {
            const StallReasonPresentation& presentation =
                get_stall_reason_presentation(reason.kind);
            render_stall_reason_legend_row(
                reason.text.c_str(), m_settings.GetColor(presentation.color),
                reason.count_text.c_str(),
                IsaDataBuilder::CalculatePercentage(reason.count, share_total));
        }
        if(row.unclassified_stall_count > 0)
        {
            const uint64_t unclassified = row.unclassified_stall_count;
            render_stall_reason_legend_row(
                SAMPLING_STATE_UNCLASSIFIED_STALLS_TEXT,
                m_settings.GetColor(Colors::kTextDim),
                row.unclassified_stall_count_text.c_str(),
                IsaDataBuilder::CalculatePercentage(unclassified, share_total));
        }
        if(row.sampling.unclassified_count > 0)
        {
            const std::string unclassified_count =
                IsaDataBuilder::FormatSampleCount(row.sampling.unclassified_count);
            render_stall_reason_legend_row(
                SAMPLING_STATE_UNCLASSIFIED_PROGRESS_TEXT,
                m_settings.GetColor(Colors::kTextDim), unclassified_count.c_str(),
                IsaDataBuilder::CalculatePercentage(row.sampling.unclassified_count,
                                                    share_total));
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
        RenderSamplingStateCell(isa_row, row_clicked);
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
