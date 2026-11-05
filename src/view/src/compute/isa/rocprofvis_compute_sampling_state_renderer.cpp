// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_sampling_state_renderer.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <iterator>
#include <utility>

#include "imgui_internal.h"

#include "widgets/rocprofvis_gui_helpers.h"

namespace RocProfVis
{
namespace View
{

constexpr float       SAMPLING_STATE_BAR_HEIGHT_FRACTION              = 0.5f;
constexpr const char* SAMPLING_STATE_COLUMN_LABEL                     = "Sampling State";
constexpr float       STALL_REASON_TOOLTIP_REASON_WIDTH_IN_CHARACTERS = 42.0f;
constexpr float       STALL_REASON_TOOLTIP_MAX_HEIGHT_FRACTION        = 0.5f;
constexpr float       STALL_REASON_TOOLTIP_SCROLL_LINES               = 3.0f;
constexpr float       SAMPLE_METRIC_BAR_HEIGHT_FRACTION               = 0.12f;
constexpr float       SAMPLE_METRIC_TRACK_ALPHA                       = 0.15f;
constexpr float       SAMPLE_METRIC_BAR_ALPHA                         = 0.6f;
constexpr double      SAMPLE_METRIC_PERCENT_SCALE                     = 100.0;
constexpr float       SAMPLE_METRIC_TOOLTIP_WIDTH_IN_CHARACTERS       = 64.0f;

struct SampleMetricPresentation
{
    const char* label;
    const char* title;
    const char* isa_description;
    const char* source_description;
    const char* developer_information;
};

static const SampleMetricPresentation&
get_sample_metric_presentation(SamplingStateRenderer::Metric metric)
{
    static constexpr SampleMetricPresentation OCCUPANCY{
        "Wave occupancy %", "Wave occupancy",
        "Wave occupancy (%) reported by the profiler at sampled PCs.\n"
        "Sample-weighted mean for this instruction.\n"
        "Bar scale: 0-100%, independent of the hottest instruction.",
        "Wave occupancy (%) reported by the profiler at sampled PCs.\n"
        "Sample-weighted mean over this line's mapped instructions.\n"
        "Bar scale: 0-100%, independent of the hottest source line.",
        "DB: compute_pc_sample_state.wave_occupancy_percent\n"
        "ISA: group by instruction_uuid. Source: first primary mapping per instruction\n"
        "in the selected file; duplicate mappings and inline frames do not add weight.\n"
        "SUM(total_count * percent) / SUM(total_count), ignoring zero-sample states.\n"
        "Missing measurements propagate; invalid values or weight overflow are "
        "unavailable.\n"
        "Bar scale: 0-100%, independent of sample hotness."
    };
    static constexpr SampleMetricPresentation ACTIVE_THREADS{
        "Active threads %", "Active threads",
        "Active-thread percentage reported by the profiler at sampled PCs.\n"
        "Sample-weighted mean for this instruction.\n"
        "Separate from issued/stalled sample shares; bar scale: 0-100%.",
        "Active-thread percentage reported by the profiler at sampled PCs.\n"
        "Sample-weighted mean over this line's mapped instructions.\n"
        "Separate from issued/stalled sample shares; bar scale: 0-100%.",
        "DB: compute_pc_sample_state.active_thread_percent\n"
        "ISA: group by instruction_uuid. Source: first primary mapping per instruction\n"
        "in the selected file; duplicate mappings and inline frames do not add weight.\n"
        "SUM(total_count * percent) / SUM(total_count), ignoring zero-sample states.\n"
        "Missing measurements propagate; invalid values or weight overflow are "
        "unavailable.\n"
        "Bar scale: 0-100%, independent of sample hotness."
    };
    return metric == SamplingStateRenderer::Metric::kWaveOccupancy ? OCCUPANCY
                                                                   : ACTIVE_THREADS;
}

constexpr SamplingStateRenderer::Metric SAMPLING_COLUMN_METRICS[] = {
    SamplingStateRenderer::Metric::kSamplingState,
    SamplingStateRenderer::Metric::kWaveOccupancy,
    SamplingStateRenderer::Metric::kActiveThreads
};

static const char*
get_sampling_column_label(SamplingStateRenderer::Metric metric)
{
    return metric == SamplingStateRenderer::Metric::kSamplingState
               ? SAMPLING_STATE_COLUMN_LABEL
               : get_sample_metric_presentation(metric).label;
}

static const IsaDataBuilder::PercentageSummary&
get_sample_metric_summary(const IsaDataBuilder::SamplingRow& row,
                          SamplingStateRenderer::Metric      metric)
{
    return metric == SamplingStateRenderer::Metric::kWaveOccupancy ? row.wave_occupancy
                                                                   : row.active_threads;
}

static void
push_sample_metric_tooltip_wrap()
{
    const float width =
        std::min(ImGui::CalcTextSize("0").x * SAMPLE_METRIC_TOOLTIP_WIDTH_IN_CHARACTERS,
                 ImGui::GetWindowViewport()->WorkSize.x -
                     ImGui::GetStyle().WindowPadding.x * 2.0f);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() +
                           std::max(width, ImGui::GetTextLineHeight()));
}

struct StallReasonPresentation
{
    IsaDataBuilder::StallReasonKind kind;
    Colors                          color;
};

constexpr StallReasonPresentation STALL_REASON_PRESENTATIONS[] = {
    { IsaDataBuilder::StallReasonKind::kInstructionFetch,
      Colors::kPcSamplingInstructionFetch },
    { IsaDataBuilder::StallReasonKind::kAluDependency, Colors::kPcSamplingAluDependency },
    { IsaDataBuilder::StallReasonKind::kWaitcnt, Colors::kPcSamplingWaitcnt },
    { IsaDataBuilder::StallReasonKind::kInternalInstruction,
      Colors::kPcSamplingInternalInstruction },
    { IsaDataBuilder::StallReasonKind::kBarrier, Colors::kPcSamplingBarrier },
    { IsaDataBuilder::StallReasonKind::kArbiter, Colors::kPcSamplingArbiter },
    { IsaDataBuilder::StallReasonKind::kPipelineStall, Colors::kPcSamplingPipelineStall },
    { IsaDataBuilder::StallReasonKind::kOtherWait, Colors::kPcSamplingOtherWait },
    { IsaDataBuilder::StallReasonKind::kSleeping, Colors::kPcSamplingSleeping }
};
constexpr StallReasonPresentation UNKNOWN_STALL_REASON_PRESENTATION{
    IsaDataBuilder::StallReasonKind::kUnknown, Colors::kTextDim
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

constexpr HeaderTooltipText SAMPLING_STATE_HEADER_TOOLTIP{
    nullptr,
    "DB: compute_pc_sample_state; group by instruction_uuid.\n"
    "T = SUM(total_count), I = SUM(issue_count), S = SUM(stall_count).\n"
    "H = MAX(T) across displayed instructions excluding invalid counts.\n"
    "R = SUM(normalized reason counts).\n"
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
    "Missing progress gives a gray total bar; invalid counts or zero H give no bar.\n"
    "Failed stall details retain total-only bars; failed sample loading disables the "
    "column."
};

constexpr HeaderTooltipText SOURCE_SAMPLING_STATE_HEADER_TOOLTIP{
    nullptr,
    "DB: compute_pc_sample_state + compute_pc_sample_stall_reason and reason lookup.\n"
    "Map instruction_uuid through compute_instruction_source_line (frame_index = 0).\n"
    "Use the first mapping per instruction; include only the selected file's lines.\n"
    "Sum total/issued/stalled counts and normalized reasons across mapped instructions.\n"
    "Normalize issued samples out of OTHER_WAIT per sample state before summing.\n"
    "T/I/S = line total/issued/stalled counts.\n"
    "H = hottest displayed source line excluding invalid counts.\n"
    "R = line normalized reason total. Bar fractions: total T/H, issued I/H.\n"
    "Each stall reason: (S/H) * reason_count / max(S, R).\n"
    "Order: issued first; reasons by descending count, then ascending name.\n"
    "Gray: unclassified stalls and progress; missing progress is neutral.\n"
    "Invalid counts do not set the scale or draw a bar.\n"
    "Missing counts propagate; any invalid contributing sample state invalidates the "
    "line.\n"
    "Cell tooltip: count/T * 100 for Issued and each reason; zero denominator gives 0%.\n"
    "Failed stall details retain total-only bars; failed sample loading disables the "
    "column."
};

constexpr const char* SAMPLING_STATE_NO_SAMPLES_TOOLTIP =
    "No PC samples recorded for this line.";
constexpr const char* SAMPLING_STATE_MISSING_ISSUED_COUNTS_TOOLTIP =
    "Issued sample counts unavailable.";
constexpr const char* SAMPLING_STATE_MISSING_STALLED_COUNTS_TOOLTIP =
    "Stalled sample counts unavailable.";
constexpr const char* SAMPLING_STATE_MISSING_PROGRESS_COUNTS_TOOLTIP =
    "No issued/stalled data recorded.\n"
    "Host-trap sampling does not collect stall data (expected).";
constexpr const char* SAMPLING_STATE_INVALID_COUNTS_TOOLTIP =
    "Issued/stalled counts are inconsistent with total samples.";
constexpr const char* SAMPLING_STATE_SAMPLE_COUNT_TOOLTIP = "Samples: %s";
constexpr const char* SAMPLING_STATE_BAR_PERCENT_TOOLTIP =
    "Bar: %.1f%% of maximum samples.";
constexpr const char* SAMPLING_STATE_LOADING_TOOLTIP = "Loading sampling data...";
constexpr const char* SAMPLING_STATE_FAILED_TOOLTIP =
    "Sampling data could not be loaded.\nRight-click a column header to retry.";
constexpr const char* SAMPLING_STATE_TOTALS_ONLY_TOOLTIP =
    "Stall details could not be loaded. The bar shows total samples.";
constexpr const char* SAMPLING_STATE_LOADING_DETAILS_TOOLTIP =
    "Loading stall details. The bar shows total samples.";

constexpr const char* SAMPLING_STATE_ISSUED_TEXT                = "Issued";
constexpr const char* SAMPLING_STATE_UNCLASSIFIED_STALLS_TEXT   = "Unclassified stalls";
constexpr const char* SAMPLING_STATE_UNCLASSIFIED_PROGRESS_TEXT = "Unclassified progress";
constexpr const char* STALL_REASON_TOOLTIP_SUMMARY = "Stalled: %s/%s (%.1f%%)";
constexpr const char* STALL_REASON_TOOLTIP_NO_STALLS =
    "No stalled samples were recorded for this line.";
constexpr const char* STALL_REASON_TOOLTIP_UNAVAILABLE =
    "No stall-reason details were recorded for this line.";
constexpr const char* STALL_REASON_TOOLTIP_OVERFLOW =
    "Stall-reason counts exceed the supported range; the reason breakdown is "
    "unavailable.";
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
finish_code_cell(const CodeCellBounds& cell, const char* interaction_id,
                 bool disabled = false)
{
    ImGui::SetCursorScreenPos(cell.start);
    ImGui::BeginDisabled(disabled);
    const bool clicked = ImGui::InvisibleButton(interaction_id, cell.size);
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    ImGui::EndDisabled();
    return { clicked, hovered };
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
    std::array<ColorLegendEntry, std::size(STALL_REASON_PRESENTATIONS) + 2> entries{};
    entries.front() = { SAMPLING_STATE_ISSUED_TEXT, Colors::kTextSuccess };
    for(size_t i = 0; i < std::size(STALL_REASON_PRESENTATIONS); ++i)
    {
        const StallReasonPresentation& presentation = STALL_REASON_PRESENTATIONS[i];
        entries[i + 1] = { IsaDataBuilder::GetStallReasonName(presentation.kind),
                           presentation.color };
    }
    entries.back() = { "Unknown / unclassified / N/A", Colors::kTextDim };
    SamplingStateRenderer::RenderColorLegend("##SamplingStateColorLegend", entries.data(),
                                             entries.size());
}

void
SamplingStateRenderer::RenderColorLegend(const char* id, const ColorLegendEntry* entries,
                                         size_t count)
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
    if(ImGui::BeginTable(id, 2, flags))
    {
        for(size_t i = 0; i < count; ++i)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            render_sampling_color_swatch(entries[i].name,
                                         settings.GetColor(entries[i].color),
                                         SWATCH_SIZE_FRACTION);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(entries[i].name);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

SamplingStateRenderer::SamplingStateRenderer(SettingsManager& settings)
: m_settings(settings)
{}

void
SamplingStateRenderer::RenderHeaderWithTooltip(int column, const char* label,
                                               const HeaderTooltipText& tooltip,
                                               void (*render_user_content)())
{
    if(!ImGui::TableSetColumnIndex(column))
    {
        return;
    }
    ImGui::TableHeader(label);
    RenderHeaderTooltip(tooltip, render_user_content);
}

void
SamplingStateRenderer::RenderHeaderTooltip(const HeaderTooltipText& tooltip,
                                           void (*render_user_content)())
{
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
    ImGui::TextDisabled("Developer information");
    ImGui::TextUnformatted(tooltip.developer_information);
#endif
    EndTooltipStyled();
}

bool
SamplingStateRenderer::IsDisabled() const
{
    return m_load_state == LoadState::kLoading || m_load_state == LoadState::kFailed;
}

bool
SamplingStateRenderer::RenderMetricCell(const SamplingRow& row, Metric metric,
                                        CodePane pane)
{
    const SampleMetricPresentation& presentation = get_sample_metric_presentation(metric);
    const IsaDataBuilder::PercentageSummary& summary =
        get_sample_metric_summary(row, metric);
    const bool valid =
        !IsDisabled() && summary.availability == SamplingAvailability::kValid;
    const CodeCellBounds cell = get_code_cell_bounds();
    const std::string    text = IsaDataBuilder::FormatSamplePercentage(
        IsDisabled() ? IsaDataBuilder::PercentageSummary{} : summary);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(cell.start, cell.End(), true);
    if(valid)
    {
        const float  height = cell.size.y * SAMPLE_METRIC_BAR_HEIGHT_FRACTION;
        const ImVec2 start(cell.start.x, cell.End().y - height);
        draw_list->AddRectFilled(
            start, cell.End(),
            ImGui::GetColorU32(
                ThemeColor(m_settings, Colors::kTextDim, SAMPLE_METRIC_TRACK_ALPHA)));
        const float width = cell.size.x * static_cast<float>(summary.percent /
                                                             SAMPLE_METRIC_PERCENT_SCALE);
        if(width > 0.0f)
        {
            draw_list->AddRectFilled(
                start, ImVec2(start.x + width, cell.End().y),
                ImGui::GetColorU32(
                    ThemeColor(m_settings, Colors::kAccent, SAMPLE_METRIC_BAR_ALPHA)));
        }
    }
    const float text_width = ImGui::CalcTextSize(text.c_str()).x;
    draw_list->AddText(
        ImVec2(cell.start.x + std::max(0.0f, cell.size.x - text_width), cell.start.y),
        m_settings.GetColor(valid ? Colors::kTextMain : Colors::kTextDim), text.c_str());
    draw_list->PopClipRect();
    // Missing metrics remain selectable, just like the other code-row cells.
    const CodeCellInteraction interaction = finish_code_cell(cell, presentation.label);
    if(interaction.hovered)
    {
        RenderMetricTooltip(row, metric, pane);
    }
    return interaction.clicked;
}

SamplingStateRenderer::MetricTooltipText
SamplingStateRenderer::GetMetricTooltipText(const SamplingRow& row, Metric metric) const
{
    if(IsDisabled())
    {
        return m_load_state == LoadState::kLoading
                   ? MetricTooltipText{ "loading", SAMPLING_STATE_LOADING_TOOLTIP }
                   : MetricTooltipText{ "load failed", SAMPLING_STATE_FAILED_TOOLTIP };
    }
    const SamplingAvailability availability =
        get_sample_metric_summary(row, metric).availability;
    if(availability == SamplingAvailability::kValid)
    {
        return { nullptr, nullptr };
    }
    if(availability == SamplingAvailability::kInvalid)
    {
        return { "invalid",
                 "A recorded percentage is outside 0-100%, is not finite, "
                 "or the contributing sample counts exceed the supported range." };
    }
    if(row.sample_counts.total_count == 0)
    {
        return { "no samples", SAMPLING_STATE_NO_SAMPLES_TOOLTIP };
    }
    return { "not recorded", "No measurement was recorded for one or more contributing "
                             "samples. This is different from a measured zero." };
}

void
SamplingStateRenderer::RenderMetricValue(const SamplingRow& row, Metric metric)
{
    const SampleMetricPresentation& presentation = get_sample_metric_presentation(metric);
    const IsaDataBuilder::PercentageSummary& summary =
        get_sample_metric_summary(row, metric);
    const std::string value = IsaDataBuilder::FormatSamplePercentage(
        IsDisabled() ? IsaDataBuilder::PercentageSummary{} : summary);
    const MetricTooltipText tooltip = GetMetricTooltipText(row, metric);
    if(tooltip.status)
    {
        ImGui::Text("%s: %s (%s)", presentation.title, value.c_str(), tooltip.status);
    }
    else
    {
        ImGui::Text("%s: %s", presentation.title, value.c_str());
    }
}

void
SamplingStateRenderer::RenderMetricAggregation(CodePane pane)
{
    ImGui::PushStyleColor(ImGuiCol_Text, m_settings.GetColor(Colors::kTextDim));
    ImGui::TextWrapped("%s",
                       pane == CodePane::kSource
                           ? "Sample-weighted mean over this line's mapped instructions."
                           : "Sample-weighted mean for this instruction.");
    ImGui::PopStyleColor();
}

void
SamplingStateRenderer::RenderMetricTooltip(const SamplingRow& row, Metric metric,
                                           CodePane pane)
{
    BeginTooltipStyled();
    push_sample_metric_tooltip_wrap();
    RenderMetricValue(row, metric);
    const MetricTooltipText tooltip = GetMetricTooltipText(row, metric);
    if(tooltip.description)
    {
        ImGui::TextWrapped("%s", tooltip.description);
    }
    else
    {
        const std::string samples =
            IsaDataBuilder::FormatSampleCount(row.sample_counts.total_count);
        ImGui::Text(SAMPLING_STATE_SAMPLE_COUNT_TOOLTIP, samples.c_str());
    }
    if(!IsDisabled())
    {
        RenderMetricAggregation(pane);
    }
    ImGui::PopTextWrapPos();
    EndTooltipStyled();
}

void
SamplingStateRenderer::RenderMetricsSummary(const SamplingRow& row, CodePane pane)
{
    ImGui::Separator();
    RenderMetricValue(row, Metric::kWaveOccupancy);
    RenderMetricValue(row, Metric::kActiveThreads);
    if(!IsDisabled())
    {
        if(row.wave_occupancy.availability == SamplingAvailability::kInvalid ||
           row.active_threads.availability == SamplingAvailability::kInvalid)
        {
            const Metric invalid_metric =
                row.wave_occupancy.availability == SamplingAvailability::kInvalid
                    ? Metric::kWaveOccupancy
                    : Metric::kActiveThreads;
            ImGui::TextWrapped("%s",
                               GetMetricTooltipText(row, invalid_metric).description);
        }
        RenderMetricAggregation(pane);
    }
}

bool
SamplingStateRenderer::RenderCell(const SamplingRow& row, uint64_t hottest_samples,
                                  CodePane pane)
{
    if(m_selected_metric != Metric::kSamplingState)
    {
        return RenderMetricCell(row, m_selected_metric, pane);
    }
    const SampleCounts&  counts     = row.sample_counts;
    const CodeCellBounds cell       = get_code_cell_bounds();
    const float          bar_height = cell.size.y * SAMPLING_STATE_BAR_HEIGHT_FRACTION;
    const ImVec2         bar_start(cell.start.x,
                                   cell.start.y + (cell.size.y - bar_height) * 0.5f);
    const double         bar_percent =
        row.sampling.availability == SamplingAvailability::kInvalid
            ? 0.0
            : IsaDataBuilder::CalculatePercentage(counts.total_count, hottest_samples);
    const float  bar_width = cell.size.x * static_cast<float>(bar_percent / 100.0);
    const ImVec2 bar_end(bar_start.x + bar_width, bar_start.y + bar_height);
    if(!IsDisabled() && bar_width > 0.0f)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRect(cell.start, cell.End(), true);
        draw_list->AddRectFilled(bar_start, bar_end,
                                 m_settings.GetColor(Colors::kTextDim));
        if(m_load_state == LoadState::kReady &&
           row.sampling.availability == SamplingAvailability::kValid)
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
        finish_code_cell(cell, "##sampling_state_cell", IsDisabled());
    if(interaction.hovered)
    {
        RenderTooltip(row, bar_percent, pane);
    }
    return interaction.clicked;
}

const char*
SamplingStateRenderer::GetUnavailableSamplingTooltip(const SamplingRow& row)
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
SamplingStateRenderer::DrawStallReasonSegments(const SamplingRow& row,
                                               const ImVec2& start, const ImVec2& end)
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
SamplingStateRenderer::RenderTooltip(const SamplingRow& row, double bar_percent,
                                     CodePane pane)
{
    if(IsDisabled())
    {
        BeginTooltipStyled();
        push_sample_metric_tooltip_wrap();
        ImGui::TextUnformatted(m_load_state == LoadState::kLoading
                                   ? SAMPLING_STATE_LOADING_TOOLTIP
                                   : SAMPLING_STATE_FAILED_TOOLTIP);
        RenderMetricsSummary(row, pane);
        ImGui::PopTextWrapPos();
        EndTooltipStyled();
        return;
    }
    if(m_load_state != LoadState::kReady ||
       row.sampling.availability != SamplingAvailability::kValid)
    {
        const std::string total_count =
            IsaDataBuilder::FormatSampleCount(row.sample_counts.total_count);
        BeginTooltipStyled();
        push_sample_metric_tooltip_wrap();
        const char* description = m_load_state == LoadState::kTotalsOnly
                                      ? SAMPLING_STATE_TOTALS_ONLY_TOOLTIP
                                  : m_load_state == LoadState::kLoadingDetails
                                      ? SAMPLING_STATE_LOADING_DETAILS_TOOLTIP
                                      : GetUnavailableSamplingTooltip(row);
        ImGui::TextUnformatted(description);
        if(row.sampling.availability == SamplingAvailability::kInvalid &&
           m_load_state != LoadState::kReady)
        {
            ImGui::TextUnformatted(GetUnavailableSamplingTooltip(row));
        }
        ImGui::Text(SAMPLING_STATE_SAMPLE_COUNT_TOOLTIP, total_count.c_str());
        if(row.sample_counts.total_count > 0 &&
           row.sampling.availability != SamplingAvailability::kInvalid)
        {
            ImGui::Text(SAMPLING_STATE_BAR_PERCENT_TOOLTIP, bar_percent);
        }
        RenderMetricsSummary(row, pane);
        ImGui::PopTextWrapPos();
        EndTooltipStyled();
        return;
    }

    const TooltipRows        rows   = BuildTooltipRows(row);
    const TooltipTableLayout layout = CalculateTooltipLayout(rows);
    const float mouse_wheel         = layout.size.y > 0.0f && !ImGui::GetIO().KeyCtrl &&
                                      ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY)
                                          ? ImGui::GetIO().MouseWheel
                                          : 0.0f;
    BeginTooltipStyled();
    ImGui::PushID(this);
    ImGui::PushID(static_cast<int>(row.id));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + layout.size.x);
    RenderSummary(row);
    ImGui::Text(SAMPLING_STATE_BAR_PERCENT_TOOLTIP, bar_percent);
    if(layout.size.y > 0.0f)
    {
        ImGui::TextDisabled("Hover the cell and scroll to see all sampling states.");
    }
    RenderTooltipTable(rows, layout, mouse_wheel);
    RenderFooter(row);
    RenderMetricsSummary(row, pane);
    ImGui::PopTextWrapPos();
    ImGui::PopID();
    ImGui::PopID();
    EndTooltipStyled();
}

void
SamplingStateRenderer::RenderSummary(const SamplingRow& row)
{
    const uint64_t    stall_count_value = *row.sample_counts.stall_count;
    const double      stall_percent     = row.sampling.stall_percent;
    const std::string stall_count = IsaDataBuilder::FormatSampleCount(stall_count_value);
    const std::string total_count =
        IsaDataBuilder::FormatSampleCount(row.sample_counts.total_count);

    ImGui::Text(STALL_REASON_TOOLTIP_SUMMARY, stall_count.c_str(), total_count.c_str(),
                stall_percent);

    if(row.stall_reasons.empty() && !row.stall_reason_counts_overflow)
    {
        ImGui::TextDisabled(stall_count_value == 0 ? STALL_REASON_TOOLTIP_NO_STALLS
                                                   : STALL_REASON_TOOLTIP_UNAVAILABLE);
    }
}

void
SamplingStateRenderer::RenderFooter(const SamplingRow& row)
{
    if(row.stall_reason_counts_overflow)
    {
        ImGui::TextDisabled("%s", STALL_REASON_TOOLTIP_OVERFLOW);
        return;
    }
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

SamplingStateRenderer::TooltipRows
SamplingStateRenderer::BuildTooltipRows(const SamplingRow& row)
{
    TooltipRows rows;
    if(row.sampling.availability != SamplingAvailability::kValid)
    {
        return rows;
    }

    // In addition to reasons: issued, unclassified stalls, and unclassified progress.
    constexpr size_t ADDITIONAL_STATE_ROW_COUNT = 3;
    rows.reserve(row.stall_reasons.size() + ADDITIONAL_STATE_ROW_COUNT);
    const auto append = [&rows](const char* label, Colors color, std::string count_text,
                                double share) {
        TooltipRow tooltip_row{ label, color, std::move(count_text), {} };
        std::snprintf(tooltip_row.share_text, sizeof(tooltip_row.share_text), "%.1f%%",
                      share);
        rows.push_back(std::move(tooltip_row));
    };
    append(SAMPLING_STATE_ISSUED_TEXT, Colors::kTextSuccess,
           IsaDataBuilder::FormatSampleCount(*row.sample_counts.issue_count),
           row.sampling.issue_percent);
    const uint64_t total = row.sample_counts.total_count;
    for(const StallReason& reason : row.stall_reasons)
    {
        append(reason.text.c_str(), get_stall_reason_presentation(reason.kind).color,
               reason.count_text,
               IsaDataBuilder::CalculatePercentage(reason.count, total));
    }
    if(row.unclassified_stall_count > 0)
    {
        append(SAMPLING_STATE_UNCLASSIFIED_STALLS_TEXT, Colors::kTextDim,
               row.unclassified_stall_count_text,
               IsaDataBuilder::CalculatePercentage(row.unclassified_stall_count, total));
    }
    if(row.sampling.unclassified_count > 0)
    {
        append(
            SAMPLING_STATE_UNCLASSIFIED_PROGRESS_TEXT, Colors::kTextDim,
            IsaDataBuilder::FormatSampleCount(row.sampling.unclassified_count),
            IsaDataBuilder::CalculatePercentage(row.sampling.unclassified_count, total));
    }
    return rows;
}

SamplingStateRenderer::TooltipTableLayout
SamplingStateRenderer::CalculateTooltipLayout(const TooltipRows& rows)
{
    const ImGuiStyle& style         = ImGui::GetStyle();
    const ImVec2      viewport_size = ImGui::GetWindowViewport()->WorkSize;
    float count_width = ImGui::CalcTextSize(SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[2]).x;
    float state_text_width =
        ImGui::CalcTextSize(SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[1]).x;
    float share_width =
        std::max(ImGui::CalcTextSize(SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[3]).x,
                 ImGui::CalcTextSize("100.0%").x);
    for(const TooltipRow& row : rows)
    {
        count_width =
            std::max(count_width, ImGui::CalcTextSize(row.count_text.c_str()).x);
        state_text_width = std::max(state_text_width, ImGui::CalcTextSize(row.label).x);
        share_width      = std::max(share_width, ImGui::CalcTextSize(row.share_text).x);
    }
    const float color_width =
        std::max(ImGui::CalcTextSize(SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[0]).x,
                 ImGui::GetTextLineHeight());
    const float other_columns_width =
        color_width + count_width + share_width + style.CellPadding.x * 8.0f;
    const float state_width = std::min(
        { state_text_width,
          ImGui::CalcTextSize("0").x * STALL_REASON_TOOLTIP_REASON_WIDTH_IN_CHARACTERS,
          std::max(ImGui::GetTextLineHeight(),
                   viewport_size.x - style.WindowPadding.x * 2.0f -
                       style.DisplaySafeAreaPadding.x * 2.0f - other_columns_width -
                       style.ScrollbarSize) });
    float height = ImGui::GetTextLineHeight() + style.CellPadding.y * 2.0f;
    for(const TooltipRow& row : rows)
    {
        height +=
            std::max(ImGui::GetTextLineHeight(),
                     ImGui::CalcTextSize(row.label, nullptr, false, state_width).y) +
            style.CellPadding.y * 2.0f;
    }
    const float max_height = viewport_size.y * STALL_REASON_TOOLTIP_MAX_HEIGHT_FRACTION;
    const bool  scrollable = height > max_height;
    return { ImVec2(state_width + other_columns_width +
                        (scrollable ? style.ScrollbarSize : 0.0f),
                    scrollable ? max_height : 0.0f),
             state_width, color_width, count_width, share_width };
}

void
SamplingStateRenderer::RenderTooltipTable(const TooltipRows&        rows,
                                          const TooltipTableLayout& layout,
                                          float                     mouse_wheel)
{
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
                                layout.state_width);
        ImGui::TableSetupColumn(SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[2], column_flags,
                                layout.count_width);
        ImGui::TableSetupColumn(SAMPLING_STATE_TOOLTIP_COLUMN_HEADERS[3], column_flags,
                                layout.share_width);
        ImGui::TableHeadersRow();
        for(const TooltipRow& row : rows)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            render_sampling_color_swatch(row.label, m_settings.GetColor(row.color));
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", row.label);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(row.count_text.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(row.share_text);
        }
        ImGui::EndTable();
    }
}

void
SamplingStateRenderer::SetupColumn()
{
    // Reuse ImGui's column menu and append the layer retry action after the headers.
    ImGui::GetCurrentTable()->DisableDefaultContextMenu = true;
    float label_width                                   = 0.0f;
    for(const Metric metric : SAMPLING_COLUMN_METRICS)
    {
        label_width = std::max(label_width,
                               ImGui::CalcTextSize(get_sampling_column_label(metric)).x);
    }
    const float width = label_width + ImGui::GetStyle().FramePadding.x * 2.0f +
                        ImGui::GetTextLineHeight();
    ImGui::TableSetupColumn("Sampling metric",
                            ImGuiTableColumnFlags_WidthFixed |
                                ImGuiTableColumnFlags_NoHide |
                                ImGuiTableColumnFlags_NoReorder,
                            width);
}

bool
SamplingStateRenderer::RenderTableContextMenu()
{
    bool        retry = false;
    ImGuiTable* table = ImGui::GetCurrentTable();
    if(ImGui::TableBeginContextMenuPopup(table))
    {
        ImGui::TableDrawDefaultContextMenu(table, table->Flags);
        if(m_load_state == LoadState::kFailed || m_load_state == LoadState::kTotalsOnly)
        {
            ImGui::Separator();
            retry = ImGui::MenuItem("Refetch sampling data");
        }
        ImGui::EndPopup();
    }
    return retry;
}

void
SamplingStateRenderer::RenderHeader(int column, CodePane pane)
{
    if(!ImGui::TableSetColumnIndex(column))
    {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    const bool open = ImGui::BeginCombo("##sampling_metric",
                                        get_sampling_column_label(m_selected_metric));
    if(open)
    {
        for(const Metric metric : SAMPLING_COLUMN_METRICS)
        {
            const bool selected = metric == m_selected_metric;
            if(ImGui::Selectable(get_sampling_column_label(metric), selected))
            {
                m_selected_metric = metric;
            }
            if(selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    else
    {
        if(ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            ImGui::TableOpenContextMenu(column);
        }
        HeaderTooltipText tooltip = pane == CodePane::kSource
                                        ? SOURCE_SAMPLING_STATE_HEADER_TOOLTIP
                                        : SAMPLING_STATE_HEADER_TOOLTIP;
        if(m_selected_metric != Metric::kSamplingState)
        {
            const SampleMetricPresentation& presentation =
                get_sample_metric_presentation(m_selected_metric);
            tooltip = { pane == CodePane::kSource ? presentation.source_description
                                                  : presentation.isa_description,
                        presentation.developer_information };
        }
        if(IsDisabled())
        {
            tooltip.user_description = m_load_state == LoadState::kLoading
                                           ? SAMPLING_STATE_LOADING_TOOLTIP
                                           : SAMPLING_STATE_FAILED_TOOLTIP;
        }
        RenderHeaderTooltip(tooltip,
                            m_selected_metric == Metric::kSamplingState && !IsDisabled()
                                ? render_sampling_state_color_legend
                                : nullptr);
    }
    ImGui::PopStyleVar();
}

}  // namespace View
}  // namespace RocProfVis
