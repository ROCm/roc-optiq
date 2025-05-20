// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <filesystem>
#include <unordered_map>
#include "rocprofvis_core_assert.h"
#include "csv.hpp"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

typedef struct rocprofvis_compute_metric_t
{
    std::string m_name;
    float m_value;
    std::string m_unit;
} rocprofvis_compute_metric_t;

typedef struct rocprofvis_compute_metric_plot_axis_t
{
    std::string m_label;
    float m_max;
    float m_min;
} rocprofvis_compute_metric_plot_axis_t;

typedef struct rocprofvis_compute_metrics_plot_t
{
    std::string m_title;
    std::vector<const char*> m_series_names;
    rocprofvis_compute_metric_plot_axis_t m_x_axis;
    rocprofvis_compute_metric_plot_axis_t m_y_axis;
    std::vector<float> m_values;
} rocprofvis_compute_metric_plot_t;

typedef struct rocprofvis_compute_metrics_table_cell_t
{
    std::string m_value;
    bool m_colorize;
    rocprofvis_compute_metric_t* m_metric;
} rocprofvis_compute_metrics_table_cell_t;

typedef struct rocprofvis_compute_metrics_table_t
{
    std::string m_title;
    std::vector<std::string> m_column_names;
    std::vector<std::vector<rocprofvis_compute_metrics_table_cell_t>> m_values;
} rocprofvis_compute_metrics_table_t;

typedef struct rocprofvis_compute_metrics_group_t
{
    rocprofvis_compute_metrics_table_t m_table;
    std::vector<rocprofvis_compute_metrics_plot_t> m_plots;
    std::unordered_map<std::string, rocprofvis_compute_metric_t> m_metrics;
} rocprofvis_compute_metric_group_t;

typedef struct metric_table_info_t
{
    std::string m_title;
} metric_table_info_t;

typedef struct metric_plot_info_t
{
    std::string m_title;
    std::string x_axis_label;
    std::string y_axis_label;
    std::vector<std::string> m_metric_names;
} metric_plot_info_t;

typedef struct metric_group_info_t
{
    metric_table_info_t m_table_info;
    std::vector<metric_plot_info_t> m_plot_infos;
} metric_category_info_t;

class ComputeDataProvider
{
public:
   rocprofvis_compute_metrics_group_t* GetMetricGroup(std::string &group_id);
    rocprofvis_compute_metric_t* GetMetric(std::string &group_id, std::string &metric_id);

    void SetMetricsPath(std::filesystem::path& path);
    void LoadMetricsFromCSV();
    bool MetricsLoaded();

    ComputeDataProvider();
    ~ComputeDataProvider();

private:
    void BuildPlots();

    csv::CSVFormat csv_format;
    bool m_metrics_loaded;
    bool m_attempt_metrics_load;
    std::filesystem::path m_metrics_path;

    std::unordered_map<std::string, std::unique_ptr<rocprofvis_compute_metrics_group_t>> m_metrics_group_map;
};

}  // namespace View
}  // namespace RocProfVis
