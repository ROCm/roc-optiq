// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "model/compute/rocprofvis_compute_model_types.h"
#include "rocprofvis_widget.h"

#include <map>

namespace RocProfVis
{
namespace View
{

struct MetricId
{
    std::string ToString() const
    {
        return std::to_string(category_id) + "." + std::to_string(table_id) + "." +
               std::to_string(entry_id);
    };
    uint32_t category_id;
    uint32_t table_id;
    uint32_t entry_id;
};

class MetricTableCache
{
public:
    using MetricValueLookup = std::function<std::shared_ptr<MetricValue>(uint32_t entry_id)>;

    struct Row
    {
        MetricId                 metric_id;
        std::string              name;
        std::string              description;
        std::vector<std::string> values;
        std::string              unit;
    };

    MetricTableCache() = default;
    MetricTableCache(std::function<void(MetricId)> add_row_func);

    void Populate(const AvailableMetrics::Table& table,
                  const MetricValueLookup&       get_value);

    void Clear();
    void Render() const;

    bool Empty() const;

private:
    std::string                   m_title;
    std::string                   m_table_id;
    std::vector<std::string>      m_column_names;
    std::vector<Row>              m_rows;
    std::function<void(MetricId)> m_add_row_to_custom;
};

constexpr uint32_t METRIC_CAT_SOL   = 2;
constexpr uint32_t METRIC_TABLE_SOL = 1;

class DataProvider;
class ComputeSelection;

class MetricTableWidget : public RocWidget
{
public:
    MetricTableWidget(DataProvider& data_provider,
                      std::shared_ptr<ComputeSelection> compute_selection,
                      uint32_t category_id, uint32_t table_id);

    void Render() override;

    void     FetchMetrics();
    void     UpdateTable();
    void     Clear();
    uint64_t GetClientId() const { return m_client_id; }

private:
    DataProvider& m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;
    uint32_t m_category_id;
    uint32_t m_table_id;
    uint64_t m_client_id;

    MetricTableCache m_table;
};

class CustomTable
{
public:
    CustomTable(DataProvider&                     data_provider,
                std::shared_ptr<ComputeSelection> compute_selection, uint64_t client_id);
    void AddRow(MetricId metric_id);
    void Render();
    bool Empty();

private:
    void UpdateColumns(const std::vector<std::string>& value_names);
    void FillTableRow(const AvailableMetrics::Table& table, const MetricId& metric_id);
    void FillCommons(const MetricId&                  metric_id,
                           const AvailableMetrics::Table&   table,
                           std::map<uint32_t, std::string>& row, std::shared_ptr<MetricValue>);
    std::optional<uint32_t> GetColumnIndex(const std::string& column_name);
    void RenderCommonColumns(std::map<uint32_t, std::string>& row);
    std::map<uint32_t, std::string>              m_columns;
    std::uint32_t                                m_lust_column_index;
    std::vector<std::map<uint32_t, std::string>> m_rows;
    std::vector<MetricId>                        m_metric_ids;
    uint64_t                                     m_client_id;

    std::shared_ptr<ComputeSelection> m_compute_selection;
    DataProvider& m_data_provider;

};

}  // namespace View
}  // namespace RocProfVis
