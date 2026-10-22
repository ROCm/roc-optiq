// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_summary_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "implot/implot.h"
#include "model/rocprofvis_common_defs.h"
#include "model/rocprofvis_model_types.h"
#include "model/rocprofvis_topology_model.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_split_containers.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace RocProfVis
{
namespace View
{

constexpr float       IMPLOT_LEGEND_ICON_SHRINK       = 2.0f;  // Implot_internal.h
constexpr double      PIE_CHART_RADIUS                = 1.0;
constexpr double      BAR_CHART_THICKNESS             = 0.67;
constexpr ImVec2      CHART_FIT_PADDING               = ImVec2(0.1f, 0.1f);
constexpr float       FILTER_COMBO_RELATIVE_MIN_WIDTH = 17.0f;
constexpr float       INITIAL_RELATIVE_SIZE           = 0.8f;
constexpr float       INITIAL_RELATIVE_SIZE_LOADING   = 0.2f;
constexpr ImVec2      MIN_SIZE_RELATIVE               = ImVec2(34.0f, 12.0f);
constexpr const char* SIGMA                           = u8"\u03C3";
constexpr std::pair<const char*, size_t> DISPLAY_PROPS_EVENT_TYPES[] = {
    { "Kernel Executions", 0 },
    { "Memory Operations", 2 },
    { "Thread Operations", 3 }
};
constexpr const char* DISPLAY_NAMES_TOP_EVENT_TYPES[] = {
    "Top Kernels by Execution Time", "Top Memory Operations by Execution Time",
    "Top Thread Operations by Execution Time"
};
constexpr const char* DISPLAY_NAMES_METRIC_TYPES[] = { "Trace", "Node", "Processor",
                                                       "Process", "Thread" };

SummaryView::SummaryView(DataProvider&                      dp,
                         std::shared_ptr<TimelineSelection> timeline_selection)
: m_tab_container(nullptr)
, m_overview(nullptr)
, m_topology(nullptr)
, m_top_events({})
, m_fetched_revision(0)
, m_data_changed(false)
, m_init_window(true)
, m_progress(0.0f)
, m_data_provider(dp)
, m_settings(SettingsManager::GetInstance())
, m_time_format_changed_token(EventManager::InvalidSubscriptionToken)
, m_progress_token(EventManager::InvalidSubscriptionToken)
{
    m_tab_container = std::make_unique<TabContainer>();
    m_topology      = std::make_shared<SummaryTopology>(m_data_provider);
    m_top_events[kRPVAnalysisSummaryCategoryKernel] = std::make_shared<SummaryTopEvents>(
        m_data_provider, timeline_selection, kRPVAnalysisSummaryCategoryKernel,
        TableType::kSummaryKernelTable, kRPVControllerTableTypeSummaryKernelInstances,
        DataProvider::SUMMARY_KERNEL_INSTANCE_TABLE_REQUEST_ID);
    m_top_events[kRPVAnalysisSummaryCategoryMemory] = std::make_shared<SummaryTopEvents>(
        m_data_provider, timeline_selection, kRPVAnalysisSummaryCategoryMemory,
        TableType::kSummaryMemoryTable, kRPVControllerTableTypeSummaryMemoryInstances,
        DataProvider::SUMMARY_MEMORY_INSTANCE_TABLE_REQUEST_ID);
    m_top_events[kRPVAnalysisSummaryCategoryThread] = std::make_shared<SummaryTopEvents>(
        m_data_provider, timeline_selection, kRPVAnalysisSummaryCategoryThread,
        TableType::kSummaryThreadTable, kRPVControllerTableTypeSummaryThreadInstances,
        DataProvider::SUMMARY_THREAD_INSTANCE_TABLE_REQUEST_ID);
    m_overview = std::make_shared<SummaryOverview>(m_data_provider, m_topology,
                                                   m_top_events, *m_tab_container.get());
    m_data_provider.SetSummaryDataReadyCallback([this]() {
        m_topology->Fetched();
        for(std::shared_ptr<SummaryTopEvents>& top_events : m_top_events)
        {
            top_events->Fetched();
        }
        m_data_changed = true;
    });
    m_time_format_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged),
        [this](std::shared_ptr<RocEvent> e) {
            (void) e;
            m_data_provider.DataModel().GetSummary().UpdateValueStrings();
            m_topology->TimeFormatChanged();
            for(std::shared_ptr<SummaryTopEvents>& top_events : m_top_events)
            {
                top_events->TimeFormatChanged();
            }
        });
    m_progress_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kRequestProgressUpdate),
        [this](std::shared_ptr<RocEvent> e) {
            std::shared_ptr<RequestProgressUpdateEvent> event =
                std::dynamic_pointer_cast<RequestProgressUpdateEvent>(e);
            if(event && event->GetRequestType() == RequestType::kFetchSummary &&
               event->GetSourceId() == m_data_provider.GetTraceFilePath())
            {
                m_progress = event->GetProgressPercent() / 100.0f;
            }
        });
}

SummaryView::~SummaryView()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged), m_time_format_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kRequestProgressUpdate), m_progress_token);
}

void
SummaryView::Update()
{
    if(m_data_provider.GetState() == ProviderState::kReady)
    {
        if(m_settings.GetAppWindowSettings().show_summary &&
           m_fetched_revision < m_data_provider.DataModel().GetTopology().GetRevision() &&
           !m_data_provider.IsRequestPending(DataProvider::SUMMARY_REQUEST_ID))
        {
            if(m_fetched_revision <
               m_data_provider.DataModel().GetSummary().GetRevision())
            {
                m_fetched_revision =
                    m_data_provider.DataModel().GetSummary().GetRevision();
            }
            else if(m_data_provider.FetchSummary())
            {
                m_fetched_revision =
                    m_data_provider.DataModel().GetTopology().GetRevision();
                m_progress = 0.0f;
            }
        }
    }
    if(m_data_changed)
    {
        while(!m_tab_container->GetTabs().empty())
        {
            m_tab_container->RemoveTab(0);
        }
        for(size_t i = 0; i < m_top_events.size(); i++)
        {
            if(m_top_events[i] && !m_top_events[i]->Empty())
            {
                if(m_tab_container->GetTabs().empty())
                {
                    if(m_overview)
                    {
                        m_tab_container->AddTab(
                            RocWidget::CreateTabItem("Overview", "overview", m_overview));
                    }
                    if(m_topology)
                    {
                        m_tab_container->AddTab(RocWidget::CreateTabItem(
                            "System Topology", "topology", m_topology));
                    }
                }
                m_tab_container->AddTab(RocWidget::CreateTabItem(
                    DISPLAY_PROPS_EVENT_TYPES[i].first,
                    DISPLAY_PROPS_EVENT_TYPES[i].first, m_top_events[i]));
            }
        }
        m_init_window  = true;
        m_data_changed = false;
    }
    if(m_tab_container)
    {
        m_tab_container->Update();
    }
}

void
SummaryView::Render()
{
    if(SettingsManager::GetInstance().GetAppWindowSettings().show_summary &&
       m_tab_container)
    {
        bool loading = m_fetched_revision <
                           m_data_provider.DataModel().GetTopology().GetRevision() ||
                       m_data_provider.IsRequestPending(DataProvider::SUMMARY_REQUEST_ID);
        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        if(m_init_window)
        {
            ImVec2 window_size = ImGui::GetWindowSize();
            ImVec2 min_size =
                MinSize() + ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing() * 2.0f);
            ImVec2 pref_size = PreferredSize();
            ImGui::SetNextWindowPos(ImGui::GetWindowPos() + window_size * 0.5f,
                                    ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(
                loading ? ImVec2(ImGui::GetFrameHeightWithSpacing() * 8.0f,
                                 ImGui::GetFrameHeightWithSpacing() * 2.0f)
                        : ImVec2(window_size.x < min_size.x
                                     ? window_size.x
                                     : std::clamp(window_size.x * 0.8f, min_size.x,
                                                  pref_size.x),
                                 window_size.y < min_size.y
                                     ? window_size.y
                                     : std::clamp(window_size.y * 0.8f, min_size.y,
                                                  pref_size.y)));
            m_init_window = false;
        }
        if(loading)
        {
            ImGui::Begin(
                "Analyzing Trace...", &m_settings.GetAppWindowSettings().show_summary,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                    ImGuiWindowFlags_NoResize);
            ImGui::ProgressBar(m_progress, ImVec2(-FLT_MIN, -FLT_MIN), "");
            ImGui::SetCursorScreenPos(
                ImGui::GetItemRectMin() +
                ImVec2(0.0f, ImGui::GetItemRectSize().y * 0.5f - 5.0f));
            RenderLoadingIndicator(
                SettingsManager::GetInstance().GetColor(Colors::kTextMain), nullptr,
                kCenterHorizontal);
        }
        else
        {
            ImGui::Begin("Summary", &m_settings.GetAppWindowSettings().show_summary,
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::SetNextWindowSizeConstraints(MinSize(), ImVec2(FLT_MAX, FLT_MAX));
            ImGui::BeginChild(
                "summary_clamped_view", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            if(m_tab_container->GetTabs().empty() ||
               m_data_provider.DataModel().GetSummary().GetRevision() <
                   m_data_provider.DataModel().GetTopology().GetRevision())
            {
                ImGui::SetCursorPos((ImGui::GetContentRegionAvail() -
                                     ImGui::CalcTextSize("No data available.")) *
                                    0.5f);
                ImGui::TextDisabled("No data available.");
            }
            else
            {
                m_tab_container->Render();
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

ImVec2
SummaryView::MinSize() const
{
    const ImGuiStyle& style = m_settings.GetDefaultStyle();
    return ImVec2(3.0f * (ImGui::GetTextLineHeight() +
                          ImGui::CalcTextSize(DISPLAY_PROPS_EVENT_TYPES[1].first).x +
                          ImGui::CalcTextSize("Details").x +
                          2.0f * (style.FramePadding.x + style.WindowPadding.x +
                                  style.ItemSpacing.x)) +
                      2.0f * (style.WindowPadding.x + style.ItemSpacing.x),
                  12.0f * ImGui::GetFrameHeightWithSpacing());
}

ImVec2
SummaryView::PreferredSize() const
{
    return MinSize() * 2.0f;
}

SummaryOverview::SummaryOverview(
    DataProvider& dp, std::shared_ptr<SummaryTopology> topology,
    const std::array<std::shared_ptr<SummaryTopEvents>,
                     __kRPVAnalysisSummaryCategoryLast>& top_events,
    TabContainer&                                        tab_container)
: m_tab_container(tab_container)
, m_topology(topology)
, m_top_events(top_events)
, m_data_provider(dp)
, m_settings(SettingsManager::GetInstance())
{}

void
SummaryOverview::Render()
{
    const ImGuiStyle&                    style = m_settings.GetDefaultStyle();
    const SummaryModel&                  model = m_data_provider.DataModel().GetSummary();
    const SummaryInfo::AggregateMetrics& metrics = model.GetSummaryData();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, m_settings.GetColor(Colors::kBorderColor));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        m_settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        m_settings.GetDefaultStyle().ItemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        m_settings.GetDefaultStyle().ChildRounding);
    ImGui::Spacing();
    const ImVec2        region   = ImGui::GetContentRegionAvail();
    const TopologyTree& topology = m_data_provider.DataModel().GetTopology();
    uint8_t             tile_count =
        3 + (topology.GetGPUCount() ? 1 : 0) + (topology.GetCPUCount() ? 1 : 0);
    ImVec2 tile_size =
        ImVec2((region.x - (static_cast<float>(tile_count) - 1) * style.ItemSpacing.x) /
                   static_cast<float>(tile_count),
               2.0f * style.FramePadding.y);
    ImGui::PushFont(nullptr, m_settings.GetFontManager().GetFontSize(FontSize::kSmall));
    tile_size.y += 2.0f * ImGui::GetTextLineHeightWithSpacing();
    ImGui::PopFont();
    ImGui::PushFont(nullptr, m_settings.GetFontManager().GetFontSize(FontSize::kLarge));
    tile_size.y += ImGui::GetTextLineHeightWithSpacing();
    ImGui::PopFont();
    Tile(1, tile_size, "Trace Span",
         nanosecond_to_formatted_str(
             m_data_provider.DataModel().GetTimeline().GetEndTime() -
                 m_data_provider.DataModel().GetTimeline().GetStartTime(),
             m_settings.GetUserSettings().unit_settings.time_format, true)
             .c_str(),
         nullptr);
    ImGui::SameLine();
    Tile(2, tile_size, "Tracked Events",
         std::to_string(
             model.GetSummaryData()
                 .gpu.dispatch.metrics[kRPVAnalysisSummaryTopEventsMetricInvocations]
                 .numeric.uint +
             model.GetSummaryData()
                 .memory.metrics[kRPVAnalysisSummaryTopEventsMetricInvocations]
                 .numeric.uint +
             model.GetSummaryData()
                 .thread.metrics[kRPVAnalysisSummaryTopEventsMetricInvocations]
                 .numeric.uint)
             .c_str(),
         nullptr);
    ImGui::SameLine();
    Tile(3, tile_size, "Nodes", std::to_string(model.GetSummaryNodes().size()).c_str(),
         [&model]() -> void {
             if(model.GetSummaryNodes().size())
             {
                 ImGui::BeginDisabled();
                 ElidedText(model.GetSummaryNodes()[0].info.node->host_name.c_str(),
                            ImGui::GetContentRegionAvail().x,
                            ImGui::GetContentRegionAvail().x);
                 ImGui::EndDisabled();
             }
         });
    if(topology.GetGPUCount())
    {
        ImGui::SameLine();
        Tile(4, tile_size, "GPUs", std::to_string(topology.GetGPUCount()).c_str(),
             [&topology]() -> void {
                 if(topology.GetGPUCount() && topology.GetMostNumerousGPU()->first.size())
                 {
                     ImGui::BeginDisabled();
                     ImGui::Text("%llu x ", topology.GetMostNumerousGPU()->second.size());
                     ImGui::SameLine(0.0f, 0.0f);
                     ElidedText(topology.GetMostNumerousGPU()->first.data(),
                                ImGui::GetContentRegionAvail().x,
                                ImGui::GetContentRegionAvail().x);
                     ImGui::EndDisabled();
                 }
             });
    }
    if(topology.GetCPUCount())
    {
        ImGui::SameLine();
        Tile(5, tile_size, "CPUs", std::to_string(topology.GetCPUCount()).c_str(),
             [&topology]() -> void {
                 if(topology.GetCPUCount() && topology.GetMostNumerousCPU()->first.size())
                 {
                     ImGui::BeginDisabled();
                     ImGui::Text("%llu x ", topology.GetMostNumerousCPU()->second.size());
                     ImGui::SameLine(0.0f, 0.0f);
                     ElidedText(topology.GetMostNumerousCPU()->first.data(),
                                ImGui::GetContentRegionAvail().x,
                                ImGui::GetContentRegionAvail().x);
                     ImGui::EndDisabled();
                 }
             });
    }
    tile_size.x = region.x;
    tile_size.y = 4.0f * ImGui::GetFrameHeightWithSpacing();
    Tile(6, tile_size, [this, &model]() -> void { m_topology->RenderPreview(); });
    tile_count = 0 + (!m_top_events[kRPVAnalysisSummaryCategoryKernel]->Empty() ? 1 : 0) +
                 (!m_top_events[kRPVAnalysisSummaryCategoryMemory]->Empty() ? 1 : 0) +
                 (!m_top_events[kRPVAnalysisSummaryCategoryThread]->Empty() ? 1 : 0);
    tile_size.x =
        (region.x - (static_cast<float>(tile_count) - 1) * style.ItemSpacing.x) /
        static_cast<float>(std::max(static_cast<uint8_t>(1), tile_count));
    tile_size.y    = ImGui::GetContentRegionAvail().y;
    bool same_line = false;
    for(size_t i = 0; i < m_top_events.size(); i++)
    {
        if(m_top_events[i] && !m_top_events[i]->Empty())
        {
            if(same_line)
            {
                ImGui::SameLine();
            }
            Tile(static_cast<ImGuiID>(7 + i), tile_size, [this, i]() -> void {
                if(m_top_events[i]->RenderPreview())
                {
                    m_tab_container.SetActiveTab(DISPLAY_PROPS_EVENT_TYPES[i].first);
                }
            });
            same_line = true;
        }
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void
SummaryOverview::Tile(ImGuiID id, ImVec2 size, const char* header, const char* body,
                      std::function<void()> footer) const
{
    Tile(id, size, [this, header, body, footer]() -> void {
        ImGui::PushFont(nullptr,
                        m_settings.GetFontManager().GetFontSize(FontSize::kSmall));
        if(header)
        {
            ImGui::TextDisabled(header);
        }
        ImGui::PushFont(nullptr,
                        m_settings.GetFontManager().GetFontSize(FontSize::kLarge));
        if(body)
        {
            ImGui::TextUnformatted(body);
        }
        ImGui::PopFont();
        if(footer)
        {
            footer();
        }
        ImGui::PopFont();
    });
}

void
SummaryOverview::Tile(ImGuiID id, ImVec2 size, std::function<void()> content) const
{
    ImGui::BeginChild(id, size,
                      size.y == 0.0 ? ImGuiChildFlags_AlwaysUseWindowPadding |
                                          ImGuiChildFlags_AutoResizeY
                                    : ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if(content)
    {
        content();
    }
    ImGui::EndChild();
}

SummaryTopology::SummaryTopology(DataProvider& dp)
: m_h_container(nullptr)
, m_composition_table_headers({})
, m_counters(nullptr)
, m_data_changed(false)
, m_timeformat_changed(true)
, m_data_provider(dp)
, m_settings(SettingsManager::GetInstance())
{
    LayoutItem::Ptr topology = LayoutItem::CreateFromWidget(
        std::make_shared<RocCustomWidget>([this]() { RenderTopology(); }));
    topology->m_child_flags      = ImGuiChildFlags_AlwaysUseWindowPadding;
    topology->m_window_padding   = m_settings.GetDefaultStyle().WindowPadding;
    topology->m_item_spacing     = m_settings.GetDefaultStyle().ItemSpacing;
    topology->m_inherit_bg_color = true;
    LayoutItem::Ptr counters     = LayoutItem::CreateFromWidget(
        std::make_shared<RocCustomWidget>([this]() { RenderCounters(); }));
    counters->m_child_flags      = ImGuiChildFlags_AlwaysUseWindowPadding;
    counters->m_window_padding   = m_settings.GetDefaultStyle().WindowPadding;
    counters->m_item_spacing     = m_settings.GetDefaultStyle().ItemSpacing;
    counters->m_inherit_bg_color = true;
    m_h_container                = std::make_unique<HSplitContainer>(topology, counters);
    m_h_container->SetSplit(0.7f);
    m_h_container->SetSplitterSize(6.0f);
    m_h_container->SetSplitSpacing(m_settings.GetDefaultStyle().ItemSpacing.x);
}

void
SummaryTopology::Update()
{
    if(m_data_changed)
    {
        m_counters     = m_data_provider.DataModel()
                                     .GetSummary()
                                     .GetSummaryData()
                                     .gpu.counters.list.size() ||
                                 m_data_provider.DataModel()
                                     .GetSummary()
                                     .GetSummaryData()
                                     .cpu.counters.list.size()
                             ? &m_data_provider.DataModel().GetSummary().GetSummaryData()
                             : nullptr;
        m_data_changed = false;
    }
    if(m_timeformat_changed)
    {
        TimeFormat timeformat = m_settings.GetUserSettings().unit_settings.time_format;
        m_composition_table_headers = { "##label",
                                        "Counters",
                                        "Event Composition",
                                        "Kernels (" + timeformat_sufix(timeformat) + ")",
                                        "Memory (" + timeformat_sufix(timeformat) + ")",
                                        "Launch (" + timeformat_sufix(timeformat) + ")" };
        m_timeformat_changed        = false;
    }
}

void
SummaryTopology::Render()
{
    if(m_h_container)
    {
        const ImGuiStyle& style = m_settings.GetDefaultStyle();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgPanel));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                            m_settings.GetDefaultStyle().FramePadding);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            m_settings.GetDefaultStyle().ItemSpacing);
        ImGui::Spacing();
        float min_split = ImGui::CalcTextSize("System Topology").x + style.ItemSpacing.x;
        for(size_t i = 0; i < static_cast<size_t>(__kRPVAnalysisSummaryCategoryLast); i++)
        {
            min_split += 2.0f * style.ItemSpacing.x + ImGui::GetTextLineHeight() +
                         ImGui::CalcTextSize(DISPLAY_PROPS_EVENT_TYPES[i].first).x;
        }
        m_h_container->SetMinLeftWidth(min_split);
        m_h_container->SetMinRightWidth(2.0f * ImGui::CalcTextSize("Max: XXXXXXXX").x +
                                        style.ItemSpacing.x);
        m_h_container->Render();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
}

void
SummaryTopology::TimeFormatChanged()
{
    m_timeformat_changed = true;
}

void
SummaryTopology::Fetched()
{
    m_data_changed = true;
}

bool
SummaryTopology::RenderPreview() const
{
    ImGui::TextUnformatted("Event Composition");
    CompositionBar(ImGui::GetContentRegionAvail(),
                   m_data_provider.DataModel().GetSummary().GetSummaryData(), true);
    return false;
}

void
SummaryTopology::RenderTopology()
{
    const ImGuiStyle&   style = m_settings.GetDefaultStyle();
    const SummaryModel& model = m_data_provider.DataModel().GetSummary();
    ImGui::TextUnformatted("System Topology");
    float legend_pos = ImGui::GetContentRegionMax().x;
    for(size_t i = 0; i < static_cast<size_t>(__kRPVAnalysisSummaryCategoryLast); i++)
    {
        legend_pos -= ((i > 0) ? 2.0f : 1.0f) * style.ItemSpacing.x +
                      ImGui::GetTextLineHeight() +
                      ImGui::CalcTextSize(DISPLAY_PROPS_EVENT_TYPES[i].first).x;
    }
    ImGui::SameLine(legend_pos);
    for(size_t i = 0; i < static_cast<size_t>(__kRPVAnalysisSummaryCategoryLast); i++)
    {
        if(i > 0)
        {
            ImGui::SameLine();
        }
        ImGui::Dummy(ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
            m_settings.GetColorWheel()[DISPLAY_PROPS_EVENT_TYPES[i].second],
            style.FrameRounding);
        ImGui::SameLine();
        ImGui::TextUnformatted(DISPLAY_PROPS_EVENT_TYPES[i].first);
    }
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                          ThemeColor(m_settings, Colors::kTableHeaderBg));
    if(ImGui::BeginTable("topology", m_composition_table_headers.size(),
                         ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoBordersInBody |
                             ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX |
                             ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupScrollFreeze(3, 1);
        ImGui::TableSetupColumn(m_composition_table_headers[0].c_str());
        ImGui::TableSetupColumn(
            m_composition_table_headers[1].c_str(), ImGuiTableColumnFlags_WidthFixed,
            ImGui::CalcTextSize(m_composition_table_headers[1].c_str()).x);
        ImGui::TableSetupColumn(m_composition_table_headers[2].c_str());
        ImGui::TableSetupColumn(
            m_composition_table_headers[3].c_str(), ImGuiTableColumnFlags_WidthFixed,
            std::max(ImGui::CalcTextSize(m_composition_table_headers[3].c_str()).x,
                     ImGui::CalcTextSize(
                         model.GetSummaryData()
                             .gpu.dispatch
                             .metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                             .str.c_str())
                         .x));
        ImGui::TableSetupColumn(
            m_composition_table_headers[4].c_str(), ImGuiTableColumnFlags_WidthFixed,
            std::max(
                ImGui::CalcTextSize(m_composition_table_headers[4].c_str()).x,
                ImGui::CalcTextSize(
                    model.GetSummaryData()
                        .memory.metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                        .str.c_str())
                    .x));
        ImGui::TableSetupColumn(
            m_composition_table_headers[5].c_str(), ImGuiTableColumnFlags_WidthFixed,
            std::max(
                ImGui::CalcTextSize(m_composition_table_headers[5].c_str()).x,
                ImGui::CalcTextSize(
                    model.GetSummaryData()
                        .thread.metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                        .str.c_str())
                    .x));
        ImGui::TableHeadersRow();
        CompositionTable(model.GetSummaryData(), ImGui::GetWindowWidth() * 0.4f);
        ImGui::EndTable();
    }
    ImGui::PopStyleColor();
}

void
SummaryTopology::RenderCounters() const
{
    ImGui::TextUnformatted("Counters");
    if(m_counters)
    {
        ImGui::BeginDisabled();
        switch(m_counters->type)
        {
            case kRPVAnalysisSummaryAggregationLevelTrace:
            {
                ImGui::SameLine();
                ImGui::TextUnformatted(DISPLAY_NAMES_METRIC_TYPES[static_cast<size_t>(
                    m_counters->type - __kRPVAnalysisSummaryAggregationLevelFirst)]);
                break;
            }
            case kRPVAnalysisSummaryAggregationLevelNode:
            {
                ImGui::SameLine();
                ImGui::Text(
                    "%s: ",
                    DISPLAY_NAMES_METRIC_TYPES[static_cast<size_t>(
                        m_counters->type - __kRPVAnalysisSummaryAggregationLevelFirst)]);
                ImGui::SameLine(0.0f, 0.0f);
                ElidedText(m_counters->info.node->GetName().c_str(),
                           ImGui::GetContentRegionAvail().x, ImGui::GetWindowWidth());
                break;
            }
            case kRPVAnalysisSummaryAggregationLevelProcessor:
            {
                ImGui::SameLine();
                ImGui::Text(
                    "%s%llu: ",
                    m_data_provider.DataModel().GetTopology().GetProcessorTypeName(
                        m_counters->info.processor->type),
                    m_counters->info.processor->type_index);
                ImGui::SameLine(0.0f, 0.0f);
                ElidedText(m_counters->info.processor->GetName().c_str(),
                           ImGui::GetContentRegionAvail().x, ImGui::GetWindowWidth());
                break;
            }
            default:
            {
                break;
            }
        }
        ImGui::EndDisabled();
    }
    ImGui::BeginChild("sv");
    Counters();
    ImGui::EndChild();
}

void
SummaryTopology::CompositionBar(ImVec2 size, const SummaryInfo::AggregateMetrics& metrics,
                                bool labels) const
{
    const ImGuiStyle&   style = m_settings.GetDefaultStyle();
    const SummaryModel& model = m_data_provider.DataModel().GetSummary();
    uint64_t            total_duration =
        model.TopOpEvents(metrics, kRPVAnalysisSummaryCategoryKernel)
            ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
            .numeric.uint +
        model.TopOpEvents(metrics, kRPVAnalysisSummaryCategoryMemory)
            ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
            .numeric.uint +
        model.TopOpEvents(metrics, kRPVAnalysisSummaryCategoryThread)
            ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
            .numeric.uint;
    if(total_duration > 0)
    {
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        ImGui::PushFont(nullptr,
                        m_settings.GetFontManager().GetFontSize(FontSize::kXSmall));
        float label_band =
            labels ? ImGui::GetTextLineHeight() + style.ItemSpacing.y : 0.0f;
        ImGui::PopFont();
        ImVec2 bar_pos   = ImVec2(cursor_pos.x, cursor_pos.y + label_band);
        ImVec2 bar_size  = ImVec2(size.x, size.y - 2.0f * label_band);
        float  segment_x = bar_pos.x;
        CompositionBarSegment(
            bar_pos, bar_size,
            static_cast<float>(
                model.TopOpEvents(metrics, kRPVAnalysisSummaryCategoryKernel)
                    ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                    .numeric.uint) /
                total_duration,
            kRPVAnalysisSummaryCategoryKernel, labels, segment_x);
        CompositionBarSegment(
            bar_pos, bar_size,
            static_cast<float>(
                model.TopOpEvents(metrics, kRPVAnalysisSummaryCategoryMemory)
                    ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                    .numeric.uint) /
                total_duration,
            kRPVAnalysisSummaryCategoryMemory, labels, segment_x);
        CompositionBarSegment(
            bar_pos, bar_size,
            static_cast<float>(
                model.TopOpEvents(metrics, kRPVAnalysisSummaryCategoryThread)
                    ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                    .numeric.uint) /
                total_duration,
            kRPVAnalysisSummaryCategoryThread, labels, segment_x);
        ImGui::SetCursorScreenPos(cursor_pos);
        ImGui::Dummy(size);
    }
}

void
SummaryTopology::CompositionBarSegment(ImVec2 bar_pos, ImVec2 bar_size, float segment_pct,
                                       rocprofvis_analysis_summary_category_t category,
                                       bool labels, float& segment_x) const
{
    const ImGuiStyle& style         = m_settings.GetDefaultStyle();
    float             segment_width = bar_size.x * segment_pct;
    if(segment_width > 0.0f)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(
            ImVec2(segment_x, bar_pos.y),
            ImVec2(segment_x + segment_width, bar_pos.y + bar_size.y),
            m_settings.GetColorWheel()[DISPLAY_PROPS_EVENT_TYPES[category].second],
            style.FrameRounding);
        if(labels)
        {
            float label_width =
                ImGui::CalcTextSize(DISPLAY_PROPS_EVENT_TYPES[category].first).x +
                ImGui::CalcTextSize(" 100.0%").x;
            bool label_fits = label_width + 2.0f * style.FramePadding.x <= segment_width;
            if(label_fits)
            {
                ImGui::SetCursorScreenPos(
                    ImVec2(segment_x + style.FramePadding.x,
                           bar_pos.y + 0.5f * (bar_size.y - ImGui::GetTextLineHeight())));
            }
            else
            {
                float edge_y   = category % 2 == 0 ? bar_pos.y + bar_size.y : bar_pos.y;
                float center_x = segment_x + 0.5f * segment_width;
                draw_list->AddLine(ImVec2(center_x, edge_y),
                                   ImVec2(center_x, category % 2 == 0
                                                        ? edge_y + style.ItemSpacing.y
                                                        : edge_y - style.ItemSpacing.y),
                                   ImGui::GetColorU32(ImGuiCol_Text));
                ImGui::PushFont(
                    nullptr, m_settings.GetFontManager().GetFontSize(FontSize::kXSmall));
                label_width =
                    ImGui::CalcTextSize(DISPLAY_PROPS_EVENT_TYPES[category].first).x +
                    ImGui::CalcTextSize(" 100.0%").x;
                ImGui::SetCursorScreenPos(ImVec2(
                    std::max(bar_pos.x,
                             std::min(segment_x, bar_pos.x + bar_size.x - label_width)),
                    category % 2 == 0
                        ? edge_y + style.ItemSpacing.y
                        : edge_y - style.ItemSpacing.y - ImGui::GetTextLineHeight()));
            }
            ImGui::Text("%s %.1f%%", DISPLAY_PROPS_EVENT_TYPES[category].first,
                        100.0 * segment_pct);
            if(!label_fits)
            {
                ImGui::PopFont();
            }
        }
    }
    segment_x += segment_width;
}

void
SummaryTopology::CompositionTable(const SummaryInfo::AggregateMetrics& metrics,
                                  float                                max_name_width)
{
    const SummaryModel& model = m_data_provider.DataModel().GetSummary();
    if((model.TopOpEvents(metrics, kRPVAnalysisSummaryCategoryKernel)
            ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
            .numeric.uint +
        model.TopOpEvents(metrics, kRPVAnalysisSummaryCategoryMemory)
            ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
            .numeric.uint +
        model.TopOpEvents(metrics, kRPVAnalysisSummaryCategoryThread)
            ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
            .numeric.uint) ||
       metrics.gpu.counters.list.size() || metrics.cpu.counters.list.size())
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushID(&metrics);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ThemeColor(m_settings, Colors::kBgPanel));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                              ThemeColor(m_settings, Colors::kBgPanel));
        ImGui::SetNextItemAllowOverlap();
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAllColumns;
        if(metrics.type == kRPVAnalysisSummaryAggregationLevelTrace)
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        if(metrics.sub_metrics.empty())
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        bool open = ImGui::TreeNodeEx("", flags);
        ImGui::PopStyleColor(2);
        ImGui::SameLine(0.0f, 0.0f);
        switch(metrics.type)
        {
            case kRPVAnalysisSummaryAggregationLevelTrace:
            {
                ImGui::TextUnformatted(DISPLAY_NAMES_METRIC_TYPES[static_cast<size_t>(
                    kRPVAnalysisSummaryAggregationLevelTrace -
                    __kRPVAnalysisSummaryAggregationLevelFirst)]);
                break;
            }
            case kRPVAnalysisSummaryAggregationLevelNode:
            {
                ImGui::Text(
                    "%s: ",
                    DISPLAY_NAMES_METRIC_TYPES[static_cast<size_t>(
                        metrics.type - __kRPVAnalysisSummaryAggregationLevelFirst)]);
                if(metrics.info.node->host_name.size())
                {
                    ImGui::SameLine(0.0f, 0.0f);
                    ElidedText(
                        metrics.info.node->host_name.c_str(),
                        std::min(
                            max_name_width -
                                (ImGui::GetCursorScreenPos().x - ImGui::GetWindowPos().x),
                            ImGui::CalcTextSize(metrics.info.node->host_name.c_str()).x),
                        max_name_width);
                }
                break;
            }
            case kRPVAnalysisSummaryAggregationLevelProcessor:
            {
                ImGui::Text(
                    "%s%llu: ",
                    m_data_provider.DataModel().GetTopology().GetProcessorTypeName(
                        metrics.info.processor->type),
                    metrics.info.processor->type_index);
                if(metrics.info.processor->product_name.size())
                {
                    ImGui::SameLine(0.0f, 0.0f);
                    ElidedText(metrics.info.processor->product_name.c_str(),
                               std::min(max_name_width - (ImGui::GetCursorScreenPos().x -
                                                          ImGui::GetWindowPos().x),
                                        ImGui::CalcTextSize(
                                            metrics.info.processor->product_name.c_str())
                                            .x),
                               max_name_width);
                }
                break;
            }
            case kRPVAnalysisSummaryAggregationLevelProcess:
            {
                ImGui::Text(
                    "%s %llu: ",
                    DISPLAY_NAMES_METRIC_TYPES[static_cast<size_t>(
                        metrics.type - __kRPVAnalysisSummaryAggregationLevelFirst)],
                    metrics.info.process->GetId());
                if(metrics.info.process->command.size())
                {
                    ImGui::SameLine(0.0f, 0.0f);
                    ElidedText(
                        metrics.info.process->command.c_str(),
                        std::min(
                            max_name_width -
                                (ImGui::GetCursorScreenPos().x - ImGui::GetWindowPos().x),
                            ImGui::CalcTextSize(metrics.info.process->command.c_str()).x),
                        max_name_width);
                }
                break;
            }
            case kRPVAnalysisSummaryAggregationLevelThread:
            {
                if(metrics.info.thread->GetName().size())
                {
                    ElidedText(metrics.info.thread->GetName().c_str(),
                               std::min(max_name_width - (ImGui::GetCursorScreenPos().x -
                                                          ImGui::GetWindowPos().x),
                                        ImGui::CalcTextSize(
                                            metrics.info.thread->GetName().c_str())
                                            .x),
                               max_name_width);
                }
                break;
            }
            default:
            {
                break;
            }
        }
        ImGui::TableNextColumn();
        if(metrics.gpu.counters.list.size() || metrics.cpu.counters.list.size())
        {
            ImFont* icons = m_settings.GetFontManager().GetFont(FontType::kIcon);
            ImGui::PushFont(icons);
            CenterNextTextItem(ICON_ARROW_IN_BOX);
            if(IconButton(ICON_ARROW_IN_BOX, icons))
            {
                m_counters = &metrics;
            }
            ImGui::PopFont();
        }
        ImGui::TableNextColumn();
        CompositionBar(
            ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight()), metrics,
            false);
        ImGui::TableNextColumn();
        if(m_data_provider.DataModel()
               .GetSummary()
               .TopOpEvents(metrics, kRPVAnalysisSummaryCategoryKernel)
               ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
               .numeric.uint)
        {
            ImGui::TextUnformatted(
                m_data_provider.DataModel()
                    .GetSummary()
                    .TopOpEvents(metrics, kRPVAnalysisSummaryCategoryKernel)
                    ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                    .str.c_str());
        }
        ImGui::TableNextColumn();
        if(m_data_provider.DataModel()
               .GetSummary()
               .TopOpEvents(metrics, kRPVAnalysisSummaryCategoryMemory)
               ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
               .numeric.uint)
        {
            ImGui::TextUnformatted(
                m_data_provider.DataModel()
                    .GetSummary()
                    .TopOpEvents(metrics, kRPVAnalysisSummaryCategoryMemory)
                    ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                    .str.c_str());
        }
        ImGui::TableNextColumn();
        if(m_data_provider.DataModel()
               .GetSummary()
               .TopOpEvents(metrics, kRPVAnalysisSummaryCategoryThread)
               ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
               .numeric.uint)
        {
            ImGui::TextUnformatted(
                m_data_provider.DataModel()
                    .GetSummary()
                    .TopOpEvents(metrics, kRPVAnalysisSummaryCategoryThread)
                    ->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                    .str.c_str());
        }
        if(open)
        {
            for(const SummaryInfo::AggregateMetrics& sub : metrics.sub_metrics)
            {
                CompositionTable(sub, max_name_width);
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void
SummaryTopology::Counters() const
{
    if(m_counters)
    {
        size_t color = 0;
        for(const SummaryInfo::Counters::Counter& counter : m_counters->gpu.counters.list)
        {
            ImGui::PushID(static_cast<int>(color));
            Counter(
                counter,
                m_settings.GetColorWheel()[color++ % m_settings.GetColorWheel().size()]);
            ImGui::PopID();
        }
        for(const SummaryInfo::Counters::Counter& counter : m_counters->cpu.counters.list)
        {
            ImGui::PushID(static_cast<int>(color));
            Counter(
                counter,
                m_settings.GetColorWheel()[color++ % m_settings.GetColorWheel().size()]);
            ImGui::PopID();
        }
    }
    else
    {
        ImGui::SetCursorPos(
            (ImGui::GetContentRegionAvail() - ImGui::CalcTextSize("No data available.")) *
            0.5f);
        ImGui::TextDisabled("No data available.");
    }
}

void
SummaryTopology::Counter(const SummaryInfo::Counters::Counter& counter, ImU32 color) const
{
    const ImGuiStyle&    style = m_settings.GetDefaultStyle();
    const ProcessorInfo* processor =
        m_data_provider.DataModel().GetTopology().GetProcessor(
            counter.info->GetProcessorId());
    float region_metadata =
        (processor ? ImGui::CalcTextSize(
                         m_data_provider.DataModel().GetTopology().GetProcessorTypeName(
                             processor->type))
                             .x +
                         style.ItemSpacing.x
                   : 0.0f) +
        (counter.info->units.size()
             ? ImGui::CalcTextSize(counter.info->units.c_str()).x +
                   ImGui::CalcTextSize("()").x + style.ItemSpacing.x
             : 0.0f) +
        (counter.info->value_type.size()
             ? ImGui::CalcTextSize(counter.info->value_type.c_str()).x +
                   style.ItemSpacing.x
             : 0.0f) -
        style.ItemSpacing.x;
    float region_name = ImGui::GetContentRegionAvail().x -
                        (counter.info->description.size()
                             ? ImGui::GetTextLineHeight() + style.ItemSpacing.x
                             : 0.0f);
    if(region_name - region_metadata > ImGui::GetFrameHeightWithSpacing())
    {
        region_name -= region_metadata;
    }
    else
    {
        region_metadata = 0;
    }
    if(region_name > ImGui::CalcTextSize(counter.info->GetName().c_str()).x)
    {
        ImGui::TextUnformatted(counter.info->GetName().c_str());
    }
    else
    {
        ElidedText(counter.info->GetName().c_str(), region_name, ImGui::GetWindowWidth());
    }
    ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
    if(counter.info->description.size())
    {
        ImGui::SameLine();
        IconButton(ICON_QUESTION_MARK,
                   m_settings.GetFontManager().GetFont(FontType::kIcon));
        if(ImGui::IsItemHovered())
        {
            float window_width = ImGui::GetWindowWidth();
            BeginTooltipStyled();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + window_width);
            ImGui::TextUnformatted(counter.info->description.c_str());
            ImGui::PopTextWrapPos();
            EndTooltipStyled();
        }
    }
    ImGui::PopStyleColor();
    if(region_metadata)
    {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - region_metadata);
        if(counter.info->units.size())
        {
            ImGui::Text("(%s)", counter.info->units.c_str());
        }
        if(counter.info->value_type.size())
        {
            ImGui::SameLine(ImGui::GetCursorPosX());
            ImGui::TextDisabled("%s", counter.info->value_type.c_str());
        }
        if(processor)
        {
            ImGui::SameLine(ImGui::GetCursorPosX());
            ImGui::TextDisabled(
                "%s", m_data_provider.DataModel().GetTopology().GetProcessorTypeName(
                          processor->type));
        }
    }
    ImGui::PushFont(nullptr, m_settings.GetFontManager().GetFontSize(FontSize::kXSmall));
    ImGui::Text("Min: %s",
                counter.metrics[kRPVAnalysisSummaryCounterMetricMin].str.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Max: ").x -
                    ImGui::CalcTextSize(
                        counter.metrics[kRPVAnalysisSummaryCounterMetricMax].str.c_str())
                        .x);
    ImGui::Text("Max: %s",
                counter.metrics[kRPVAnalysisSummaryCounterMetricMax].str.c_str());
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeight()));
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
        m_settings.GetColor(Colors::kBorderGray), style.FrameRounding);
    double min_value  = counter.metrics[kRPVAnalysisSummaryCounterMetricMin].numeric.dbl;
    double max_value  = counter.metrics[kRPVAnalysisSummaryCounterMetricMax].numeric.dbl;
    double mean_value = counter.metrics[kRPVAnalysisSummaryCounterMetricMean].numeric.dbl;
    double std_dev_value =
        counter.metrics[kRPVAnalysisSummaryCounterMetricStdDev].numeric.dbl;
    double range     = max_value - min_value;
    ImVec2 bar_min   = ImGui::GetItemRectMin();
    ImVec2 bar_max   = ImGui::GetItemRectMax();
    float  bar_width = bar_max.x - bar_min.x;
    float  label_x   = bar_min.x + 0.5f * bar_width;
    if(range > 0.0)
    {
        label_x =
            bar_min.x + bar_width * static_cast<float>((mean_value - min_value) / range);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(bar_min.x +
                       bar_width *
                           static_cast<float>(
                               (std::max(min_value, mean_value - std_dev_value * 0.5f) -
                                min_value) /
                               range),
                   bar_min.y),
            ImVec2(bar_min.x +
                       bar_width *
                           static_cast<float>(
                               (std::min(max_value, mean_value + std_dev_value * 0.5f) -
                                min_value) /
                               range),
                   bar_max.y),
            color, style.FrameRounding);
    }
    ImGui::GetWindowDrawList()->AddLine(ImVec2(label_x, bar_min.y),
                                        ImVec2(label_x, bar_max.y),
                                        ImGui::GetColorU32(ImGuiCol_Text));
    ImGui::PushFont(nullptr, m_settings.GetFontManager().GetFontSize(FontSize::kXSmall));
    float label_width =
        ImGui::CalcTextSize("Mean: ").x +
        ImGui::CalcTextSize(
            counter.metrics[kRPVAnalysisSummaryCounterMetricMean].str.c_str())
            .x +
        ImGui::CalcTextSize(" | ").x + ImGui::CalcTextSize(SIGMA).x +
        ImGui::CalcTextSize(": ").x +
        ImGui::CalcTextSize(
            counter.metrics[kRPVAnalysisSummaryCounterMetricStdDev].str.c_str())
            .x;
    ImGui::SetCursorScreenPos(
        ImVec2(std::max(bar_min.x,
                        std::min(label_x - 0.5f * label_width, bar_max.x - label_width)),
               bar_max.y + style.ItemSpacing.y));
    ImGui::Text("Mean: %s | %s: %s",
                counter.metrics[kRPVAnalysisSummaryCounterMetricMean].str.c_str(), SIGMA,
                counter.metrics[kRPVAnalysisSummaryCounterMetricStdDev].str.c_str());
    ImGui::PopFont();
}

SummaryTopEvents::SummaryTopEvents(DataProvider&                      dp,
                                   std::shared_ptr<TimelineSelection> timeline_selection,
                                   rocprofvis_analysis_summary_category_t type,
                                   TableType                              table_type,
                                   rocprofvis_controller_table_type_t request_table_type,
                                   uint64_t                           request_id)
: m_v_container(nullptr)
, m_instance_table(std::make_unique<InstanceTable>(
      dp, timeline_selection, type, table_type, request_table_type, request_id))
, m_op_events(nullptr)
, m_filters()
, m_pie(PieModel{ {}, {}, {} })
, m_table_headers({})
, m_type(type)
, m_display_mode(Pie)
, m_show_legend(true)
, m_hovered_idx(std::nullopt)
, m_selected_idx(std::nullopt)
, m_padded_idx(std::nullopt)
, m_data_changed(false)
, m_filter_changed(false)
, m_timeformat_changed(true)
, m_data_provider(dp)
, m_settings(SettingsManager::GetInstance())
{
    LayoutItem::Ptr top_events = LayoutItem::CreateFromWidget(
        std::make_shared<RocCustomWidget>([this]() { RenderTopEvents(); }));
    top_events->m_child_flags       = ImGuiChildFlags_AlwaysUseWindowPadding;
    top_events->m_window_padding    = m_settings.GetDefaultStyle().WindowPadding;
    top_events->m_item_spacing      = m_settings.GetDefaultStyle().ItemSpacing;
    top_events->m_inherit_bg_color  = true;
    LayoutItem::Ptr event_instances = LayoutItem::CreateFromWidget(
        std::make_shared<RocCustomWidget>([this]() { RenderEventInstances(); }));
    event_instances->m_child_flags  = ImGuiChildFlags_None;
    event_instances->m_item_spacing = m_settings.GetDefaultStyle().ItemSpacing;
    m_v_container = std::make_unique<VSplitContainer>(top_events, event_instances);
    m_v_container->SetSplit(0.5f);
    m_filters.resize(2);
    m_filters[0] = FilterModel{ DISPLAY_NAMES_METRIC_TYPES[static_cast<size_t>(
                                    kRPVAnalysisSummaryAggregationLevelNode -
                                    __kRPVAnalysisSummaryAggregationLevelFirst)],
                                0,
                                nullptr,
                                { nullptr },
                                { "All" },
                                { std::nullopt } };
    switch(m_type)
    {
        case kRPVAnalysisSummaryCategoryKernel:
        {
            m_filters[1] = FilterModel{ "GPU",       0,         nullptr,
                                        { nullptr }, { "All" }, { std::nullopt } };
            break;
        }
        case kRPVAnalysisSummaryCategoryMemory:
        {
            m_filters[1] = FilterModel{ DISPLAY_NAMES_METRIC_TYPES[static_cast<size_t>(
                                            kRPVAnalysisSummaryAggregationLevelProcessor -
                                            __kRPVAnalysisSummaryAggregationLevelFirst)],
                                        0,
                                        nullptr,
                                        { nullptr },
                                        { "All" },
                                        { std::nullopt } };
            break;
        }
        case kRPVAnalysisSummaryCategoryThread:
        {
            m_filters.resize(3);
            m_filters[1] = FilterModel{ DISPLAY_NAMES_METRIC_TYPES[static_cast<size_t>(
                                            kRPVAnalysisSummaryAggregationLevelProcess -
                                            __kRPVAnalysisSummaryAggregationLevelFirst)],
                                        0,
                                        nullptr,
                                        { nullptr },
                                        { "All" },
                                        { std::nullopt } };
            m_filters[2] = FilterModel{ DISPLAY_NAMES_METRIC_TYPES[static_cast<size_t>(
                                            kRPVAnalysisSummaryAggregationLevelThread -
                                            __kRPVAnalysisSummaryAggregationLevelFirst)],
                                        0,
                                        nullptr,
                                        { nullptr },
                                        { "All" },
                                        { std::nullopt } };
            break;
        }
        default:
        {
            ROCPROFVIS_ASSERT(false);
            break;
        }
    }
}

void
SummaryTopEvents::Update()
{
    if(m_data_provider.GetState() == ProviderState::kReady)
    {
        if(m_data_changed)
        {
            const SummaryModel& model = m_data_provider.DataModel().GetSummary();
            const SummaryInfo::AggregateMetrics& metrics = model.GetSummaryData();
            m_op_events               = model.TopOpEvents(metrics, m_type);
            m_filters[0].selected_idx = 0;
            m_filters[0].parent_info  = nullptr;
            m_filters[0].info         = { nullptr };
            m_filters[0].labels       = { "All" };
            m_filters[0].query        = { std::nullopt };
            for(const SummaryInfo::AggregateMetrics& node : metrics.sub_metrics)
            {
                if(node.type == kRPVAnalysisSummaryAggregationLevelNode &&
                   node.info.node &&
                   model.TopOpEvents(node, m_type)
                       ->metrics[kRPVAnalysisSummaryTopEventsMetricInvocations]
                       .numeric.uint)
                {
                    m_filters[0].info.push_back(&node);
                    m_filters[0].labels.push_back(node.info.node->host_name);
                    m_filters[0].query.push_back(
                        rocprofvis_dm_query_criteria_t{ node.info.node->GetId() });
                }
            }
            m_filter_changed = true;
            m_data_changed   = false;
        }
        if(m_filter_changed)
        {
            const SummaryModel& model = m_data_provider.DataModel().GetSummary();
            const SummaryInfo::AggregateMetrics& metrics = model.GetSummaryData();
            m_op_events    = model.TopOpEvents(metrics, m_type);
            m_pie          = PieModel{};
            m_hovered_idx  = std::nullopt;
            m_selected_idx = std::nullopt;
            m_padded_idx   = std::nullopt;
            m_instance_table->Clear();
            ImPlot::BustColorCache();
            for(size_t i = 1; i < m_filters.size(); i++)
            {
                if(m_filters[i].parent_info !=
                       m_filters[i - 1].info[m_filters[i - 1].selected_idx] ||
                   !m_filters[i].parent_info)
                {
                    m_filters[i].selected_idx = 0;
                    m_filters[i].parent_info =
                        m_filters[i - 1].info[m_filters[i - 1].selected_idx];
                    m_filters[i].info   = { nullptr };
                    m_filters[i].labels = { "All" };
                    m_filters[i].query  = {
                        m_filters[i - 1].query[m_filters[i - 1].selected_idx]
                    };
                    if(m_filters[i].parent_info)
                    {
                        for(const SummaryInfo::AggregateMetrics& metric :
                            m_filters[i].parent_info->sub_metrics)
                        {
                            if(metric.info.node &&
                               model.TopOpEvents(metric, m_type)
                                   ->metrics
                                       [kRPVAnalysisSummaryTopEventsMetricInvocations]
                                   .numeric.uint)
                            {
                                switch(m_type)
                                {
                                    case kRPVAnalysisSummaryCategoryKernel:
                                    case kRPVAnalysisSummaryCategoryMemory:
                                    {
                                        if(metric.type ==
                                               kRPVAnalysisSummaryAggregationLevelProcessor &&
                                           ((m_type ==
                                                 kRPVAnalysisSummaryCategoryKernel &&
                                             metric.info.processor->type ==
                                                 kRPVControllerProcessorTypeGPU) ||
                                            (m_type ==
                                                 kRPVAnalysisSummaryCategoryMemory &&
                                             (metric.info.processor->type ==
                                                  kRPVControllerProcessorTypeGPU ||
                                              metric.info.processor->type ==
                                                  kRPVControllerProcessorTypeCPU))))
                                        {
                                            m_filters[i].info.push_back(&metric);
                                            m_filters[i].labels.push_back(std::string());
                                            m_data_provider.DataModel()
                                                .GetTopology()
                                                .GetProcessorTypeLabel(
                                                    *metric.info.processor,
                                                    m_filters[i].labels.back());
                                            m_filters[i].labels.back() +=
                                                ": " +
                                                metric.info.processor->product_name;
                                            m_filters[i].query.push_back(
                                                m_filters[i - 1].query
                                                    [m_filters[i - 1].selected_idx]);
                                            m_filters[i].query.back()->agent_id =
                                                metric.info.processor->GetId();
                                        }
                                        break;
                                    }
                                    case kRPVAnalysisSummaryCategoryThread:
                                    {
                                        if(metric.type ==
                                           kRPVAnalysisSummaryAggregationLevelProcess)
                                        {
                                            m_filters[i].info.push_back(&metric);
                                            m_filters[i].labels.push_back(
                                                std::string(DISPLAY_NAMES_METRIC_TYPES[static_cast<
                                                    size_t>(
                                                    kRPVAnalysisSummaryAggregationLevelProcess -
                                                    __kRPVAnalysisSummaryAggregationLevelFirst)]) +
                                                " " +
                                                std::to_string(
                                                    metric.info.process->GetId()) +
                                                ": " + metric.info.process->command);
                                            m_filters[i].query.push_back(
                                                m_filters[i - 1].query
                                                    [m_filters[i - 1].selected_idx]);
                                            m_filters[i].query.back()->process_id =
                                                metric.info.process->GetId();
                                        }
                                        else if(metric.type ==
                                                kRPVAnalysisSummaryAggregationLevelThread)
                                        {
                                            m_filters[i].info.push_back(&metric);
                                            m_filters[i].labels.push_back(
                                                metric.info.thread->GetName());
                                            m_filters[i].query.push_back(
                                                m_filters[i - 1].query
                                                    [m_filters[i - 1].selected_idx]);
                                            m_filters[i].query.back()->thread_id =
                                                metric.info.thread->GetId();
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            for(const FilterModel& filter : m_filters)
            {
                if(filter.selected_idx > 0 &&
                   static_cast<size_t>(filter.selected_idx) < filter.info.size() &&
                   filter.info[filter.selected_idx])
                {
                    m_op_events =
                        model.TopOpEvents(*filter.info[filter.selected_idx], m_type);
                }
                else
                {
                    break;
                }
            }
            if(m_op_events && !m_op_events->top.empty())
            {
                for(const SummaryInfo::TopOpEvents::Event& event : m_op_events->top)
                {
                    m_pie.labels.push_back(event.name.c_str());
                    m_pie.duration_pct_of_total.push_back(
                        event.metrics[kRPVAnalysisSummaryTopEventMetricDurationPct]
                            .numeric.flt);
                }
                float angle_sum = 0.0f;
                for(const float& duration_pct_of_total : m_pie.duration_pct_of_total)
                {
                    float angle = duration_pct_of_total * 360.0f;
                    m_pie.slices.push_back({ angle_sum, angle });
                    angle_sum += angle;
                }
                if(m_op_events->top.back().name == "Others")
                {
                    m_padded_idx = m_op_events->top.size() - 1;
                }
            }
            m_filter_changed = false;
        }
    }
    if(m_timeformat_changed)
    {
        TimeFormat timeformat = m_settings.GetUserSettings().unit_settings.time_format;
        m_table_headers       = { "Name",
                                  "Invocation(s)",
                                  "Total Duration (" + timeformat_sufix(timeformat) + ")",
                                  "Total Duration (%)",
                                  "Avg Duration (" + timeformat_sufix(timeformat) + ")",
                                  "Max Duration (" + timeformat_sufix(timeformat) + ")",
                                  "Min Duration (" + timeformat_sufix(timeformat) + ")" };
        m_timeformat_changed  = false;
    }
    m_instance_table->Update();
}

void
SummaryTopEvents::Render()
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
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
}

void
SummaryTopEvents::TimeFormatChanged()
{
    m_timeformat_changed = true;
}

void
SummaryTopEvents::Fetched()
{
    m_data_changed = true;
}

bool
SummaryTopEvents::Empty() const
{
    const SummaryModel& model = m_data_provider.DataModel().GetSummary();
    return model.TopOpEvents(model.GetSummaryData(), m_type)
               ->metrics[kRPVAnalysisSummaryTopEventsMetricInvocations]
               .numeric.uint == 0;
}

bool
SummaryTopEvents::RenderPreview() const
{
    const SummaryModel&                  model = m_data_provider.DataModel().GetSummary();
    const SummaryInfo::AggregateMetrics& metrics   = model.GetSummaryData();
    const SummaryInfo::TopOpEvents*      op_events = model.TopOpEvents(metrics, m_type);
    const ImGuiStyle&                    style     = m_settings.GetDefaultStyle();
    ImGui::AlignTextToFramePadding();
    ImGui::Dummy(ImVec2(ImGui::GetTextLineHeight(), ImGui::GetTextLineHeight()));
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin() + ImVec2(0.0f, style.FramePadding.y),
        ImGui::GetItemRectMax() + ImVec2(0.0f, style.FramePadding.y),
        m_settings.GetColorWheel()[DISPLAY_PROPS_EVENT_TYPES[m_type].second],
        style.FrameRounding);
    ImGui::SameLine();
    ImGui::TextUnformatted(DISPLAY_PROPS_EVENT_TYPES[m_type].first);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Details").x -
                    2.0f * style.FramePadding.x);
    bool clicked = ImGui::Button("Details");
    if(op_events && op_events->top.size())
    {
        ImGui::PushFont(nullptr,
                        m_settings.GetFontManager().GetFontSize(FontSize::kLarge));
        if(m_settings.GetUserSettings().unit_settings.time_format >
           TimeFormat::kTimecodeCondensed)
        {
            ImGui::Text(
                "%s %s",
                op_events->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                    .str.c_str(),
                timeformat_sufix(m_settings.GetUserSettings().unit_settings.time_format)
                    .c_str());
        }
        else
        {
            ImGui::TextUnformatted(
                op_events->metrics[kRPVAnalysisSummaryTopEventsMetricDurationSum]
                    .str.c_str());
        }
        ImGui::SameLine();
        if(ImGui::GetContentRegionAvail().x >
           ImGui::CalcTextSize(
               op_events->metrics[kRPVAnalysisSummaryTopEventsMetricInvocations]
                   .str.c_str())
                   .x +
               ImGui::CalcTextSize(" invocations").x)
        {
            ImGui::TextDisabled(
                "%s invocations",
                op_events->metrics[kRPVAnalysisSummaryTopEventsMetricInvocations]
                    .str.c_str());
        }
        else
        {
            ImGui::NewLine();
        }
        ImGui::PopFont();
        ImGui::PushFont(nullptr,
                        m_settings.GetFontManager().GetFontSize(FontSize::kSmall));
        ImGui::TextDisabled("Top by Duration");
        ImGui::PopFont();
        for(size_t i = 0;
            i < op_events->top.size() &&
            ImGui::GetContentRegionAvail().y > ImGui::GetTextLineHeightWithSpacing();
            i++)
        {
            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginGroup();
            ElidedText(op_events->top[i].name.c_str(),
                       ImGui::GetContentRegionAvail().x -
                           ImGui::CalcTextSize("100.0%").x - style.ItemSpacing.x,
                       ImGui::GetContentRegionAvail().x);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x -
                            ImGui::CalcTextSize("100.0%").x);
            ImGui::Text("%0.1f",
                        op_events->top[i]
                                .metrics[kRPVAnalysisSummaryTopEventMetricDurationPct]
                                .numeric.flt *
                            100.0f);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x -
                            ImGui::CalcTextSize("%").x);
            ImGui::TextUnformatted("%");
            ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 0.25f * style.ItemSpacing.y);
            ImGui::EndGroup();
            ImGui::PopID();
            ImGui::Dummy(
                ImVec2(ImGui::GetContentRegionAvail().x, style.ItemSpacing.y * 0.75f));
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                m_settings.GetColor(Colors::kBorderGray), style.FrameRounding);
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetItemRectMin(),
                ImVec2(ImGui::GetItemRectMin().x +
                           ImGui::GetItemRectSize().x *
                               op_events->top[i]
                                   .metrics[kRPVAnalysisSummaryTopEventMetricDurationPct]
                                   .numeric.flt,
                       ImGui::GetItemRectMax().y),
                m_settings.GetColorWheel()[DISPLAY_PROPS_EVENT_TYPES[m_type].second],
                style.FrameRounding);
            ImGui::PopStyleVar();
        }
    }
    return clicked;
}

void
SummaryTopEvents::RenderTopEvents()
{
    if(!m_data_changed && !m_filter_changed && m_op_events)
    {
        const ImVec2       region     = ImGui::GetContentRegionAvail();
        const ImGuiStyle&  style      = m_settings.GetDefaultStyle();
        const ImPlotStyle& plot_style = ImPlot::GetStyle();
        ImGui::SetCursorPos(ImVec2(
            ImGui::GetContentRegionAvail().x * 0.5f -
                ImGui::CalcTextSize(DISPLAY_NAMES_TOP_EVENT_TYPES[m_type]).x * 0.5f,
            ImGui::GetWindowContentRegionMin().y));
        ImGui::TextUnformatted(DISPLAY_NAMES_TOP_EVENT_TYPES[m_type]);
        if(m_op_events->top.empty())
        {
            ImGui::GetWindowDrawList()->AddRect(
                ImGui::GetCursorScreenPos(),
                ImGui::GetWindowPos() + ImGui::GetContentRegionMax() -
                    ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing()),
                ImGui::GetColorU32(style.Colors[ImGuiCol_TextDisabled]));
            ImGui::SetCursorPos(
                (region -
                 ImGui::CalcTextSize("No data available for the selected filters.")) *
                0.5f);
            ImGui::TextDisabled("No data available for the selected filters.");
        }
        else
        {
            // Chart/table...
            int        hovered_idx = -1;
            TimeFormat time_format =
                m_settings.GetUserSettings().unit_settings.time_format;
            ImPlot::PushColormap(m_settings.GetFlameColormapName());
            ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, CHART_FIT_PADDING);
            ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0.0f, 0.0f));
            ImPlot::PushStyleColor(ImPlotCol_FrameBg,
                                   ThemeColor(m_settings, Colors::kTransparent));
            ImPlot::PushStyleColor(ImPlotCol_PlotBg,
                                   ThemeColor(m_settings, Colors::kBgFrame));
            ImPlot::PushStyleColor(ImPlotCol_PlotBorder,
                                   ThemeColor(m_settings, Colors::kBorderColor, 0.85f));
            ImPlot::PushStyleColor(ImPlotCol_AxisText,
                                   ThemeColor(m_settings, Colors::kTextMain));
            ImPlot::PushStyleColor(ImPlotCol_AxisGrid,
                                   ThemeColor(m_settings, Colors::kBorderColor, 0.7f));
            ImPlot::PushStyleColor(ImPlotCol_AxisTick,
                                   ThemeColor(m_settings, Colors::kTextDim, 0.56f));
            switch(m_display_mode)
            {
                case Pie:
                {
                    RenderPieChart(region, style, plot_style, hovered_idx);
                    break;
                }
                case Bar:
                {
                    RenderBarChart(region, style, plot_style, time_format, hovered_idx);
                    break;
                }
                case Table:
                {
                    RenderTable(plot_style);
                    break;
                }
            }
            // Legend...
            if(m_display_mode == Pie || m_display_mode == Bar)
            {
                ImGui::SameLine();
                RenderLegend(region, style, plot_style, hovered_idx);
            }
            ImPlot::PopStyleColor(6);
            ImPlot::PopStyleVar(2);
            ImPlot::PopColormap();

            m_hovered_idx = hovered_idx == -1 ? std::nullopt
                                              : std::make_optional<size_t>(hovered_idx);
        }
        // Chart/table switcher
        ImGui::SetCursorPos(
            ImVec2(ImGui::GetWindowContentRegionMin().x,
                   ImGui::GetContentRegionMax().y - ImGui::GetFrameHeight()));
        ImGui::BeginGroup();
        if(IconButton(ICON_CHART_PIE,
                      m_settings.GetFontManager().GetFont(FontType::kIcon), ImVec2(0, 0),
                      nullptr, false, style.FramePadding,
                      m_settings.GetColor(m_display_mode == Pie ? Colors::kButton
                                                                : Colors::kTransparent),
                      m_settings.GetColor(Colors::kButtonHovered),
                      m_settings.GetColor(Colors::kButtonActive)))
        {
            m_display_mode = Pie;
        }
        ImGui::SameLine(0.0f, 0.0f);
        if(IconButton(ICON_CHART_BAR,
                      m_settings.GetFontManager().GetFont(FontType::kIcon), ImVec2(0, 0),
                      nullptr, false, style.FramePadding,
                      m_settings.GetColor(m_display_mode == Bar ? Colors::kButton
                                                                : Colors::kTransparent),
                      m_settings.GetColor(Colors::kButtonHovered),
                      m_settings.GetColor(Colors::kButtonActive)))
        {
            m_display_mode = Bar;
        }
        ImGui::SameLine(0.0f, 0.0f);
        if(IconButton(ICON_LIST, m_settings.GetFontManager().GetFont(FontType::kIcon),
                      ImVec2(0, 0), nullptr, false, style.FramePadding,
                      m_settings.GetColor(m_display_mode == Table ? Colors::kButton
                                                                  : Colors::kTransparent),
                      m_settings.GetColor(Colors::kButtonHovered),
                      m_settings.GetColor(Colors::kButtonActive)))
        {
            m_display_mode = Table;
        }
        ImGui::EndGroup();
        // Filter combos...
        ImGui::SameLine();
        float combo_width =
            ImGui::GetContentRegionAvail().x -
            (static_cast<float>(m_filters.size()) * 2.0f - 1.0f) * style.ItemSpacing.x;
        for(const FilterModel& filter : m_filters)
        {
            combo_width -= ImGui::CalcTextSize(filter.name).x;
        }
        combo_width /= static_cast<float>(m_filters.size());
        for(size_t i = 0; i < m_filters.size(); i++)
        {
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::BeginDisabled(m_filters[i].info.size() < 2);
            ImGui::TextUnformatted(m_filters[i].name);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(combo_width);
            PushComboStyles();
            ImGui::PushID(static_cast<int>(i));
            if(ImGui::BeginCombo(
                   "##combo", m_filters[i].selected_idx <
                                      static_cast<int>(m_filters[i].labels.size())
                                  ? m_filters[i].labels[m_filters[i].selected_idx].c_str()
                                  : "-"))
            {
                for(size_t j = 0; j < m_filters[i].labels.size(); j++)
                {
                    ImGui::PushID(static_cast<int>(j));
                    if(ImGui::Selectable("", m_filters[i].selected_idx ==
                                                 static_cast<int>(j)))
                    {
                        m_filters[i].selected_idx = static_cast<int>(j);
                        m_filter_changed          = true;
                    }
                    ImGui::SameLine(ImGui::GetCursorPosX());
                    ElidedText(m_filters[i].labels[j].c_str(),
                               ImGui::GetContentRegionAvail().x,
                               ImGui::GetContentRegionAvail().x);
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
            PopComboStyles();
            ImGui::EndDisabled();
        }
    }
}

void
SummaryTopEvents::RenderEventInstances()
{
    m_instance_table->Render();
}

void
SummaryTopEvents::RenderPieChart(const ImVec2 region, const ImGuiStyle& style,
                                 const ImPlotStyle& plot_style, int& hovered_idx)
{
    if(ImPlot::BeginPlot(
           "##Pie",
           ImVec2(m_show_legend ? region.x * 0.75f - style.ItemSpacing.x : region.x,
                  ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()),
           ImPlotFlags_Equal | ImPlotFlags_NoFrame | ImPlotFlags_CanvasOnly))
    {
        ImPlot::SetupAxes(nullptr, nullptr,
                          ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoDecorations |
                              ImPlotAxisFlags_NoHighlight,
                          ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoDecorations |
                              ImPlotAxisFlags_NoHighlight);
        hovered_idx = PlotHoverIdx();
        ImPlot::PlotPieChart(
            m_pie.labels.data(), m_pie.duration_pct_of_total.data(),
            static_cast<int>(m_pie.duration_pct_of_total.size()), 0.0, 0.0,
            PIE_CHART_RADIUS,
            [](double value, char* buff, int size, void* user_data) -> int {
                (void) user_data;
                if(value * 100.0 > 10.0)
                {
                    snprintf(buff, size, "%.1f%%", value * 100.0);
                }
                else
                {
                    buff[0] = '\0';
                }
                return 0;
            });
        if(m_hovered_idx)
        {
            ImPlot::PushColormap(m_settings.GetContrastColormapName());
            ImGui::PushID(m_settings.GetContrastColormapName());
            ImPlot::PlotPieChart(
                &m_pie.labels[m_hovered_idx.value()],
                &m_pie.duration_pct_of_total[m_hovered_idx.value()], 1, 0.0, 0.0,
                PIE_CHART_RADIUS,
                [](double value, char* buff, int size, void* user_data) -> int {
                    (void) user_data;
                    if(value)
                    {
                        snprintf(buff, size, "%.1f%%", value * 100.0);
                    }
                    else
                    {
                        buff[0] = '\0';
                    }
                    return 0;
                },
                nullptr, m_pie.slices[m_hovered_idx.value()].angle + 90);
            ImGui::PopID();
            ImPlot::PopColormap();
        }
        ImPlot::EndPlot();
    }
    PlotInputHandler();
}

void
SummaryTopEvents::RenderBarChart(const ImVec2 region, const ImGuiStyle& style,
                                 const ImPlotStyle& plot_style, TimeFormat time_format,
                                 int& hovered_idx)
{
    if(ImPlot::BeginPlot(
           "##Bar",
           ImVec2(m_show_legend ? region.x * 0.75f - style.ItemSpacing.x : region.x,
                  ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing()),
           ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_CanvasOnly))
    {
        ImPlot::SetupAxes(
            nullptr,
            std::string("Total Duration (" + timeformat_sufix(time_format) + ")").c_str(),
            ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoDecorations |
                ImPlotAxisFlags_NoHighlight,
            ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxisFormat(
            ImAxis_Y1,
            [](double value, char* buff, int size, void* user_data) -> int {
                if(value && user_data)
                {
                    const std::string fmt = nanosecond_to_formatted_str(
                        value, *static_cast<TimeFormat*>(user_data));
                    const size_t len_fmt =
                        std::min(fmt.length(), static_cast<size_t>(size - 1));
                    std::memcpy(buff, fmt.c_str(), len_fmt);
                    buff[len_fmt] = '\0';
                }
                else
                {
                    buff[0] = '\0';
                }
                return 0;
            },
            &time_format);
        hovered_idx = PlotHoverIdx();
        for(size_t i = 0; i < m_op_events->top.size(); i++)
        {
            if(i == m_hovered_idx)
            {
                ImPlot::PushColormap(m_settings.GetContrastColormapName());
            }
            ImPlot::SetNextFillStyle(ImPlot::GetColormapColor(static_cast<int>(i)));
            ImU64 duration_sum = static_cast<ImU64>(
                m_op_events->top[i]
                    .metrics[kRPVAnalysisSummaryTopEventMetricDurationSum]
                    .numeric.uint);
            ImPlot::PlotBars(m_op_events->top[i].name.c_str(), &duration_sum, 1,
                             BAR_CHART_THICKNESS, static_cast<double>(i));
            if(i == m_hovered_idx)
            {
                ImPlot::PopColormap();
            }
        }
        ImPlot::EndPlot();
    }
    PlotInputHandler();
}

void
SummaryTopEvents::RenderTable(const ImPlotStyle& plot_style)
{
    if(ImGui::BeginTable("Table", m_table_headers.size(),
                         ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                             ImGuiTableFlags_Resizable |
                             ImGuiTableFlags_NoSavedSettings,
                         ImGui::GetContentRegionAvail() -
                             ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing())))
    {
        ImGui::PushStyleColor(ImGuiCol_Header, m_settings.GetColor(Colors::kSelection));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              m_settings.GetColor(Colors::kHighlightChart));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                              m_settings.GetColor(Colors::kHighlightChart));
        ImGui::TableSetupScrollFreeze(0, 1);
        for(const std::string& column : m_table_headers)
        {
            ImGui::TableSetupColumn(column.c_str());
        }
        ImGui::TableHeadersRow();
        for(size_t i = 0; i < m_op_events->top.size(); i++)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(i));
            ImVec2 pos = ImGui::GetCursorPos();
            if(ImGui::Selectable("", m_selected_idx == i,
                                 ImGuiSelectableFlags_SpanAllColumns |
                                     ImGuiSelectableFlags_AllowOverlap))
            {
                ToggleSelectEvent(i);
            }
            ImGui::SetCursorPos(pos);
            ImGui::BeginDisabled(i == m_padded_idx);
            ElidedText(m_op_events->top[i].name.c_str(), ImGui::GetContentRegionAvail().x,
                       ImGui::GetContentRegionAvail().x);
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                m_op_events->top[i]
                    .metrics[kRPVAnalysisSummaryTopEventMetricInvocations]
                    .str.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                m_op_events->top[i]
                    .metrics[kRPVAnalysisSummaryTopEventMetricDurationSum]
                    .str.c_str());
            ImGui::TableNextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                                  ImPlot::GetColormapColor(static_cast<int>(i)));
            ImGui::ProgressBar(m_op_events->top[i]
                                   .metrics[kRPVAnalysisSummaryTopEventMetricDurationPct]
                                   .numeric.flt,
                               ImVec2(ImGui::GetContentRegionAvail().x -
                                          ImGui::CalcTextSize("100.0").x -
                                          ImGui::GetStyle().ItemSpacing.x,
                                      0.0f),
                               "");
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::SameLine();
            ImGui::Text("%0.1f",
                        m_op_events->top[i]
                                .metrics[kRPVAnalysisSummaryTopEventMetricDurationPct]
                                .numeric.flt *
                            100.0f);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                i == m_padded_idx
                    ? "-"
                    : m_op_events->top[i]
                          .metrics[kRPVAnalysisSummaryTopEventMetricDurationAvg]
                          .str.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                i == m_padded_idx
                    ? "-"
                    : m_op_events->top[i]
                          .metrics[kRPVAnalysisSummaryTopEventMetricDurationMax]
                          .str.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(
                i == m_padded_idx
                    ? "-"
                    : m_op_events->top[i]
                          .metrics[kRPVAnalysisSummaryTopEventMetricDurationMin]
                          .str.c_str());
            ImGui::PopID();
        }
        ImGui::PopStyleColor(3);
        ImGui::EndTable();
    }
}

void
SummaryTopEvents::RenderLegend(const ImVec2 region, const ImGuiStyle& style,
                               const ImPlotStyle& plot_style, int& hovered_idx)
{
    ImVec2 legend_pos =
        ImVec2(0.75f * region.x + 0.5f * style.ItemSpacing.x, ImGui::GetCursorPosY());
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    if(m_show_legend)
    {
        ImGui::SetCursorPos(legend_pos -
                            ImVec2(ImGui::GetFrameHeightWithSpacing(), 0.0f));
        if(ImGui::ArrowButton("hide_legend", ImGuiDir_Right))
        {
            m_show_legend = false;
        }
    }
    else
    {
        ImGui::SetCursorPos(
            ImVec2(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight(),
                   ImGui::GetCursorPosY()));
        if(ImGui::ArrowButton("show_legend", ImGuiDir_Left))
        {
            m_show_legend = true;
        }
    }
    ImGui::PopStyleVar();
    if(m_show_legend)
    {
        ImGui::SetCursorPos(legend_pos);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, plot_style.Colors[ImPlotCol_PlotBg]);
        ImGui::PushStyleColor(ImGuiCol_Border, plot_style.Colors[ImPlotCol_PlotBorder]);
        ImGui::PushStyleColor(ImGuiCol_Header, m_settings.GetColor(Colors::kSelection));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              m_settings.GetColor(Colors::kHighlightChart));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                              m_settings.GetColor(Colors::kHighlightChart));
        ImGui::BeginChild("legend",
                          ImVec2(0.0f, ImGui::GetContentRegionAvail().y -
                                           ImGui::GetFrameHeightWithSpacing()),
                          ImGuiChildFlags_Borders);
        float icon_width       = ImGui::GetFontSize();
        float scroll_bar_width = ImGui::GetScrollMaxY() ? style.ScrollbarSize : 0.0f;
        for(size_t i = 0; i < m_op_events->top.size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            ImVec2 pos = ImGui::GetCursorPos();
            ImGui::BeginGroup();
            ImGui::Dummy(ImVec2(icon_width, icon_width));
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::BeginGroup();
            ImGui::BeginDisabled(i == m_padded_idx);
            ElidedText(m_op_events->top[i].name.c_str(), ImGui::GetContentRegionAvail().x,
                       region.x * 0.5f);
            ImGui::EndDisabled();
            ImGui::PushFont(nullptr,
                            m_settings.GetFontManager().GetFontSize(FontSize::kXSmall));
            if(ImGui::CalcTextSize("100.0%").x < ImGui::GetContentRegionAvail().x)
            {
                ImGui::TextDisabled(
                    "%0.1f%%",
                    m_op_events->top[i]
                            .metrics[kRPVAnalysisSummaryTopEventMetricDurationPct]
                            .numeric.flt *
                        100.0f);
                ImGui::SameLine(0.0f, 0.0f);
                if(ImGui::CalcTextSize(
                       m_op_events->top[i]
                           .metrics[kRPVAnalysisSummaryTopEventMetricInvocations]
                           .str.c_str())
                           .x +
                       ImGui::CalcTextSize(
                           m_op_events->top[i]
                               .metrics[kRPVAnalysisSummaryTopEventMetricInvocations]
                               .str.c_str())
                           .x +
                       ImGui::CalcTextSize("| invocation(s)").x <
                   ImGui::GetContentRegionAvail().x)
                {
                    ImGui::TextDisabled(
                        " | %s invocation(s)",
                        m_op_events->top[i]
                            .metrics[kRPVAnalysisSummaryTopEventMetricInvocations]
                            .str.c_str());
                    ImGui::SameLine(0.0f, 0.0f);
                    if(ImGui::CalcTextSize(
                           m_op_events->top[i]
                               .metrics[kRPVAnalysisSummaryTopEventMetricDurationSum]
                               .str.c_str())
                               .x +
                           ImGui::CalcTextSize(
                               timeformat_sufix(
                                   m_settings.GetUserSettings().unit_settings.time_format)
                                   .c_str())
                               .x <
                       ImGui::GetContentRegionAvail().x)
                    {
                        ImGui::TextDisabled(
                            " | %s %s",
                            m_op_events->top[i]
                                .metrics[kRPVAnalysisSummaryTopEventMetricDurationSum]
                                .str.c_str(),
                            timeformat_sufix(
                                m_settings.GetUserSettings().unit_settings.time_format)
                                .c_str());
                    }
                }
            }
            ImGui::PopFont();
            ImGui::EndGroup();
            ImGui::EndGroup();
            ImGui::SetCursorPos(pos);
            bool row_clicked =
                ImGui::Selectable("", m_selected_idx == i,
                                  m_hovered_idx == i ? ImGuiSelectableFlags_Highlight
                                                     : ImGuiSelectableFlags_None,
                                  ImGui::GetItemRectSize());
            bool row_hovered = ImGui::IsItemHovered();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetItemRectMin() + ImVec2(2.0f * IMPLOT_LEGEND_ICON_SHRINK,
                                                 2.0f * IMPLOT_LEGEND_ICON_SHRINK),
                ImGui::GetItemRectMin() +
                    ImVec2(icon_width - 2.0f * IMPLOT_LEGEND_ICON_SHRINK,
                           ImGui::GetItemRectSize().y - 2.0f * IMPLOT_LEGEND_ICON_SHRINK),
                ImGui::GetColorU32(
                    ImGui::GetColorU32(ImPlot::GetColormapColor(static_cast<int>(i))),
                    row_hovered ? 0.75f : 1.0f));
            if(row_hovered)
            {
                hovered_idx = static_cast<int>(i);
                if(row_clicked)
                {
                    ToggleSelectEvent(i);
                }
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();
    }
}

int
SummaryTopEvents::PlotHoverIdx() const
{
    int idx = -1;
    if(ImPlot::IsPlotHovered() && m_op_events)
    {
        ImPlotPoint mouse_pos = ImPlot::GetPlotMousePos();
        switch(m_display_mode)
        {
            case Pie:
            {
                if(sqrt(mouse_pos.x * mouse_pos.x + mouse_pos.y * mouse_pos.y) <=
                   PIE_CHART_RADIUS)
                {
                    double angle = fmod(
                        -atan2(mouse_pos.x, mouse_pos.y) * 180.0 / PI + 360.0, 360.0);
                    for(size_t i = 0; i < m_pie.slices.size(); i++)
                    {
                        if(angle >= m_pie.slices[i].angle &&
                           angle < m_pie.slices[i].angle + m_pie.slices[i].size_angle)
                        {
                            idx = static_cast<int>(i);
                            break;
                        }
                    }
                }
                break;
            }
            case Bar:
            {
                for(size_t i = 0; i < m_op_events->top.size(); i++)
                {
                    if(mouse_pos.x >=
                           static_cast<double>(i) - BAR_CHART_THICKNESS / 2.0 &&
                       mouse_pos.x <=
                           static_cast<double>(i) + BAR_CHART_THICKNESS / 2.0 &&
                       mouse_pos.y >= 0.0 &&
                       mouse_pos.y <=
                           m_op_events->top[i]
                               .metrics[kRPVAnalysisSummaryTopEventMetricDurationSum]
                               .numeric.uint)
                    {
                        idx = static_cast<int>(i);
                        break;
                    }
                }
                break;
            }
        }
    }
    return idx;
}

void
SummaryTopEvents::PlotInputHandler()
{
    if(m_hovered_idx && m_op_events && m_hovered_idx < m_op_events->top.size())
    {
        if(ImGui::IsItemHovered())
        {
            if(BeginItemTooltipStyled())
            {
                ImGui::Text("Invocation(s): %s",
                            m_op_events->top[m_hovered_idx.value()]
                                .metrics[kRPVAnalysisSummaryTopEventMetricInvocations]
                                .str.c_str());
                ImGui::Text("Total Duration: %s %s",
                            m_op_events->top[m_hovered_idx.value()]
                                .metrics[kRPVAnalysisSummaryTopEventMetricDurationSum]
                                .str.c_str(),
                            timeformat_sufix(
                                m_settings.GetUserSettings().unit_settings.time_format)
                                .c_str());
                EndTooltipStyled();
            }
        }
        if(ImGui::IsItemClicked())
        {
            ToggleSelectEvent(m_hovered_idx.value());
        }
    }
}

void
SummaryTopEvents::ToggleSelectEvent(const size_t& idx)
{
    if(m_op_events && idx < m_op_events->top.size())
    {
        if(m_selected_idx == idx || m_padded_idx == idx)
        {
            m_selected_idx = std::nullopt;
            m_instance_table->Clear();
        }
        else
        {
            m_selected_idx = idx;
            m_instance_table->SearchInstances(
                m_op_events->top[idx].name,
                m_filters.back().query[m_filters.back().selected_idx]);
        }
    }
}

SummaryTopEvents::InstanceTable::InstanceTable(
    DataProvider& dp, std::shared_ptr<TimelineSelection> timeline_selection,
    rocprofvis_analysis_summary_category_t type, TableType table_type,
    rocprofvis_controller_table_type_t request_table_type, uint64_t request_id)
: InfiniteScrollTable(
      dp, table_type, request_table_type, request_id,
      [&dp]() -> const TablesModel& { return dp.DataModel().GetSummary().GetTables(); },
      [&dp]() -> TablesModel& { return dp.DataModel().GetSummary().GetTables(); },
      timeline_selection)
, m_type(type)
, m_source_filter(std::nullopt)
, m_fetched(false)
{
    m_widget_name = GenUniqueName("summary_top_event_instances");
}

void
SummaryTopEvents::InstanceTable::Render()
{
    const ImGuiStyle& style = m_settings.GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kBgPanel));
    if(m_fetched)
    {
        ImGui::BeginChild("##status", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
        ImGui::Text("Showing %llu invocation(s)",
                    m_table_model().GetTableTotalRowCount(m_table_type));
        ImGui::EndChild();
        InfiniteScrollTable::Render();
    }
    else
    {
        ImGui::BeginChild("##status", ImGui::GetContentRegionAvail(),
                          ImGuiChildFlags_Borders);
        ImGui::SetCursorPos(
            (ImGui::GetContentRegionAvail() -
             ImGui::CalcTextSize("Select an event to view its invocation(s).")) *
            0.5f);
        ImGui::TextDisabled("Select an event to view its invocation(s).");
        ImGui::EndChild();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void
SummaryTopEvents::InstanceTable::SearchInstances(
    const std::string&                            event_name,
    std::optional<rocprofvis_dm_query_criteria_t> source_filter)
{
    m_event_name    = event_name;
    m_source_filter = source_filter;
    RequestFetch();
    m_fetched = true;
}

void
SummaryTopEvents::InstanceTable::Clear()
{
    m_data_provider.CancelRequest(m_request_id);
    m_table_model_mutable().ClearTable(m_table_type);
    m_fetched = false;
}

void
SummaryTopEvents::InstanceTable::UpdateFetchParams(
    std::shared_ptr<TableRequestParams>& params) const
{
    if(!params)
    {
        params = std::make_shared<EventSearchRequestParams>();
    }
    InfiniteScrollTable::UpdateFetchParams(params);
    if(params)
    {
        std::shared_ptr<EventSearchRequestParams> search_params =
            std::static_pointer_cast<EventSearchRequestParams>(params);
        const TimelineModel& timeline = m_data_provider.DataModel().GetTimeline();
        switch(m_type)
        {
            case kRPVAnalysisSummaryCategoryKernel:
            {
                search_params->m_op_types = { kRocProfVisDmOperationDispatch };
                break;
            }
            case kRPVAnalysisSummaryCategoryMemory:
            {
                search_params->m_op_types = { kRocProfVisDmOperationMemoryAllocate,
                                              kRocProfVisDmOperationMemoryCopy };
                break;
            }
            case kRPVAnalysisSummaryCategoryThread:
            {
                search_params->m_op_types = { kRocProfVisDmOperationLaunch,
                                              kRocProfVisDmOperationLaunchSample };
                break;
            }
            default:
            {
                ROCPROFVIS_ASSERT(false);
                break;
            }
        }
        search_params->m_string_table_filters = { m_event_name };
        search_params->m_include_substrings   = false;
        search_params->m_include_category     = false;
        search_params->m_partial_matching     = false;
        params->m_start_ts                    = timeline.GetStartTime();
        params->m_end_ts                      = timeline.GetEndTime();
        params->m_source_filter               = m_source_filter;
    }
}

}  // namespace View
}  // namespace RocProfVis
