// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_data_provider.h"

namespace RocProfVis
{
namespace View
{

rocprofvis_compute_metric_group_t* ComputeDataProvider::GetMetricGroup(std::string group)
{
    rocprofvis_compute_metric_group_t* metric_group = nullptr;
    if (m_metrics.count(group) > 0)
    {
        metric_group = m_metrics[group].get();
    }
    return metric_group;
}

void ComputeDataProvider::SetMetricsPath(std::filesystem::path path)
{
    if (m_metrics_loaded)
    {
        FreeMetrics();
        m_metrics.clear(); 
        m_metrics_loaded = false;
    }
    m_metrics_path = path;
    m_attempt_metrics_load = true;
}

void
ComputeDataProvider::LoadMetricsFromCSV()
{
    if (!m_metrics_loaded && m_attempt_metrics_load)
    {
        if (std::filesystem::exists(m_metrics_path) && std::filesystem::is_directory(m_metrics_path))
        {        
            for (auto& entry : std::filesystem::directory_iterator{m_metrics_path})
            {
                if (entry.path().extension() == ".csv")
                {
                    csv::CSVReader csv(entry.path().string(), csv_format);

                    std::unique_ptr metric_group = std::make_unique<rocprofvis_compute_metric_group_t>();
                    metric_group->m_table_column_names = csv.get_col_names();
                    std::unordered_map<int, int> numeric_columns_map;

                    for (csv::CSVRow& row : csv)
                    {
                        if (csv.n_rows() == 1)
                        {
                            int numeric_columns_count = 0;
                            for (int col = 0; col < row.size(); col ++)
                            {
                                if (row[col].is_num())
                                {
                                    numeric_columns_map[col] = numeric_columns_count;
                                    metric_group->m_plot_values.push_back(std::vector<float>{});
                                    numeric_columns_count ++;
                                }
                            }
                        }
                        if (row[0].is_str())
                        {
                            std::string plot_name = row[0].get();
                            char* plot_label = new char[plot_name.length() + 1];
                            strcpy(plot_label, plot_name.c_str());
                            metric_group->m_plot_labels.push_back(plot_label);
                        }
                        std::vector<std::string> table_values;
                        for (int col = 0; col < row.size(); col ++)
                        {
                            if (numeric_columns_map.count(col) > 0 && row[col].is_num())
                            {
                                metric_group->m_plot_values[numeric_columns_map[col]].push_back(row[col].get<float>());
                            }
                            table_values.push_back(row[col].get());
                        }

                        metric_group->m_table_values.push_back(table_values);
                    }

                    m_metrics[entry.path().filename().string()] = std::move(metric_group);
                    spdlog::info("ComputeDataProvider::LoadMetricsFromCSV() - Loaded {}", entry.path().string());
                }
            }
            m_metrics_loaded = true;
        }
        else
        {
            spdlog::info("ComputeDataProvider::LoadMetricsFromCSV() - Invalid path");
        }
    }
    m_attempt_metrics_load = false;
}

bool ComputeDataProvider::MetricsLoaded()
{
    return m_metrics_loaded;
}

void ComputeDataProvider::FreeMetrics()
{
    for (auto it = m_metrics.begin(); it != m_metrics.end(); it ++)
    {
        for (const char *label : it->second->m_plot_labels)
        {
            delete[] label;
        }
    }
}

ComputeDataProvider::ComputeDataProvider() 
: m_metrics_loaded(false)
, m_attempt_metrics_load(true)
, m_metrics_path(std::filesystem::path("C:/Users/drchen/OneDrive - Advanced Micro Devices Inc/Documents/Notes/workloads/monte_carlo/MI300/analyze/dfs"))
{
    csv_format.delimiter(',');
    csv_format.header_row(0);
}

ComputeDataProvider::~ComputeDataProvider() {
    FreeMetrics();
}

}  // namespace View
}  // namespace RocProfVis
