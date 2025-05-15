// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include <algorithm>
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
    double m_value;
    std::string m_unit;
} rocprofvis_compute_metric_t;

typedef struct rocprofvis_compute_metric_plot_axis_t
{
    std::string m_label;
    std::vector<const char*> m_tick_labels;
    double m_max;
    double m_min;
} rocprofvis_compute_metric_plot_axis_t;

typedef struct rocprofvis_compute_metric_plot_series_t
{
    std::string m_name;
    std::vector<double> m_x_values;
    std::vector<double> m_y_values;
} rocprofvis_compute_metric_plot_series_t;

typedef struct rocprofvis_compute_metrics_plot_t
{
    std::string m_title;
    rocprofvis_compute_metric_plot_axis_t m_x_axis;
    rocprofvis_compute_metric_plot_axis_t m_y_axis;
    std::vector<rocprofvis_compute_metric_plot_series_t> m_series;
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

typedef enum roofline_grouping_mode_t
{
    kRooflineGroupByDispatch = 0,
    kRooflineGroupByKernel
} roofline_grouping_mode_t;

typedef enum roofline_number_format_t
{
    kRooflineNumberformatFP32 = 0,
    kRooflineNumberformatFP64,
    kRooflineNumberformatFP16,
    kRooflineNumberformatINT8,
    kRooflineNumberformatCount
} roofline_number_format_t;

typedef enum roofline_pipe_t
{
    kRooflinePipeVALU = 0,
    kRooflinePipeMFMA
} roofline_pipe_t;

typedef struct roofline_format_info_t
{
    std::string m_name;
    roofline_number_format_t m_format;
    std::unordered_map<roofline_pipe_t, std::string> m_supported_pipes;
} roofline_format_info_t;

typedef struct metric_roofline_info_t
{
    std::vector<std::string> m_memory_levels;
    std::array<roofline_format_info_t, kRooflineNumberformatCount> m_number_formats;
} metric_roofline_info_t;

class ComputeDataProvider
{
public:
    rocprofvis_compute_metrics_group_t* GetMetricGroup(std::string &group_id);
    rocprofvis_compute_metric_t* GetMetric(std::string &group_id, std::string &metric_id);

    void SetProfilePath(std::filesystem::path& path);
    void LoadProfile();
    bool ProfileLoaded();

    ComputeDataProvider();
    ~ComputeDataProvider();

private:
    void LoadSystemInfo(std::filesystem::directory_entry csv_entry);
    void BuildPlots();
    void BuildRoofline();

    csv::CSVFormat m_csv_format;
    bool m_profile_loaded;
    bool m_attempt_profile_load;
    std::filesystem::path m_profile_path;

    std::unordered_map<std::string, std::unique_ptr<rocprofvis_compute_metrics_group_t>> m_metrics_group_map;
};

}  // namespace View
}  // namespace RocProfVis
