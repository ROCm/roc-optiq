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

class DataProvider;
class TimelineSelection;
class TraceView;
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
enum class ScriptApproval : uint8_t;
#endif

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
 * @brief Everything the assistant is allowed to do to Optiq, expressed the way
 * a user would do it.
 *
 * Each method reproduces one real interaction - a click, a drag, a menu item -
 * including the event traffic the rest of the app listens for, so a tool never
 * has to re-derive the correct sequence. Selecting an event, for instance, is
 * what makes TraceView load its details, flow arrows, and call stack; callers
 * must not fetch those by hand as well.
 *
 * Every method is safe to call when the relevant part of the app is missing,
 * and returns false rather than doing half the work. Add a capability as one
 * method here, not as wiring inside a tool.
 */
class OptiqActions
{
public:
    OptiqActions(DataProvider* data_provider, TimelineSelection* timeline_selection,
                 TraceView* trace_view);

    bool HasTimeline() const;

    // --- Panels -----------------------------------------------------------

    // Open or close a panel, the same as ticking its View-menu item. Panels
    // that need both the persisted setting and a live widget update are paired
    // up here, so callers do not have to know which ones those are.
    bool ShowPanel(OptiqPanel panel, bool visible);

    // Accepts what a user would call the panel, not just its canonical name:
    // "navbar" and "tree" both mean topology. Case, spacing and punctuation are
    // ignored, and so is trailing filler, so "the Topology view" lands as well.
    // Every alias is unique across panels, so a name never resolves two ways.
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

#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    // --- Scripts ----------------------------------------------------------

    // Puts a script in the Script tab and brings the user to it. Nothing runs:
    // they press Run or Reject, and ScriptProposalState reports which.
    bool           ProposeScript(const std::string& source);
    ScriptApproval ScriptProposalState() const;
    void           ClearScriptProposal();
#endif

    // --- Timeline options -------------------------------------------------

    // Flow arrows between linked events, and whether they fan out from the
    // selected event or chain through the sequence.
    bool SetFlowArrowsVisible(bool visible);
    bool SetFlowRenderChained(bool chained);

    // Zoom the visible window, rather than only selecting a range.
    bool ZoomToRange(double start_ns, double end_ns);

    // --- Leaving something behind ------------------------------------------

    // Pins a sticky note on the timeline. Saved with the project, so this is
    // the one action that outlives the conversation.
    bool AddNote(double time_ns, const std::string& title, const std::string& text,
                 double v_min, double v_max, uint64_t track_id);

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

    // Move the visible window without changing the selection.
    bool ShowRange(double start_ns, double end_ns);

    // Scroll a track into view.
    bool ScrollToTrack(uint64_t track_id);

    // --- Events -----------------------------------------------------------

    // The literal equivalent of clicking an event: drops the previous
    // selection, then selects this one, which is what triggers the detail,
    // flow-arrow, and call-stack loads. Do not pair this with a manual
    // DataProvider::FetchEvent - the duplicate collides on the shared request
    // id and gets dropped.
    bool ClickEvent(uint64_t track_id, uint64_t event_uuid);

    // Make an event glow until something clears it.
    bool HighlightEvent(uint64_t track_id, uint64_t event_uuid);
    bool ClearHighlights();

    // Scroll to an event and frame it, the same as jumping from a table row.
    bool NavigateToEvent(uint64_t track_id, uint64_t event_uuid, double start_ns,
                         double duration_ns);

private:
    std::string SourceId() const;

    DataProvider*      m_data_provider;
    TimelineSelection* m_timeline_selection;
    TraceView*         m_trace_view;
};

}  // namespace View
}  // namespace RocProfVis
