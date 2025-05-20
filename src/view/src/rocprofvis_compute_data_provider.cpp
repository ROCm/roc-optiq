// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_data_provider.h"

namespace RocProfVis
{
namespace View
{

const std::unordered_map<std::string, metric_group_info_t> METRIC_GROUP_DEFINITIONS = {
    {"0.1_Top_Kernels.csv", metric_group_info_t{metric_table_info_t{"Kernels by Duration"}, {
        metric_plot_info_t{"Top Kernels", "", "", {"Pct#"}},
        metric_plot_info_t{"Kernels by Duration", "", "Kernel", {"Mean(ns)#"}}
    }}},
    {"0.2_Dispatch_List.csv", metric_group_info_t{metric_table_info_t{"Dispatch List"}, {}}},
    {"1.1.csv", metric_group_info_t{metric_table_info_t{"System Info"}, {}},},
    {"2.1_Speed-of-Light.csv", metric_group_info_t{metric_table_info_t{"System Speed of Light"}, {}}},
    {"3.1_Memory_Chart.csv", metric_group_info_t{metric_table_info_t{"Memory Chart"}, {}}},
    {"5.1_Command_Processor_Fetcher.csv", metric_group_info_t{metric_table_info_t{"Command Processor Fetcher"}, {}}},
    {"5.2_Packet_Processor.csv", metric_group_info_t{metric_table_info_t{"Command Processor Packet Processor"}, {}}},
    {"6.1_Workgroup_Manager_Utilizations.csv", metric_group_info_t{metric_table_info_t{"Workgroup Manager Utilizations"}, {}}},
    {"6.2_Workgroup_Manager_-_Resource_Allocation.csv", metric_group_info_t{metric_table_info_t{"Workgroup Manager Resource Allocation"}, {}}},
    {"7.1_Wavefront_Launch_Stats.csv", metric_group_info_t{metric_table_info_t{"Wavefront Launch Stats"}, {}}},
    {"7.2_Wavefront_Runtime_Stats.csv", metric_group_info_t{metric_table_info_t{"Wavefront Runtime Stats"}, {}}},
    {"10.1_Overall_Instruction_Mix.csv", metric_group_info_t{metric_table_info_t{"Compute Units Overall Instruction Mix"}, {
        metric_plot_info_t{"Overall Instruction Mix", "", "Compute Pipeline", {"VALU Avg", "VMEM Avg", "LDS Avg", "MFMA Avg", "SALU Avg", "SMEM Avg", "Branch Avg"}}
    }}},
    {"10.2_VALU_Arithmetic_Instr_Mix.csv", metric_group_info_t{metric_table_info_t{"Compute Units VALU Arithemtic Instruction Mix"}, {
        metric_plot_info_t{"Arithmetic Instruction Mix", "", "Instruction Type", {"INT32 Avg", "INT64 Avg", "F16-ADD Avg", "F16-MUL Avg", "F16-FMA Avg", "F16-Trans Avg", 
            "F32-ADD Avg", "F32-MUL Avg", "F32-FMA Avg", "F32-Trans Avg", "F64-ADD Avg", "F64-MUL Avg", "F64-FMA Avg", "F64-Trans Avg", "Conversion Avg"}}
    }}},
    {"10.3_VMEM_Instr_Mix.csv", metric_group_info_t{metric_table_info_t{"Compute Units VMEM Instruction Mix"}, {}}},
    {"10.4_MFMA_Arithmetic_Instr_Mix.csv", metric_group_info_t{metric_table_info_t{"Compute Units MFMA Arithemetic Instruction Mix"}, {}}},
    {"11.1_Speed-of-Light.csv", metric_group_info_t{metric_table_info_t{"Compute Units Speed of Light"}, {
        metric_plot_info_t{"Speed of Light", "% of Peak", "Operation", {"VALU FLOPs Pct of Peak", "VALU IOPs Pct of Peak", 
            "MFMA FLOPs (BF16) Pct of Peak", "MFMA FLOPs (F16) Pct of Peak", "MFMA FLOPs (F32) Pct of Peak", "MFMA FLOPs (F64) Pct of Peak", "MFMA IOPs (INT8) Pct of Peak"}}
    }}},
    {"11.2_Pipeline_Stats.csv", metric_group_info_t{metric_table_info_t{"Compute Units Pipeline Stats"}, {}}},
    {"11.3_Arithmetic_Operations.csv", metric_group_info_t{metric_table_info_t{"Compute Units Arithmetic Operations"}, {}}},
    {"12.1_Speed-of-Light.csv", metric_group_info_t{metric_table_info_t{"Local Data Share Speed of Light"}, {
        metric_plot_info_t{"Speed of Light", "% of Peak", "", {"Utilization Avg", "Access Rate Avg", "Theoretical Bandwidth (% of Peak) Avg", "Bank Conflict Rate Avg"}}
    }}},
    {"12.2_LDS_Stats.csv", metric_group_info_t{ metric_table_info_t{ "Local Data Share Stats" }, {}}},
    {"13.1_Speed-of-Light.csv", metric_group_info_t{metric_table_info_t{"Instruction Cache Speed of Light"}, {
        metric_plot_info_t{"Speed of Light", "% of Peak", "", {"Bandwidth Avg", "Cache Hit Rate Avg", "L1I-L2 Bandwidth Avg"}}
    }}},
    {"13.2_Instruction_Cache_Accesses.csv", metric_group_info_t{metric_table_info_t{"Instruction Cache Accesses"}, {}}},
    {"13.3_Instruction_Cache_-_L2_Interface.csv", metric_group_info_t{metric_table_info_t{"Instruction Cache - L2 Cache Interface"}, {}}},
    {"14.1_Speed-of-Light.csv", metric_group_info_t{metric_table_info_t{"Scalar L1 Data Cache Speed of Light"}, {
        metric_plot_info_t{"Speed of Light", "% of Peak", "", {"Bandwidth Avg", "Cache Hit Rate Avg", "sL1D-L2 BW Avg"}}
    }}},
    {"14.2_Scalar_L1D_Cache_Accesses.csv", metric_group_info_t{metric_table_info_t{"Scalar L1 Data Cache Accesses"}, {}}},
    {"14.3_Scalar_L1D_Cache_-_L2_Interface.csv", metric_group_info_t{metric_table_info_t{"Scaler L1 Data Cache - L2 Cache Interface"}, {}}},
    {"15.1_Address_Processing_Unit.csv", metric_group_info_t{metric_table_info_t{"Address Processing Unit"}, {}}},
    {"16.1_Speed-of-Light.csv", metric_group_info_t{metric_table_info_t{"Vector L1 Data Cache Speed of Light"}, {
        metric_plot_info_t{"Speed of Light", "% of Peak", "", {"Hit rate Avg", "Bandwidth Avg", "Utilization Avg", "Coalescing Avg"}}
    }}},
    {"16.2_L1D_Cache_Stalls_(%).csv", metric_group_info_t{metric_table_info_t{"Vector L1 Data Cache Stalls"}, {}}},
    {"16.3_L1D_Cache_Accesses.csv", metric_group_info_t{metric_table_info_t{"Vector L1 Data Cache Accesses"}, {}}},
    {"16.4_L1D_-_L2_Transactions.csv", metric_group_info_t{metric_table_info_t{"Vector L1 Data Cache - L2 Cache Transactions"}, {
        metric_plot_info_t{"L1D - L2 Non-hardware-Coherent Memory (NC) Transactions", "", "", {"NC - Read Avg", "NC - Write Avg", "NC - Atomic Avg"}},
        metric_plot_info_t{"L1D - L2 Uncached Memory (UC) Transactions", "", "", {"UC - Read Avg", "UC - Write Avg", "UC - Atomic Avg"}},
        metric_plot_info_t{"L1D - L2 Read/Write Coherent Memory (RW) Transactions", "", "", {"RW - Read Avg", "RW - Write Avg", "RW - Atomic Avg"}},            
        metric_plot_info_t{"L1D - L2 Coherently Cachable (CC) Transactions", "", "", {"CC - Read Avg", "CC - Write Avg", "CC - Atomic Avg"}}
    }}},
    {"16.5_L1D_Addr_Translation.csv", metric_group_info_t{metric_table_info_t{"Vector L1 Data Cache Address Translation"}, {}}},
    {"17.1_Speed-of-Light.csv", metric_group_info_t{metric_table_info_t{"L2 Cache Speed of Light"}, {
        metric_plot_info_t{"Speed of Light", "% of Peak", "", {"Utilization Avg", "Bandwidth Avg", "Hit Rate Avg"}},
        metric_plot_info_t{"Speed of Light", "Gb/s", "", {"L2-Fabric Read BW Avg", "L2-Fabric Write and Atomic BW Avg"}}
    }}},
    {"17.2_L2_-_Fabric_Transactions.csv", metric_group_info_t{metric_table_info_t{"L2 Cache - Fabric Transactions"}, {}}},
    {"17.3_L2_Cache_Accesses.csv", metric_group_info_t{metric_table_info_t{"L2 Cache Accesses"}, {}}},
    {"17.4_L2_-_Fabric_Interface_Stalls.csv", metric_group_info_t{metric_table_info_t{"L2 Cache - Fabric Interface Stalls"}, {
        metric_plot_info_t{"L2 - Infinity Fabric Interface Stalls", "%", "", {"Read - PCIe Stall Avg", "Read - Infinity Fabric™ Stall Avg", "Read - HBM Stall Avg"}},
        metric_plot_info_t{"L2 - Infinity Fabric Interface Stalls", "%", "", {"Write - PCIe Stall Avg", "Write - Infinity Fabric™ Stall Avg", "Write - HBM Stall Avg", "Write - Credit Starvation Avg"}}
    }}},
    {"17.5_L2_-_Fabric_Detailed_Transaction_Breakdown.csv", metric_group_info_t{metric_table_info_t{"L2 Cache - Fabric Detailed Transaction Breakdown"}, {}}},
    {"18.1_Aggregate_Stats_(All_channels).csv", metric_group_info_t{metric_table_info_t{"L2 Cache Aggregate Stats"}, {}}},
    {"18.2_L2_Cache_Hit_Rate_(pct)", metric_group_info_t{ metric_table_info_t{ "L2 Cache Hit Rate Per Channel" }, {}}},
    {"18.3_L2_Requests_(per_normUnit).csv", metric_group_info_t{metric_table_info_t{"L2 Cache Requests Per Channel"}, {}}},
    {"18.4_L2_Requests_(per_normUnit).csv", metric_group_info_t{metric_table_info_t{"L2 Cache Requests Per Channel"}, {}}},
    {"18.5_L2-Fabric_Requests_(per_normUnit).csv", metric_group_info_t{metric_table_info_t{"L2 Cache - Fabric Requests Per Channel"}, {}}},
    {"18.6_L2-Fabric_Read_Latency_(Cycles).csv", metric_group_info_t{metric_table_info_t{"L2 Cache - Fabric Read Latency Per Channel"}, {}}},
    {"18.7_L2-Fabric_Write_and_Atomic_Latency_(Cycles).csv", metric_group_info_t{metric_table_info_t{"L2 Cache - Fabric Write and Atomic Latency Per Channel"}, {}}},
    {"18.8_L2-Fabric_Atomic_Latency_(Cycles).csv", metric_group_info_t{metric_table_info_t{"L2 Cache - Fabric Atomic Latency Per Channel"}, {}}},
    {"18.9_L2-Fabric_Read_Stall_(Cycles_per_normUnit).csv", metric_group_info_t{metric_table_info_t{"L2 Cache - Fabric Read Stall Per Channel"}, {}}},
    {"18.10_L2-Fabric_Write_and_Atomic_Stall_(Cycles_per_normUnit).csv", metric_group_info_t{metric_table_info_t{"L2 Cache - Fabric Detailed Transaction Breakdown"}, {}}},
    {"18.12_L2-Fabric_(128B_read_requests_per_normUnit).csv", metric_group_info_t{metric_table_info_t{"L2 Cache - Fabric 128B Read Requests Per Channel"}, {}}}
};

rocprofvis_compute_metrics_group_t* ComputeDataProvider::GetMetricGroup(std::string& group_id)
{
    rocprofvis_compute_metrics_group_t* metric_group = nullptr;
    if(m_metrics_group_map.count(group_id) > 0)
    {
        metric_group = m_metrics_group_map[group_id].get();
    }
    return metric_group;
}

rocprofvis_compute_metric_t* ComputeDataProvider::GetMetric(std::string& group_id, std::string& metric_id)
{
    rocprofvis_compute_metric_t* metric = nullptr;
    rocprofvis_compute_metrics_group_t* metric_group = GetMetricGroup(group_id);
    if (metric_group && metric_group->m_metrics.count(metric_id) > 0)
    {
        metric = &metric_group->m_metrics[metric_id];
    }
    return metric;
}

void ComputeDataProvider::SetMetricsPath(std::filesystem::path& path)
{
    if (m_metrics_loaded)
    {
        m_metrics_group_map.clear();
        m_metrics_loaded = false;
    }
    m_metrics_path = path;
    m_attempt_metrics_load = true;
}

void ComputeDataProvider::LoadMetricsFromCSV()
{
    if (!m_metrics_loaded && m_attempt_metrics_load)
    {
        if (std::filesystem::exists(m_metrics_path) && std::filesystem::is_directory(m_metrics_path))
        {        
            for (auto& entry : std::filesystem::directory_iterator{m_metrics_path})
            {
                if (entry.path().extension() == ".csv" && METRIC_GROUP_DEFINITIONS.count(entry.path().filename().string()) > 0)
                {
                    spdlog::info("ComputeDataProvider::LoadMetricsFromCSV() - Loading {}", entry.path().string());
                    csv::CSVReader csv(entry.path().string(), csv_format);
                    
                    std::unique_ptr metric_group = std::make_unique<rocprofvis_compute_metrics_group_t>();
                    
                    rocprofvis_compute_metrics_table_t table;
                    table.m_title = METRIC_GROUP_DEFINITIONS.at(entry.path().filename().string()).m_table_info.m_title;
                    table.m_column_names = csv.get_col_names();
                    int unit_col = (csv.index_of("Unit") != csv::CSV_NOT_FOUND) ? csv.index_of("Unit") : csv.index_of("Units");
                    std::string unit;

                    for (csv::CSVRow& row : csv)
                    {
                        std::vector<rocprofvis_compute_metrics_table_cell_t> table_row;

                        for (int col = 0; col < row.size(); col ++)
                        {
                            rocprofvis_compute_metrics_table_cell_t table_cell;
                            table_cell.m_value = row[col].get();
                            table_cell.m_metric = nullptr;
                            table_cell.m_colorize = false;

                            size_t pct_pos = table_cell.m_value.find("%");
                            if (pct_pos != std::string::npos)
                            {
                                table_cell.m_value.replace(pct_pos, 1, "%%");
                            } 

                            if (!row[col].is_str())
                            {
                                rocprofvis_compute_metric_t metric;
                                std::string key = row[0].get().append(" ").append(table.m_column_names[col]);
                                metric.m_name = row[0].get();
                                metric.m_value = row[col].is_num() ? row[col].get<float>() : 0;

                                if (table.m_column_names[col] == "Pct of Peak")
                                {
                                    unit = "% of peak";
                                }
                                else if (table.m_column_names[col].find("(ns)") != std::string::npos)
                                {
                                    unit = "Ns";
                                }
                                else if (unit_col != csv::CSV_NOT_FOUND)
                                {
                                    unit = row[unit_col].get();
                                    size_t pct_pos = unit.find("Pct");
                                    if (pct_pos != std::string::npos)
                                    {
                                        unit.replace(pct_pos, 3, "%");
                                    }            
                                }
                                else
                                {
                                    if (metric.m_name.find("Lat") != std::string::npos)
                                    {
                                        unit = "Cycles";
                                    }
                                    else if (metric.m_name.find("Hit") != std::string::npos || 
                                             metric.m_name.find("Coales") != std::string::npos || 
                                             metric.m_name.find("Util") != std::string::npos ||
                                             metric.m_name.find("Stall") != std::string::npos)
                                    {
                                        unit = "%";
                                    }
                                    else
                                    {
                                        unit = "";
                                    }
                                }

                                metric.m_unit = unit;
                                metric_group->m_metrics[key] = metric;

                                table_cell.m_metric = &metric_group->m_metrics[key];
                                if ((table.m_column_names[col] == "Avg" && metric.m_unit == ("%")) || metric.m_unit == "% of peak")
                                {
                                    table_cell.m_colorize = true;
                                }
                            }

                            table_row.push_back(table_cell);
                        }

                        table.m_values.push_back(table_row);
                    }

                    metric_group->m_table = table;
                    m_metrics_group_map[entry.path().filename().string()] = std::move(metric_group);
                }
            }
            BuildPlots();
            m_metrics_loaded = true;
        }
        else
        {
            spdlog::info("ComputeDataProvider::LoadMetricsFromCSV() - Invalid path");
        }
    }
    m_attempt_metrics_load = false;
}

void ComputeDataProvider::BuildPlots()
{
    for (auto& group : METRIC_GROUP_DEFINITIONS)
    {
        const std::string& csv_name = group.first;
        const std::vector<metric_plot_info_t>& plot_infos = group.second.m_plot_infos;
        for (const metric_plot_info_t& plot : plot_infos)
        {
            const std::string& title = plot.m_title;
            const std::string& x_axis_label = plot.x_axis_label;
            const std::string& y_axis_label = plot.y_axis_label;
            const std::vector<std::string>& metric_keys = plot.m_metric_names;

            rocprofvis_compute_metrics_plot_t p;
            p.m_title = std::string(title).append("##" + csv_name + std::to_string(m_metrics_group_map[csv_name]->m_plots.size()));
            p.m_x_axis.m_label = x_axis_label; 
            p.m_x_axis.m_min = 0;
            p.m_x_axis.m_max = 0;
            p.m_y_axis.m_label = y_axis_label.empty() ? "Metric" : y_axis_label;

            std::unordered_map<std::string, rocprofvis_compute_metric_t>& metrics = m_metrics_group_map[csv_name]->m_metrics;
            for (const std::string& m_key : metric_keys)
            {
                if (metrics.count(m_key) > 0)
                {
                    p.m_series_names.push_back(metrics[m_key].m_name.c_str());
                    p.m_values.push_back(metrics[m_key].m_value);
                    if (p.m_values.back() > p.m_x_axis.m_max)
                    {
                        p.m_x_axis.m_max = p.m_values.back() * 1.01f;
                    }
                    if (p.m_x_axis.m_label.empty())
                    {
                        p.m_x_axis.m_label = metrics[m_key].m_unit;
                    }
                }
                else if (m_key.back() == '#')
                {
                    std::string search_metric = m_key;
                    search_metric.pop_back();
                    for (auto& metric : metrics)
                    {
                        if (metric.first.find(search_metric) != std::string::npos)
                        {
                            p.m_series_names.push_back(metric.second.m_name.c_str());
                            p.m_values.push_back(metric.second.m_value);
                            if (p.m_values.back() > p.m_x_axis.m_max)
                            {
                                p.m_x_axis.m_max = p.m_values.back() * 1.01f;
                            }
                            if (p.m_x_axis.m_label.empty())
                            {
                                p.m_x_axis.m_label = metric.second.m_unit;
                            }
                        }
                    }
                }
                else
                {
                    spdlog::error("ComputeDataProvider::BuildPlots() - Invalid metric {}, {}", csv_name, m_key);
                }
            }
            if (p.m_x_axis.m_label.find('%') != std::string::npos || p.m_x_axis.m_max == 0)
            {
                p.m_x_axis.m_max = 100;
            }
            m_metrics_group_map[csv_name]->m_plots.push_back(p);
        }
    }
}

bool ComputeDataProvider::MetricsLoaded()
{
    return m_metrics_loaded;
}

ComputeDataProvider::ComputeDataProvider() 
: m_metrics_loaded(false)
, m_attempt_metrics_load(true)
, m_metrics_path(std::filesystem::path("C:/Users/drchen/OneDrive - Advanced Micro Devices Inc/Documents/Notes/workloads/monte_carlo/MI308X/analyze/dfs"))
{
    csv_format.delimiter(',');
    csv_format.header_row(0);
}

ComputeDataProvider::~ComputeDataProvider() {}

}  // namespace View
}  // namespace RocProfVis
