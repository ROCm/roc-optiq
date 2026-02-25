// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "compute/rocprofvis_compute_model_types.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_widget.h"
#include <memory>

struct ImPlotStyle;

namespace RocProfVis
{
namespace View
{

class DataProvider;
class SettingsManager;
class ComputeSelection;
class Roofline;
class ComputeTopKernels;

class ComputeSummaryView : public RocWidget
{
public:
    ComputeSummaryView(DataProvider&                     data_provider,
                       std::shared_ptr<ComputeSelection> compute_selection);
    ~ComputeSummaryView();

    void Update() override;
    void Render() override;

private:
    DataProvider& m_data_provider;

    std::shared_ptr<ComputeSelection>  m_compute_selection;
    std::unique_ptr<Roofline>          m_roofline;
    std::unique_ptr<ComputeTopKernels> m_top_kernels;

    uint64_t m_client_id;

    EventManager::SubscriptionToken m_workload_selection_changed_token;
    EventManager::SubscriptionToken m_metrics_fetched_token;
};

class ComputeTopKernels : public RocWidget
{
public:
    ComputeTopKernels(DataProvider& dp);
    ~ComputeTopKernels();

    void Update() override;
    void Render() override;

    void SetWorkload(uint32_t id);

private:
    enum DisplayMode
    {
        Pie,
        Bar,
    };
    struct KernelPieModel
    {
        struct Slice
        {
            float angle;
            float size_angle;
        };
        // Metric specific configuration...
        struct MetricSet
        {
            KernelInfo::DispatchMetric metric;  // Id.
            bool               available;   // Some metrics cannot be displayed in pie.
            std::vector<float> pct_values;  // Values as a % of 360.
            std::vector<Slice> slices;      // Geometry for hover detection.
        };
        std::vector<const char*>                      labels;
        KernelInfo::DispatchMetric                    selected_metric;
        std::array<MetricSet, KernelInfo::NumMetrics> metric_sets;
    };
    struct KernelBarModel
    {
        // Metric specific configuration...
        struct MetricSet
        {
            KernelInfo::DispatchMetric metric;     // Id.
            uint64_t                   max_value;  // Max X value for X axis definition.
            std::vector<double> axis_tick_label_positions;  // X axis tick positions for X
                                                            // axis definition.
            std::string axis_title;
        };
        KernelInfo::DispatchMetric                    selected_metric;
        std::array<MetricSet, KernelInfo::NumMetrics> metric_sets;
        float width;                // Width of plot for X axis definition.
        bool  axis_label_dirty;     // User changed time format.
        bool  tick_interval_dirty;  // User changed time format/selected_metric/width.
    };
    struct TopKernelsComparator
    {
        bool operator()(const KernelInfo* a, const KernelInfo* b) const
        {
            return a->dispatch_metrics[KernelInfo::DurationTotal] >
                   b->dispatch_metrics[KernelInfo::DurationTotal];
        }
    };

    void GetTopKernels(std::vector<const KernelInfo*>& kernels);
    void RenderPieChart(const ImPlotStyle& plot_style, TimeFormat time_format);
    void RenderBarChart(const ImPlotStyle& plot_style, TimeFormat time_format);
    void RenderTable(const ImPlotStyle& plot_style, TimeFormat time_format,
                     std::optional<size_t>& hovered_idx);
    void RenderPlotTooltip(KernelInfo::DispatchMetric metric, TimeFormat time_format);

    // Input handling...
    void PlotHoverIdx();

    // Interfaces...
    DataProvider&    m_data_provider;
    SettingsManager& m_settings;

    // Kernel/Pie specific state...
    KernelPieModel m_kernel_pie;
    KernelBarModel m_kernel_bar;

    // Common state...
    DisplayMode                    m_display_mode;
    bool                           m_show_legend;
    std::vector<const KernelInfo*> m_kernels;  // top kernels
    std::optional<size_t>          m_hovered_idx;
    std::optional<size_t>          m_padded_idx;   // "Others" position
    std::unique_ptr<KernelInfo>    m_padded_info;  // "Others" data

    // External flags...
    bool                m_workload_changed;
    const WorkloadInfo* m_workload;
    uint32_t            m_requested_workload_id;

    EventManager::SubscriptionToken m_time_format_changed_token;
};

}  // namespace View
}  // namespace RocProfVis
