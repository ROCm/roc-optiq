// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_infinite_scroll_table.h"
#include "widgets/rocprofvis_split_containers.h"

struct ImPlotStyle;

namespace RocProfVis
{
namespace View
{

class DataProvider;
class SettingsManager;
class KernelInstanceTable;
class HWUtilization;
class TopKernels;
enum class TimeFormat;

class SummaryView : public RocWidget
{
public:
    SummaryView(DataProvider& dp);
    void Update() override;
    void Render() override;

private:
    DataProvider&    m_data_provider;
    SettingsManager& m_settings;

    std::unique_ptr<HSplitContainer>     m_h_container;
    std::shared_ptr<VSplitContainer>     m_v_container;
    std::shared_ptr<KernelInstanceTable> m_kernel_instance_table;
    std::shared_ptr<HWUtilization>       m_hw_utilization;
    std::shared_ptr<TopKernels>          m_top_kernels;

    bool m_open;
    bool m_fetched;
};

class HWUtilization : public RocWidget
{
public:
    HWUtilization(DataProvider& dp);
    void Update() override;
    void Render() override;

    float MinWidth() const;

private:
    std::string MemoryString(float mb);
    void        AlignedBar(const SummaryInfo::AggregateMetrics& info,
                           const std::optional<float>& value, const char* overlay = nullptr);

    DataProvider&    m_data_provider;
    SettingsManager& m_settings;

    bool  m_aligned_bar_dirty;
    float m_aligned_bar_position;
    float m_min_width;
};

class TopKernels : public RocWidget
{
public:
    TopKernels(DataProvider& dp,
               std::function<void(const char* kernel_name, const uint64_t* node_id,
                                  const uint64_t* device_id)>
                                     selection_callback   = nullptr,
               std::function<void()> unselection_callback = nullptr);
    virtual ~TopKernels();
    void Update() override;
    void Render() override;

    float MinWidth() const;
    float MinHeight() const;

private:
    enum DisplayMode
    {
        Pie,
        Bar,
        Table,
    };
    struct NodeFilterComboModel
    {
        std::vector<const SummaryInfo::AggregateMetrics*> info;
        std::vector<const char*>                          labels;
        int                                               selected_idx;
    };
    struct GPUFilterComboModel
    {
        std::vector<const SummaryInfo::AggregateMetrics*> info;
        std::list<std::string>                            labels;
        std::vector<const char*>                          labels_ptr;
        int                                               selected_idx;
        const SummaryInfo::AggregateMetrics*              parent_info;
    };
    struct KernelPieModel
    {
        struct Slice
        {
            float angle;
            float size_angle;
        };
        std::vector<const char*> labels;
        std::vector<float>       exec_time_pct;
        std::vector<Slice>       slices;
    };

    void RenderPieChart(const ImVec2 region, const ImPlotStyle& plot_style,
                        int& hovered_idx);
    void RenderBarChart(const ImVec2 region, const ImPlotStyle& plot_style,
                        TimeFormat time_format, int& hovered_idx);
    void RenderTable(const ImPlotStyle& plot_style, TimeFormat time_format);
    void RenderLegend(const ImVec2 region, const ImGuiStyle& style,
                      const ImPlotStyle& plot_style, int& hovered_idx);

    // Input handling...
    int  PlotHoverIdx() const;
    void PlotInputHandler();
    void ToggleSelectKernel(const size_t& idx);

    // Interfaces...
    DataProvider&    m_data_provider;
    SettingsManager& m_settings;
    std::function<void(const char* kernel_name, const uint64_t* node_id,
                       const uint64_t* device_id)>
                          m_selection_callback;
    std::function<void()> m_unselection_callback;

    // Widget models...
    NodeFilterComboModel m_node_combo;
    GPUFilterComboModel  m_gpu_combo;
    KernelPieModel       m_kernel_pie;

    // Widget state/properties...
    const std::vector<SummaryInfo::KernelMetrics>* m_kernels;
    DisplayMode                                    m_display_mode;
    bool                                           m_show_legend;
    std::optional<size_t>                          m_hovered_idx;
    std::optional<size_t>                          m_selected_idx;
    std::optional<size_t> m_padded_idx;  // Position of "Others" slice
    ImVec2                m_min_size;

    // Update flags.
    bool m_data_dirty;
    bool m_filter_dirty;
};

class KernelInstanceTable : public InfiniteScrollTable
{
public:
    KernelInstanceTable(DataProvider& dp);
    void Update() override;
    void Render() override;

    void  ToggleSelectKernel(const std::string& kernel_name, const uint64_t* node_id,
                             const uint64_t* device_id);
    void  Clear();
    float MinHeight() const;

private:
    void FormatData() const override;
    void Fetch();

    std::string m_kernel_name;
    std::string m_where;

    bool m_fetched;
    bool m_fetch_deferred;
};

}  // namespace View
}  // namespace RocProfVis
