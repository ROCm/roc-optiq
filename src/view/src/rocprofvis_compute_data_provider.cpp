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
    {"18.12_L2-Fabric_(128B_read_requests_per_normUnit).csv", metric_group_info_t{metric_table_info_t{"L2 Cache - Fabric 128B Read Requests Per Channel"}, {}}},
    {"pmc_perf.csv", metric_group_info_t{metric_table_info_t{}, {}}}, 
    {"roofline.csv", metric_group_info_t{metric_table_info_t{}, {}}}
};

metric_roofline_info_t ROOFLINE_DEFINITIONS {{"HBM", "L2", "L1", "LDS"}, {
    roofline_format_info_t{"FP32", kRooflineNumberformatFP32, {{kRooflinePipeVALU, "FP32Flops"}, {kRooflinePipeMFMA, "MFMAF32Flops"}}},
    roofline_format_info_t{"FP64", kRooflineNumberformatFP64, {{kRooflinePipeVALU, "FP64Flops"}, {kRooflinePipeMFMA, "MFMAF64Flops"}}},
    roofline_format_info_t{"FP16", kRooflineNumberformatFP16, {{kRooflinePipeMFMA, "MFMAF16Flops"}}},
    roofline_format_info_t{"INT8", kRooflineNumberformatINT8, {{kRooflinePipeMFMA, "MFMAFI8Ops"}}}
}};

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

void ComputeDataProvider::SetProfilePath(std::filesystem::path& path)
{
    if (m_profile_loaded)
    {
        m_metrics_group_map.clear();
        m_profile_loaded = false;
    }
    m_profile_path = path;
    m_attempt_profile_load = true;
}

void ComputeDataProvider::LoadProfile()
{
    if (!m_profile_loaded && m_attempt_profile_load)
    {
        if (std::filesystem::exists(m_profile_path) && std::filesystem::is_directory(m_profile_path))
        {        
            for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator{m_profile_path})
            {
                if (entry.path().extension() == ".csv" && METRIC_GROUP_DEFINITIONS.count(entry.path().filename().string()) > 0)
                {
                    spdlog::info("ComputeDataProvider::LoadProfile() - Loading {}", entry.path().string());

                    if (entry.path().filename().string() == "1.1.csv")
                    {
                        LoadSystemInfo(entry);
                        continue;
                    }

                    csv::CSVReader csv(entry.path().string(), m_csv_format);
                    
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
                            table_cell.m_highlight = false;

                            size_t pct_pos = table_cell.m_value.find("%");
                            if (pct_pos != std::string::npos)
                            {
                                table_cell.m_value.replace(pct_pos, 1, "%%");
                            } 

                            if (!row[col].is_str())
                            {
                                rocprofvis_compute_metric_t metric;
                                // Key is the identifier of the metric that can be used later to retrieve it from the map.
                                std::string key;
                                if (col == 0)
                                {
                                    // First column contain headers for each row, not metrics.
                                    key = "";
                                    metric.m_name = "";
                                }
                                else
                                {
                                    // Key is for any metric is [row header][space][column header].
                                    key = row[0].get().append(" ").append(table.m_column_names[col]);
                                    metric.m_name = row[0].get();
                                }
                                metric.m_value = row[col].is_num() ? row[col].get<double>() : 0;

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
            BuildRoofline();
        }

        m_profile_loaded = !m_metrics_group_map.empty();
        m_attempt_profile_load = false;
    }
}

void ComputeDataProvider::LoadSystemInfo(std::filesystem::directory_entry csv_entry)
{
    // Pulled from MI308X, others may have different fields/ordering.
    const std::array field_names = {
        "Workload Name", "Command", "IP Blocks", "Timestamp", "Version", "Host Name", "CPU Model", "SBIOS", "Linux Distro", "Linux Kernel Version", "AMD GPU Kernel Version",
        "CPU Memory", "GPU Memory", "ROCm Version", "VBIOS", "Compute Partition", "Memory Partition", "GPU Series", "GPU Model", "GPU Arch", "GPU L1", "GPU L2", "CU per GPU", 
        "SIMD per CU", "SE per CU", "Wave Size", "Workgroup Max Size", "Chip ID", "Max Waves per CU", "Max SCLK", "Max MCLK", "Current SCLK", "Current MCLK", "Total L2 Channels", 
        "LDS Banks per CU", "SQC per GPU", "Pipes per GPU", "HBM Bandwidth", "Number of XCD"
    };

    std::unique_ptr metric_group = std::make_unique<rocprofvis_compute_metrics_group_t>();

    csv::CSVReader csv(csv_entry.path().string(), m_csv_format);
    rocprofvis_compute_metrics_table_t table;
    table.m_title = METRIC_GROUP_DEFINITIONS.at(csv_entry.path().filename().string()).m_table_info.m_title;
    table.m_column_names = {"Field", "Info"};

    for (csv::CSVRow& row : csv)
    {
        std::vector<rocprofvis_compute_metrics_table_cell_t> table_row;
        table_row.push_back(rocprofvis_compute_metrics_table_cell_t{field_names[csv.n_rows() - 1], false, false, nullptr});
        table_row.push_back(rocprofvis_compute_metrics_table_cell_t{row[0].get(), false, false, nullptr});
        table.m_values.push_back(table_row);
    }
    metric_group->m_table = table;
    m_metrics_group_map[csv_entry.path().filename().string()] = std::move(metric_group);

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

            if (m_metrics_group_map.count(csv_name) > 0)
            {
                rocprofvis_compute_metrics_plot_t p;
                p.m_title = std::string(title).append("##" + csv_name + std::to_string(m_metrics_group_map[csv_name]->m_plots.size()));
                p.m_x_axis.m_label = x_axis_label; 
                p.m_x_axis.m_min = 0;
                p.m_x_axis.m_max = 0;
                p.m_y_axis.m_label = y_axis_label.empty() ? "Metric" : y_axis_label;

                rocprofvis_compute_metric_plot_series_t s;

                std::unordered_map<std::string, rocprofvis_compute_metric_t>& metrics = m_metrics_group_map[csv_name]->m_metrics;
                int count = 0;
                for (const std::string& m_key : metric_keys)
                {
                    if (metrics.count(m_key) > 0)
                    {
                        p.m_y_axis.m_tick_labels.push_back(metrics[m_key].m_name.c_str());
                        s.m_x_values.push_back(metrics[m_key].m_value);
                        s.m_y_values.push_back(count ++);
                        p.m_x_axis.m_max = std::max(p.m_x_axis.m_max, s.m_x_values.back() * 1.01f);
                        if (p.m_x_axis.m_label.empty())
                        {
                            p.m_x_axis.m_label = metrics[m_key].m_unit;
                        }
                        p.m_series = {s};
                    }
                    else if (m_key.back() == '#')
                    {
                        std::string search_metric = m_key;
                        search_metric.pop_back();
                        for (auto& metric : metrics)
                        {
                            if (metric.first.find(search_metric) != std::string::npos)
                            {
                                p.m_y_axis.m_tick_labels.push_back(metric.second.m_name.c_str());
                                s.m_x_values.push_back(metric.second.m_value);
                                s.m_y_values.push_back(count ++);
                                p.m_x_axis.m_max = std::max(p.m_x_axis.m_max, s.m_x_values.back() * 1.01f);
                                if (p.m_x_axis.m_label.empty())
                                {
                                    p.m_x_axis.m_label = metric.second.m_unit;
                                }
                            }
                        }
                        p.m_series = {s};
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
}

void ComputeDataProvider::BuildRoofline()
{
    if (m_metrics_group_map.count("roofline.csv") > 0 && m_metrics_group_map.count("pmc_perf.csv") > 0)
    {
        const int kernel_name_column = 1;
        const int dispatch_column = 0;

        std::array sort_groups = {
            std::unordered_map<std::string, std::vector<std::string>> {},
            std::unordered_map<std::string, std::vector<std::string>> {}
        };

        for (int r = 0; r < m_metrics_group_map["pmc_perf.csv"]->m_table.m_values.size(); r ++)
        {
            std::string& kernel_name = m_metrics_group_map["pmc_perf.csv"]->m_table.m_values[r][kernel_name_column].m_value;
            std::string& dispatch_id = m_metrics_group_map["pmc_perf.csv"]->m_table.m_values[r][dispatch_column].m_value;
            sort_groups[kRooflineGroupByDispatch]["Dispatch ID:" + dispatch_id + ":" + kernel_name] = {dispatch_id};
            sort_groups[kRooflineGroupByKernel]["Kernel:" + kernel_name].push_back(dispatch_id);
        }

        int series_count = 1;
        double x_max = 1000;
        double x_min = 0.01;
        double y_max = DBL_MIN;
        double y_min = DBL_MAX;
        for (std::unordered_map<std::string, std::vector<std::string>> group : sort_groups)
        {
            rocprofvis_compute_metrics_plot_t p;
            for (auto& it : group)
            {
                double flops = 0;
                double l2_data = 0;
                double l1_data = 0;
                double hbm_data = 0; 
                double duration = 0;

                const std::string& label = it.first;
                const std::vector<std::string>& dispatch_list = it.second;

                for (const std::string& dispatch_id : dispatch_list)
                {
                    flops += 64 * (m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_ADD_F16"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_MUL_F16"].m_value
                                    + 2 * m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_FMA_F16"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_TRANS_F16"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_ADD_F32"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_MUL_F32"].m_value
                                    + 2 * m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_FMA_F32"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_TRANS_F32"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_ADD_F64"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_MUL_F64"].m_value
                                    + 2 * m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_FMA_F64"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_TRANS_F64"].m_value)
                            + 512 * (m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_MFMA_MOPS_F16"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_MFMA_MOPS_BF16"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_MFMA_MOPS_F32"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " SQ_INSTS_VALU_MFMA_MOPS_F64"].m_value);
                    l2_data += 64 * (m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCP_TCC_WRITE_REQ_sum"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCP_TCC_ATOMIC_WITH_RET_REQ_sum"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCP_TCC_ATOMIC_WITHOUT_RET_REQ_sum"].m_value
                                    + m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCP_TCC_READ_REQ_sum"].m_value);
                    l1_data += 64 * m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCP_TOTAL_CACHE_ACCESSES_sum"].m_value;
                    // HBM is calculated differently on MI200 vs all others. Below is none-MI200 equation.
                    hbm_data += 32 * (m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCC_EA0_RDREQ_32B_sum"].m_value
                                    + (m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCC_EA0_WRREQ_sum"].m_value
                                    - m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCC_EA0_WRREQ_64B_sum"].m_value))
                            + 64 * (m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCC_EA0_WRREQ_64B_sum"].m_value
                                    + (m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCC_EA0_RDREQ_sum"].m_value
                                    - m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCC_BUBBLE_sum"].m_value
                                    - m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCC_EA0_RDREQ_32B_sum"].m_value))
                            + 128 * m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " TCC_BUBBLE_sum"].m_value;
                    duration += m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " End_Timestamp"].m_value
                                    - m_metrics_group_map["pmc_perf.csv"]->m_metrics[dispatch_id + " Start_Timestamp"].m_value;
                }

                if (flops > 0 && duration > 0)
                {
                    const int& num_dispatches = dispatch_list.size();
                    std::string short_name = label.substr(0, 20);
                    std::string id = std::to_string(series_count);
                    std::array intensity_definitions = {
                        std::make_pair(std::string("L1"), l1_data), 
                        std::make_pair(std::string("L2"), l2_data), 
                        std::make_pair(std::string("HBM"), hbm_data)
                    };

                    for (std::pair<std::string, double>& intensity : intensity_definitions)
                    {
                        if (intensity.second > 0)
                        {
                            double x = flops / num_dispatches / intensity.second / num_dispatches;
                            double y = flops / num_dispatches / duration / num_dispatches;
                            p.m_series.push_back(rocprofvis_compute_metric_plot_series_t{short_name + "-" + intensity.first + "##" + id, {x}, {y}});
                        
                            x_max = std::max(x_max, x * 2);
                            x_min = std::min(x_min, x / 2);
                            y_max = std::max(y_max, y * 2);
                            y_min = std::min(y_min, y / 2);
                        }
                    }
                }

                series_count ++;
            }
            m_metrics_group_map["pmc_perf.csv"]->m_plots.push_back(p);
        }

        const std::string device_id = "0";
        for (roofline_format_info_t& format : ROOFLINE_DEFINITIONS.m_number_formats)
        {
            double max_bw = 0;

            rocprofvis_compute_metrics_plot_t p;
            p.m_title = "Emperical Roofline Analysis (" + format.m_name + ")";
            if (format.m_format == kRooflineNumberformatINT8)
            {
                p.m_x_axis.m_label = "Arithmetic Intensity (IOP/Byte)";
                p.m_y_axis.m_label = "Performance (GIOP/s)";
            }
            else
            {
                p.m_x_axis.m_label = "Arithmetic Intensity (FLOP/Byte)";
                p.m_y_axis.m_label = "Performance (GFLOP/s)";
            }

            for (std::string& level : ROOFLINE_DEFINITIONS.m_memory_levels)
            {
                double& bw = m_metrics_group_map["roofline.csv"]->m_metrics[device_id + " " + level + "Bw"].m_value;
                rocprofvis_compute_metric_plot_series_t s{level + "-" + format.m_name, {0.01f}, {0.01f * bw}};
                max_bw = std::max(bw, max_bw);

                double max_ops = 0;
                for(auto& it : format.m_supported_pipes)
                {
                    max_ops = std::max(m_metrics_group_map["roofline.csv"]->m_metrics[device_id + " " + it.second].m_value, max_ops);
                }
                double x = max_ops / bw;
                double& y = max_ops;

                p.m_x_axis.m_max = std::max(x_max, x * 2);
                p.m_x_axis.m_min = std::min(x > 0 ? x_min, x / 2 : x_min, x_min);
                p.m_y_axis.m_max = std::max(y_max, y * 2);
                p.m_y_axis.m_min = std::min(y > 0 ? y_min, y / 2 : y_min, y_min);

                s.m_x_values.push_back(x);
                s.m_y_values.push_back(y);
                p.m_series.push_back(s);
            }

            for (auto& it : format.m_supported_pipes)
            {
                double& ops =  m_metrics_group_map["roofline.csv"]->m_metrics[device_id + " " + it.second].m_value;
                p.m_series.push_back(
                    rocprofvis_compute_metric_plot_series_t{(it.first == kRooflinePipeVALU ? "Peak VALU-" : "Peak MFMA-") + format.m_name, {ops / max_bw, 1000}, {ops, ops}}
                );
            }

            m_metrics_group_map["roofline.csv"]->m_plots.push_back(p);
        }
    }
    else
    {
        spdlog::info("ComputeDataProvider::BuildRoofline() - Roofline skipped");
    }
}

bool ComputeDataProvider::ProfileLoaded()
{
    return m_profile_loaded;
}

ComputeDataProvider::ComputeDataProvider() 
: m_profile_loaded(false)
, m_attempt_profile_load(true)
{
    m_csv_format.delimiter(',');
    m_csv_format.header_row(0);
}

ComputeDataProvider::~ComputeDataProvider() {}

}  // namespace View
}  // namespace RocProfVis
