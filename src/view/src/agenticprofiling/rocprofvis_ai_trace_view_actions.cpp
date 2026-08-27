// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The TraceView methods that exist only so OptiqActions can drive a system
// trace. They are TraceView members because each one reaches into the widgets
// TraceView owns, but the bodies live here: nothing else in the app calls them,
// and keeping them beside the assistant means an agentic-off build does not
// compile them at all.
//
// A capability belongs here only while the assistant is its sole caller. Once
// the toolbar or a menu needs one, move it back into rocprofvis_trace_view.cpp.
#include "rocprofvis_trace_view.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "imgui.h"

#include "model/rocprofvis_timeline_model.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_annotations.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_measurement_controller.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_view.h"
#include "rocprofvis_utils.h"
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
#    include "widgets/rocprofvis_script_editor.h"
#endif

namespace RocProfVis
{
namespace View
{

// Notes dropped by the assistant land near the top of the track area at a
// readable default size; the user can drag and resize them from there.
constexpr float  ANNOTATION_DEFAULT_Y_OFFSET = 40.0f;
constexpr ImVec2 ANNOTATION_DEFAULT_SIZE(260.0f, 120.0f);

// Bookmark slots exposed in the toolbar dropdown.
constexpr int BOOKMARK_MIN_SLOT = 0;
constexpr int BOOKMARK_MAX_SLOT = 9;

namespace
{

bool
IsValidBookmarkSlot(int slot)
{
    return slot >= BOOKMARK_MIN_SLOT && slot <= BOOKMARK_MAX_SLOT;
}

}  // namespace

void
TraceView::SetMinimapVisibility(bool visibility)
{
    m_show_minimap_popup = visibility;
}

bool
TraceView::IsMinimapVisible() const
{
    return m_show_minimap_popup;
}

void
TraceView::SetFlowArrowsVisible(bool visible)
{
    if(m_timeline_view)
    {
        m_timeline_view->GetArrowLayer().SetFlowDisplayMode(
            visible ? FlowDisplayMode::kShowAll : FlowDisplayMode::kHide);
    }
}

bool
TraceView::AreFlowArrowsVisible() const
{
    return m_timeline_view && m_timeline_view->GetArrowLayer().GetFlowDisplayMode() ==
                                  FlowDisplayMode::kShowAll;
}

void
TraceView::SetFlowRenderChained(bool chained)
{
    if(m_timeline_view)
    {
        m_timeline_view->GetArrowLayer().SetRenderStyle(
            chained ? TimelineArrow::RenderStyle::kChain
                    : TimelineArrow::RenderStyle::kFan);
    }
}

std::vector<std::string>
TraceView::ListAnalysisTabs()
{
    return m_analysis_view ? m_analysis_view->ListTabs() : std::vector<std::string>();
}

bool
TraceView::SelectAnalysisTab(const std::string& name)
{
    if(!m_analysis_view || !m_analysis_view->SelectTab(name))
    {
        return false;
    }
    // A tab the user cannot see is not a switch, so make sure the panel is up.
    // show_details_panel is global, so it has to be applied to every open trace
    // rather than to this one alone.
    SettingsManager::GetInstance().GetAppWindowSettings().show_details_panel = true;
    AppWindow::GetInstance()->ApplyPanelVisibilitySettings();
    return true;
}

std::string
TraceView::ActiveAnalysisTab()
{
    return m_analysis_view ? m_analysis_view->ActiveTab() : std::string();
}

#ifdef ROCPROFVIS_ENABLE_SCRIPTING
bool
TraceView::ProposeScript(const std::string& source)
{
    if(!m_analysis_view || !m_analysis_view->GetScriptEditor())
    {
        return false;
    }
    m_analysis_view->GetScriptEditor()->ProposeScript(source);
    return true;
}

ScriptApproval
TraceView::ScriptProposalState() const
{
    if(!m_analysis_view || !m_analysis_view->GetScriptEditor())
    {
        return ScriptApproval::kNone;
    }
    return m_analysis_view->GetScriptEditor()->ProposalState();
}

void
TraceView::ClearScriptProposal()
{
    if(m_analysis_view && m_analysis_view->GetScriptEditor())
    {
        m_analysis_view->GetScriptEditor()->ClearProposal();
    }
}
#endif

void
TraceView::ResetView()
{
    if(m_timeline_view)
    {
        const TimelineModel& timeline = m_data_provider.DataModel().GetTimeline();
        m_timeline_view->MoveToPosition(timeline.GetStartTime(), timeline.GetEndTime(),
                                        0.0, false);
    }
}

void
TraceView::SetAnnotationsVisible(bool visible)
{
    if(m_annotations)
    {
        m_annotations->SetVisible(visible);
    }
}

bool
TraceView::AreAnnotationsVisible() const
{
    return m_annotations && m_annotations->IsVisibile();
}

std::vector<int>
TraceView::ListBookmarks() const
{
    std::vector<int> slots;
    slots.reserve(m_bookmarks.size());
    for(const std::pair<const int, ViewCoords>& bookmark : m_bookmarks)
    {
        slots.push_back(bookmark.first);
    }
    std::sort(slots.begin(), slots.end());
    return slots;
}

bool
TraceView::SaveBookmark(int slot)
{
    if(!m_timeline_view || !IsValidBookmarkSlot(slot))
    {
        return false;
    }
    m_bookmarks[slot] = m_timeline_view->GetViewCoords();
    return true;
}

bool
TraceView::GotoBookmark(int slot)
{
    if(!IsValidBookmarkSlot(slot))
    {
        return false;
    }
    const std::unordered_map<int, ViewCoords>::const_iterator it = m_bookmarks.find(slot);
    if(it == m_bookmarks.end() || !m_timeline_view)
    {
        return false;
    }
    m_timeline_view->MoveToPosition(it->second.v_min_x, it->second.v_max_x, it->second.y,
                                    false);
    return true;
}

bool
TraceView::RemoveBookmark(int slot)
{
    return IsValidBookmarkSlot(slot) && m_bookmarks.erase(slot) > 0;
}

bool
TraceView::MeasureRange(double start_ns, double end_ns)
{
    if(!m_measurement || !is_usable_time_range(start_ns, end_ns))
    {
        return false;
    }
    // EnterMeasurementMode keeps any existing pins, so clear before dropping the
    // new pair or the second point would land in the wrong slot.
    m_measurement->EnterMeasurementMode();
    m_measurement->ClearMeasurement();
    m_measurement->SetFreehandMode(true);
    m_measurement->SetFreehandMeasurementPoint(start_ns);
    m_measurement->SetFreehandMeasurementPoint(end_ns);
    return true;
}

void
TraceView::ClearMeasurement()
{
    if(m_measurement)
    {
        m_measurement->ClearMeasurement();
        m_measurement->ExitMeasurementMode();
    }
}

void
TraceView::ZoomToRange(double start_ns, double end_ns)
{
    if(m_timeline_view && is_usable_time_range(start_ns, end_ns))
    {
        m_timeline_view->SetViewableRangeNS(start_ns, end_ns);
    }
}

bool
TraceView::AddNote(double time_ns, const std::string& title, const std::string& text,
                   double v_min, double v_max, uint64_t track_id)
{
    if(!m_annotations || !std::isfinite(time_ns) || !is_usable_time_range(v_min, v_max))
    {
        return false;
    }
    m_annotations->SetVisible(true);
    m_annotations->AddSticky(time_ns, ANNOTATION_DEFAULT_Y_OFFSET,
                             ANNOTATION_DEFAULT_SIZE, text, title, v_min, v_max,
                             track_id, false, false);
    return true;
}

}  // namespace View
}  // namespace RocProfVis
