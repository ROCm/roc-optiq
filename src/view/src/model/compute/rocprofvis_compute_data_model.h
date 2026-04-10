// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_compute_model_types.h"
#include <map>
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
    using MetricValuesByEntryId = std::map<uint32_t, std::shared_ptr<MetricValue>>;

    ComputeDataModel();
    ~ComputeDataModel() = default;

    const std::unordered_map<uint32_t, WorkloadInfo>& GetWorkloads() const;
    const WorkloadInfo* GetWorkload(uint32_t workload_id) const;

    const std::vector<std::shared_ptr<MetricValue>>* GetKernelMetricsData(
        uint64_t store_id, uint32_t kernel_id) const;

    void AddWorkload(WorkloadInfo& workload);
    bool AddMetricValue(uint64_t                                   store_id,
                        rocprofvis_controller_metric_source_type_t source_type,
                        uint32_t workload_id, uint32_t kernel_id, uint32_t category_id,
                        uint32_t table_id, uint32_t entry_id, std::string& value_name,
                        double value);

    // Accessor methods for metric values, by workload or kernel.

    // Workload metric accessors
    std::shared_ptr<MetricValue> GetWorkloadMetricValue(uint64_t store_id,
                                                        uint32_t workload_id,
                                                        uint32_t category_id,
                                                        uint32_t table_id,
                                                        uint32_t entry_id) const;
    std::shared_ptr<MetricValue> GetWorkloadMetricValue(uint64_t store_id,
                                                        uint32_t workload_id,
                                                        uint64_t metric_key) const;

    // Kernel metric accessors
    std::shared_ptr<MetricValue> GetKernelMetricValue(uint64_t store_id,
                                                      uint32_t kernel_id,
                                                      uint32_t category_id,
                                                      uint32_t table_id,
                                                      uint32_t entry_id) const;
    std::shared_ptr<MetricValue> GetKernelMetricValue(uint64_t store_id,
                                                      uint32_t kernel_id,
                                                      uint64_t metric_key) const;

    MetricValuesByEntryId* GetKernelMetricValuesByTable(uint64_t store_id,
                                                        uint32_t kernel_id,
                                                        uint32_t category_id,
                                                        uint32_t table_id);
    MetricValuesByEntryId* GetKernelMetricValuesByTable(uint64_t store_id,
                                                        uint32_t kernel_id,
                                                        uint64_t table_key);

    const AvailableMetrics::Entry* GetMetricInfo(uint32_t workload_id,
                                                 uint32_t category_id, uint32_t table_id,
                                                 uint32_t entry_id) const;

    static const AvailableMetrics::Entry* GetMetricInfo(const WorkloadInfo& workload,
                                                        uint32_t            category_id,
                                                        uint32_t            table_id,
                                                        uint32_t            entry_id);

    // Clear entire model (workloads, metrics, tables, etc)
    void Clear();
    // Clear all kernel and workload metric values for all stores (clients)
    void ClearAllMetricValues();

    // Clear metric values for a specific store (client)
    void ClearKernelMetricValues(uint64_t store_id);
    // Clear metric values for a specific store and kernel
    void ClearKernelMetricValues(uint64_t store_id, uint32_t kernel_id);

    // Clear metric values for a specific store (client)
    void ClearWorkloadMetricValues(uint64_t store_id);
    // Clear metric values for a specific store and workload
    void ClearWorkloadMetricValues(uint64_t store_id, uint32_t workload_id);

    ComputeKernelSelectionTable& GetKernelSelectionTable();

    std::vector<const KernelInfo*> GetKernelInfoList(uint32_t workload_id) const;

    const KernelInfo* GetKernelInfo(uint32_t workload_id, uint32_t kernel_id) const;

private:
    void OrderAvailableMetrics(WorkloadInfo& workload);

    std::unordered_map<uint32_t, WorkloadInfo> m_workloads;

    struct MetricStore
    {
        std::vector<std::shared_ptr<MetricValue>> m_metrics_data;

        // Look up map to find metric by id (ex: 2.1.3), key is generated by
        // MetricKey union.
        std::unordered_map<uint64_t, std::shared_ptr<MetricValue>> m_metrics_map;

        // Look up map to find list of metric by table id (ex: 2.1), key is generated by
        // TableKey union.
        std::unordered_map<uint64_t, MetricValuesByEntryId> m_metrics_by_table_id;
    };

    // Map of metrics organized by kernel_id
    using IdToMetricStoreMap = std::unordered_map<uint32_t, MetricStore>;

    // Maps of metrics organized by store_id (client id)
    std::unordered_map<uint64_t, IdToMetricStoreMap> m_kernel_metrics;
    std::unordered_map<uint64_t, IdToMetricStoreMap> m_workload_metrics;

    ComputeKernelSelectionTable m_kernel_selection_table;
};

}  // namespace View
}  // namespace RocProfVis
