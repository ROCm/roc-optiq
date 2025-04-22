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

typedef struct rocprofvis_compute_metric_group_t
{
    std::vector<std::string> m_table_column_names;
    std::vector<std::vector<std::string>> m_table_values;

    std::vector<const char*> m_plot_labels;
    std::vector<std::vector<float>> m_plot_values;
} rocprofvis_compute_metric_group_t;

class ComputeDataProvider
{
public:
    rocprofvis_compute_metric_group_t* GetMetricGroup(std::string group);

    void SetMetricsPath(std::filesystem::path path);
    void LoadMetricsFromCSV();
    bool MetricsLoaded();

    ComputeDataProvider();
    ~ComputeDataProvider();

private:
    void FreeMetrics();

    csv::CSVFormat csv_format;
    bool m_metrics_loaded;
    bool m_attempt_metrics_load;
    std::filesystem::path m_metrics_path;
    std::unordered_map<std::string, std::unique_ptr<rocprofvis_compute_metric_group_t>> m_metrics;

};

}  // namespace View
}  // namespace RocProfVis
