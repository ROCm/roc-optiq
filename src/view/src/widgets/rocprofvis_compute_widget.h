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
inline constexpr float TABLE_TOOLTIP_MAX_WIDTH = 400.0f;

struct MetricId
{
    std::string ToString() const
    {
        return std::to_string(category_id) + "." + std::to_string(table_id) + "." +
               std::to_string(entry_id);
    };

    bool operator==(const MetricId& other) const noexcept
    {
        return category_id == other.category_id &&
               table_id == other.table_id &&
               entry_id == other.entry_id;
    }

    uint32_t category_id;
    uint32_t table_id;
    uint32_t entry_id;
};

struct MetricIdHash
{
    size_t operator()(const MetricId& id) const noexcept
    {
        uint64_t value = (static_cast<uint64_t>(id.category_id) << 48) ^
                     (static_cast<uint64_t>(id.table_id) << 16) ^
                     static_cast<uint64_t>(id.entry_id);

        return std::hash<uint64_t>{}(value);
    }
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

inline constexpr uint32_t METRIC_CAT_SOL   = 2;
inline constexpr uint32_t METRIC_TABLE_SOL = 1;

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

class PinedMetricTable: public RocWidget
{
public:
    struct RowValue
    {
        std::string value;
        std::string tooltip;
    };
    using Row = std::map<uint32_t, RowValue>;
    PinedMetricTable(DataProvider&                     data_provider,
                std::shared_ptr<ComputeSelection> compute_selection, uint64_t client_id);
    void AddRow(MetricId metric_id);
    void Render();
    void RefillTable();
    void Update();

private:
    const AvailableMetrics::Table& GetTable(const MetricId& metric_id, uint32_t workload_id);
    float GetTableHight() const;
    void                           UpdateColumns(MetricId metric_id);
    void                           FillTableRow(const MetricId& metric_id);
    void                           SetDefaultColumns();
    void ContextMenu(const char* value_to_copy, MetricId id_to_delete);

    std::optional<uint32_t> GetColumnIndex(const std::string& column_name);
    void FillCommons(const MetricId& metric_id, const AvailableMetrics::Table& table,
                     Row& row,
                     std::shared_ptr<MetricValue>     metric_value);
    void RenderTooltip(const RowValue& row);
    void RenderRowValues(uint32_t index, const std::pair<MetricId, Row>& row,
                   std::function<void(const char* value_to_copy)> menu_func);
    void RenderUnitValue(const std::pair<MetricId, Row>& row,
                         std::function<void(const char* value_to_copy)> menu_func);

    std::map<uint32_t, std::string>                 m_columns;
    std::uint32_t                                   m_lust_column_index;
    std::unordered_map<MetricId, Row, MetricIdHash> m_rows;
    uint64_t                                        m_client_id;

    std::shared_ptr<ComputeSelection> m_compute_selection;
    DataProvider&                     m_data_provider;
    std::optional<MetricId>           m_id_to_delete;

};

}  // namespace View
}  // namespace RocProfVis
