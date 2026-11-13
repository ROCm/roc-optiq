// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_presets.h"
#include "widgets/rocprofvis_compute_widget.h"
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{

class ComputeSelection;

// Metric tables for the selected kernel, shown as a pane of
// ComputeKernelDetailsView. Only System Speed-of-Light is listed until the user
// turns more on from the Tables picker; each shown table sits under a
// collapsible, closable header. With a compare target every table (and the
// pins) compares the selected kernel (A) with it (B).
class ComputeTableView : public RocWidget
{
public:
    static constexpr const char* NO_METRICS_MESSAGE =
        "This database file has no available metrics.";

    ComputeTableView(DataProvider&                     data_provider,
                     std::shared_ptr<ComputeSelection> compute_selection,
                     bool                              has_available_metrics = true);
    ~ComputeTableView();

    void Update() override;
    void Render() override;

    // Skip the card and title, for a container that draws its own pane header.
    void SetChromeless(bool chromeless) { m_chromeless = chromeless; }

    // Compare with this kernel (B), possibly of another workload, until
    // ClearCompareTarget().
    void SetCompareTarget(uint32_t workload_id, uint32_t kernel_id);
    void ClearCompareTarget();
    // Call before A and B trade places (the selection becomes B and B the old
    // selection), so each pin keeps naming the same metric.
    void SwapComparePins();

private:
    enum class HeaderRequest : uint8_t
    {
        kNone,
        kExpandAll,
        kCollapseAll,
    };

    // A table listed for the current kernels: A's, B's, or (keyed alike) both.
    struct ListedTable
    {
        uint32_t                       category_id;
        const std::string*             category_name;
        const AvailableMetrics::Table* a;
        const AvailableMetrics::Table* b;
        // A's table, else B's.
        const AvailableMetrics::Table& Named() const { return a ? *a : *b; }
        uint64_t Key() const { return MetricId::GetTableKey(category_id, Named().id); }
    };

    // Fills `listed` with the tables of `workload` (A's), then those only B's
    // workload has.
    void ListTables(const WorkloadInfo& workload, std::vector<ListedTable>& listed) const;
    void RenderToolbar(const std::vector<ListedTable>& listed);
    void RenderTablePicker(const std::vector<ListedTable>& listed);
    void RenderCompareOptions();
    // Above the scrolling list of tables, so the pins stay in view.
    void RenderPinnedMetrics();
    void RenderTables(const std::vector<ListedTable>& listed);
    void RenderEmptyState();
    void ApplyHeaderRequest();
    void ResetWorkloadData();
    void FetchAllMetrics();
    void FetchCompareMetrics();
    // Requests every table of `workload` for `kernel_id` under `client_id`.
    // `loading` is set once a request is made, and `pending` too when it is
    // refused (to be retried).
    void RequestAllTables(const WorkloadInfo& workload, uint32_t kernel_id,
                          uint64_t client_id, bool& loading, bool& pending);
    void RebuildTableDataCache();
    void AddTable(const ListedTable& listed);
    void RestoreMetricPining();
    // Points the pins at the compare target (or back at A alone) and refills them.
    void RefreshPins();

    class Preset : public PresetComponent
    {
    public:
        Preset(ComputeTableView& widget);

        bool ToJson(jt::Json& json) override;
        bool FromJson(jt::Json& json) override;
        void Reset() override;

    private:
        ComputeTableView& m_widget;
    };

    DataProvider&                             m_data_provider;
    std::shared_ptr<ComputeSelection>         m_compute_selection;
    uint64_t                                  m_client_id;
    bool                                      m_has_available_metrics;
    bool                                      m_fetch_pending   = false;
    // True from the moment a metric fetch is submitted until its data lands, so
    // shown tables read "Loading" rather than "No data".
    bool                                      m_metrics_loading = false;
    std::unordered_map<uint64_t, MetricTable> m_table_widgets;
    PinnedMetricTable                         m_pinned_metric_table;
    std::set<MetricId>                        m_pinned_metrics;

    // Tables the user turned on, by MetricId::GetTableKey. Kept across workload
    // changes; keys a workload lacks are simply not listed.
    std::set<uint64_t> m_enabled_tables;
    std::string        m_picker_filter;
    HeaderRequest      m_header_request = HeaderRequest::kNone;
    bool               m_chromeless     = false;
    // Render's listing, refilled every frame: it points into the model.
    std::vector<ListedTable> m_listed_tables;

    // Compare target (B) and its metrics, fetched under their own client id.
    bool                 m_compare_active = false;
    uint32_t             m_compare_workload_id;
    uint32_t             m_compare_kernel_id;
    uint64_t             m_compare_client_id;
    bool                 m_compare_fetch_pending = false;
    bool                 m_compare_loading       = false;
    MetricCompareOptions m_compare_options;

    EventManager::SubscriptionToken m_workload_selection_changed_token;
    EventManager::SubscriptionToken m_kernel_selection_changed_token;
    EventManager::SubscriptionToken m_metrics_fetched_token;

    std::unique_ptr<Preset> m_preset;

    friend struct ComputeTableViewTestPeer;
};

}  // namespace View
}  // namespace RocProfVis
