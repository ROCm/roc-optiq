// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "model/compute/rocprofvis_compute_model_types.h"
#include "rocprofvis_widget.h"

#include <bitset>
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

// Columns a two-kernel comparison can show for each metric value: A (the
// baseline), B (the compare target), their difference B - A, and it as a
// percentage of A.
enum class MetricCompareColumn : uint32_t
{
    kA,
    kB,
    kDelta,
    kDeltaPct,
    kCount
};
inline constexpr size_t METRIC_COMPARE_COLUMN_COUNT =
    static_cast<size_t>(MetricCompareColumn::kCount);

struct MetricCompareOptions
{
    MetricCompareOptions();

    std::bitset<METRIC_COMPARE_COLUMN_COUNT> columns;
    // Also list metrics found on only one side (e.g. across GPU architectures).
    bool show_unmatched = true;
    // Shade the cells of values that changed by at least this many percent of A.
    float highlight_pct = 0.0f;

    bool Shows(MetricCompareColumn column) const
    {
        return columns.test(static_cast<size_t>(column));
    }
};

// `workload`'s table `table_id` of category `category_id`; null when the workload
// is null or lacks it.
const AvailableMetrics::Table*
FindMetricTable(const WorkloadInfo* workload, uint32_t category_id, uint32_t table_id);

// The key a compare pin takes once A (`a_workload`) and B (`b_workload`) trade
// places, so it keeps naming the same metric: A's metric becomes its match in B
// (else B's only), and a metric only B had becomes A's.
MetricId
SwappedComparePinKey(const MetricId& key, const WorkloadInfo* a_workload,
                     const WorkloadInfo* b_workload);

class MetricTableBase : public RocWidget
{
public:
    struct RowValue
    {
        std::string value;
        std::string tooltip;
        ImU32       bg_color = 0;  // Cell shading; 0 for none.
    };
    struct Row
    {
        std::map<uint32_t, RowValue> values;
        bool                         pinned    = false;
        // A metric found on only one side of a comparison.
        bool                         unmatched = false;
    };
    // Metric id order, with a comparison's metric only B has right after A's of
    // the same id.
    struct RowOrder
    {
        bool operator()(const MetricId& lhs, const MetricId& rhs) const;
    };
    using RowMap = std::map<MetricId, Row, RowOrder>;

    explicit MetricTableBase(std::string event_source_id = {});
    virtual ~MetricTableBase() = default;
    void Render() override;
    // Pins and pin callbacks use the row's key: the metric's id, or for a row
    // only B has, its id with the B-only bit, so it stays apart from A's metric
    // of the same id.
    void ChangePinState(const MetricId& metric_id);
    bool IsMetricPinned(MetricId metric_id);
    void SetPinMetricCallback(std::function<void(MetricId)> callback);
    // Drop the card and title, for a table listed under its container's own
    // (collapsible) header. The table is as tall as its rows up to the
    // SetMaxHeight limit, past which they scroll under the header row.
    void SetEmbedded(bool embedded);
    // An embedded table's limit; 0 for none.
    void SetMaxHeight(float max_height);
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
    // Only way to set the title: the ImGui ids are derived from it.
    void SetTableTitle(std::string title);

    std::map<uint32_t, std::string> m_columns;
    std::uint32_t                   m_last_column_index = 0;
    RowMap                          m_rows;

    std::function<void(MetricId)> m_pin_metric_clicked;
    std::string                   m_event_source_id;

    static constexpr uint32_t LAST_INDEX = std::numeric_limits<uint32_t>::max();
    ImGuiTableFlags           m_table_flags;
    uint32_t                  m_max_rows_in_table;
    // Suppress the outer card when an embedding container paints its own.
    bool                      m_no_panel   = false;
    bool                      m_embedded   = false;
    float                     m_max_height = 0.0f;

    uint32_t m_freezed_columns = 0;
    uint32_t m_freezed_rows    = 0;

private:
    float GetTableHeight() const;
    void  RenderPinCheckBox(std::pair<const MetricId, Row>& row);
    // Flips the pin of the row `key` and reports it.
    void  TogglePin(const MetricId& key);

    std::string m_table_title;
    std::string m_panel_id_str;
    std::string m_table_id_str;

    friend struct MetricTableTestPeer;
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
    // One table compared across two kernels: `a_table` from A's workload and
    // `b_table` from B's (either may be null when only one workload has it).
    // Metrics pair up by id, else by name; B-only metrics are listed by id
    // among A's.
    void PopulateComparison(uint32_t category_id, const AvailableMetrics::Table* a_table,
                            const AvailableMetrics::Table* b_table,
                            const MetricValueLookup& get_a, const MetricValueLookup& get_b,
                            const MetricCompareOptions& options);

private:
    void ContextMenu(const char* value_to_copy, uint32_t column_index,
                     std::pair<const MetricId, Row>& row) override;
    virtual void RenderEmptyTable() override;
    // The id, name and unit cells every row starts with.
    static void FillEntryCells(const AvailableMetrics::Entry& entry, Row& row);
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
    // Compare each pin with B: `client_id`'s fetched metrics for the kernel.
    // Takes effect with the next RefillTable.
    void SetCompareSource(uint64_t client_id, uint32_t workload_id, uint32_t kernel_id,
                          const MetricCompareOptions& options);
    void ClearCompareSource();

private:
    struct CompareSource
    {
        uint64_t             client_id;
        uint32_t             workload_id;
        uint32_t             kernel_id;
        MetricCompareOptions options;
    };
    // A pin of a row only B has compares B's metric with an empty A; a pin A's
    // workload lacks is unavailable.
    void FillCompareRow(const MetricId& key, std::map<uint32_t, std::string>& columns,
                        uint32_t& last_column_index, RowMap& rows);

    void ContextMenu(const char* value_to_copy, uint32_t column_index,
                     std::pair<const MetricId, Row>& row) override;
    bool HasMetricInCurrentWorkload(const MetricId& metric_id) const;
    void FillUnavailableRow(const MetricId& key,
                            RowMap& rows);
    void UpdateColumns(MetricId                        metric_id,
                       std::map<uint32_t, std::string>& columns,
                       uint32_t&                        last_column_index);
    void FillTableRow(const MetricId&              metric_id,
                      const std::map<uint32_t, std::string>& columns,
                      RowMap&                      rows);
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
    std::optional<CompareSource>      m_compare;
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
