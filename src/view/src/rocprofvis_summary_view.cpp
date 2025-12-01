// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_summary_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "implot/implot.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_infinite_scroll_table.h"
#include <cmath>
#include <cstring>
#include <sstream>

namespace RocProfVis
{
namespace View
{

constexpr float  IMPLOT_LEGEND_ICON_SHRINK       = 2.0f;  // Implot_internal.h
constexpr double PIE_CHART_RADIUS                = 1.0;
constexpr double BAR_CHART_THICKNESS             = 0.67;
constexpr ImVec2 CHART_FIT_PADDING               = ImVec2(0.1f, 0.1f);
constexpr float  FILTER_COMBO_RELATIVE_MIN_WIDTH = 17.0f;
constexpr float  LOADING_DOT_SIZE                = 5.0f;
constexpr float  LOADING_DOT_SPACING             = 5.0f;
constexpr int    LOADING_DOT_COUNT               = 3;
constexpr float  LOADING_DOT_SPEED               = 5.0f;
constexpr ImVec2 INITIAL_RELATIVE_POS            = ImVec2(0.1f, 0.2f);
constexpr float  INITIAL_RELATIVE_SIZE           = 0.8f;

SummaryView::SummaryView(DataProvider& dp)
: m_data_provider(dp)
, m_settings(SettingsManager::GetInstance())
, m_h_container(nullptr)
, m_v_container(nullptr)
, m_kernel_instance_table(nullptr)
, m_top_kernels(nullptr)
, m_hw_utilization(nullptr)
, m_open(SettingsManager::GetInstance().GetAppWindowSettings().show_summary)
, m_fetched(false)
{
    m_kernel_instance_table = std::make_shared<KernelInstanceTable>(m_data_provider);
    m_top_kernels           = std::make_shared<TopKernels>(
        m_data_provider,
        [this](const char* kernel_name, const uint64_t* node_id,
               const uint64_t* device_id) {
            m_kernel_instance_table->ToggleSelectKernel(kernel_name, node_id, device_id);
        },
        [this]() { m_kernel_instance_table->Clear(); });
    m_hw_utilization = std::make_shared<HWUtilization>(m_data_provider);
    m_v_container    = std::make_shared<VSplitContainer>(
        LayoutItem::CreateFromWidget(m_top_kernels),
        LayoutItem::CreateFromWidget(m_kernel_instance_table));
    m_v_container->SetSplit(0.5f);
    m_h_container =
        std::make_unique<HSplitContainer>(LayoutItem::CreateFromWidget(m_hw_utilization),
                                          LayoutItem::CreateFromWidget(m_v_container));
    m_h_container->SetSplit(0.25f);
}

void
SummaryView::Update()
{
    if(m_data_provider.GetState() == ProviderState::kReady)
    {
        if(m_kernel_instance_table)
        {
            m_kernel_instance_table->Update();
        }
        if(m_hw_utilization)
        {
            m_hw_utilization->Update();
        }
        if(m_top_kernels)
        {
            m_top_kernels->Update();
        }
        if(m_settings.GetAppWindowSettings().show_summary && !m_fetched &&
           !m_data_provider.IsRequestPending(DataProvider::SUMMARY_REQUEST_ID))
        {
            m_fetched = m_data_provider.FetchSummary();
        }
    }
}

void
SummaryView::Render()
{
    if(m_h_container && m_v_container && m_settings.GetAppWindowSettings().show_summary)
    {
        m_v_container->SetMinTopHeight(m_top_kernels->MinHeight());
        m_v_container->SetMinBottomHeight(m_kernel_instance_table->MinHeight());
        m_h_container->SetMinLeftWidth(m_hw_utilization->MinWidth());
        m_h_container->SetMinRightWidth(m_top_kernels->MinWidth());
        ImGui::SetNextWindowPos(ImGui::GetWindowSize() * INITIAL_RELATIVE_POS,
                                ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImGui::GetWindowSize() * INITIAL_RELATIVE_SIZE,
                                 ImGuiCond_FirstUseEver);
        ImGui::Begin("Summary", &m_settings.GetAppWindowSettings().show_summary,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoScrollWithMouse);
        if(m_data_provider.IsRequestPending(DataProvider::SUMMARY_REQUEST_ID) ||
           m_data_provider.GetState() == ProviderState::kLoading)
        {
            ImGui::SetCursorPos(
                (ImGui::GetWindowContentRegionMax() -
                 MeasureLoadingIndicatorDots(LOADING_DOT_SIZE, LOADING_DOT_COUNT,
                                             LOADING_DOT_SPACING)) *
                0.5f);
            RenderLoadingIndicatorDots(
                LOADING_DOT_SIZE, LOADING_DOT_COUNT, LOADING_DOT_SPACING,
                m_settings.GetColor(Colors::kTextMain), LOADING_DOT_SPEED);
        }
        else
        {
            ImGui::SetNextWindowSizeConstraints(
                ImVec2(m_h_container->GetMinSize(), m_v_container->GetMinSize()),
                ImVec2(FLT_MAX, FLT_MAX));
            ImGui::BeginChild("summary_clamped_view", ImVec2(0, 0), ImGuiChildFlags_None,
                              ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            m_h_container->Render();
            ImGui::EndChild();
        }
        ImGui::End();
    }
}

HWUtilization::HWUtilization(DataProvider& dp)
: m_data_provider(dp)
, m_settings(SettingsManager::GetInstance())
, m_aligned_bar_dirty(true)
, m_aligned_bar_position(0.0f)
, m_min_width(0.0f)
{}

void
HWUtilization::Update()
{
    if(m_aligned_bar_dirty)
    {
        m_aligned_bar_position = 0.0f;
        m_min_width            = 0.0f;
        m_aligned_bar_dirty    = false;
    }
}

void
HWUtilization::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        m_settings.GetDefaultIMGUIStyle().FramePadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        m_settings.GetDefaultIMGUIStyle().ItemSpacing);
    ImGui::PushStyleColor(ImGuiCol_Header, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                          m_settings.GetColor(Colors::kTransparent));
    const summary_info_t::AggregateMetrics& metrics = m_data_provider.GetSummaryInfo();
    ImGui::TextUnformatted("Hardware Utilization");
    ImGui::Separator();
    ImGui::TextUnformatted("GPU");
    if(ImGui::TreeNodeEx("Compute", ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_Leaf |
                                        ImGuiTreeNodeFlags_Framed))
    {
        AlignedBar(metrics, metrics.gpu.gfx_utilization);
        if(metrics.gpu.gfx_utilization &&
           ImGui::TreeNodeEx("Node##gfx", ImGuiTreeNodeFlags_Framed))
        {
            for(const summary_info_t::AggregateMetrics& node : metrics.sub_metrics)
            {
                AlignedBar(node, node.gpu.gfx_utilization);
                if(node.gpu.gfx_utilization &&
                   ImGui::TreeNodeEx("Device##gfx", ImGuiTreeNodeFlags_Framed))
                {
                    for(const summary_info_t::AggregateMetrics& device : node.sub_metrics)
                    {
                        if(device.device_type == kRPVControllerProcessorTypeGPU)
                        {
                            AlignedBar(device, device.gpu.gfx_utilization);
                        }
                    }
                    ImGui::TreePop();
                }
                if(ImGui::IsItemToggledOpen())
                {
                    m_aligned_bar_dirty = true;
                }
            }
            ImGui::TreePop();
        }
        if(ImGui::IsItemToggledOpen())
        {
            m_aligned_bar_dirty = true;
        }
        ImGui::TreePop();
    }
    if(ImGui::IsItemToggledOpen())
    {
        m_aligned_bar_dirty = true;
    }
    if(ImGui::TreeNodeEx("Memory", ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_Leaf |
                                       ImGuiTreeNodeFlags_Framed))
    {
        AlignedBar(metrics, 0.0f,
                   metrics.gpu.mem_utilization
                       ? MemoryString(metrics.gpu.mem_utilization.value()).c_str()
                       : "N/A");
        if(metrics.gpu.mem_utilization &&
           ImGui::TreeNodeEx("Node##mem", ImGuiTreeNodeFlags_Framed))
        {
            for(const summary_info_t::AggregateMetrics& node : metrics.sub_metrics)
            {
                AlignedBar(node, 0.0f,
                           node.gpu.mem_utilization
                               ? MemoryString(node.gpu.mem_utilization.value()).c_str()
                               : "N/A");
                if(node.gpu.mem_utilization &&
                   ImGui::TreeNodeEx("Device##mem", ImGuiTreeNodeFlags_Framed))
                {
                    for(const summary_info_t::AggregateMetrics& device : node.sub_metrics)
                    {
                        if(device.device_type == kRPVControllerProcessorTypeGPU)
                        {
                            AlignedBar(
                                device, 0.0f,
                                device.gpu.mem_utilization
                                    ? MemoryString(device.gpu.mem_utilization.value())
                                          .c_str()
                                    : "N/A");
                        }
                    }
                    ImGui::TreePop();
                }
                if(ImGui::IsItemToggledOpen())
                {
                    m_aligned_bar_dirty = true;
                }
            }
            ImGui::TreePop();
        }
        if(ImGui::IsItemToggledOpen())
        {
            m_aligned_bar_dirty = true;
        }
        ImGui::TreePop();
    }
    if(ImGui::IsItemToggledOpen())
    {
        m_aligned_bar_dirty = true;
    }
    ImGui::Separator();
    // CPU...
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

float
HWUtilization::MinWidth() const
{
    return m_min_width;
}

std::string
HWUtilization::MemoryString(float mb)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    if(mb >= 1000.0f * 1000.0f)
    {
        oss << (mb / (1000.0f * 1000.0f)) << " TB";
    }
    else if(mb >= 1000.0f)
    {
        oss << (mb / 1000.0f) << " GB";
    }
    else
    {
        oss << mb << " MB";
    }
    return oss.str();
}

void
HWUtilization::AlignedBar(const summary_info_t::AggregateMetrics& info,
                          const std::optional<float>& value, const char* overlay)
{
    if(info.type == kRPVControllerSummaryAggregationLevelNode)
    {
        if(info.name)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(info.name.value().c_str());
        }
    }
    else if(info.type == kRPVControllerSummaryAggregationLevelProcessor)
    {
        if(info.name)
        {
            ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, 0.0f);
            ImGui::AlignTextToFramePadding();
            if(info.device_type_index)
            {
                ImGui::TextUnformatted(
                    std::to_string(info.device_type_index.value()).c_str());
                ImGui::SameLine();
                ImGui::TextUnformatted(": ");
                ImGui::SameLine();
            }
            ImGui::TextUnformatted(info.name.value().c_str());
            ImGui::PopStyleVar();
        }
    }
    ImGui::SameLine();
    m_aligned_bar_position = std::max(m_aligned_bar_position, ImGui::GetCursorPosX());
    ImGui::SameLine(m_aligned_bar_position);
    if(value)
    {
        ImGui::ProgressBar(value.value() / 100.0f, ImVec2(-1.0f, 0.0f), overlay);
    }
    else
    {
        ImGui::ProgressBar(0.0f, ImVec2(-1.0f, 0.0f), "N/A");
    }
    m_min_width = std::max(
        m_min_width,
        ImGui::GetScrollMaxY()
            ? m_aligned_bar_position +
                  ImGui::CalcTextSize(overlay ? overlay : (value ? "100%" : "N/A")).x +
                  2 * ImGui::GetStyle().FramePadding.x
            : m_aligned_bar_position +
                  ImGui::CalcTextSize(overlay ? overlay : (value ? "100%" : "N/A")).x +
                  2 * ImGui::GetStyle().FramePadding.x + ImGui::GetStyle().ScrollbarSize);
}

TopKernels::TopKernels(
    DataProvider& dp,
    std::function<void(const char* kernel_name, const uint64_t* node_id,
                       const uint64_t* device_id)>
                          selection_callback,
    std::function<void()> unselection_callback)
: m_data_provider(dp)
, m_settings(SettingsManager::GetInstance())
, m_selection_callback(selection_callback)
, m_unselection_callback(unselection_callback)
, m_data_dirty(false)
, m_filter_dirty(false)
, m_node_combo(NodeFilterComboModel{ {}, {}, 0 })
, m_gpu_combo(GPUFilterComboModel{ {}, {}, {}, 0, nullptr })
, m_kernel_pie(KernelPieModel{ {}, {}, {} })
, m_kernels(nullptr)
, m_display_mode(Pie)
, m_show_legend(true)
, m_hovered_idx(std::nullopt)
, m_selected_idx(std::nullopt)
, m_padded_idx(std::nullopt)
, m_min_size(0.0f, 0.0f)
{
    m_data_provider.SetSummaryDataReadyCallback([this]() {
        m_data_dirty   = true;
        m_filter_dirty = true;
    });
}

TopKernels::~TopKernels() { m_data_provider.SetSummaryDataReadyCallback(nullptr); }

void
TopKernels::Update()
{
    if(m_data_provider.GetState() == ProviderState::kReady)
    {
        if(m_data_dirty)
        {
            m_node_combo = NodeFilterComboModel{ { nullptr }, { "-" }, 0 };
            for(const summary_info_t::AggregateMetrics& node :
                m_data_provider.GetSummaryInfo().sub_metrics)
            {
                if(node.type == kRPVControllerSummaryAggregationLevelNode && node.id &&
                   node.name)
                {
                    m_node_combo.info.push_back(&node);
                    m_node_combo.labels.push_back(node.name.value().c_str());
                }
            }
            m_data_dirty = false;
        }
        if(m_filter_dirty)
        {
            m_kernels      = nullptr;
            m_kernel_pie   = KernelPieModel{};
            m_selected_idx = std::nullopt;
            m_padded_idx   = std::nullopt;
            if(m_unselection_callback)
            {
                m_unselection_callback();
            }
            if(m_node_combo.selected_idx > 0 &&
               m_node_combo.selected_idx < m_node_combo.info.size())
            {
                if(m_gpu_combo.parent_info !=
                       m_node_combo.info[m_node_combo.selected_idx] &&
                   m_node_combo.info[m_node_combo.selected_idx])
                {
                    m_gpu_combo = GPUFilterComboModel{
                        { nullptr },
                        { "-" },
                        { "-" },
                        0,
                        m_node_combo.info[m_node_combo.selected_idx]
                    };
                    for(const summary_info_t::AggregateMetrics& device :
                        m_gpu_combo.parent_info->sub_metrics)
                    {
                        if(device.type ==
                               kRPVControllerSummaryAggregationLevelProcessor &&
                           device.id && device.name && device.device_type)
                        {
                            switch(device.device_type.value())
                            {
                                case kRPVControllerProcessorTypeGPU:
                                {
                                    m_gpu_combo.info.push_back(&device);
                                    m_gpu_combo.labels.emplace_back(
                                        std::to_string(device.device_type_index.value()) +
                                        ": " + device.name.value());
                                    m_gpu_combo.labels_ptr.push_back(
                                        m_gpu_combo.labels.back().c_str());
                                    break;
                                }
                                case kRPVControllerProcessorTypeCPU:
                                {
                                    break;
                                }
                            }
                        }
                    }
                }
                if(m_gpu_combo.selected_idx > 0 &&
                   m_gpu_combo.selected_idx < m_gpu_combo.info.size())
                {
                    m_kernels =
                        &m_gpu_combo.info[m_gpu_combo.selected_idx]->gpu.top_kernels;
                }
                else
                {
                    m_kernels =
                        &m_node_combo.info[m_node_combo.selected_idx]->gpu.top_kernels;
                }
            }
            else
            {
                m_gpu_combo =
                    GPUFilterComboModel{ { nullptr }, { "-" }, { "-" }, 0, nullptr };
                m_kernels = &m_data_provider.GetSummaryInfo().gpu.top_kernels;
            }
            if(m_kernels && !m_kernels->empty())
            {
                for(const summary_info_t::KernelMetrics& kernel : (*m_kernels))
                {
                    m_kernel_pie.labels.push_back(kernel.name.c_str());
                    m_kernel_pie.exec_time_pct.push_back(kernel.exec_time_pct);
                }
                float angle_sum = 0.0f;
                for(const float& exec_time_pct : m_kernel_pie.exec_time_pct)
                {
                    float angle = exec_time_pct * 360.0f;
                    m_kernel_pie.slices.push_back({ angle_sum, angle });
                    angle_sum += angle;
                }
                if((*m_kernels).back().name == "Others")
                {
                    m_padded_idx = (*m_kernels).size() - 1;
                }
            }
            m_filter_dirty = false;
        }
    }
}

void
TopKernels::Render()
{
    if(!m_data_dirty && !m_filter_dirty && m_kernels)
    {
        const ImVec2       region     = ImGui::GetContentRegionAvail();
        const ImGuiStyle&  style      = ImGui::GetStyle();
        const ImPlotStyle& plot_style = ImPlot::GetStyle();
        ImGui::SetCursorPos(
            ImVec2(region.x * 0.5f - plot_style.PlotBorderSize -
                       ImGui::CalcTextSize("Top Kernels by Execution Time").x * 0.5f,
                   plot_style.PlotPadding.y));
        ImGui::TextUnformatted("Top Kernels by Execution Time");
        if(m_kernels->empty())
        {
            ImGui::GetWindowDrawList()->AddRect(
                ImGui::GetCursorScreenPos() +
                    ImVec2(plot_style.PlotBorderSize + plot_style.PlotPadding.x,
                           plot_style.PlotBorderSize + plot_style.PlotPadding.y),
                ImGui::GetWindowPos() + region -
                    ImVec2(plot_style.PlotBorderSize + plot_style.PlotPadding.x,
                           2 * plot_style.PlotPadding.y + ImGui::GetFrameHeight()),
                ImGui::GetColorU32(style.Colors[ImGuiCol_TableBorderStrong]));
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
            ImPlot::PushColormap("flame");
            switch(m_display_mode)
            {
                case Pie:
                {
                    RenderPieChart(region, plot_style, hovered_idx);
                    break;
                }
                case Bar:
                {
                    RenderBarChart(region, plot_style, time_format, hovered_idx);
                    break;
                }
                case Table:
                {
                    RenderTable(plot_style, time_format);
                    break;
                }
            }
            // Legend...
            if(m_display_mode == Pie || m_display_mode == Bar)
            {
                RenderLegend(region, style, plot_style, hovered_idx);
            }
            ImPlot::PopColormap();
            m_hovered_idx = hovered_idx == -1 ? std::nullopt
                                              : std::make_optional<size_t>(hovered_idx);
        }
        // Chart/table switcher
        ImGui::SetCursorPos(ImVec2(plot_style.PlotPadding.x,
                                   region.y - ImGui::GetFrameHeightWithSpacing() -
                                       plot_style.PlotPadding.y));
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
        ImGui::SameLine();
        if(IconButton(ICON_LIST,
                      m_settings.GetFontManager().GetIconFont(FontType::kDefault),
                      ImVec2(0, 0), nullptr, ImVec2(0, 0), false, style.FramePadding,
                      m_settings.GetColor(m_display_mode == Table ? Colors::kButton
                                                                  : Colors::kTransparent),
                      m_settings.GetColor(Colors::kButtonHovered),
                      m_settings.GetColor(Colors::kButtonActive)))
        {
            m_display_mode = Table;
        }
        ImGui::EndGroup();
        const float mode_cluster_width = ImGui::GetItemRectSize().x;
        // Filter combos...
        ImGui::SetCursorPos(ImVec2(region.x - ImGui::CalcTextSize("Node:GPU:").x -
                                       region.x / 2.0f - 4.0f * plot_style.PlotPadding.x,
                                   region.y - ImGui::GetFrameHeightWithSpacing() -
                                       plot_style.PlotPadding.y));
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, plot_style.PlotPadding.x);
        ImGui::BeginDisabled(m_node_combo.info.size() < 2);
        ImGui::TextUnformatted("Node:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(region.x * 0.25f);
        if(ImGui::Combo("##node_combo", &m_node_combo.selected_idx,
                        m_node_combo.labels.data(),
                        static_cast<int>(m_node_combo.labels.size())))
        {
            m_filter_dirty = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        ImGui::BeginDisabled(m_node_combo.selected_idx == 0);
        ImGui::TextUnformatted("GPU:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(region.x * 0.25f);
        if(ImGui::Combo("##gpu_combo", &m_gpu_combo.selected_idx,
                        m_gpu_combo.labels_ptr.data(),
                        static_cast<int>(m_gpu_combo.labels.size())))
        {
            m_filter_dirty = true;
        }
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
        // Update size requirements...
        m_min_size.x =
            ImGui::GetFrameHeightWithSpacing() * FILTER_COMBO_RELATIVE_MIN_WIDTH;
        m_min_size.y =
            ImGui::CalcTextSize("Total Duration (timecode)").x +
            2.0f * (ImGui::GetFrameHeightWithSpacing() + plot_style.PlotPadding.y);
    }
}

float
TopKernels::MinWidth() const
{
    return m_min_size.x;
}

float
TopKernels::MinHeight() const
{
    return m_min_size.y;
}

void
TopKernels::RenderPieChart(const ImVec2 region, const ImPlotStyle& plot_style,
                           int& hovered_idx)
{
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, CHART_FIT_PADDING);
    ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImPlot::PushStyleColor(ImPlotCol_FrameBg, m_settings.GetColor(Colors::kTransparent));
    if(ImPlot::BeginPlot("##Pie",
                         ImVec2(-1, region.y - 2 * ImGui::GetFrameHeightWithSpacing() -
                                        plot_style.PlotPadding.y),
                         ImPlotFlags_Equal | ImPlotFlags_NoFrame |
                             ImPlotFlags_CanvasOnly))
    {
        ImPlot::SetupAxes(nullptr, nullptr,
                          ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoDecorations |
                              ImPlotAxisFlags_NoHighlight,
                          ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoDecorations |
                              ImPlotAxisFlags_NoHighlight);
        hovered_idx = PlotHoverIdx();
        ImPlot::PlotPieChart(
            m_kernel_pie.labels.data(), m_kernel_pie.exec_time_pct.data(),
            m_kernel_pie.exec_time_pct.size(), 0.0, 0.0, PIE_CHART_RADIUS,
            [](double value, char* buff, int size, void* user_data) -> int {
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
                &m_kernel_pie.exec_time_pct[m_hovered_idx.value()], 1, 0.0, 0.0,
                PIE_CHART_RADIUS,
                [](double value, char* buff, int size, void* user_data) -> int {
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
                nullptr, m_kernel_pie.slices[m_hovered_idx.value()].angle + 90);
            ImGui::PopID();
            ImPlot::PopColormap();
        }
        ImPlot::EndPlot();
        ImPlot::PopStyleColor(2);
        ImPlot::PopStyleVar();
    }
    PlotInputHandler();
}

void
TopKernels::RenderBarChart(const ImVec2 region, const ImPlotStyle& plot_style,
                           TimeFormat time_format, int& hovered_idx)
{
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, CHART_FIT_PADDING);
    ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImPlot::PushStyleColor(ImPlotCol_FrameBg, m_settings.GetColor(Colors::kTransparent));
    if(ImPlot::BeginPlot("##Bar",
                         ImVec2(-1, region.y - 2 * ImGui::GetFrameHeightWithSpacing() -
                                        plot_style.PlotPadding.y),
                         ImPlotFlags_NoTitle | ImPlotFlags_NoLegend |
                             ImPlotFlags_CanvasOnly))
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
        for(size_t i = 0; i < (*m_kernels).size(); i++)
        {
            if(i == m_hovered_idx)
            {
                ImPlot::PushColormap("white");
            }
            ImPlot::SetNextFillStyle(ImPlot::GetColormapColor(i));
            ImPlot::PlotBars((*m_kernels)[i].name.c_str(), &(*m_kernels)[i].exec_time_sum,
                             1, BAR_CHART_THICKNESS, i);
            if(i == m_hovered_idx)
            {
                ImPlot::PopColormap();
            }
        }
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleColor(2);
    ImPlot::PopStyleVar();
    PlotInputHandler();
}

void
TopKernels::RenderTable(const ImPlotStyle& plot_style, TimeFormat time_format)
{
    ImGui::SetCursorPos(ImVec2(plot_style.PlotBorderSize + plot_style.PlotPadding.x,
                               ImGui::GetFontSize() + plot_style.PlotBorderSize +
                                   plot_style.PlotPadding.y +
                                   2 * plot_style.LabelPadding.y));
    if(ImGui::BeginTable(
           "Table", 5,
           ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable,
           ImGui::GetContentRegionAvail() -
               ImVec2(plot_style.PlotBorderSize + plot_style.PlotPadding.x,
                      ImGui::GetFrameHeightWithSpacing() + 2 * plot_style.PlotPadding.y +
                          plot_style.PlotBorderSize)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Invocations");
        ImGui::TableSetupColumn(
            std::string("Total Duration (" + timeformat_sufix(time_format) + ")")
                .c_str());
        ImGui::TableSetupColumn(
            std::string("Max Duration (" + timeformat_sufix(time_format) + ")").c_str());
        ImGui::TableSetupColumn(
            std::string("Min Duration (" + timeformat_sufix(time_format) + ")").c_str());
        ImGui::TableHeadersRow();
        for(size_t i = 0; i < m_kernels->size(); i++)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if(ImGui::Selectable((*m_kernels)[i].name.c_str(), m_selected_idx == i,
                                 ImGuiSelectableFlags_SpanAllColumns |
                                     ImGuiSelectableFlags_AllowItemOverlap))
            {
                ToggleSelectKernel(i);
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(
                i == m_padded_idx ? "-"
                                  : std::to_string((*m_kernels)[i].invocations).c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(
                nanosecond_to_formatted_str((*m_kernels)[i].exec_time_sum, time_format)
                    .c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(i == m_padded_idx
                                       ? "-"
                                       : nanosecond_to_formatted_str(
                                             (*m_kernels)[i].exec_time_max, time_format)
                                             .c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(i == m_padded_idx
                                       ? "-"
                                       : nanosecond_to_formatted_str(
                                             (*m_kernels)[i].exec_time_min, time_format)
                                             .c_str());
        }
        ImGui::EndTable();
    }
}

void
TopKernels::RenderLegend(const ImVec2 region, const ImGuiStyle& style,
                         const ImPlotStyle& plot_style, int& hovered_idx)
{
    ImVec2 legend_pos = ImVec2(0.75f * region.x - 2 * plot_style.PlotPadding.x,
                               ImGui::GetFontSize() + plot_style.PlotBorderSize +
                                   3 * plot_style.PlotPadding.y);
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
        ImGui::SetCursorPos(ImVec2(region.x - 2 * plot_style.PlotPadding.x -
                                       ImGui::GetFrameHeightWithSpacing(),
                                   ImGui::GetFontSize() + plot_style.PlotBorderSize +
                                       3 * plot_style.PlotPadding.y));
        if(ImGui::ArrowButton("show_legend", ImGuiDir_Left))
        {
            m_show_legend = true;
        }
    }
    if(m_show_legend)
    {
        ImGui::SetCursorPos(legend_pos);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(region.x * 0.25f, 0),
            ImVec2(region.x * 0.25f,
                   ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() -
                       3 * plot_style.PlotPadding.y - plot_style.PlotBorderSize));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, plot_style.LegendInnerPadding);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, style.Colors[ImGuiCol_WindowBg]);
        ImGui::BeginChild("legend", ImVec2(region.x * 0.25f, 0),
                          ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);
        float legend_width     = ImGui::GetWindowWidth() - 2 * style.WindowPadding.x;
        float icon_width       = ImGui::GetFontSize();
        float elide_width      = ImGui::CalcTextSize(" [...]").x;
        float scroll_bar_width = ImGui::GetScrollMaxY() ? style.ScrollbarSize : 0.0f;
        ImGui::BeginChild(
            "legend_scroll_view", ImVec2(legend_width - scroll_bar_width, 0),
            ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollWithMouse);
        for(size_t i = 0; i < m_kernels->size(); i++)
        {
            float text_width = ImGui::CalcTextSize((*m_kernels)[i].name.c_str()).x;
            ImGui::PushID(i);
            ImVec2 pos = ImGui::GetCursorPos();
            bool   row_clicked =
                ImGui::Selectable("", m_selected_idx == i,
                                  m_hovered_idx == i ? ImGuiSelectableFlags_Highlight
                                                     : ImGuiSelectableFlags_None);
            bool row_hovered = ImGui::IsItemHovered();
            ImGui::SetCursorPos(pos);
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImGui::GetCursorScreenPos() +
                    ImVec2(2 * IMPLOT_LEGEND_ICON_SHRINK, 2 * IMPLOT_LEGEND_ICON_SHRINK),
                ImGui::GetCursorScreenPos() +
                    ImVec2(icon_width - 2 * IMPLOT_LEGEND_ICON_SHRINK,
                           icon_width - 2 * IMPLOT_LEGEND_ICON_SHRINK),
                ImGui::GetColorU32(ImGui::GetColorU32(ImPlot::GetColormapColor(i)),
                                   row_hovered ? 0.75f : 1.0f));
            bool elide = icon_width + text_width + scroll_bar_width > legend_width;
            if(elide)
            {
                ImGui::PushClipRect(
                    ImGui::GetCursorScreenPos(),
                    ImGui::GetCursorScreenPos() +
                        ImVec2(legend_width - scroll_bar_width - elide_width,
                               ImGui::GetFrameHeightWithSpacing()),
                    true);
            }
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetFontSize());
            ImGui::BeginDisabled(i == m_padded_idx);
            ImGui::TextUnformatted((*m_kernels)[i].name.c_str());
            if(elide)
            {
                ImGui::PopClipRect();
                ImGui::SameLine(legend_width - scroll_bar_width - elide_width);
                ImGui::TextUnformatted(" [...]");
            }
            ImGui::EndDisabled();
            if(row_hovered)
            {
                hovered_idx = i;
                if(elide)
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                                        m_settings.GetDefaultIMGUIStyle().WindowPadding);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                                        m_settings.GetDefaultStyle().FrameRounding);
                    if(ImGui::BeginItemTooltip())
                    {
                        ImGui::PushTextWrapPos(region.x * 0.5f);
                        ImGui::TextWrapped("%s", (*m_kernels)[i].name.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::EndTooltip();
                    }
                    ImGui::PopStyleVar(2);
                }
                if(row_clicked && m_selection_callback)
                {
                    ToggleSelectKernel(i);
                }
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
    }
}

int
TopKernels::PlotHoverIdx() const
{
    int idx = -1;
    if(ImPlot::IsPlotHovered() && m_kernels)
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
                    for(size_t i = 0; i < m_kernel_pie.slices.size(); i++)
                    {
                        if(angle >= m_kernel_pie.slices[i].angle &&
                           angle < m_kernel_pie.slices[i].angle +
                                       m_kernel_pie.slices[i].size_angle)
                        {
                            idx = i;
                            break;
                        }
                    }
                }
                break;
            }
            case Bar:
            {
                for(size_t i = 0; i < m_kernels->size(); i++)
                {
                    if(mouse_pos.x >= static_cast<double>(i) - PIE_CHART_RADIUS / 2.0 &&
                       mouse_pos.x <= static_cast<double>(i) + PIE_CHART_RADIUS / 2.0 &&
                       mouse_pos.y >= 0.0 && mouse_pos.y <= (*m_kernels)[i].exec_time_sum)
                    {
                        idx = i;
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
TopKernels::PlotInputHandler()
{
    if(m_hovered_idx && m_kernels && 0 <= m_hovered_idx &&
       m_hovered_idx < m_kernels->size())
    {
        if(ImGui::IsItemHovered())
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                                m_settings.GetDefaultIMGUIStyle().WindowPadding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                                m_settings.GetDefaultStyle().FrameRounding);
            if(ImGui::BeginItemTooltip())
            {
                if(m_hovered_idx != m_padded_idx)
                {
                    ImGui::TextUnformatted("Dispatch(es):");
                    ImGui::SameLine();
                    ImGui::TextUnformatted(
                        std::to_string((*m_kernels)[m_hovered_idx.value()].invocations)
                            .c_str());
                }
                ImGui::TextUnformatted("Total Duration:");
                ImGui::SameLine();
                ImGui::TextUnformatted(
                    nanosecond_to_formatted_str(
                        (*m_kernels)[m_hovered_idx.value()].exec_time_sum,
                        m_settings.GetUserSettings().unit_settings.time_format, true)
                        .c_str());
                ImGui::EndTooltip();
            }
            ImGui::PopStyleVar(2);
        }
        if(ImGui::IsItemClicked() && m_selection_callback)
        {
            ToggleSelectKernel(m_hovered_idx.value());
        }
    }
}

void
TopKernels::ToggleSelectKernel(const size_t& idx)
{
    if(m_kernels && 0 <= idx && idx < m_kernels->size())
    {
        if(m_selected_idx == idx || m_padded_idx == idx)
        {
            m_selected_idx = std::nullopt;
            m_unselection_callback();
        }
        else
        {
            m_selected_idx = idx;
            m_selection_callback(
                (*m_kernels)[m_selected_idx.value()].name.c_str(),
                0 < m_node_combo.selected_idx &&
                        m_node_combo.selected_idx < m_node_combo.info.size()
                    ? &m_node_combo.info[m_node_combo.selected_idx]->id.value()
                    : nullptr,
                0 < m_gpu_combo.selected_idx &&
                        m_gpu_combo.selected_idx < m_gpu_combo.info.size()
                    ? &m_gpu_combo.info[m_gpu_combo.selected_idx]->id.value()
                    : nullptr);
        }
    }
}

KernelInstanceTable::KernelInstanceTable(DataProvider& dp)
: InfiniteScrollTable(dp, TableType::kSummaryKernelTable)
, m_fetched(false)
, m_fetch_deferred(false)
{
    m_widget_name = GenUniqueName("summary_top_kernel_instances");
}

void
KernelInstanceTable::Update()
{
    if(m_fetch_deferred && !m_data_provider.IsRequestPending(GetRequestID()))
    {
        Fetch();
    }
    InfiniteScrollTable::Update();
}

void
KernelInstanceTable::Render()
{
    if(m_fetched)
    {
        ImGui::SetNextWindowSize(ImGui::GetContentRegionAvail() -
                                 ImVec2(0, ImGui::GetFrameHeightWithSpacing()));
        InfiniteScrollTable::Render();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Showing %llu dispatch(es)",
                    m_data_provider.GetTableTotalRowCount(m_table_type));
    }
    else
    {
        ImGui::SetCursorPos(
            (ImGui::GetContentRegionAvail() -
             ImGui::CalcTextSize("Select a kernel to view dispatch(es).")) *
            0.5f);
        ImGui::TextDisabled("Select a kernel to view dispatch(es).");
    }
}

void
KernelInstanceTable::ToggleSelectKernel(const std::string& kernel_name,
                                        const uint64_t*    node_id,
                                        const uint64_t*    device_id)
{
    m_kernel_name = kernel_name;
    if(node_id)
    {
        m_where = "nodeId = " + std::to_string(*node_id);
        if(device_id)
        {
            m_where += " AND agentId = " + std::to_string(*device_id);
        }
    }
    Fetch();
}

void
KernelInstanceTable::Clear()
{
    m_data_provider.CancelRequest(GetRequestID());
    m_data_provider.ClearTable(m_table_type);
    m_fetched = false;
}

float
KernelInstanceTable::MinHeight() const
{
    return 4.0f * ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ScrollbarSize;
}

void
KernelInstanceTable::FormatData() const
{
    std::vector<formatted_column_info_t>& formatted_column_data =
        m_data_provider.GetMutableFormattedTableData(m_table_type);
    formatted_column_data.clear();
    formatted_column_data.resize(m_data_provider.GetTableHeader(m_table_type).size());
    InfiniteScrollTable::FormatTimeColumns();
}

void
KernelInstanceTable::Fetch()
{
    m_data_provider.CancelRequest(GetRequestID());
    m_fetch_deferred = !m_data_provider.FetchTable(TableRequestParams(
        m_req_table_type, {}, { kRocProfVisDmOperationDispatch },
        m_data_provider.GetStartTime(), m_data_provider.GetEndTime(), m_where.c_str(), "",
        "", "", { m_kernel_name }, 0, m_fetch_chunk_size));
    m_fetched        = true;
}

}  // namespace View
}  // namespace RocProfVis
