// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <regex>
#include "implot.h"
#include "rocprofvis_compute_data_provider.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

typedef enum compute_metric_group_render_modes_t
{
    kComputeMetricTable = 1 << 0,
    kComputeMetricPie = 1 << 1,
    kComputeMetricBar = 1 << 2
} compute_metric_group_render_modes_t;

typedef int compute_metric_group_render_flags_t;

class ComputeMetric
{
public:
    const std::string FormattedString();
    void Update();
    ComputeMetric(std::shared_ptr<ComputeDataProvider> data_provider, std::string metric_category, std::string metric_id, std::string display_name);
    ~ComputeMetric();

private:
    std::shared_ptr<ComputeDataProvider> m_data_provider;
    std::string m_metric_category;
    std::string m_metric_id;
    std::string m_display_name;
    rocprofvis_compute_metric_t* m_metric_data;
    std::string m_formatted_string;
};

class ComputeMetricGroup : public RocWidget
{
public:
    void Render();
    void Update();
    void Search(const std::string& term);
    ComputeMetricGroup(std::shared_ptr<ComputeDataProvider> data_provider, std::string metric_category, 
                  compute_metric_group_render_flags_t render_flags = kComputeMetricTable, std::vector<int> pie_data_index = {0}, std::vector<int> bar_data_index = {0});
    ~ComputeMetricGroup();

private:
    std::shared_ptr<ComputeDataProvider> m_data_provider;
    std::string m_metric_category;
    compute_metric_group_render_flags_t m_render_flags;
    rocprofvis_compute_metrics_group_t* m_metric_data;
    std::vector<int> m_bar_data_index;
    std::vector<int> m_pie_data_index;
};

class ComputeMetricRoofline : public RocWidget
{
public:
    roofline_grouping_mode_t GetGroupMode();
    void SetGroupMode(roofline_grouping_mode_t grouping);
    void Render();
    void Update();
    ComputeMetricRoofline(std::shared_ptr<ComputeDataProvider> data_provider, roofline_grouping_mode_t grouping);
    ~ComputeMetricRoofline();

private:
    std::shared_ptr<ComputeDataProvider> m_data_provider;
    rocprofvis_compute_metrics_group_t* m_roof_data;
    rocprofvis_compute_metrics_group_t* m_intensity_data;
    roofline_grouping_mode_t m_group_mode;
};

}  // namespace View
}  // namespace RocProfVis
