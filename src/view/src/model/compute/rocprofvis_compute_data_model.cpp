// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_data_model.h"
#include <algorithm>
#include "spdlog/spdlog.h"

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

const WorkloadInfo*
ComputeDataModel::GetWorkload(uint32_t workload_id) const
{
    if(m_workloads.count(workload_id))
    {
        return &m_workloads.at(workload_id);
    }
    return nullptr;
}

const std::vector<std::shared_ptr<MetricValue>>*
ComputeDataModel::GetKernelMetricsData(uint64_t store_id, uint32_t kernel_id) const
{
    if (m_kernel_metrics.count(store_id) && m_kernel_metrics.at(store_id).count(kernel_id)){
        return &m_kernel_metrics.at(store_id).at(kernel_id).m_metrics_data;
    }
    return nullptr;
}

void
ComputeDataModel::AddWorkload(WorkloadInfo& workload)
{
    uint32_t id = workload.id;
    m_workloads[id] = std::move(workload);
    OrderAvailableMetrics(m_workloads[id]);
}

void
ComputeDataModel::OrderAvailableMetrics(WorkloadInfo& workload)
{
    AvailableMetrics& available_metrics = workload.available_metrics;
    available_metrics.ordered_categories.clear();
    available_metrics.ordered_categories.reserve(available_metrics.tree.size());
    for(std::pair<const uint32_t, AvailableMetrics::Category>& tree_category :
        available_metrics.tree)
    {
        AvailableMetrics::Category& category = tree_category.second;
        available_metrics.ordered_categories.push_back(&tree_category.second);
        category.ordered_tables.clear();
        category.ordered_tables.reserve(category.tables.size());
        for(std::pair<const uint32_t, AvailableMetrics::Table>& tree_table :
            category.tables)
        {
            AvailableMetrics::Table& table = tree_table.second;
            category.ordered_tables.push_back(&table);
            table.ordered_entries.clear();
            table.ordered_entries.reserve(table.entries.size());
            for(const std::pair<const uint32_t, AvailableMetrics::Entry&>& tree_entry :
                table.entries)
            {
                table.ordered_entries.push_back(&tree_entry.second);
            }
            std::sort(table.ordered_entries.begin(), table.ordered_entries.end(),
                      [](const AvailableMetrics::Entry* a,
                         const AvailableMetrics::Entry* b) { return a->id < b->id; });
        }
        std::sort(category.ordered_tables.begin(), category.ordered_tables.end(),
                  [](const AvailableMetrics::Table* a, const AvailableMetrics::Table* b) {
                      return a->id < b->id;
                  });
    }
    std::sort(available_metrics.ordered_categories.begin(),
              available_metrics.ordered_categories.end(),
              [](const AvailableMetrics::Category* a,
                 const AvailableMetrics::Category* b) { return a->id < b->id; });
}

bool
ComputeDataModel::AddMetricValue(uint64_t                                   store_id,
                                 rocprofvis_controller_metric_source_type_t source_type,
                                 uint32_t workload_id, uint32_t kernel_id,
                                 uint32_t category_id, uint32_t table_id,
                                 uint32_t entry_id, std::string& value_name, double value)
{
    bool valid = false;
    if(m_workloads.count(workload_id))
    {
        WorkloadInfo& workload = m_workloads.at(workload_id);
        if(source_type == kRPVControllerMetricSourceTypeWorkload)
        {
            valid = true;
        }
        else if(source_type == kRPVControllerMetricSourceTypeKernel)
        {
            valid = workload.kernels.count(kernel_id) > 0;
        }
        
        if(valid)
        {
            valid =
                workload.available_metrics.tree.count(category_id) > 0 &&
                workload.available_metrics.tree.at(category_id).tables.count(table_id) >
                    0 &&
                workload.available_metrics.tree.at(category_id)
                        .tables.at(table_id)
                        .entries.count(entry_id) > 0;
            if(valid)
            {
                AvailableMetrics::Entry& entry =
                    workload.available_metrics.tree.at(category_id)
                        .tables.at(table_id)
                        .entries.at(entry_id);
                MetricKey metric_id{};
                metric_id.fields.category_id = category_id;
                metric_id.fields.table_id    = table_id;
                metric_id.fields.entry_id    = entry_id;

                if(source_type == kRPVControllerMetricSourceTypeWorkload)
                {
                    // Do something...(spdlog header include can be removed after)
                    spdlog::info("Workload metric: id={}.{}.{} name={} workload={} "
                                 "value_name={} value={}",
                                 category_id, table_id, entry_id, entry.name,
                                 workload.name, value_name, value);

                    MetricStore& ms = m_workload_metrics[store_id][workload_id];
                    // Check if metric value already exists for the given metric ID
                    if(ms.m_metrics_map.count(metric_id.id))
                    {
                        // Update existing metric value
                        ms.m_metrics_map[metric_id.id]->values[value_name] = value;
                        ms.m_metrics_map[metric_id.id]->entry              = &entry;
                        ms.m_metrics_map[metric_id.id]->source_type        = source_type;
                        ms.m_metrics_map[metric_id.id]->workload           = &workload;
                        ms.m_metrics_map[metric_id.id]->kernel             = nullptr;
                    }
                    else
                    {
                        // Create new workload metric value and add to the map and vector
                        auto metric = std::make_shared<MetricValue>(
                            MetricValue{ &entry,
                                         source_type,
                                         &workload,
                                         nullptr,
                                         { { value_name, value } } });

                        ms.m_metrics_data.push_back(metric);
                        ms.m_metrics_map[metric_id.id] = metric;

                        // Also add to the table ID map for easy lookup by table
                        TableKey table_id_union{};
                        table_id_union.fields.category_id = category_id;
                        table_id_union.fields.table_id    = table_id;

                        ms.m_metrics_by_table_id[table_id_union.id][entry_id] = metric;
                    }
                }
                else if(source_type == kRPVControllerMetricSourceTypeKernel)
                {
                    MetricStore& ms = m_kernel_metrics[store_id][kernel_id];
                    // Check if metric value already exists for the given metric ID
                    if(ms.m_metrics_map.count(metric_id.id))
                    {
                        // Update existing metric value
                        ms.m_metrics_map[metric_id.id]->values[value_name] = value;
                        ms.m_metrics_map[metric_id.id]->entry              = &entry;
                        ms.m_metrics_map[metric_id.id]->source_type        = source_type;
                        ms.m_metrics_map[metric_id.id]->workload           = nullptr;
                        ms.m_metrics_map[metric_id.id]->kernel =
                            &workload.kernels.at(kernel_id);
                    }
                    else
                    {
                        // Create new metric value and add to the map and vector
                        auto metric = std::make_shared<MetricValue>(
                            MetricValue{ &entry,
                                         source_type,
                                         nullptr,
                                         &workload.kernels.at(kernel_id),
                                         { { value_name, value } } });

                        ms.m_metrics_data.push_back(metric);
                        ms.m_metrics_map[metric_id.id] = metric;

                        // Also add to the table ID map for easy lookup by table
                        TableKey table_id_union{};
                        table_id_union.fields.category_id = category_id;
                        table_id_union.fields.table_id    = table_id;

                        ms.m_metrics_by_table_id[table_id_union.id][entry_id] = metric;
                    }
                }
            }
        }
        else
        {
            spdlog::warn("Could not add metric. Invalid kernel ID {} for workload {}",
                         kernel_id, workload.name);
        }
    }
    else
    {
        spdlog::warn("Could not add metric. Invalid workload ID {}", workload_id);
    }
    return valid;
}

const AvailableMetrics::Entry*
ComputeDataModel::GetMetricInfo(uint32_t workload_id, uint32_t category_id,
                                uint32_t table_id, uint32_t entry_id) const
{
    if(m_workloads.count(workload_id))
    {
        const WorkloadInfo& workload = m_workloads.at(workload_id);
        return GetMetricInfo(workload, category_id, table_id, entry_id);
    }
    return nullptr;
}

const AvailableMetrics::Entry*
ComputeDataModel::GetMetricInfo(const WorkloadInfo& workload, uint32_t category_id, uint32_t table_id,
                               uint32_t entry_id)
{
    if(workload.available_metrics.tree.count(category_id))
    {
        const AvailableMetrics::Category& category =
            workload.available_metrics.tree.at(category_id);
        if(category.tables.count(table_id) &&
           category.tables.at(table_id).entries.count(entry_id))
        {
            return &category.tables.at(table_id).entries.at(entry_id);
        }
    }
    return nullptr;
}

void
ComputeDataModel::Clear()
{
    ClearAllMetricValues();
    m_workloads.clear();
    m_kernel_selection_table.Clear();
}

void
ComputeDataModel::ClearAllMetricValues()
{
    for (auto& store_pair : m_kernel_metrics) {
        for (auto& kernel_pair : store_pair.second) {
            kernel_pair.second.m_metrics_data.clear();
            kernel_pair.second.m_metrics_map.clear();
            kernel_pair.second.m_metrics_by_table_id.clear();
        }
        store_pair.second.clear();
    }
    m_kernel_metrics.clear();

    for (auto& store_pair : m_workload_metrics) {
        for (auto& workload_pair : store_pair.second) {
            workload_pair.second.m_metrics_data.clear();
            workload_pair.second.m_metrics_map.clear();
            workload_pair.second.m_metrics_by_table_id.clear();
        }
        store_pair.second.clear();
    }
    m_workload_metrics.clear();
}

void
ComputeDataModel::ClearKernelMetricValues(uint64_t store_id)
{
    if (m_kernel_metrics.count(store_id)){
        for (auto& kernel_pair : m_kernel_metrics.at(store_id)) {
            kernel_pair.second.m_metrics_data.clear();
            kernel_pair.second.m_metrics_map.clear();
            kernel_pair.second.m_metrics_by_table_id.clear();
        }
        m_kernel_metrics.at(store_id).clear();
    }
}

void
ComputeDataModel::ClearWorkloadMetricValues(uint64_t store_id)
{
    if (m_workload_metrics.count(store_id)){
        for (auto& workload_pair : m_workload_metrics.at(store_id)) {
            workload_pair.second.m_metrics_data.clear();
            workload_pair.second.m_metrics_map.clear();
            workload_pair.second.m_metrics_by_table_id.clear();
        }
        m_workload_metrics.at(store_id).clear();
    }
}

void
ComputeDataModel::ClearKernelMetricValues(uint64_t store_id, uint32_t kernel_id)
{
    if (m_kernel_metrics.count(store_id)){
        if( m_kernel_metrics.at(store_id).count(kernel_id)) {
            MetricStore& ms = m_kernel_metrics.at(store_id).at(kernel_id);
            ms.m_metrics_data.clear();
            ms.m_metrics_map.clear();
            ms.m_metrics_by_table_id.clear();
        
            m_kernel_metrics.at(store_id).erase(kernel_id);
        }
    }
}


void
ComputeDataModel::ClearWorkloadMetricValues(uint64_t store_id, uint32_t workload_id)
{
    if (m_workload_metrics.count(store_id)){
        if( m_workload_metrics.at(store_id).count(workload_id)) {
            MetricStore& ms = m_workload_metrics.at(store_id).at(workload_id);
            ms.m_metrics_data.clear();
            ms.m_metrics_map.clear();
            ms.m_metrics_by_table_id.clear();
        
            m_workload_metrics.at(store_id).erase(workload_id);
        }
    }
}

std::shared_ptr<MetricValue>
ComputeDataModel::GetWorkloadMetricValue(uint64_t store_id, uint32_t workload_id,
                                 uint32_t category_id, uint32_t table_id,
                                 uint32_t entry_id) const
{
    MetricKey metric_id{};
    metric_id.fields.category_id = category_id;
    metric_id.fields.table_id    = table_id;
    metric_id.fields.entry_id    = entry_id;

    return GetWorkloadMetricValue(store_id, workload_id, metric_id.id);
}


std::shared_ptr<MetricValue>
ComputeDataModel::GetWorkloadMetricValue(uint64_t store_id, uint32_t workload_id,
                                 uint64_t metric_key) const
{
    if(m_workload_metrics.count(store_id) && m_workload_metrics.at(store_id).count(workload_id) &&
       m_workload_metrics.at(store_id).at(workload_id).m_metrics_map.count(metric_key))
    {
        return m_workload_metrics.at(store_id).at(workload_id).m_metrics_map.at(metric_key);
    }

    return nullptr;
}

std::shared_ptr<MetricValue>
ComputeDataModel::GetKernelMetricValue(uint64_t store_id, uint32_t kernel_id,
                                 uint32_t category_id, uint32_t table_id,
                                 uint32_t entry_id) const
{
    MetricKey metric_id{};
    metric_id.fields.category_id = category_id;
    metric_id.fields.table_id    = table_id;
    metric_id.fields.entry_id    = entry_id;

    return GetKernelMetricValue(store_id, kernel_id, metric_id.id);
}

std::shared_ptr<MetricValue>
ComputeDataModel::GetKernelMetricValue(uint64_t store_id, uint32_t kernel_id,
                                 uint64_t metric_key) const
{
    if(m_kernel_metrics.count(store_id) && m_kernel_metrics.at(store_id).count(kernel_id) &&
       m_kernel_metrics.at(store_id).at(kernel_id).m_metrics_map.count(metric_key))
    {
        return m_kernel_metrics.at(store_id).at(kernel_id).m_metrics_map.at(metric_key);
    }

    return nullptr;
}

ComputeDataModel::MetricValuesByEntryId*
ComputeDataModel::GetKernelMetricValuesByTable(uint64_t store_id, uint32_t kernel_id,
                                         uint32_t category_id, uint32_t table_id)
{
    TableKey table_id_union{};
    table_id_union.fields.category_id = category_id;
    table_id_union.fields.table_id    = table_id;

    return GetKernelMetricValuesByTable(store_id, kernel_id, table_id_union.id);
}

ComputeDataModel::MetricValuesByEntryId*
ComputeDataModel::GetKernelMetricValuesByTable(uint64_t store_id, uint32_t kernel_id,
                                         uint64_t table_key)
{
    if(m_kernel_metrics.count(store_id) && m_kernel_metrics.at(store_id).count(kernel_id) &&
       m_kernel_metrics.at(store_id).at(kernel_id).m_metrics_by_table_id.count(table_key))
    {
        return &m_kernel_metrics.at(store_id).at(kernel_id).m_metrics_by_table_id.at(table_key);
    }
    return nullptr;
}

std::vector<const KernelInfo*>
ComputeDataModel::GetKernelInfoList(uint32_t workload_id) const
{
    std::vector<const KernelInfo*> kernel_info_list;
    if(m_workloads.count(workload_id))
    {
        const WorkloadInfo& workload = m_workloads.at(workload_id);
        for(const auto& kernel_pair : workload.kernels)
        {
            kernel_info_list.push_back(&kernel_pair.second);
        }
    }
    return kernel_info_list;
}

const KernelInfo*
ComputeDataModel::GetKernelInfo(uint32_t workload_id, uint32_t kernel_id) const
{
    if(m_workloads.count(workload_id))
    {
        const WorkloadInfo& workload = m_workloads.at(workload_id);
        if(workload.kernels.count(kernel_id))
        {
            return &workload.kernels.at(kernel_id);
        }
    }
    return nullptr;
}

ComputeKernelSelectionTable&
ComputeDataModel::GetKernelSelectionTable()
{
    return m_kernel_selection_table;
}

}  // namespace View
}  // namespace RocProfVis
