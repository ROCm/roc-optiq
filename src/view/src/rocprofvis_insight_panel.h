// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_bottleneck_detector.h"
#include "widgets/rocprofvis_widget.h"
#include <functional>
#include <vector>

namespace RocProfVis
{
namespace View
{

// ---------------------------------------------------------------------------
// InsightPanel
//
// Renders a scrollable, filterable list of Insight results in an ImGui panel.
// Each insight is displayed as a card with severity badge, description, and
// a "Go To" button that fires a NavigationEvent to center the timeline on
// the relevant region.
//
// The panel also shows:
//   - A summary header with counts per severity.
//   - Filter toggles for each InsightCategory.
//   - A spinner / "Analyzing..." state while the async task is running.
// ---------------------------------------------------------------------------
class InsightPanel
{
public:
    enum class State
    {
        kIdle,       // No analysis requested yet
        kRunning,    // Async task in progress
        kReady       // Results available
    };

    InsightPanel();
    ~InsightPanel() = default;

    // Set the analysis results (called from main thread when future completes).
    void SetResults(std::vector<Insight>&& insights);

    // Mark the panel as running (show spinner).
    void SetRunning();

    // Main render call — draws inside the current ImGui context.
    void Render();

    State GetState() const { return m_state; }

private:
    void RenderHeader();
    void RenderFilters();
    void RenderInsightCard(const Insight& insight, int index);
    void RenderEmptyState();

    ImU32       SeverityColor(InsightSeverity sev) const;
    const char* SeverityLabel(InsightSeverity sev) const;
    const char* CategoryLabel(InsightCategory cat) const;
    const char* CategoryIcon(InsightCategory cat) const;

    void NavigateToInsight(const Insight& insight);

    std::vector<Insight> m_insights;
    State                m_state;

    // Per-category filter (true = show)
    bool m_filter_hotspot;
    bool m_filter_gpu_starvation;
    bool m_filter_cpu_blocked;
    bool m_filter_sync_barrier;
    bool m_filter_load_imbalance;
    bool m_filter_counter_anomaly;
    bool m_filter_serialisation;

    // Severity filter
    bool m_filter_critical;
    bool m_filter_warning;
    bool m_filter_info;

    bool PassesFilter(const Insight& ins) const;
};

}  // namespace View
}  // namespace RocProfVis
