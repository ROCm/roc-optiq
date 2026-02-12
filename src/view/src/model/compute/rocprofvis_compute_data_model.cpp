// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_data_model.h"

namespace RocProfVis
{
namespace View
{

ComputeKernelSelectionTable::ComputeKernelSelectionTable() {}

const std::vector<std::string>&
ComputeKernelSelectionTable::GetTableHeader() const
{
    return m_table_info.table_header;
}

const std::vector<std::vector<std::string>>&
ComputeKernelSelectionTable::GetTableData() const
{
    return m_table_info.table_data;
}

const ComputeTableInfo&
ComputeKernelSelectionTable::GetTableInfo() const
{
    return m_table_info;
}

ComputeTableInfo&
ComputeKernelSelectionTable::GetTableInfoMutable()
{
    return m_table_info;
}

void
ComputeKernelSelectionTable::Clear()
{
    m_table_info.table_header.clear();
    m_table_info.table_data.clear();
    m_table_info.table_params = nullptr;
}

ComputeDataModel::ComputeDataModel() {}

const std::unordered_map<uint32_t, WorkloadInfo>&
ComputeDataModel::GetWorkloads() const
{
    return m_workloads;
}

const std::vector<std::unique_ptr<MetricValue>>&
ComputeDataModel::GetMetricsData() const
{
    return m_metrics_data;
}

void
ComputeDataModel::AddWorkload(WorkloadInfo& workload)
{
    m_workloads[workload.id] = std::move(workload);
}

bool
ComputeDataModel::AddMetricValue(uint32_t workload_id, uint32_t kernel_id,
                                 uint32_t category_id, uint32_t table_id,
                                 uint32_t entry_id, std::string& value_name, double value)
{
    bool valid = false;
    if(m_workloads.count(workload_id))
    {
        WorkloadInfo& workload = m_workloads.at(workload_id);
        if(workload.kernels.count(kernel_id) &&
           workload.available_metrics.tree.count(category_id))
        {
            KernelInfo&                 kernel = workload.kernels.at(kernel_id);
            AvailableMetrics::Category& category =
                workload.available_metrics.tree.at(category_id);
            if(category.tables.count(table_id) &&
               category.tables.at(table_id).entries.count(entry_id))
            {
                AvailableMetrics::Entry& entry =
                    category.tables.at(table_id).entries.at(entry_id);
                m_metrics_data.push_back(std::make_unique<MetricValue>(
                    MetricValue{ value_name, value, entry, kernel }));
                valid = true;
            }
        }
    }
    return valid;
}

void
ComputeDataModel::Clear()
{
    m_metrics_data.clear();
    m_workloads.clear();
}

void
ComputeDataModel::ClearMetricValues()
{
    m_metrics_data.clear();
}

ComputeKernelSelectionTable&
ComputeDataModel::GetKernelSelectionTable()
{
    return m_kernel_selection_table;
}

}  // namespace View
}  // namespace RocProfVis
