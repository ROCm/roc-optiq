// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "model/compute/rocprofvis_compute_model_types.h"
#include "rocprofvis_widget.h"

#include <map>
#include <set>
#include <limits>
#include <vector>
#include <optional>

namespace RocProfVis
{
namespace View
{
inline constexpr float TABLE_TOOLTIP_MAX_WIDTH = 400.0f;
inline constexpr uint32_t METRIC_CAT_SOL       = 2;
inline constexpr uint32_t METRIC_TABLE_SOL     = 1;

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
    struct Row
    {
        std::map<uint32_t, RowValue> values;
        bool                         pinned = false;
    };
    explicit MetricTableBase(std::string event_source_id = {});
    virtual ~MetricTableBase() = default;
    void Render() override;
    void ChangePinState(const MetricId& metric_id);
    bool IsMetricPinned(MetricId metric_id);
    void SetPinMetricCallback(std::function<void(MetricId)> callback);
protected:
    virtual void ContextMenu(const char* value_to_copy, uint32_t column_index,
                             std::pair<const MetricId, Row>& row);

    void RenderTooltip(const RowValue& row);
    void RenderRowValues(uint32_t index, std::pair<const MetricId, Row>& row,
                         std::function<void(const char* value_to_copy)> menu_func);
    void RenderUnitValue(std::pair<const MetricId, Row>& row);
    void FillDefaultColumns(std::map<uint32_t, std::string>& columns,
                            std::uint32_t&                    last_column_index);
    void AddMetricToKernelDetails(const MetricId& metric_id, const std::string& value_name);
    bool IsValueColumn(uint32_t column_index) const;
    bool CanBePinned();
    virtual void RenderEmptyTable() = 0;

    std::map<uint32_t, std::string> m_columns;
    std::uint32_t                   m_last_column_index = 0;
    std::map<MetricId, Row>         m_rows;

    std::function<void(MetricId)> m_pin_metric_clicked;
    std::string                   m_event_source_id;
    std::string                   m_table_title;

    static constexpr uint32_t LAST_INDEX = std::numeric_limits<uint32_t>::max();
    ImGuiTableFlags           m_table_flags;
    uint32_t                  m_max_rows_in_table;
    // Suppress the outer card when an embedding container paints its own.
    bool                      m_no_panel = false;

    uint32_t m_freezed_columns = 0;
    uint32_t m_freezed_rows    = 0;

private:
    float GetTableHeight() const;
    void  RenderPinCheckBox(std::pair<const MetricId, Row>& row);
};


class MetricTable : public MetricTableBase
{
public:
    using MetricValueLookup =
        std::function<std::shared_ptr<MetricValue>(uint32_t entry_id)>;

    explicit MetricTable(std::string event_source_id = {});

    void Clear();
    bool Empty() const;
    void Populate(const AvailableMetrics::Table& table,
                  const MetricValueLookup&       get_value);

private:
    void ContextMenu(const char* value_to_copy, uint32_t column_index,
                     std::pair<const MetricId, Row>& row) override;
    virtual void RenderEmptyTable() override;
};

class PinnedMetricTable: public MetricTableBase
{
public:
    PinnedMetricTable(DataProvider&                     data_provider,
                      std::shared_ptr<ComputeSelection> compute_selection,
                      uint64_t                          client_id);
    void Update() override;
    void AddRow(MetricId metric_id);
    void RefillTable(const std::set<MetricId>& pinned_ids);

private:
    void ContextMenu(const char* value_to_copy, uint32_t column_index,
                     std::pair<const MetricId, Row>& row) override;
    bool HasMetricInCurrentWorkload(const MetricId& metric_id) const;
    void FillUnavailableRow(const MetricId& metric_id,
                            std::map<MetricId, Row>& rows);
    void UpdateColumns(MetricId                        metric_id,
                       std::map<uint32_t, std::string>& columns,
                       uint32_t&                        last_column_index);
    void FillTableRow(const MetricId&              metric_id,
                      const std::map<uint32_t, std::string>& columns,
                      std::map<MetricId, Row>&     rows);
    void FillMandatoryColumns(const MetricId&                metric_id,
                              const AvailableMetrics::Table& table,
                              Row&                           row);

    const AvailableMetrics::Table& GetTable(const MetricId& metric_id, uint32_t workload_id);

    std::optional<uint32_t> GetColumnIndex(const std::string&                  column_name,
                                           const std::map<uint32_t, std::string>& columns);

    virtual void RenderEmptyTable() override;

    std::set<MetricId>                m_pending_pinned_ids;
    bool                              m_rebuild_pending = false;
    DataProvider&                     m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;
    uint64_t                          m_client_id;
};

class MetricTableWidget : public RocWidget
{
public:
    MetricTableWidget(DataProvider&                     data_provider,
                      std::shared_ptr<ComputeSelection> compute_selection,
                      uint32_t category_id, uint32_t table_id);

    void Render() override;

    virtual void FetchMetrics();
    virtual void UpdateTable();
    void         Clear();
    uint64_t     GetClientId() const { return m_client_id; }

protected:
    DataProvider&                     m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;
    uint32_t                          m_category_id;
    uint32_t                          m_table_id;
    uint64_t                          m_client_id;
    MetricTable                       m_table;
};

class WorkloadMetricTableWidget : public MetricTableWidget
{
public:
    WorkloadMetricTableWidget(DataProvider&                     data_provider,
                              std::shared_ptr<ComputeSelection> compute_selection,
                              uint32_t category_id, uint32_t table_id);

    virtual void FetchMetrics() override;
    virtual void UpdateTable() override;
};

}  // namespace View
}  // namespace RocProfVis
