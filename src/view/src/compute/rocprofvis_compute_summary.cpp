// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_summary.h"
#include "icons/rocprovfis_icon_defines.h"
#include "implot/implot.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace RocProfVis
{
namespace View
{

constexpr float       IMPLOT_LEGEND_ICON_SHRINK       = 2.0f;  // Implot_internal.h
constexpr double      PIE_CHART_RADIUS                = 1.0;
constexpr double      BAR_CHART_THICKNESS             = 0.67;
constexpr size_t      NUM_TOP_KERNELS                 = 10;
constexpr ImVec2      CHART_FIT_PADDING               = ImVec2(0.1f, 0.1f);
constexpr float       FILTER_COMBO_RELATIVE_MIN_WIDTH = 17.0f;
constexpr float       TABLE_PANEL_MIN_WIDTH            = 800.0f;
constexpr float       CHART_PANEL_MIN_WIDTH            = 700.0f;
constexpr float       TABLE_PANEL_FLEX_GROW            = 2.0f;
constexpr float       CHART_PANEL_FLEX_GROW            = 1.0f;
constexpr const char* DISPLAY_STRING_METRICS[]{ "Invocation(s)", "Total Duration",
                                                "Min Duration",  "Max Duration",
                                                "Mean Duration", "Median Duration" };

ComputeSummaryView::ComputeSummaryView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: RocWidget()
, m_data_provider(data_provider)
, m_roofline(nullptr)
, m_top_kernels(nullptr)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
{
    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto selection_changed_event =
               std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() !=
               selection_changed_event->GetSourceId())
            {
                return;
            }
            if(m_top_kernels)
            {
                m_top_kernels->SetWorkload(selection_changed_event->GetId());
            }
            if(m_roofline)
            {
                m_roofline->SetWorkload(selection_changed_event->GetId());
            }
        }
    };

    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        workload_changed_handler);

    auto metrics_fetched_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto metrics_fetched_event =
               std::dynamic_pointer_cast<ComputeMetricsFetchedEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() != metrics_fetched_event->GetSourceId())
            {
                return;
            }
        }
    };

    m_metrics_fetched_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), metrics_fetched_handler);

    m_top_kernels = std::make_unique<ComputeTopKernels>(m_data_provider);
    m_roofline    = std::make_unique<Roofline>(m_data_provider, Roofline::AllKernels);

    m_widget_name = GenUniqueName("ComputeSummaryView");
}

ComputeSummaryView::~ComputeSummaryView()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        m_workload_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), m_metrics_fetched_token);
}

void
ComputeSummaryView::Update()
{
    if(m_top_kernels)
    {
        m_top_kernels->Update();
    }
    if(m_roofline)
    {
        m_roofline->Update();
    }
}

void
ComputeSummaryView::Render()
{
    ImGui::BeginChild("summary");
    if(m_top_kernels)
    {
        m_top_kernels->Render();
    }
    if(m_roofline)
    {
        m_roofline->Render();
    }
    ImGui::EndChild();
}

ComputeTopKernels::ComputeTopKernels(DataProvider& dp)
: m_data_provider(dp)
, m_settings(SettingsManager::GetInstance())
, m_workload_changed(false)
, m_requested_workload_id(ComputeSelection::INVALID_SELECTION_ID)
, m_workload(nullptr)
, m_display_mode(Pie)
, m_show_legend(true)
, m_hovered_idx(std::nullopt)
, m_padded_idx(std::nullopt)
, m_padded_info(nullptr)
, m_time_format_changed_token(EventManager::InvalidSubscriptionToken)
{
    m_kernel_pie.metric_sets = {
        KernelPieModel::MetricSet{ KernelInfo::InvocationCount, true, {}, {} },
        KernelPieModel::MetricSet{ KernelInfo::DurationTotal, true, {}, {} },
        KernelPieModel::MetricSet{ KernelInfo::DurationMin, false, {}, {} },
        KernelPieModel::MetricSet{ KernelInfo::DurationMax, false, {}, {} },
        KernelPieModel::MetricSet{ KernelInfo::DurationMean, false, {}, {} },
        KernelPieModel::MetricSet{ KernelInfo::DurationMedian, false, {}, {} },
    };
    m_kernel_pie.selected_metric = KernelInfo::DurationTotal;
    m_kernel_bar.metric_sets     = {
        KernelBarModel::MetricSet{ KernelInfo::InvocationCount, 0, {}, "" },
        KernelBarModel::MetricSet{ KernelInfo::DurationTotal, 0, {}, "" },
        KernelBarModel::MetricSet{ KernelInfo::DurationMin, 0, {}, "" },
        KernelBarModel::MetricSet{ KernelInfo::DurationMax, 0, {}, "" },
        KernelBarModel::MetricSet{ KernelInfo::DurationMean, 0, {}, "" },
        KernelBarModel::MetricSet{ KernelInfo::DurationMedian, 0, {}, "" },
    };
    m_kernel_bar.selected_metric     = KernelInfo::DurationTotal;
    m_kernel_bar.width               = 0.0f;
    m_kernel_bar.tick_interval_dirty = false;
    m_kernel_bar.axis_label_dirty    = false;
    auto time_format_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        m_kernel_bar.tick_interval_dirty = true;
        m_kernel_bar.axis_label_dirty    = true;
    };
    m_time_format_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged), time_format_changed_handler);

    auto table_widget = std::make_shared<RocCustomWidget>([this]() { RenderTableContent(); });
    auto chart_widget = std::make_shared<RocCustomWidget>([this]() { RenderChartContent(); });

    m_flex_container.items = {
        {"top_kernels_table", table_widget, TABLE_PANEL_MIN_WIDTH, 0.0f, TABLE_PANEL_FLEX_GROW},
        {"top_kernels_chart", chart_widget, CHART_PANEL_MIN_WIDTH, 0.0f, CHART_PANEL_FLEX_GROW},
    };
    m_flex_container.gap = 0.0f;
}

ComputeTopKernels::~ComputeTopKernels()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged), m_time_format_changed_token);
}

void
ComputeTopKernels::GetTopKernels(std::vector<const KernelInfo*>& kernels)
{
    if(kernels.size() <= NUM_TOP_KERNELS)
    {
        std::sort(kernels.begin(), kernels.end(), TopKernelsComparator{});
    }
    else
    {
        std::partial_sort(kernels.begin(), kernels.begin() + NUM_TOP_KERNELS,
                          kernels.end(), TopKernelsComparator{});
        m_padded_info                   = std::make_unique<KernelInfo>();
        m_padded_info->name             = "Others";
        m_padded_info->dispatch_metrics = {};
        for(size_t i = NUM_TOP_KERNELS; i < kernels.size(); ++i)
        {
            m_padded_info->dispatch_metrics[KernelInfo::InvocationCount] +=
                kernels[i]->dispatch_metrics[KernelInfo::InvocationCount];
            m_padded_info->dispatch_metrics[KernelInfo::DurationTotal] +=
                kernels[i]->dispatch_metrics[KernelInfo::DurationTotal];
        }
        kernels.resize(NUM_TOP_KERNELS);
        kernels.push_back(m_padded_info.get());
        m_padded_idx = kernels.size() - 1;
    }
    m_kernels = kernels;
}

void
ComputeTopKernels::Update()
{
    if(m_data_provider.GetState() == ProviderState::kReady)
    {
        if(m_workload_changed)
        {
            m_workload = nullptr;
            m_kernels.clear();
            m_padded_info = nullptr;
            m_padded_idx  = std::nullopt;
            const std::unordered_map<uint32_t, WorkloadInfo>& workloads =
                m_data_provider.ComputeModel().GetWorkloads();
            if(workloads.count(m_requested_workload_id) > 0)
            {
                m_workload = &workloads.at(m_requested_workload_id);
            }
            if(m_workload)
            {
                std::vector<const KernelInfo*> all_kernels =
                    m_data_provider.ComputeModel().GetKernelInfoList(m_workload->id);
                GetTopKernels(all_kernels);
                std::array<uint64_t, KernelInfo::NumMetrics> dispatch_metrics_total{};
                std::array<uint64_t, KernelInfo::NumMetrics> dispatch_metrics_max{};
                for(const KernelInfo* kernel : m_kernels)
                {
                    for(size_t i = 0; i < kernel->dispatch_metrics.size(); i++)
                    {
                        KernelInfo::DispatchMetric metric =
                            static_cast<KernelInfo::DispatchMetric>(i);
                        dispatch_metrics_total[metric] +=
                            kernel->dispatch_metrics[metric];
                        dispatch_metrics_max[metric] =
                            std::max(dispatch_metrics_max[metric],
                                     kernel->dispatch_metrics[metric]);
                    }
                }
                std::array<float, KernelInfo::NumMetrics> accumulated_pie_angles{};
                m_kernel_pie.labels.resize(m_kernels.size());
                for(size_t i = 0; i < m_kernels.size(); i++)
                {
                    m_kernel_pie.labels[i] = m_kernels[i]->name.c_str();
                    for(size_t j = 0; j < m_kernels[i]->dispatch_metrics.size(); j++)
                    {
                        KernelInfo::DispatchMetric metric =
                            static_cast<KernelInfo::DispatchMetric>(j);
                        if(m_kernel_pie.metric_sets[metric].available)
                        {
                            m_kernel_pie.metric_sets[metric].pct_values.resize(
                                m_kernels.size());
                            m_kernel_pie.metric_sets[metric].slices.resize(
                                m_kernels.size());
                            float value = static_cast<float>(
                                              m_kernels[i]->dispatch_metrics[metric]) /
                                          dispatch_metrics_total[metric];
                            m_kernel_pie.metric_sets[metric].pct_values[i] = value;
                            value *= 360.0f;
                            m_kernel_pie.metric_sets[metric].slices[i] =
                                KernelPieModel::Slice{ accumulated_pie_angles[metric],
                                                       value };
                            accumulated_pie_angles[metric] += value;
                        }
                    }
                }
                for(KernelBarModel::MetricSet& metric_set : m_kernel_bar.metric_sets)
                {
                    metric_set.max_value = dispatch_metrics_max[metric_set.metric];
                }
            }
            m_workload_changed               = false;
            m_kernel_bar.tick_interval_dirty = true;
            m_kernel_bar.axis_label_dirty    = true;
        }
        if(m_kernel_bar.tick_interval_dirty)
        {
            for(KernelBarModel::MetricSet& metric_set : m_kernel_bar.metric_sets)
            {
                float label_width;
                switch(metric_set.metric)
                {
                    case KernelInfo::InvocationCount:
                    {
                        label_width = m_kernel_bar.width / 10.0f;
                        break;
                    }
                    default:
                    {
                        label_width =
                            ImGui::CalcTextSize(
                                std::string(nanosecond_to_formatted_str(
                                                static_cast<double>(metric_set.max_value),
                                                m_settings.GetUserSettings()
                                                    .unit_settings.time_format,
                                                true) +
                                            "gap")
                                    .c_str())
                                .x;
                        break;
                    }
                }
                FittedGraphAxisInterval fitted_interval =
                    fit_graph_axis_interval(static_cast<double>(metric_set.max_value),
                                            m_kernel_bar.width, label_width, false, 0);
                metric_set.axis_tick_label_positions.resize(
                    fitted_interval.interval_count);
                for(int i = 0; i < fitted_interval.interval_count; i++)
                {
                    metric_set.axis_tick_label_positions[i] =
                        static_cast<double>(i) * fitted_interval.interval_ns;
                }
            }
            m_kernel_bar.tick_interval_dirty = false;
        }
        if(m_kernel_bar.axis_label_dirty)
        {
            for(KernelBarModel::MetricSet& metric_set : m_kernel_bar.metric_sets)
            {
                switch(m_kernel_bar.selected_metric)
                {
                    case KernelInfo::InvocationCount:
                    {
                        metric_set.axis_title =
                            DISPLAY_STRING_METRICS[m_kernel_bar.selected_metric];
                        break;
                    }
                    default:
                    {
                        metric_set.axis_title =
                            std::string(
                                DISPLAY_STRING_METRICS[m_kernel_bar.selected_metric]) +
                            " (" +
                            timeformat_sufix(
                                m_settings.GetUserSettings().unit_settings.time_format) +
                            ")";
                        break;
                    }
                }
            }
            m_kernel_bar.axis_label_dirty = false;
        }
    }
}

void
ComputeTopKernels::Render()
{
    SectionTitle("Top Kernels by Execution Time");

    if(!m_workload || m_kernels.empty())
    {
        ImGui::TextDisabled("No data available.");
        return;
    }

    float panel_h = ImGui::GetContentRegionAvail().x * 0.25f;
    for(auto& item : m_flex_container.items)
        item.height = panel_h;

    ImPlot::PushColormap("flame");
    m_flex_container.Render();
    ImPlot::PopColormap();
}

void
ComputeTopKernels::RenderTableContent()
{
    const ImPlotStyle& plot_style = ImPlot::GetStyle();
    TimeFormat time_format = m_settings.GetUserSettings().unit_settings.time_format;

    std::optional<size_t> hovered_idx = std::nullopt;
    RenderTable(plot_style, time_format, hovered_idx);
    m_hovered_idx = hovered_idx;
}

void
ComputeTopKernels::RenderChartContent()
{
    const ImGuiStyle&  style      = ImGui::GetStyle();
    const ImPlotStyle& plot_style = ImPlot::GetStyle();
    TimeFormat         time_format = m_settings.GetUserSettings().unit_settings.time_format;
    float              avail_w     = ImGui::GetContentRegionAvail().x;
    float              controls_h  = ImGui::GetFrameHeightWithSpacing();
    float              chart_h     = ImGui::GetContentRegionAvail().y - controls_h;

    ImGui::BeginChild("chart_area", ImVec2(-1, chart_h));
    switch(m_display_mode)
    {
        case Pie: RenderPieChart(plot_style, time_format); break;
        case Bar: RenderBarChart(plot_style, time_format); break;
    }
    ImGui::EndChild();

    ImGui::SetCursorPosX(plot_style.PlotPadding.x);
    ImGui::BeginGroup();
    if(IconButton(ICON_CHART_PIE,
                  m_settings.GetFontManager().GetIconFont(FontType::kDefault),
                  ImVec2(0, 0), nullptr, ImVec2(0, 0), false, style.FramePadding,
                  m_settings.GetColor(m_display_mode == Pie ? Colors::kButton
                                                            : Colors::kTransparent),
                  m_settings.GetColor(Colors::kButtonHovered),
                  m_settings.GetColor(Colors::kButtonActive)))
    {
        m_display_mode = Pie;
    }
    ImGui::SameLine();
    if(IconButton(ICON_CHART_BAR,
                  m_settings.GetFontManager().GetIconFont(FontType::kDefault),
                  ImVec2(0, 0), nullptr, ImVec2(0, 0), false, style.FramePadding,
                  m_settings.GetColor(m_display_mode == Bar ? Colors::kButton
                                                            : Colors::kTransparent),
                  m_settings.GetColor(Colors::kButtonHovered),
                  m_settings.GetColor(Colors::kButtonActive)))
    {
        m_display_mode = Bar;
    }
    ImGui::EndGroup();

    KernelInfo::DispatchMetric* selected_metric = nullptr;
    switch(m_display_mode)
    {
        case Pie: selected_metric = &m_kernel_pie.selected_metric; break;
        case Bar: selected_metric = &m_kernel_bar.selected_metric; break;
    }
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x -
                         ImGui::CalcTextSize("Plot:").x - avail_w * 0.25f -
                         2.0f * plot_style.PlotPadding.x);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, plot_style.PlotPadding.x);
    ImGui::TextUnformatted("Plot:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(avail_w * 0.25f);
    if(selected_metric &&
       ImGui::BeginCombo("##plot_combo",
                         DISPLAY_STRING_METRICS[static_cast<int>(*selected_metric)]))
    {
        for(int i = 0; i < IM_ARRAYSIZE(DISPLAY_STRING_METRICS); i++)
        {
            if((m_display_mode == Pie ? m_kernel_pie.metric_sets[i].available
                                      : true) &&
               ImGui::Selectable(DISPLAY_STRING_METRICS[i]))
            {
                *selected_metric = static_cast<KernelInfo::DispatchMetric>(i);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();
}

void
ComputeTopKernels::SetWorkload(uint32_t id)
{
    m_requested_workload_id = id;
    m_workload_changed      = true;
}

void
ComputeTopKernels::RenderPieChart(const ImPlotStyle& plot_style, TimeFormat time_format)
{
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, CHART_FIT_PADDING);
    ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImPlot::PushStyleColor(ImPlotCol_FrameBg, m_settings.GetColor(Colors::kTransparent));
    if(ImPlot::BeginPlot(
           "##Pie", ImVec2(-1, -1),
           ImPlotFlags_Equal | ImPlotFlags_NoFrame | ImPlotFlags_CanvasOnly))
    {
        ImPlot::SetupAxes(nullptr, nullptr,
                          ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoDecorations |
                              ImPlotAxisFlags_NoHighlight,
                          ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoDecorations |
                              ImPlotAxisFlags_NoHighlight);
        PlotHoverIdx();
        ImPlot::PlotPieChart(
            m_kernel_pie.labels.data(),
            m_kernel_pie.metric_sets[m_kernel_pie.selected_metric].pct_values.data(),
            m_kernel_pie.metric_sets[m_kernel_pie.selected_metric].pct_values.size(), 0.0,
            0.0, PIE_CHART_RADIUS,
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
            ImPlot::PushColormap("white");
            ImGui::PushID(1);
            ImPlot::PlotPieChart(
                &m_kernel_pie.labels[m_hovered_idx.value()],
                &m_kernel_pie.metric_sets[m_kernel_pie.selected_metric]
                     .pct_values[m_hovered_idx.value()],
                1, 0.0, 0.0, PIE_CHART_RADIUS,
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
                nullptr,
                m_kernel_pie.metric_sets[m_kernel_pie.selected_metric]
                        .slices[m_hovered_idx.value()]
                        .angle +
                    90);
            ImGui::PopID();
            ImPlot::PopColormap();
        }
        if(ImPlot::IsPlotHovered())
        {
            RenderPlotTooltip(m_kernel_bar.selected_metric, time_format);
        }
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleColor(2);
    ImPlot::PopStyleVar();
}

void
ComputeTopKernels::RenderBarChart(const ImPlotStyle& plot_style, TimeFormat time_format)
{
    ImGui::BeginChild("bar_area", ImVec2(0, 0));
    float y_axis_width = 0.15f * ImGui::GetContentRegionAvail().x;
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, CHART_FIT_PADDING);
    ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImPlot::PushStyleColor(ImPlotCol_FrameBg, m_settings.GetColor(Colors::kTransparent));
    ImGui::SetCursorPos(ImVec2(y_axis_width, 0.0f));
    if(ImPlot::BeginPlot("##Bar", ImVec2(-1, -1),
                         ImPlotFlags_NoTitle | ImPlotFlags_NoLegend |
                             ImPlotFlags_CanvasOnly))
    {
        ImPlot::SetupAxes(
            m_kernel_bar.metric_sets[m_kernel_bar.selected_metric].axis_title.c_str(),
            nullptr, ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoHighlight,
            ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoTickLabels |
                ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_Invert);
        ImPlot::SetupAxisLimits(
            ImAxis_X1, 0.0,
            m_kernel_bar.metric_sets[m_kernel_bar.selected_metric].max_value *
                (1 + CHART_FIT_PADDING.x * 0.5),
            ImPlotCond_Always);
        if(m_kernel_bar.selected_metric != KernelInfo::InvocationCount)
        {
            ImPlot::SetupAxisFormat(
                ImAxis_X1,
                [](double value, char* buff, int size, void* user_data) -> int {
                    if(value >= 0.0 && user_data)
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
        }
        ImPlot::SetupAxisTicks(ImAxis_X1,
                               m_kernel_bar.metric_sets[m_kernel_bar.selected_metric]
                                   .axis_tick_label_positions.data(),
                               m_kernel_bar.metric_sets[m_kernel_bar.selected_metric]
                                   .axis_tick_label_positions.size());
        PlotHoverIdx();
        for(size_t i = 0; i < m_kernels.size(); i++)
        {
            if(i != m_padded_idx ||
               m_kernel_bar.selected_metric == KernelInfo::InvocationCount ||
               m_kernel_bar.selected_metric == KernelInfo::DurationTotal)
            {
                ImGui::PushID(i);
                ImGui::SetCursorScreenPos(ImVec2(
                    ImGui::GetWindowPos().x + plot_style.PlotPadding.x,
                    ImPlot::PlotToPixels(ImPlotPoint(0, i), IMPLOT_AUTO, IMPLOT_AUTO).y -
                        ImGui::GetFontSize() * 0.5f));
                ElidedText(m_kernels[i]->name.c_str(),
                           y_axis_width - plot_style.LabelPadding.x,
                           ImGui::GetContentRegionAvail().x * 0.5f, true);
                if(i == m_hovered_idx)
                {
                    ImPlot::PushColormap("white");
                }
                ImPlot::SetNextFillStyle(ImPlot::GetColormapColor(static_cast<int>(i)));
                // PlotBars(uint64_t) may be undefined on Linux, cast to ImU64.
                const ImU64 bar_value = static_cast<ImU64>(
                    m_kernels[i]->dispatch_metrics[m_kernel_bar.selected_metric]);
                ImPlot::PlotBars(m_kernels[i]->name.c_str(), &bar_value, 1,
                                 BAR_CHART_THICKNESS, i, ImPlotBarsFlags_Horizontal);
                if(i == m_hovered_idx)
                {
                    ImPlot::PopColormap();
                }
                ImGui::PopID();
            }
        }
        float plot_width = ImPlot::GetPlotSize().x;
        if(m_kernel_bar.width != plot_width)
        {
            m_kernel_bar.width               = plot_width;
            m_kernel_bar.tick_interval_dirty = true;
        }
        if(ImPlot::IsPlotHovered())
        {
            RenderPlotTooltip(m_kernel_bar.selected_metric, time_format);
        }
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleColor(2);
    ImPlot::PopStyleVar();
    ImGui::EndChild();
}

void
ComputeTopKernels::RenderTable(const ImPlotStyle& plot_style, TimeFormat time_format,
                               std::optional<size_t>& hovered_idx)
{
    if(ImGui::BeginTable(
           "Table", 2 + KernelInfo::NumMetrics,
           ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("##legend",
                                ImGuiTableColumnFlags_NoResize |
                                    ImGuiTableColumnFlags_NoSort |
                                    ImGuiTableColumnFlags_WidthFixed,
                                ImGui::GetFontSize());
        ImGui::TableSetupColumn("Name");
        for(int i = 0; i < KernelInfo::NumMetrics; i++)
        {
            KernelInfo::DispatchMetric metric =
                static_cast<KernelInfo::DispatchMetric>(i);
            switch(metric)
            {
                case KernelInfo::InvocationCount:
                {
                    ImGui::TableSetupColumn(
                        DISPLAY_STRING_METRICS[KernelInfo::InvocationCount]);
                    break;
                }
                default:
                {
                    ImGui::TableSetupColumn(
                        std::string(std::string(DISPLAY_STRING_METRICS[metric]) + " (" +
                                    timeformat_sufix(time_format) + ")")
                            .c_str());
                    break;
                }
            }
        }
        ImGui::TableHeadersRow();
        for(size_t i = 0; i < m_kernels.size(); i++)
        {
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetCursorScreenPos() +
                    ImVec2(2 * IMPLOT_LEGEND_ICON_SHRINK, 2 * IMPLOT_LEGEND_ICON_SHRINK),
                ImGui::GetCursorScreenPos() +
                    ImVec2(ImGui::GetFontSize() - 2 * IMPLOT_LEGEND_ICON_SHRINK,
                           ImGui::GetFontSize() - 2 * IMPLOT_LEGEND_ICON_SHRINK),
                ImGui::GetColorU32(
                    ImGui::GetColorU32(ImPlot::GetColormapColor(static_cast<int>(i))),
                    false ? 0.75f : 1.0f));
            ImGui::Selectable("", false,
                              (m_hovered_idx == i ? ImGuiSelectableFlags_Highlight
                                                  : ImGuiSelectableFlags_None) |
                                  ImGuiSelectableFlags_SpanAllColumns);
            if(ImGui::IsItemHovered())
            {
                hovered_idx = i;
            }
            ImGui::TableSetColumnIndex(1);
            ElidedText(m_kernels[i]->name.c_str(), ImGui::GetContentRegionAvail().x,
                       ImGui::GetWindowWidth() * 0.5f);
            for(size_t j = 0; j < m_kernels[i]->dispatch_metrics.size(); j++)
            {
                ImGui::TableSetColumnIndex(2 + j);
                KernelInfo::DispatchMetric metric =
                    static_cast<KernelInfo::DispatchMetric>(j);
                switch(metric)
                {
                    case KernelInfo::InvocationCount:
                    {
                        ImGui::Text("%u", m_kernels[i]->dispatch_metrics[metric]);
                        break;
                    }
                    case KernelInfo::DurationTotal:
                    {
                        ImGui::TextUnformatted(
                            nanosecond_to_formatted_str(
                                static_cast<double>(
                                    m_kernels[i]->dispatch_metrics[metric]),
                                time_format)
                                .c_str());
                        break;
                    }
                    default:
                    {
                        ImGui::TextUnformatted(
                            i == m_padded_idx
                                ? "-"
                                : nanosecond_to_formatted_str(
                                      static_cast<double>(
                                          m_kernels[i]->dispatch_metrics[metric]),
                                      time_format)
                                      .c_str());
                        break;
                    }
                }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void
ComputeTopKernels::RenderPlotTooltip(KernelInfo::DispatchMetric metric,
                                     TimeFormat                 time_format)
{
    if(m_hovered_idx && 0 <= m_hovered_idx && m_hovered_idx < m_kernels.size())
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            m_settings.GetDefaultIMGUIStyle().WindowPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                            m_settings.GetDefaultStyle().FrameRounding);
        if(ImGui::BeginTooltip())
        {
            ImVec2 reserved_pos = ImGui::GetCursorPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetCursorScreenPos(),
                ImGui::GetCursorScreenPos() +
                    ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize()),
                ImGui::GetColorU32(
                    ImPlot::GetColormapColor(static_cast<int>(m_hovered_idx.value()))));
            ImGui::NewLine();
            switch(metric)
            {
                case KernelInfo::InvocationCount:
                {
                    ImGui::Text(
                        "%s: %llu", DISPLAY_STRING_METRICS[metric],
                        m_kernels[m_hovered_idx.value()]->dispatch_metrics[metric]);
                    break;
                }
                default:
                {
                    ImGui::Text(
                        "%s: %s", DISPLAY_STRING_METRICS[metric],
                        nanosecond_to_formatted_str(
                            m_kernels[m_hovered_idx.value()]->dispatch_metrics[metric],
                            time_format, true)
                            .c_str());
                    break;
                }
            }
            ImGui::SetCursorPos(reserved_pos);
            ElidedText(m_kernels[m_hovered_idx.value()]->name.c_str(),
                       ImGui::GetItemRectSize().x);
            ImGui::EndTooltip();
        }
        ImGui::PopStyleVar(2);
    }
}

void
ComputeTopKernels::PlotHoverIdx()
{
    if(ImPlot::IsPlotHovered() && !m_kernels.empty())
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
                    for(size_t i = 0;
                        i < m_kernel_pie.metric_sets[m_kernel_pie.selected_metric]
                                .slices.size();
                        i++)
                    {
                        if(angle >= m_kernel_pie.metric_sets[m_kernel_pie.selected_metric]
                                        .slices[i]
                                        .angle &&
                           angle <
                               m_kernel_pie.metric_sets[m_kernel_pie.selected_metric]
                                       .slices[i]
                                       .angle +
                                   m_kernel_pie.metric_sets[m_kernel_pie.selected_metric]
                                       .slices[i]
                                       .size_angle)
                        {
                            m_hovered_idx = i;
                            break;
                        }
                    }
                }
                break;
            }
            case Bar:
            {
                for(size_t i = 0; i < m_kernels.size(); i++)
                {
                    if(mouse_pos.x >= 0.0 &&
                       mouse_pos.x <=
                           m_kernels[i]->dispatch_metrics[m_kernel_bar.selected_metric] &&
                       mouse_pos.y >=
                           static_cast<double>(i) - BAR_CHART_THICKNESS / 2.0 &&
                       mouse_pos.y <= static_cast<double>(i) + BAR_CHART_THICKNESS / 2.0)
                    {
                        m_hovered_idx = i;
                        break;
                    }
                }
                break;
            }
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
