// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_compute_model_types.h"
#include <memory>
#include <unordered_map>

namespace RocProfVis
{
namespace View
{

class ComputeKernelSelectionTable
{
public:
    ComputeKernelSelectionTable();
    ~ComputeKernelSelectionTable() = default;

    const std::vector<std::string>&              GetTableHeader() const;
    const std::vector<std::vector<std::string>>& GetTableData() const;

    const ComputeTableInfo& GetTableInfo() const;
    ComputeTableInfo& GetTableInfoMutable();

    void SetTableInfo(const ComputeTableInfo& info);

    void Clear();

private:
    ComputeTableInfo m_table_info;
};


class ComputeDataModel
{
public:
    ComputeDataModel();
    ~ComputeDataModel() = default;

    const std::unordered_map<uint32_t, WorkloadInfo>& GetWorkloads() const;
    const std::vector<std::unique_ptr<MetricValue>>&  GetMetricsData() const;

    void AddWorkload(WorkloadInfo& workload);
    bool AddMetricValue(uint32_t workload_id, uint32_t kernel_id, uint32_t category_id,
                        uint32_t table_id, uint32_t entry_id, std::string& value_name,
                        double value);

    void Clear();
    void ClearMetricValues();

    ComputeKernelSelectionTable& GetKernelSelectionTable();

private:
    std::unordered_map<uint32_t, WorkloadInfo> m_workloads;
    std::vector<std::unique_ptr<MetricValue>>  m_metrics_data;

    ComputeKernelSelectionTable m_kernel_selection_table;
};

}  // namespace View
}  // namespace RocProfVis
