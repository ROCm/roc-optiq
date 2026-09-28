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

namespace RocProfVis
{
namespace View
{

class ComputeSelection;

// Metric tables for the selected kernel, shown as a pane of
// ComputeKernelDetailsView. Only System Speed-of-Light is listed until the user
// turns more on from the Tables picker; each shown table sits under a
// collapsible, closable header.
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

private:
    enum class HeaderRequest : uint8_t
    {
        kNone,
        kExpandAll,
        kCollapseAll,
    };

    void RenderToolbar(const WorkloadInfo& workload);
    void RenderTablePicker(const WorkloadInfo& workload);
    void RenderTables(const WorkloadInfo& workload);
    void RenderEmptyState();
    void ApplyHeaderRequest();
    void ResetWorkloadData();
    void FetchAllMetrics();
    void RebuildTableDataCache();
    void AddTable(uint32_t category_id, const AvailableMetrics::Table* table);
    void RestoreMetricPining();

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

    EventManager::SubscriptionToken m_workload_selection_changed_token;
    EventManager::SubscriptionToken m_kernel_selection_changed_token;
    EventManager::SubscriptionToken m_metrics_fetched_token;

    std::unique_ptr<Preset> m_preset;

    friend struct ComputeTableViewTestPeer;
};

}  // namespace View
}  // namespace RocProfVis
