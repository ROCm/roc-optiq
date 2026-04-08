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

class DataProvider;
class ComputeSelection;

class MetricTableBase : public RocWidget
{
public:
    struct RowValue
    {
        std::string value;
        std::string tooltip;
    };
    using Row = std::map<uint32_t, RowValue>;
    MetricTableBase() = default;
    MetricTableBase(std::function<void(MetricId metric_id, const std::string&)> set_to_kernel_table_callback);
    virtual ~MetricTableBase() = default;
    void Render() override;

protected:
    virtual void ContextMenu(const char* value_to_copy, uint32_t index,
                             const std::pair<MetricId, Row>& row);

    void RenderTooltip(const RowValue& row);
    void RenderRowValues(uint32_t index, const std::pair<MetricId, Row>& row,
                         std::function<void(const char* value_to_copy)> menu_func);
    void RenderUnitValue(const std::pair<MetricId, Row>& row);
    void FillDefaultColumns();

    std::map<uint32_t, std::string>                 m_columns;
    std::uint32_t                                   m_lust_column_index;
    std::unordered_map<MetricId, Row, MetricIdHash> m_rows;

    std::function<void(MetricId metric_id, const std::string& value_name)>
        m_set_to_kernel_table_callback;
    std::string               m_table_title;


    static constexpr uint32_t LAST_INDEX = std::numeric_limits<uint32_t>::max();
    ImGuiTableFlags           m_table_flags = ImGuiTableFlags_Borders;
    uint32_t                  m_max_rows_in_table = 0;

private:
    float GetTableHight() const;
};

class newMetricTableCache : public MetricTableBase
{
public:
    using MetricValueLookup =
        std::function<std::shared_ptr<MetricValue>(uint32_t entry_id)>;

    
    newMetricTableCache() = default;
    newMetricTableCache(
        std::function<void(MetricId)> add_row_func,
        std::function<void(MetricId metric_id, const std::string& value_name)>
            set_to_kernel_table);

    void Clear();
    bool Empty() const;
    void Populate(const AvailableMetrics::Table& table,
                  const MetricValueLookup&       get_value);

private:
    void ContextMenu(const char* value_to_copy, uint32_t index,
                     const std::pair<MetricId, Row>& row) override;
    std::function<void(MetricId)> m_pin_metric_func;

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
    MetricTableCache(
        std::function<void(MetricId)> add_row_func,
        std::function<void(MetricId metric_id, const std::string& value_name)> set_to_kernel_table);

    void Populate(const AvailableMetrics::Table& table,
                  const MetricValueLookup&       get_value);

    void Clear();
    void Render();

    bool Empty() const;

private:
    void        ContextMenu(const char* value_to_copy, const Row& row, uint32_t index);
    std::string                   m_title;
    std::string                   m_table_id;
    std::vector<std::string>      m_column_names;
    std::vector<Row>              m_rows;
    std::function<void(MetricId)> m_add_row_to_custom;
    std::function<void(MetricId metric_id, const std::string& value_name)>
        m_set_to_kernel_table_callback;
};

inline constexpr uint32_t METRIC_CAT_SOL   = 2;
inline constexpr uint32_t METRIC_TABLE_SOL = 1;



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

class PinedMetricTable: public MetricTableBase
{
public:
    
    PinedMetricTable(DataProvider&                     data_provider,
                std::shared_ptr<ComputeSelection> compute_selection, uint64_t client_id,
                     std::function<void(MetricId metric_id, const std::string&)>
                         set_to_kernel_table_callback);
    void AddRow(MetricId metric_id); 
    void RefillTable();
    void Update() override;

private:
    void ContextMenu(const char* value_to_copy, uint32_t index,
                     const std::pair<MetricId, Row>& row) override;
    void UpdateColumns(MetricId metric_id);
    void FillTableRow(const MetricId& metric_id);
    void FillMandatoryColumns(const MetricId&                metric_id,
                              const AvailableMetrics::Table& table, Row& row,
                              std::shared_ptr<MetricValue> metric_value);

    const AvailableMetrics::Table& GetTable(const MetricId& metric_id, uint32_t workload_id);

    std::optional<uint32_t> GetColumnIndex(const std::string& column_name);


    std::optional<MetricId>           m_id_to_delete;
    DataProvider&                     m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;
    uint64_t                          m_client_id;
};

}  // namespace View
}  // namespace RocProfVis
