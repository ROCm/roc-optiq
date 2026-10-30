// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_summary_compare.h"
#include "model/rocprofvis_summary_model.h"
#include "model/rocprofvis_topology_model.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_summary_view.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_split_containers.h"
#include "widgets/rocprofvis_tab_container.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <unordered_map>

namespace RocProfVis
{
namespace View
{

constexpr const char* TOP_EVENTS_REMAINDER_NAME = "Others";
constexpr const char* MISSING_VALUE             = "-";
constexpr const char* UNKNOWN_VALUE             = "n/a";
constexpr const char* UNKNOWN_TOOLTIP =
    "Not among this trace's largest events, so its figures were folded into the "
    "remainder.";
constexpr const char* NO_EVENTS_TEXT     = "No events match the selected filters.";
constexpr const char* SELECT_EVENT_TEXT  = "Select an event to see its instances.";
constexpr const char* NOT_IN_SOURCE_TEXT = "This trace has no instances of the event.";
constexpr const char* SEPARATOR          = " \xC2\xB7 ";
constexpr const char* ONLY_IN_TEXT       = "Only in";
constexpr size_t      FIGURE_COUNT       = 2;
constexpr float       SOURCE_TILE_SHARE  = 0.3f;  // Of the top row, per source
constexpr size_t      MAX_LISTED_HOSTS   = 2;
constexpr float       COMPOSITION_BAR    = 1.25f;  // Of the frame height
constexpr ImGuiWindowFlags TILE_WINDOW_FLAGS =
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
constexpr float TABLE_SPLIT          = 0.6f;
constexpr float NAME_COLUMN_WEIGHT   = 3.0f;
constexpr float VALUE_COLUMN_WEIGHT  = 1.0f;
constexpr float CHANGE_COLUMN_WEIGHT = 1.6f;
constexpr float NAME_TOOLTIP_GLYPHS  = 24.0f;

static const char*
filter_level_name(rocprofvis_analysis_summary_aggregation_level_t level)
{
    switch(level)
    {
        case kRPVAnalysisSummaryAggregationLevelNode: return "Node";
        case kRPVAnalysisSummaryAggregationLevelProcess: return "Process";
        case kRPVAnalysisSummaryAggregationLevelThread: return "Thread";
        default: return "Processor";
    }
}

static std::string
format_duration(double ns, SettingsManager& settings)
{
    return nanosecond_to_formatted_str(
        ns, settings.GetUserSettings().unit_settings.time_format, true);
}

static std::string
format_signed_duration(double ns, SettingsManager& settings)
{
    return std::string(ns < 0.0 ? "-" : "+") + format_duration(std::abs(ns), settings);
}

static uint64_t
op_invocations(const SummaryInfo::TopOpEvents* events)
{
    return events ? events->metrics[kRPVAnalysisSummaryTopEventsMetricInvocations]
                        .numeric.uint
                  : 0;
}

static double
op_duration_ns(const SummaryInfo::TopOpEvents* events)
{
    return events ? static_cast<double>(
                        events->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                            .numeric.uint)
                  : 0.0;
}

static bool
is_truncated(const SummaryInfo::TopOpEvents* events)
{
    return events && !events->top.empty() &&
           events->top.back().name == TOP_EVENTS_REMAINDER_NAME;
}

// "Only in" and the chip of the source that has an event the other never had.
static void
render_only_in(const CompareSourceInfo& source, SettingsManager& settings)
{
    ImGui::TextDisabled("%s", ONLY_IN_TEXT);
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    RenderCompareSourceBadge(source, settings);
}

// Signed time difference from a to b in the change's color, or "No change".
static void
render_difference(double a, double b, SettingsManager& settings)
{
    const double delta = b - a;
    if(std::abs(delta) <= 0.0)
    {
        ImGui::TextDisabled("%s", MISSING_VALUE);
        return;
    }
    const ImVec4 color = ImGui::ColorConvertU32ToFloat4(settings.GetColor(
        delta > 0.0 ? Colors::kComparisonGreater : Colors::kComparisonLesser));
    ImGui::TextColored(
        color, "%s",
        (std::string(delta < 0.0 ? "-" : "+") +
         nanosecond_to_formatted_str(
             std::abs(delta), settings.GetUserSettings().unit_settings.time_format))
            .c_str());
}

// The change of one event across the sources, worded for events that only one
// of them has.
static void
render_event_change(const SummaryEventDiff& diff, const TraceDataModel& model,
                    SettingsManager& settings)
{
    const CompareSourceInfo* source_a = model.GetCompareSource(COMPARE_SOURCE_A);
    const CompareSourceInfo* source_b = model.GetCompareSource(COMPARE_SOURCE_B);
    if(!diff.Known())
    {
        ImGui::TextDisabled("%s", UNKNOWN_VALUE);
        if(ImGui::IsItemHovered())
        {
            SetTooltipStyled("%s", UNKNOWN_TOOLTIP);
        }
    }
    else if(!diff.events[COMPARE_SOURCE_A] && source_b)
    {
        render_only_in(*source_b, settings);
    }
    else if(!diff.events[COMPARE_SOURCE_B] && source_a)
    {
        render_only_in(*source_a, settings);
    }
    else
    {
        RenderCompareChange(diff.TotalNs(COMPARE_SOURCE_A),
                            diff.TotalNs(COMPARE_SOURCE_B), settings);
    }
}

bool
SummaryEventDiff::Unknown(size_t source) const
{
    return !events[source] && truncated[source];
}

bool
SummaryEventDiff::Known() const
{
    bool known = true;
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        known = known && !Unknown(source);
    }
    return known;
}

double
SummaryEventDiff::TotalNs(size_t source) const
{
    return events[source]
               ? static_cast<double>(
                     events[source]
                         ->metrics[kRPVAnalysisSummaryTopEventMetricDurationSum]
                         .numeric.uint)
               : 0.0;
}

double
SummaryEventDiff::DeltaNs() const
{
    return TotalNs(COMPARE_SOURCE_B) - TotalNs(COMPARE_SOURCE_A);
}

double
SummaryEventDiff::RelativeChange() const
{
    const double a = TotalNs(COMPARE_SOURCE_A);
    return a > 0.0 ? DeltaNs() / a : (DeltaNs() > 0.0 ? HUGE_VAL : 0.0);
}

// Pairs the sources' top events by name, largest change first. The "Others"
// remainder is left out: it holds different events on each side.
static std::vector<SummaryEventDiff>
diff_top_events(
    const std::array<const SummaryInfo::TopOpEvents*, COMPARE_SOURCE_COUNT>& sources)
{
    std::vector<SummaryEventDiff>           diffs;
    std::unordered_map<std::string, size_t> index_of;
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        if(!sources[source])
        {
            continue;
        }
        for(const SummaryInfo::TopOpEvents::Event& event : sources[source]->top)
        {
            if(event.name == TOP_EVENTS_REMAINDER_NAME)
            {
                continue;
            }
            auto it = index_of.find(event.name);
            if(it == index_of.end())
            {
                it = index_of.emplace(event.name, diffs.size()).first;
                SummaryEventDiff diff;
                diff.name = event.name;
                diff.events.fill(nullptr);
                for(size_t i = 0; i < COMPARE_SOURCE_COUNT; i++)
                {
                    diff.truncated[i] = is_truncated(sources[i]);
                }
                diffs.push_back(std::move(diff));
            }
            diffs[it->second].events[source] = &event;
        }
    }
    std::stable_sort(diffs.begin(), diffs.end(),
                     [](const SummaryEventDiff& lhs, const SummaryEventDiff& rhs) {
                         return std::abs(lhs.DeltaNs()) > std::abs(rhs.DeltaNs());
                     });
    return diffs;
}

SummaryCompareOverview::SummaryCompareOverview(
    DataProvider&                                                             dp,
    const std::array<std::shared_ptr<SummaryTopology>, COMPARE_SOURCE_COUNT>& topology,
    TabContainer& tab_container)
: m_facts({})
, m_topology(topology)
, m_data_changed(false)
, m_data_provider(dp)
, m_tab_container(tab_container)
, m_settings(SettingsManager::GetInstance())
{
    m_widget_name = GenUniqueName("summary_compare_overview");
}

void
SummaryCompareOverview::Fetched()
{
    m_data_changed = true;
}

void
SummaryCompareOverview::Update()
{
    if(!m_data_changed)
    {
        return;
    }
    const TraceDataModel& model = m_data_provider.DataModel();
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        SourceFacts facts;
        // Each source's timeline starts at zero, so its own tracks give its span.
        double start = DBL_MAX;
        double end   = 0.0;
        for(const TrackInfo* track : model.GetTimeline().GetTrackList())
        {
            if(track && track->file_id == source)
            {
                start = std::min(start, track->min_ts);
                end   = std::max(end, track->max_ts);
            }
        }
        facts.span_ns = end > start ? end - start : 0.0;

        const SummaryModel* summary = model.GetSourceSummary(source);
        if(summary)
        {
            const SummaryInfo::AggregateMetrics& root = summary->GetSummaryData();
            for(size_t category = 0; category < __kRPVAnalysisSummaryCategoryLast;
                category++)
            {
                const SummaryInfo::TopOpEvents* events = summary->TopOpEvents(
                    root, static_cast<rocprofvis_analysis_summary_category_t>(category));
                facts.category_ns[category]    = op_duration_ns(events);
                facts.category_calls[category] = op_invocations(events);
                facts.events += op_invocations(events);
            }

            std::vector<std::string>                hosts;
            std::unordered_map<std::string, size_t> gpus;
            std::string                             main_gpu;
            for(const SummaryInfo::AggregateMetrics& node : root.sub_metrics)
            {
                if(node.type != kRPVAnalysisSummaryAggregationLevelNode ||
                   !node.info.node)
                {
                    continue;
                }
                hosts.push_back(node.info.node->host_name);
                for(const SummaryInfo::AggregateMetrics& child : node.sub_metrics)
                {
                    if(child.type == kRPVAnalysisSummaryAggregationLevelProcessor &&
                       child.info.processor &&
                       child.info.processor->type == kRPVControllerProcessorTypeGPU)
                    {
                        const std::string& product = child.info.processor->product_name;
                        size_t&            count   = gpus[product];
                        count++;
                        if(main_gpu.empty() || count > gpus[main_gpu])
                        {
                            main_gpu = product;
                        }
                    }
                }
            }
            if(hosts.size() > MAX_LISTED_HOSTS)
            {
                facts.system = std::to_string(hosts.size()) + " nodes";
            }
            for(size_t i = 0; i < hosts.size() && hosts.size() <= MAX_LISTED_HOSTS; i++)
            {
                facts.system += (i > 0 ? ", " : "") + hosts[i];
            }
            if(!main_gpu.empty())
            {
                facts.system += (facts.system.empty() ? "" : SEPARATOR) +
                                std::to_string(gpus[main_gpu]) + " x " + main_gpu;
            }
        }
        m_facts[source] = std::move(facts);
    }
    m_data_changed = false;
}

void
SummaryCompareOverview::Render()
{
    // The single trace overview's tile style, so both read as the same view.
    const ImGuiStyle& style = m_settings.GetDefaultStyle();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.ChildRounding);
    ImGui::Spacing();
    const float width = ImGui::GetContentRegionAvail().x;

    // Top row: the two sources, then the figures of the whole run.
    ImGui::PushFont(nullptr, m_settings.GetFontManager().GetFontSize(FontSize::kSmall));
    const float label_height = ImGui::GetTextLineHeightWithSpacing();
    ImGui::PopFont();
    const float row_width =
        width -
        static_cast<float>(COMPARE_SOURCE_COUNT + FIGURE_COUNT - 1) * style.ItemSpacing.x;
    const float tile_height = label_height +
                              static_cast<float>(COMPARE_SOURCE_COUNT + 1) *
                                  ImGui::GetTextLineHeightWithSpacing() +
                              2.0f * style.WindowPadding.y;
    RenderSources(ImVec2(row_width * SOURCE_TILE_SHARE, tile_height));
    const ImVec2 figure_size(row_width *
                                 (1.0f - COMPARE_SOURCE_COUNT * SOURCE_TILE_SHARE) /
                                 static_cast<float>(FIGURE_COUNT),
                             tile_height);
    ImGui::SameLine();
    RenderFigure("Trace Span", m_facts[COMPARE_SOURCE_A].span_ns,
                 m_facts[COMPARE_SOURCE_B].span_ns, true, figure_size);
    ImGui::SameLine();
    RenderFigure("Tracked Events", static_cast<double>(m_facts[COMPARE_SOURCE_A].events),
                 static_cast<double>(m_facts[COMPARE_SOURCE_B].events), false,
                 figure_size);

    RenderComposition(width);

    size_t shown = 0;
    for(size_t category = 0; category < __kRPVAnalysisSummaryCategoryLast; category++)
    {
        shown += (m_facts[COMPARE_SOURCE_A].category_ns[category] > 0.0 ||
                  m_facts[COMPARE_SOURCE_B].category_ns[category] > 0.0)
                     ? 1
                     : 0;
    }
    if(shown > 0)
    {
        // Header, one line per source and the change between them.
        ImGui::PushFont(nullptr,
                        m_settings.GetFontManager().GetFontSize(FontSize::kLarge));
        const float total_height = ImGui::GetTextLineHeightWithSpacing();
        ImGui::PopFont();
        const ImVec2 tile_size(
            (width - static_cast<float>(shown - 1) * style.ItemSpacing.x) /
                static_cast<float>(shown),
            2.0f * style.WindowPadding.y + ImGui::GetFrameHeightWithSpacing() +
                static_cast<float>(COMPARE_SOURCE_COUNT) * total_height +
                ImGui::GetTextLineHeightWithSpacing());
        bool same_line = false;
        for(size_t category = 0; category < __kRPVAnalysisSummaryCategoryLast; category++)
        {
            if(m_facts[COMPARE_SOURCE_A].category_ns[category] > 0.0 ||
               m_facts[COMPARE_SOURCE_B].category_ns[category] > 0.0)
            {
                if(same_line)
                {
                    ImGui::SameLine();
                }
                RenderCategory(
                    static_cast<rocprofvis_analysis_summary_category_t>(category),
                    tile_size);
                same_line = true;
            }
        }
    }
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor();
}

void
SummaryCompareOverview::RenderSources(ImVec2 tile_size)
{
    FontManager& fonts = m_settings.GetFontManager();
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        const CompareSourceInfo* info =
            m_data_provider.DataModel().GetCompareSource(source);
        if(!info)
        {
            continue;
        }
        if(source > COMPARE_SOURCE_A)
        {
            ImGui::SameLine();
        }
        ImGui::PushID(static_cast<int>(source));
        ImGui::BeginChild("##source", tile_size, ImGuiChildFlags_AlwaysUseWindowPadding,
                          TILE_WINDOW_FLAGS);
        ImGui::PushFont(nullptr, fonts.GetFontSize(FontSize::kSmall));
        ImGui::TextDisabled("%s", COMPARE_SOURCE_ROLE[source]);
        ImGui::PopFont();
        RenderCompareSourceBadge(*info, m_settings);
        ImGui::SameLine();
        ElidedText(info->name.c_str(), ImGui::GetContentRegionAvail().x,
                   ImGui::GetFontSize() * NAME_TOOLTIP_GLYPHS);
        const std::string& detail =
            m_facts[source].system.empty() ? info->path : m_facts[source].system;
        ImGui::PushID("detail");
        ImGui::BeginDisabled();
        ElidedText(detail.c_str(), ImGui::GetContentRegionAvail().x,
                   ImGui::GetContentRegionAvail().x);
        ImGui::EndDisabled();
        ImGui::PopID();
        ImGui::EndChild();
        if(ImGui::IsItemHovered())
        {
            SetTooltipStyled("%s", info->path.c_str());
        }
        ImGui::PopID();
    }
}

void
SummaryCompareOverview::RenderFigure(const char* label, double a, double b, bool is_time,
                                     ImVec2 size)
{
    ImGui::PushID(label);
    ImGui::BeginChild("##figure", size, ImGuiChildFlags_AlwaysUseWindowPadding,
                      TILE_WINDOW_FLAGS);
    ImGui::PushFont(nullptr, m_settings.GetFontManager().GetFontSize(FontSize::kSmall));
    ImGui::TextDisabled("%s", label);
    ImGui::PopFont();
    const double values[COMPARE_SOURCE_COUNT] = { a, b };
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        ImGui::PushID(static_cast<int>(source));
        const CompareSourceInfo* info =
            m_data_provider.DataModel().GetCompareSource(source);
        if(info)
        {
            RenderCompareSourceBadge(*info, m_settings);
            ImGui::SameLine();
        }
        const std::string text =
            is_time ? format_duration(values[source], m_settings)
                    : std::to_string(static_cast<uint64_t>(values[source]));
        ElidedText(text.c_str(), ImGui::GetContentRegionAvail().x,
                   ImGui::GetFontSize() * NAME_TOOLTIP_GLYPHS);
        ImGui::PopID();
    }
    if(a == 0.0 && b == 0.0)
    {
        ImGui::TextDisabled("None in either trace");
    }
    else
    {
        RenderCompareChange(a, b, m_settings,
                            is_time ? format_signed_duration(b - a, m_settings)
                                    : std::string());
    }
    ImGui::EndChild();
    ImGui::PopID();
}

void
SummaryCompareOverview::RenderComposition(float width)
{
    // One legend for both bars. Labelling each bar would stack the callouts of
    // the two on top of each other.
    const ImGuiStyle& style      = m_settings.GetDefaultStyle();
    const float       bar_height = ImGui::GetFrameHeight() * COMPOSITION_BAR;

    std::array<std::string, COMPARE_SOURCE_COUNT> totals;
    float                                         total_width = 0.0f;
    float                                         badge_width = 0.0f;
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        double ns = 0.0;
        for(double category_ns : m_facts[source].category_ns)
        {
            ns += category_ns;
        }
        totals[source] =
            ns > 0.0 ? format_duration(ns, m_settings) : std::string("No events");
        total_width =
            std::max(total_width, ImGui::CalcTextSize(totals[source].c_str()).x);
        if(const CompareSourceInfo* info =
               m_data_provider.DataModel().GetCompareSource(source))
        {
            badge_width = std::max(badge_width, CompareSourceBadgeWidth(*info));
        }
    }

    ImGui::BeginChild("##composition", ImVec2(width, 0.0f),
                      ImGuiChildFlags_AlwaysUseWindowPadding |
                          ImGuiChildFlags_AutoResizeY,
                      TILE_WINDOW_FLAGS);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Event Composition");
    float legend_width = 0.0f;
    for(size_t category = 0; category < __kRPVAnalysisSummaryCategoryLast; category++)
    {
        legend_width += ImGui::GetTextLineHeight() + style.ItemInnerSpacing.x +
                        ImGui::CalcTextSize(SUMMARY_CATEGORY_DISPLAY[category].first).x +
                        style.ItemSpacing.x;
    }
    ImGui::SameLine(ImGui::GetContentRegionMax().x - legend_width);
    for(size_t category = 0; category < __kRPVAnalysisSummaryCategoryLast; category++)
    {
        ImGui::Dummy(ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
            m_settings.GetColorWheel()[SUMMARY_CATEGORY_DISPLAY[category].second],
            style.FrameRounding);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::TextUnformatted(SUMMARY_CATEGORY_DISPLAY[category].first);
        if(category + 1 < __kRPVAnalysisSummaryCategoryLast)
        {
            ImGui::SameLine();
        }
    }
    ImGui::NewLine();

    const float left     = ImGui::GetCursorPosX();
    const float right    = ImGui::GetContentRegionMax().x;
    const float bar_left = left + badge_width + style.ItemSpacing.x;
    const float bar_width =
        std::max(0.0f, right - bar_left - total_width - style.ItemSpacing.x);
    const float first_row = ImGui::GetCursorPosY();
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        // Rows are placed by hand, and the cursor is never left past the last item:
        // ImGui asserts on a window grown by SetCursorPos alone.
        const float row_y =
            first_row + static_cast<float>(source) * (bar_height + style.ItemSpacing.y);
        const float text_y = row_y + 0.5f * (bar_height - ImGui::GetTextLineHeight());
        ImGui::PushID(static_cast<int>(source));
        if(const CompareSourceInfo* info =
               m_data_provider.DataModel().GetCompareSource(source))
        {
            ImGui::SetCursorPos(ImVec2(left, text_y));
            RenderCompareSourceBadge(*info, m_settings);
        }
        const SummaryModel* summary =
            m_data_provider.DataModel().GetSourceSummary(source);
        ImGui::SetCursorPos(ImVec2(bar_left, row_y));
        if(m_topology[source] && summary)
        {
            m_topology[source]->CompositionBar(ImVec2(bar_width, bar_height),
                                               summary->GetSummaryData(), false);
        }
        ImGui::SetCursorPos(
            ImVec2(right - ImGui::CalcTextSize(totals[source].c_str()).x, text_y));
        ImGui::TextDisabled("%s", totals[source].c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void
SummaryCompareOverview::RenderCategory(rocprofvis_analysis_summary_category_t category,
                                       ImVec2                                 size)
{
    const ImGuiStyle& style = m_settings.GetDefaultStyle();
    FontManager&      fonts = m_settings.GetFontManager();
    ImGui::PushID(static_cast<int>(category));
    ImGui::BeginChild("##category", size, ImGuiChildFlags_AlwaysUseWindowPadding,
                      TILE_WINDOW_FLAGS);

    // The single trace preview tile's header.
    ImGui::AlignTextToFramePadding();
    ImGui::Dummy(ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin() + ImVec2(0.0f, style.FramePadding.y),
        ImGui::GetItemRectMax() + ImVec2(0.0f, style.FramePadding.y),
        m_settings.GetColorWheel()[SUMMARY_CATEGORY_DISPLAY[category].second],
        style.FrameRounding);
    ImGui::SameLine();
    ImGui::TextUnformatted(SUMMARY_CATEGORY_DISPLAY[category].first);
    ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize("Details").x -
                    2.0f * style.FramePadding.x);
    if(ImGui::Button("Details"))
    {
        m_tab_container.SetActiveTab(SUMMARY_CATEGORY_DISPLAY[category].first);
    }

    // Each source's total, as large as the single preview's, then the change.
    ImGui::PushFont(nullptr, fonts.GetFontSize(FontSize::kLarge));
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        ImGui::PushID(static_cast<int>(source));
        if(const CompareSourceInfo* info =
               m_data_provider.DataModel().GetCompareSource(source))
        {
            RenderCompareSourceBadge(*info, m_settings);
            ImGui::SameLine();
        }
        ImGui::TextUnformatted(
            format_duration(m_facts[source].category_ns[category], m_settings).c_str());
        const std::string calls = CountLabel(m_facts[source].category_calls[category],
                                             "invocation", "invocations");
        if(ImGui::GetContentRegionAvail().x >
           ImGui::CalcTextSize(calls.c_str()).x + style.ItemSpacing.x)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", calls.c_str());
        }
        ImGui::PopID();
    }
    ImGui::PopFont();
    RenderCompareChange(
        m_facts[COMPARE_SOURCE_A].category_ns[category],
        m_facts[COMPARE_SOURCE_B].category_ns[category], m_settings,
        format_signed_duration(m_facts[COMPARE_SOURCE_B].category_ns[category] -
                                   m_facts[COMPARE_SOURCE_A].category_ns[category],
                               m_settings));
    ImGui::EndChild();
    ImGui::PopID();
}

SummaryCompareTopEvents::SummaryCompareTopEvents(
    DataProvider& dp, std::shared_ptr<TimelineSelection> timeline_selection,
    rocprofvis_analysis_summary_category_t category, TableType table_type,
    rocprofvis_controller_table_type_t request_table_type, RequestType request_type)
: m_category(category)
, m_filters()
, m_op_events({})
, m_criteria(std::nullopt)
, m_rows()
, m_selected(std::nullopt)
, m_sort_column(kDifference)
, m_sort_ascending(false)
, m_instances({})
, m_instance_split(nullptr)
, m_v_container(nullptr)
, m_data_changed(false)
, m_filter_changed(false)
, m_data_provider(dp)
, m_settings(SettingsManager::GetInstance())
{
    m_widget_name = GenUniqueName("summary_compare_top_events");
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        m_instances[source] = std::make_unique<SummaryInstanceTable>(
            dp, timeline_selection, category, table_type, request_table_type,
            RequestIdBuilder::MakeClientRequestId(
                request_type, DataProvider::CompareSourceClientId(source)),
            source);
    }
    m_instance_split = MakeCompareSplit(std::make_shared<RocCustomWidget>([this]() {
                                            RenderInstances(COMPARE_SOURCE_A);
                                        }),
                                        std::make_shared<RocCustomWidget>([this]() {
                                            RenderInstances(COMPARE_SOURCE_B);
                                        }));

    const ImGuiStyle& style  = m_settings.GetDefaultStyle();
    LayoutItem::Ptr   events = LayoutItem::CreateFromWidget(
        std::make_shared<RocCustomWidget>([this]() { RenderEvents(); }));
    events->m_child_flags      = ImGuiChildFlags_AlwaysUseWindowPadding;
    events->m_window_padding   = style.WindowPadding;
    events->m_item_spacing     = style.ItemSpacing;
    events->m_inherit_bg_color = true;
    LayoutItem::Ptr instances  = LayoutItem::CreateFromWidget(m_instance_split);
    instances->m_child_flags   = ImGuiChildFlags_None;
    instances->m_item_spacing  = style.ItemSpacing;
    m_v_container              = std::make_unique<VSplitContainer>(events, instances);
    m_v_container->SetSplit(TABLE_SPLIT);

    m_filters.push_back(
        { filter_level_name(kRPVAnalysisSummaryAggregationLevelNode), 0, {} });
    switch(m_category)
    {
        case kRPVAnalysisSummaryCategoryKernel:
        {
            m_filters.push_back({ "GPU", 0, {} });
            break;
        }
        case kRPVAnalysisSummaryCategoryMemory:
        {
            m_filters.push_back(
                { filter_level_name(kRPVAnalysisSummaryAggregationLevelProcessor),
                  0,
                  {} });
            break;
        }
        case kRPVAnalysisSummaryCategoryThread:
        {
            m_filters.push_back(
                { filter_level_name(kRPVAnalysisSummaryAggregationLevelProcess), 0, {} });
            m_filters.push_back(
                { filter_level_name(kRPVAnalysisSummaryAggregationLevelThread), 0, {} });
            break;
        }
        default:
        {
            ROCPROFVIS_ASSERT(false);
            break;
        }
    }
}

SummaryCompareTopEvents::~SummaryCompareTopEvents() {}

const SummaryModel*
SummaryCompareTopEvents::SourceSummary(size_t source) const
{
    return m_data_provider.DataModel().GetSourceSummary(source);
}

void
SummaryCompareTopEvents::Fetched()
{
    m_data_changed = true;
}

bool
SummaryCompareTopEvents::Empty() const
{
    bool empty = true;
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        const SummaryModel* summary = SourceSummary(source);
        empty = empty && (!summary || op_invocations(summary->TopOpEvents(
                                          summary->GetSummaryData(), m_category)) == 0);
    }
    return empty;
}

void
SummaryCompareTopEvents::RebuildFilterLevel(size_t level)
{
    FilterLevel& filter = m_filters[level];
    filter.options.clear();
    filter.selected = 0;

    FilterOption all;
    all.label = "All";
    all.metrics.fill(nullptr);
    if(level == 0)
    {
        for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
        {
            const SummaryModel* summary = SourceSummary(source);
            all.metrics[source]         = summary ? &summary->GetSummaryData() : nullptr;
        }
    }
    else
    {
        const FilterLevel&  parent        = m_filters[level - 1];
        const FilterOption& parent_option = parent.options[parent.selected];
        all.metrics                       = parent_option.metrics;
        all.criteria                      = parent_option.criteria;
    }
    filter.options.push_back(all);

    // Below an "All" the deeper levels have nothing to choose from.
    if(level > 0 && m_filters[level - 1].selected == 0)
    {
        return;
    }

    // The sources share one topology tree, so the same node, device, process or
    // thread is the same info pointer on both sides.
    std::unordered_map<const void*, size_t> option_of;
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        const SummaryModel*                  summary = SourceSummary(source);
        const SummaryInfo::AggregateMetrics* parent  = all.metrics[source];
        if(!summary || !parent)
        {
            continue;
        }
        for(const SummaryInfo::AggregateMetrics& child : parent->sub_metrics)
        {
            bool accept = false;
            switch(child.type)
            {
                case kRPVAnalysisSummaryAggregationLevelNode:
                {
                    accept = level == 0 && child.info.node;
                    break;
                }
                case kRPVAnalysisSummaryAggregationLevelProcessor:
                {
                    accept = level == 1 && child.info.processor &&
                             (m_category == kRPVAnalysisSummaryCategoryKernel
                                  ? child.info.processor->type ==
                                        kRPVControllerProcessorTypeGPU
                                  : m_category == kRPVAnalysisSummaryCategoryMemory);
                    break;
                }
                case kRPVAnalysisSummaryAggregationLevelProcess:
                {
                    accept = level == 1 && child.info.process &&
                             m_category == kRPVAnalysisSummaryCategoryThread;
                    break;
                }
                case kRPVAnalysisSummaryAggregationLevelThread:
                {
                    accept = level == 2 && child.info.thread;
                    break;
                }
                default:
                {
                    break;
                }
            }
            if(!accept || op_invocations(summary->TopOpEvents(child, m_category)) == 0)
            {
                continue;
            }

            FilterOption option;
            option.metrics.fill(nullptr);
            rocprofvis_dm_query_criteria_t criteria =
                all.criteria.value_or(rocprofvis_dm_query_criteria_t{});
            const void* entry = nullptr;
            switch(child.type)
            {
                case kRPVAnalysisSummaryAggregationLevelNode:
                {
                    entry            = child.info.node;
                    option.label     = child.info.node->host_name;
                    criteria.node_id = child.info.node->GetId();
                    break;
                }
                case kRPVAnalysisSummaryAggregationLevelProcessor:
                {
                    entry = child.info.processor;
                    m_data_provider.DataModel().GetTopology().GetProcessorTypeLabel(
                        *child.info.processor, option.label);
                    option.label += ": " + child.info.processor->product_name;
                    criteria.agent_id = child.info.processor->GetId();
                    break;
                }
                case kRPVAnalysisSummaryAggregationLevelProcess:
                {
                    entry        = child.info.process;
                    option.label = "Process " +
                                   std::to_string(child.info.process->GetId()) + ": " +
                                   child.info.process->command;
                    criteria.process_id = child.info.process->GetId();
                    break;
                }
                default:
                {
                    entry              = child.info.thread;
                    option.label       = child.info.thread->GetName();
                    criteria.thread_id = child.info.thread->GetId();
                    break;
                }
            }
            option.criteria = criteria;
            auto it         = option_of.find(entry);
            if(it == option_of.end())
            {
                it = option_of.emplace(entry, filter.options.size()).first;
                filter.options.push_back(std::move(option));
            }
            filter.options[it->second].metrics[source] = &child;
        }
    }
}

void
SummaryCompareTopEvents::RebuildRows()
{
    // The deepest real choice decides what is compared; an "All" stops there.
    std::array<const SummaryInfo::AggregateMetrics*, COMPARE_SOURCE_COUNT> metrics =
        m_filters.front().options.front().metrics;
    m_criteria = std::nullopt;
    for(const FilterLevel& filter : m_filters)
    {
        if(filter.selected == 0 || filter.selected >= filter.options.size())
        {
            break;
        }
        metrics    = filter.options[filter.selected].metrics;
        m_criteria = filter.options[filter.selected].criteria;
    }
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        const SummaryModel* summary = SourceSummary(source);
        m_op_events[source]         = summary && metrics[source]
                                          ? summary->TopOpEvents(*metrics[source], m_category)
                                          : nullptr;
    }
    m_rows = diff_top_events(m_op_events);
    SortRows();
}

void
SummaryCompareTopEvents::SortRows()
{
    auto value = [](const SummaryEventDiff& row, size_t source,
                    rocprofvis_analysis_summary_top_event_metric_t metric) -> double {
        const SummaryInfo::TopOpEvents::Event* event = row.events[source];
        if(!event)
        {
            return 0.0;
        }
        const SummaryInfo::Value& v = event->metrics[metric];
        return v.type == SummaryInfo::Value::Type::Double
                   ? v.numeric.dbl
                   : (v.type == SummaryInfo::Value::Type::Float
                          ? static_cast<double>(v.numeric.flt)
                          : static_cast<double>(v.numeric.uint));
    };
    auto key = [this, &value](const SummaryEventDiff& row) -> double {
        switch(m_sort_column)
        {
            case kCallsA:
                return value(row, COMPARE_SOURCE_A,
                             kRPVAnalysisSummaryTopEventMetricInvocations);
            case kCallsB:
                return value(row, COMPARE_SOURCE_B,
                             kRPVAnalysisSummaryTopEventMetricInvocations);
            case kTotalA: return row.TotalNs(COMPARE_SOURCE_A);
            case kTotalB: return row.TotalNs(COMPARE_SOURCE_B);
            case kAvgA:
                return value(row, COMPARE_SOURCE_A,
                             kRPVAnalysisSummaryTopEventMetricDurationAvg);
            case kAvgB:
                return value(row, COMPARE_SOURCE_B,
                             kRPVAnalysisSummaryTopEventMetricDurationAvg);
            case kChange: return row.RelativeChange();
            default: return std::abs(row.DeltaNs());
        }
    };
    // An unknown figure is hidden in a source's "Others", not zero, so the rows that
    // lack the sorted figure go last in either direction.
    auto known = [this](const SummaryEventDiff& row) -> bool {
        switch(m_sort_column)
        {
            case kName: return true;
            case kCallsA:
            case kTotalA:
            case kAvgA: return !row.Unknown(COMPARE_SOURCE_A);
            case kCallsB:
            case kTotalB:
            case kAvgB: return !row.Unknown(COMPARE_SOURCE_B);
            default: return row.Known();
        }
    };
    std::stable_sort(
        m_rows.begin(), m_rows.end(),
        [this, &key, &known](const SummaryEventDiff& lhs, const SummaryEventDiff& rhs) {
            const bool lhs_known = known(lhs);
            if(lhs_known != known(rhs))
            {
                return lhs_known;
            }
            if(m_sort_column == kName)
            {
                return m_sort_ascending ? lhs.name < rhs.name : lhs.name > rhs.name;
            }
            return m_sort_ascending ? key(lhs) < key(rhs) : key(lhs) > key(rhs);
        });
}

void
SummaryCompareTopEvents::SelectEvent(const std::string& name)
{
    m_selected = name;
    for(const SummaryEventDiff& row : m_rows)
    {
        if(row.name != name)
        {
            continue;
        }
        for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
        {
            if(row.events[source] || row.Unknown(source))
            {
                m_instances[source]->SearchInstances(name, m_criteria);
            }
            else
            {
                m_instances[source]->Clear();
            }
        }
        break;
    }
}

void
SummaryCompareTopEvents::ClearSelection()
{
    m_selected = std::nullopt;
    for(std::unique_ptr<SummaryInstanceTable>& table : m_instances)
    {
        table->Clear();
    }
}

void
SummaryCompareTopEvents::Update()
{
    if(m_data_provider.GetState() == ProviderState::kReady)
    {
        if(m_data_changed)
        {
            for(size_t level = 0; level < m_filters.size(); level++)
            {
                RebuildFilterLevel(level);
            }
            m_filter_changed = true;
            m_data_changed   = false;
        }
        if(m_filter_changed)
        {
            RebuildRows();
            ClearSelection();
            m_filter_changed = false;
        }
    }
    for(std::unique_ptr<SummaryInstanceTable>& table : m_instances)
    {
        table->Update();
    }
}

void
SummaryCompareTopEvents::Render()
{
    if(m_v_container)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgPanel));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                            m_settings.GetDefaultStyle().FramePadding);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            m_settings.GetDefaultStyle().ItemSpacing);
        ImGui::Spacing();
        m_v_container->Render();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
    }
}

void
SummaryCompareTopEvents::RenderEvents()
{
    // The filters go under the table, where the single trace view has them.
    const float filters_y =
        ImGui::GetCursorPosY() +
        std::max(0.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeight());
    const float table_height = std::max(0.0f, filters_y - ImGui::GetCursorPosY() -
                                                  ImGui::GetStyle().ItemSpacing.y);
    if(m_rows.empty())
    {
        const ImVec2 text_size = ImGui::CalcTextSize(NO_EVENTS_TEXT);
        ImGui::SetCursorPos(
            ImVec2(ImGui::GetCursorPosX() +
                       0.5f * (ImGui::GetContentRegionAvail().x - text_size.x),
                   ImGui::GetCursorPosY() + 0.5f * (table_height - text_size.y)));
        ImGui::TextDisabled("%s", NO_EVENTS_TEXT);
    }
    else
    {
        RenderTable(table_height);
    }
    ImGui::SetCursorPosY(filters_y);
    RenderFilters();
}

void
SummaryCompareTopEvents::RenderFilters()
{
    const ImGuiStyle& style     = m_settings.GetDefaultStyle();
    float             label_sum = 0.0f;
    for(const FilterLevel& filter : m_filters)
    {
        label_sum += ImGui::CalcTextSize(filter.name).x;
    }
    const float combo_width =
        (ImGui::GetContentRegionAvail().x - label_sum -
         (static_cast<float>(m_filters.size()) * 2.0f - 1.0f) * style.ItemSpacing.x) /
        static_cast<float>(m_filters.size());
    for(size_t level = 0; level < m_filters.size(); level++)
    {
        FilterLevel& filter = m_filters[level];
        if(level > 0)
        {
            ImGui::SameLine();
        }
        ImGui::PushID(static_cast<int>(level));
        ImGui::BeginDisabled(filter.options.size() < 2);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(filter.name);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(combo_width);
        PushComboStyles();
        const char* preview = filter.selected < filter.options.size()
                                  ? filter.options[filter.selected].label.c_str()
                                  : MISSING_VALUE;
        if(ImGui::BeginCombo("##filter", preview))
        {
            for(size_t i = 0; i < filter.options.size(); i++)
            {
                ImGui::PushID(static_cast<int>(i));
                if(ImGui::Selectable(filter.options[i].label.c_str(),
                                     filter.selected == i))
                {
                    if(filter.selected != i)
                    {
                        filter.selected = i;
                        for(size_t deeper = level + 1; deeper < m_filters.size();
                            deeper++)
                        {
                            RebuildFilterLevel(deeper);
                        }
                        m_filter_changed = true;
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        PopComboStyles();
        ImGui::EndDisabled();
        ImGui::PopID();
    }
}

void
SummaryCompareTopEvents::RenderTable(float height)
{
    const TraceDataModel& model = m_data_provider.DataModel();
    const std::string     unit =
        timeformat_sufix(m_settings.GetUserSettings().unit_settings.time_format);
    std::array<std::string, kColumnCount> headers;
    headers[kName]       = "Name";
    headers[kDifference] = "Difference (" + unit + ")";
    headers[kChange]     = "Change";
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        const CompareSourceInfo* info = model.GetCompareSource(source);
        const std::string        id   = info ? info->id : COMPARE_SOURCE_LABEL[source];
        headers[kCallsA + source]     = "Calls " + id;
        headers[kTotalA + source]     = "Total " + id + " (" + unit + ")";
        headers[kAvgA + source]       = "Avg " + id + " (" + unit + ")";
    }

    ImGui::PushStyleColor(ImGuiCol_Header, m_settings.GetColor(Colors::kSelection));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                          m_settings.GetColor(Colors::kHighlightChart));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                          m_settings.GetColor(Colors::kHighlightChart));
    if(ImGui::BeginTable("##compare_events", kColumnCount,
                         ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                             ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                             ImGuiTableFlags_SizingStretchProp |
                             ImGuiTableFlags_NoSavedSettings,
                         ImVec2(0.0f, height)))
    {
        ImGui::TableSetupScrollFreeze(1, 2);
        for(int column = 0; column < kColumnCount; column++)
        {
            ImGuiTableColumnFlags flags  = ImGuiTableColumnFlags_WidthStretch;
            float                 weight = VALUE_COLUMN_WEIGHT;
            if(column == kName)
            {
                flags |= ImGuiTableColumnFlags_NoHide;
                weight = NAME_COLUMN_WEIGHT;
            }
            else if(column == kDifference)
            {
                flags |= ImGuiTableColumnFlags_DefaultSort |
                         ImGuiTableColumnFlags_PreferSortDescending;
                weight = CHANGE_COLUMN_WEIGHT;
            }
            else
            {
                flags |= ImGuiTableColumnFlags_PreferSortDescending;
            }
            ImGui::TableSetupColumn(headers[column].c_str(), flags, weight,
                                    static_cast<ImGuiID>(column));
        }
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
        for(int column = 0; column < kColumnCount; column++)
        {
            ImGui::TableSetColumnIndex(column);
            ImGui::TableHeader(headers[column].c_str());
            if(column == kDifference && ImGui::IsItemHovered())
            {
                SetTooltipStyled("Sorts by the size of the difference, either way.");
            }
            else if(column == kChange && ImGui::IsItemHovered())
            {
                SetTooltipStyled("Change of B from A. Sorting descending puts the "
                                 "largest increases first.");
            }
        }

        ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
        if(sort_specs && sort_specs->SpecsDirty && sort_specs->SpecsCount > 0)
        {
            m_sort_column = static_cast<Column>(sort_specs->Specs[0].ColumnUserID);
            m_sort_ascending =
                sort_specs->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
            SortRows();
            sort_specs->SpecsDirty = false;
        }

        RenderTotalsRow();
        for(size_t i = 0; i < m_rows.size(); i++)
        {
            RenderRow(m_rows[i], i);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleColor(3);
}

void
SummaryCompareTopEvents::RenderTotalsRow()
{
    ImGui::TableNextRow();
    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                           m_settings.GetColor(Colors::kTableHeaderBg));
    ImGui::TableSetColumnIndex(kName);
    ImGui::TextUnformatted("All events");
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        const SummaryInfo::TopOpEvents* events = m_op_events[source];
        ImGui::TableSetColumnIndex(static_cast<int>(kCallsA + source));
        ImGui::TextUnformatted(std::to_string(op_invocations(events)).c_str());
        ImGui::TableSetColumnIndex(static_cast<int>(kTotalA + source));
        ImGui::TextUnformatted(
            events ? events->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                         .str.c_str()
                   : MISSING_VALUE);
        ImGui::TableSetColumnIndex(static_cast<int>(kAvgA + source));
        const uint64_t calls = op_invocations(events);
        ImGui::TextUnformatted(
            calls > 0 ? nanosecond_to_formatted_str(
                            op_duration_ns(events) / static_cast<double>(calls),
                            m_settings.GetUserSettings().unit_settings.time_format)
                            .c_str()
                      : MISSING_VALUE);
    }
    const double a = op_duration_ns(m_op_events[COMPARE_SOURCE_A]);
    const double b = op_duration_ns(m_op_events[COMPARE_SOURCE_B]);
    ImGui::TableSetColumnIndex(kDifference);
    render_difference(a, b, m_settings);
    ImGui::TableSetColumnIndex(kChange);
    RenderCompareChange(a, b, m_settings);
}

void
SummaryCompareTopEvents::RenderRow(const SummaryEventDiff& row, size_t index)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(kName);
    ImGui::PushID(static_cast<int>(index));
    const bool   selected = m_selected && m_selected.value() == row.name;
    const ImVec2 pos      = ImGui::GetCursorPos();
    if(ImGui::Selectable("##row", selected,
                         ImGuiSelectableFlags_SpanAllColumns |
                             ImGuiSelectableFlags_AllowOverlap))
    {
        if(selected)
        {
            ClearSelection();
        }
        else
        {
            SelectEvent(row.name);
        }
    }
    ImGui::SetCursorPos(pos);
    ElidedText(row.name.c_str(), ImGui::GetContentRegionAvail().x,
               ImGui::GetContentRegionAvail().x);
    for(size_t source = 0; source < COMPARE_SOURCE_COUNT; source++)
    {
        ImGui::TableSetColumnIndex(static_cast<int>(kCallsA + source));
        RenderValueCell(row, source, kRPVAnalysisSummaryTopEventMetricInvocations);
        ImGui::TableSetColumnIndex(static_cast<int>(kTotalA + source));
        RenderValueCell(row, source, kRPVAnalysisSummaryTopEventMetricDurationSum);
        ImGui::TableSetColumnIndex(static_cast<int>(kAvgA + source));
        RenderValueCell(row, source, kRPVAnalysisSummaryTopEventMetricDurationAvg);
    }
    ImGui::TableSetColumnIndex(kDifference);
    if(row.Known())
    {
        render_difference(row.TotalNs(COMPARE_SOURCE_A), row.TotalNs(COMPARE_SOURCE_B),
                          m_settings);
    }
    else
    {
        ImGui::TextDisabled("%s", UNKNOWN_VALUE);
    }
    ImGui::TableSetColumnIndex(kChange);
    render_event_change(row, m_data_provider.DataModel(), m_settings);
    ImGui::PopID();
}

void
SummaryCompareTopEvents::RenderValueCell(
    const SummaryEventDiff& row, size_t source,
    rocprofvis_analysis_summary_top_event_metric_t metric)
{
    const SummaryInfo::TopOpEvents::Event* event = row.events[source];
    if(event)
    {
        ImGui::TextUnformatted(event->metrics[metric].str.c_str());
    }
    else if(row.Unknown(source))
    {
        ImGui::TextDisabled("%s", UNKNOWN_VALUE);
        if(ImGui::IsItemHovered())
        {
            SetTooltipStyled("%s", UNKNOWN_TOOLTIP);
        }
    }
    else
    {
        ImGui::TextDisabled("%s", MISSING_VALUE);
    }
}

void
SummaryCompareTopEvents::RenderInstances(size_t source)
{
    const CompareSourceInfo* info = m_data_provider.DataModel().GetCompareSource(source);
    if(!info)
    {
        return;
    }
    SummaryInstanceTable& table = *m_instances[source];
    BeginCompareCard("##instances", m_settings);
    std::string summary;
    if(m_selected && table.Searched())
    {
        summary = table.Loading()
                      ? std::string("Loading...")
                      : CountLabel(table.InstanceCount(), "instance", "instances");
    }
    RenderCompareCardTitle(*info, m_settings, summary);

    const char* placeholder = nullptr;
    if(!m_selected)
    {
        placeholder = SELECT_EVENT_TEXT;
    }
    else if(!table.Searched())
    {
        placeholder = NOT_IN_SOURCE_TEXT;
    }
    if(placeholder)
    {
        ImGui::SetCursorPos(
            ImGui::GetCursorPos() +
            (ImGui::GetContentRegionAvail() - ImGui::CalcTextSize(placeholder)) * 0.5f);
        ImGui::TextDisabled("%s", placeholder);
    }
    else
    {
        table.RenderTable();
    }
    EndCompareCard();
}

}  // namespace View
}  // namespace RocProfVis
