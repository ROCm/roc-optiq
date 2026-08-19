// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class ComputeSelection;
class DataProvider;
class TimelineSelection;
class TraceView;

// Panels the assistant can open and close, named the way the View menu names
// them so the model and the user are talking about the same thing.
enum class OptiqPanel
{
    kMinimap,
    kHistogram,
    kTopology,
    kDetails,
    kSummary,
    kLogViewer,
    kToolbar,
    kAnnotations,
    kUnknown
};

/**
 * @brief Everything the assistant is allowed to do to Optiq, expressed the way a
 * user would do it.
 *
 * Each method reproduces one real interaction - a click, a drag, a menu item -
 * including the event traffic the rest of the app listens for. The point is that
 * a tool never has to re-derive the correct sequence: selecting an event, for
 * instance, is what makes TraceView load that event's details, flow arrows, and
 * call stack, so callers must not also fetch them by hand.
 *
 * Every method is safe to call when the relevant part of the app is missing and
 * returns false rather than doing half the work. Adding a new capability should
 * be one method here, not a block of wiring in a tool.
 */
class OptiqActions
{
public:
    OptiqActions(DataProvider* data_provider, TimelineSelection* timeline_selection,
                 ComputeSelection* compute_selection, TraceView* trace_view);

    bool HasTimeline() const;
    bool HasCompute() const;

    // --- Panels -----------------------------------------------------------

    // Open or close a panel, the same as ticking its View-menu item. Some
    // panels need both the persisted setting and a live widget update; that
    // pairing lives here so callers do not have to know about it.
    bool ShowPanel(OptiqPanel panel, bool visible);
    bool IsPanelVisible(OptiqPanel panel, bool& visible_out) const;

    // Accepts what a user would call the thing, not just the canonical name:
    // "navbar" and "tree" both mean the topology panel, for instance.
    static OptiqPanel  PanelFromName(const std::string& name);
    static const char* PanelName(OptiqPanel panel);
    static std::string PanelNameList();

    // --- Tabs -------------------------------------------------------------

    // Open traces. Names match the tab labels the user sees.
    std::vector<std::string> ListTabs() const;
    std::string              ActiveTab() const;
    bool                     SelectTab(const std::string& name);

    // The details panel's inner tabs. Selecting one also opens the panel.
    std::vector<std::string> ListAnalysisTabs() const;
    std::string              ActiveAnalysisTab() const;
    bool                     SelectAnalysisTab(const std::string& name);

    // --- Timeline options -------------------------------------------------

    // Flow arrows between linked events, and whether they fan out from the
    // selected event or chain through the sequence.
    bool SetFlowArrowsVisible(bool visible);
    bool AreFlowArrowsVisible(bool& visible_out) const;
    bool SetFlowRenderChained(bool chained);

    // Zoom the visible window, rather than only selecting a range.
    bool ZoomToRange(double start_ns, double end_ns);

    // --- Leaving something behind ------------------------------------------

    // Pins a sticky note on the timeline. It is saved with the project, so this
    // is the one action that outlives the conversation.
    bool AddNote(double time_ns, const std::string& title, const std::string& text,
                 double v_min, double v_max, uint64_t track_id);

    // Raises a toast, for telling the user something without them reading the
    // panel.
    void Notify(const std::string& message, bool is_warning = false);

    // --- Remaining toolbar actions ----------------------------------------

    bool             ResetView();
    std::vector<int> ListBookmarks() const;
    bool             SaveBookmark(int slot);
    bool             GotoBookmark(int slot);
    bool             RemoveBookmark(int slot);
    // Drops the measurement pins on a span, the same as the Measure tool.
    bool             MeasureRange(double start_ns, double end_ns);
    bool             ClearMeasurement();

    // --- Timeline ---------------------------------------------------------

    // Drag-select a time range, the same as dragging on the ruler.
    bool SelectRange(double start_ns, double end_ns);
    bool ClearRange();

    // Move the visible window without changing the selection.
    bool ShowRange(double start_ns, double end_ns);

    // Scroll a track into view, and reveal it in the topology sidebar.
    bool ScrollToTrack(uint64_t track_id);
    bool RevealTrackInTopology(uint64_t track_id);

    // --- Events -----------------------------------------------------------

    // The literal equivalent of clicking an event: drops the previous
    // selection, then selects this one. That selection is what triggers the
    // detail, flow-arrow, and call-stack loads. Do not pair this with a manual
    // DataProvider::FetchEvent - the duplicate request collides on the shared
    // request id and gets dropped.
    bool ClickEvent(uint64_t track_id, uint64_t event_uuid);

    // Like ClickEvent but additive, the same as clicking with multi-select held.
    bool ShiftClickEvent(uint64_t track_id, uint64_t event_uuid);
    bool ClearEventSelection();

    // Make an event glow until something clears it.
    bool HighlightEvent(uint64_t track_id, uint64_t event_uuid);
    bool ClearHighlights();

    // Scroll to an event and frame it, the same as jumping from a table row.
    bool NavigateToEvent(uint64_t track_id, uint64_t event_uuid, double start_ns,
                         double duration_ns);

    // --- Compute ----------------------------------------------------------

    bool SelectKernel(uint32_t kernel_id);
    bool SelectWorkload(uint32_t workload_id);

private:
    std::string SourceId() const;

    DataProvider*      m_data_provider;
    TimelineSelection* m_timeline_selection;
    ComputeSelection*  m_compute_selection;
    TraceView*         m_trace_view;
};

}  // namespace View
}  // namespace RocProfVis
