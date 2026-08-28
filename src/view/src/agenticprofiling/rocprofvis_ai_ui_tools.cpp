// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The tools that change Optiq rather than read it. Every body here goes through
// OptiqActions and answers in the same call, so none of them park a fetch or
// touch a request id. Adding a capability belongs in rocprofvis_ai_actions.h as
// one method, not as wiring inside a tool here.
#include "rocprofvis_ai_tools_internal.h"

#include <cmath>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "json.h"

#include "compute/rocprofvis_compute_selection.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "rocprofvis_ai_actions.h"
#include "rocprofvis_core_string_utils.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_json_utils.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr size_t ASSISTANT_MAX_HIGHLIGHTS      = 12;
constexpr size_t ASSISTANT_MAX_NEXT_STEPS      = 3;
constexpr size_t ASSISTANT_MAX_NEXT_STEP_CHARS = 80;

// Every UI-facing thing a tool does goes through here.
OptiqActions
Actions(const AssistantToolContext& context)
{
    return OptiqActions(context.data_provider, context.timeline_selection,
                        context.compute_selection, context.trace_view);
}

// Reads the stacked follow-up buttons out of offer_next_steps arguments.
AssistantToolStartResult
OfferNextStepsResult(const jt::Json& args)
{
    jt::Json& mutable_args = const_cast<jt::Json&>(args);
    if(!mutable_args.contains("steps") || !mutable_args["steps"].isArray())
    {
        return DoneResult("offer_next_steps needs a steps array of 1 to 3 strings.",
                          "Missing steps");
    }

    std::vector<jt::Json>& entries = mutable_args["steps"].getArray();
    std::string            length_error;
    if(!CheckArrayLength(entries, "steps", length_error))
    {
        return DoneResult(length_error, "Too many steps");
    }

    AssistantToolStartResult result =
        DoneResult("Offered next steps under the chat.", "Offered next steps");
    result.set_next_steps = true;
    for(jt::Json& entry : entries)
    {
        if(!entry.isString())
        {
            continue;
        }
        std::string step = Core::String::trim_copy(entry.getString());
        if(step.empty())
        {
            continue;
        }
        if(step.size() > ASSISTANT_MAX_NEXT_STEP_CHARS)
        {
            step.resize(ASSISTANT_MAX_NEXT_STEP_CHARS);
        }
        result.next_steps.push_back(step);
        if(result.next_steps.size() >= ASSISTANT_MAX_NEXT_STEPS)
        {
            break;
        }
    }
    if(result.next_steps.empty())
    {
        return DoneResult("offer_next_steps needs at least one non-empty step.",
                          "Missing steps");
    }
    return result;
}

// The only tool that runs without a trace, since it just puts buttons under
// the chat.
AssistantToolStartResult
ToolOfferNextSteps(const AssistantToolContext&, const jt::Json& args,
                   const std::string&)
{
    return OfferNextStepsResult(args);
}

AssistantToolStartResult
ToolShowPanel(const AssistantToolContext& context, const jt::Json& args,
              const std::string&)
{
    const std::string panel_name = JsonUtils::GetString(args, "panel", "");
    const OptiqPanel  panel      = OptiqActions::PanelFromName(panel_name);
    if(panel == OptiqPanel::kUnknown)
    {
        return DoneResult("Unknown panel \"" + panel_name + "\". Use one of: " +
                              OptiqActions::PanelNameList() + ".",
                          "Unknown panel");
    }

    const bool visible = JsonUtils::GetBool(args, "visible", true);
    if(!Actions(context).ShowPanel(panel, visible))
    {
        return DoneResult(std::string("Could not change the ") +
                              OptiqActions::PanelName(panel) + " panel here.",
                          "Panel unavailable");
    }
    return DoneResult(std::string(visible ? "Opened the " : "Closed the ") +
                          OptiqActions::PanelName(panel) + " panel.",
                      visible ? "Opened a panel" : "Closed a panel");
}

AssistantToolStartResult
ToolResetView(const AssistantToolContext& context, const jt::Json&, const std::string&)
{
    if(!Actions(context).ResetView())
    {
        return DoneResult("Reset view is only available on a system trace.",
                          "Not available");
    }
    return DoneResult("Zoomed back out to the whole trace.", "Reset the view");
}

AssistantToolStartResult
ToolMeasure(const AssistantToolContext& context, const jt::Json& args,
            const std::string&)
{
    OptiqActions actions = Actions(context);
    if(JsonUtils::GetBool(args, "clear", false))
    {
        actions.ClearMeasurement();
        return DoneResult("Cleared the measurement.", "Cleared measurement");
    }

    const double start_ns = JsonUtils::GetDouble(args, "start_ns", -1.0);
    const double end_ns   = JsonUtils::GetDouble(args, "end_ns", -1.0);
    if(start_ns < 0.0 || end_ns <= start_ns)
    {
        return DoneResult("measure needs start_ns and end_ns with end > start.",
                          "Bad range");
    }
    if(!actions.MeasureRange(start_ns, end_ns))
    {
        return DoneResult("Could not measure on this trace.", "Not available");
    }

    SettingsManager& settings    = SettingsManager::GetInstance();
    const TimeFormat time_format = settings.GetUserSettings().unit_settings.time_format;
    return DoneResult(
        "Measured " +
            nanosecond_to_formatted_str(end_ns - start_ns, time_format, true) +
            " on the timeline.",
        "Measured a span");
}

AssistantToolStartResult
ToolBookmark(const AssistantToolContext& context, const jt::Json& args,
             const std::string&)
{
    OptiqActions      actions = Actions(context);
    const std::string action  = Core::String::to_lower_copy(JsonUtils::GetString(args, "action", "list"));
    const std::vector<int> slots = actions.ListBookmarks();

    std::ostringstream saved;
    saved << "saved_bookmarks:";
    if(slots.empty())
    {
        saved << " none";
    }
    for(int slot : slots)
    {
        saved << " " << slot;
    }
    saved << "\n";

    if(action == "list")
    {
        return DoneResult(saved.str(), "Listed bookmarks");
    }

    const int slot = static_cast<int>(JsonUtils::GetInt(args, "slot", -1));
    if(slot < 0)
    {
        return DoneResult("bookmark needs a slot from 0 to 9.\n" + saved.str(),
                          "Missing slot");
    }
    if(action == "save")
    {
        if(!actions.SaveBookmark(slot))
        {
            return DoneResult("Could not save that bookmark.", "Bookmark failed");
        }
        return DoneResult("Saved the current view to bookmark " +
                              std::to_string(slot) + ".",
                          "Saved a bookmark");
    }
    if(action == "goto")
    {
        if(!actions.GotoBookmark(slot))
        {
            return DoneResult("Bookmark " + std::to_string(slot) +
                                  " is empty.\n" + saved.str(),
                              "No such bookmark");
        }
        return DoneResult("Jumped to bookmark " + std::to_string(slot) + ".",
                          "Used a bookmark");
    }
    if(action == "remove")
    {
        if(!actions.RemoveBookmark(slot))
        {
            return DoneResult("Bookmark " + std::to_string(slot) + " is empty.",
                              "No such bookmark");
        }
        return DoneResult("Removed bookmark " + std::to_string(slot) + ".",
                          "Removed a bookmark");
    }
    return DoneResult("Unknown action. Use save, goto, remove, or list.",
                      "Bad action");
}

AssistantToolStartResult
ToolAnnotate(const AssistantToolContext& context, const jt::Json& args,
             const std::string&)
{
    if(context.is_compute)
    {
        return DoneResult("Notes are a timeline feature, so system traces only.",
                          "Wrong trace kind");
    }
    const double      time_ns = JsonUtils::GetDouble(args, "time_ns", -1.0);
    const std::string title   = JsonUtils::GetString(args, "title", "");
    const std::string text    = JsonUtils::GetString(args, "text", "");
    if(time_ns < 0.0 || title.empty() || text.empty())
    {
        return DoneResult("annotate needs time_ns, title, and text.",
                          "Missing arguments");
    }

    SettingsManager& settings    = SettingsManager::GetInstance();
    const TimeFormat time_format = settings.GetUserSettings().unit_settings.time_format;

    // A note is pinned at an absolute timestamp, but the model worked that
    // number out on an earlier turn and the user can pan, zoom or reselect in
    // between. Check the anchor against the trace before pinning, and say where
    // it actually landed, so a stale number cannot be reported as the spot
    // under discussion.
    const TimelineModel& timeline = context.data_provider->DataModel().GetTimeline();
    if(time_ns < timeline.GetStartTime() || time_ns > timeline.GetEndTime())
    {
        return DoneResult(
            "time_ns " + nanosecond_to_formatted_str(time_ns, time_format, true) +
                " is outside this trace, which runs " +
                nanosecond_to_formatted_str(timeline.GetStartTime(), time_format, true) +
                " .. " +
                nanosecond_to_formatted_str(timeline.GetEndTime(), time_format, true) +
                ". Re-read the timestamp from the data rather than reusing one "
                "from an earlier trace or turn.",
            "Note failed");
    }

    double view_start = 0.0;
    double view_end   = 0.0;
    SelectedOrFullTimeRange(context, view_start, view_end);
    if(!Actions(context).AddNote(time_ns, title, text, view_start, view_end,
                                 JsonU64(args, "track_id", INVALID_UINT64_INDEX)))
    {
        return DoneResult("Could not pin a note on this trace.", "Note failed");
    }

    std::ostringstream out;
    out << "Pinned a note titled \"" << title << "\" at "
        << nanosecond_to_formatted_str(time_ns, time_format, true) << ".";
    const bool selected = context.timeline_selection != nullptr &&
                          context.timeline_selection->HasValidTimeRangeSelection();
    if(selected && (time_ns < view_start || time_ns > view_end))
    {
        // The user moved on, or the model reused a timestamp from an earlier
        // window. Either way the note is not where the conversation is.
        out << " Note that this is outside the selected range ("
            << nanosecond_to_formatted_str(view_start, time_format, true) << " .. "
            << nanosecond_to_formatted_str(view_end, time_format, true)
            << "), so say where the note went rather than calling it the "
               "selected interval, or call goto first and annotate again.";
    }
    return DoneResult(out.str(), "Left a note");
}

AssistantToolStartResult
ToolSwitchTab(const AssistantToolContext& context, const jt::Json& args,
              const std::string&)
{
    OptiqActions                   actions       = Actions(context);
    const std::vector<std::string> trace_tabs    = actions.ListTabs();
    const std::vector<std::string> analysis_tabs = actions.ListAnalysisTabs();

    std::ostringstream out;
    out << "trace_tabs:";
    for(const std::string& tab : trace_tabs)
    {
        out << "\n  " << tab;
    }
    out << "\nactive_trace_tab: " << actions.ActiveTab() << "\n";
    if(!analysis_tabs.empty())
    {
        out << "details_panel_tabs:";
        for(const std::string& tab : analysis_tabs)
        {
            out << "\n  " << tab;
        }
        out << "\nactive_details_tab: " << actions.ActiveAnalysisTab() << "\n";
    }

    const std::string name = JsonUtils::GetString(args, "name", "");
    if(name.empty())
    {
        return DoneResult(out.str(), "Listed tabs");
    }

    // Details tabs are the ones people usually mean by name, so try those
    // first; trace tabs are file names and rarely collide.
    if(actions.SelectAnalysisTab(name))
    {
        return DoneResult("Opened the " + actions.ActiveAnalysisTab() +
                              " tab in the details panel.",
                          "Switched tabs");
    }
    if(actions.SelectTab(name))
    {
        return DoneResult("Switched to trace " + actions.ActiveTab() + ".",
                          "Switched tabs");
    }

    out << "Nothing matches \"" << name << "\".\n";
    return DoneResult(out.str(), "No matching tab");
}

AssistantToolStartResult
ToolFlowArrows(const AssistantToolContext& context, const jt::Json& args,
               const std::string&)
{
    OptiqActions actions = Actions(context);
    const bool   visible = JsonUtils::GetBool(args, "visible", true);
    if(!actions.SetFlowArrowsVisible(visible))
    {
        return DoneResult("Flow arrows are only available on a system trace.",
                          "Not available");
    }

    const std::string style = Core::String::to_lower_copy(JsonUtils::GetString(args, "style", ""));
    if(style == "chain" || style == "fan")
    {
        actions.SetFlowRenderChained(style == "chain");
    }

    std::string message = visible ? "Flow arrows are on." : "Flow arrows are off.";
    if(!style.empty())
    {
        message += " Style set to " + style + ".";
    }
    if(visible)
    {
        message +=
            " They only draw for a selected event, so call goto with an "
            "event_uuid and track_id to see them.";
    }
    return DoneResult(message, visible ? "Flow arrows on" : "Flow arrows off");
}

AssistantToolStartResult
ToolGoto(const AssistantToolContext& context, const jt::Json& args,
         const std::string&)
{
    // Not const: an event's own bounds stand in when the model names the event
    // it cares about but leaves the window out.
    double start_ns = JsonUtils::GetDouble(
        args, "start_ns", JsonUtils::GetDouble(args, "start", -1.0));
    double end_ns =
        JsonUtils::GetDouble(args, "end_ns", JsonUtils::GetDouble(args, "end", -1.0));
    const uint64_t track_id =
        JsonU64(args, "track_id", TimelineSelection::INVALID_SELECTION_ID);
    const uint64_t event_uuid =
        JsonU64(args, "event_uuid",
                JsonU64(args, "event_id", TimelineSelection::INVALID_SELECTION_ID));
    const std::string kernel_name = JsonUtils::GetString(args, "kernel_name", "");

    if(context.is_compute)
    {
        if(context.compute_selection == nullptr)
        {
            return DoneResult("Compute selection is not available.", "goto failed");
        }
        const WorkloadInfo* workload = SelectedComputeWorkload(context);
        if(workload == nullptr)
        {
            return DoneResult("No compute workload is loaded.", "goto failed");
        }
        const uint32_t kernel_id = static_cast<uint32_t>(
            JsonU64(args, "kernel_id", ComputeSelection::INVALID_SELECTION_ID));
        const KernelInfo* kernel = FindComputeKernel(*workload, kernel_name, kernel_id);
        if(kernel == nullptr)
        {
            return DoneResult("Could not find that kernel to select.", "goto failed");
        }
        Actions(context).SelectKernel(kernel->id);
        return DoneResult(std::string("Selected compute kernel ") + kernel->name,
                          "Selected kernel");
    }

    if(context.timeline_selection == nullptr)
    {
        return DoneResult("Timeline selection is not available.", "goto failed");
    }

    // The model may name one event via track_id/event_uuid or several via
    // events[]; both shapes collapse to this list. Gathered before the range is
    // settled, because an event can supply the range the model left out.
    std::vector<std::pair<uint64_t, uint64_t>> targets;
    if(event_uuid != TimelineSelection::INVALID_SELECTION_ID)
    {
        targets.emplace_back(track_id, event_uuid);
    }
    jt::Json& mutable_args = const_cast<jt::Json&>(args);
    if(mutable_args.contains("events") && mutable_args["events"].isArray())
    {
        std::vector<jt::Json>& entries = mutable_args["events"].getArray();
        std::string            length_error;
        if(!CheckArrayLength(entries, "events", length_error))
        {
            return DoneResult(length_error, "goto failed");
        }
        for(jt::Json& entry : entries)
        {
            if(targets.size() >= ASSISTANT_MAX_HIGHLIGHTS)
            {
                break;
            }
            if(!entry.isObject())
            {
                continue;
            }
            const uint64_t entry_uuid =
                JsonU64(entry, "event_uuid",
                        JsonU64(entry, "__uuid",
                                TimelineSelection::INVALID_SELECTION_ID));
            if(entry_uuid != TimelineSelection::INVALID_SELECTION_ID)
            {
                targets.emplace_back(JsonU64(entry, "track_id", track_id), entry_uuid);
            }
        }
    }

    // isfinite is doing real work here: NaN fails all three comparisons, so
    // without it a NaN bound would be accepted as a valid range.
    bool has_range = std::isfinite(start_ns) && std::isfinite(end_ns) &&
                     start_ns >= 0.0 && end_ns > start_ns;

    // Naming the culprit event but no window is the obvious way to call this,
    // so derive the window from the events rather than refusing. Only works for
    // events the view has already loaded; when it does not, the selection below
    // still happens and only the framing is skipped.
    if(!has_range && !targets.empty())
    {
        std::vector<uint64_t> ids;
        ids.reserve(targets.size());
        for(const std::pair<uint64_t, uint64_t>& target : targets)
        {
            ids.push_back(target.second);
        }
        double found_start = 0.0;
        double found_end   = 0.0;
        if(context.data_provider->DataModel().GetEvents().GetEventTimeRange(
               ids, found_start, found_end) &&
           std::isfinite(found_start) && std::isfinite(found_end) &&
           found_start >= 0.0 && found_end > found_start)
        {
            start_ns  = found_start;
            end_ns    = found_end;
            has_range = true;
        }
    }

    if(!has_range && targets.empty())
    {
        return DoneResult(
            "goto needs either start_ns and end_ns (end > start), or the "
            "__uuid of an event to put the view on.",
            "goto failed");
    }

    OptiqActions actions = Actions(context);
    const bool   zoom    = JsonUtils::GetBool(args, "zoom", false);
    if(has_range)
    {
        actions.SelectRange(start_ns, end_ns);
        if(zoom)
        {
            actions.ZoomToRange(start_ns, end_ns);
        }
    }

    // The first event that takes becomes the focal point: it is clicked, which
    // is what makes the trace view load its details, flow arrows and call
    // stack. Everything after it is only highlighted, as context around it.
    //
    // The focal event is deliberately not highlighted as well. Where the two
    // overlap the highlight border wins, so highlighting the clicked event
    // would draw it exactly like the rest and leave the user a wall of equally
    // bright boxes with nothing to look at. Marking it before navigating also
    // frames the view on an event that actually took, rather than on whichever
    // one the model happened to list first.
    size_t                        marked     = 0;
    bool                          have_focus = false;
    std::pair<uint64_t, uint64_t> focus(0, 0);
    if(!targets.empty())
    {
        actions.ClearHighlights();
        for(const std::pair<uint64_t, uint64_t>& target : targets)
        {
            if(!have_focus)
            {
                if(!actions.ClickEvent(target.first, target.second))
                {
                    continue;
                }
                have_focus = true;
                focus      = target;
                actions.ScrollToTrack(target.first);
                ++marked;
                continue;
            }
            if(actions.HighlightEvent(target.first, target.second))
            {
                ++marked;
            }
        }
    }

    // Framing needs a window. Without one the selection above still stands, so
    // the user has their focal point even though the view did not move.
    if(has_range)
    {
        if(have_focus)
        {
            actions.NavigateToEvent(focus.first, focus.second, start_ns,
                                    end_ns - start_ns);
        }
        else
        {
            actions.ShowRange(start_ns, end_ns);
        }
    }

    SettingsManager& settings    = SettingsManager::GetInstance();
    const TimeFormat time_format = settings.GetUserSettings().unit_settings.time_format;
    std::ostringstream out;
    if(has_range)
    {
        out << "Moved the timeline to "
            << nanosecond_to_formatted_str(start_ns, time_format, true) << " .. "
            << nanosecond_to_formatted_str(end_ns, time_format, true);
        if(zoom)
        {
            out << ", zoomed in";
        }
    }
    else
    {
        out << "Left the timeline where it was, because no window was given and "
               "that event is not loaded in the view yet";
    }
    if(have_focus)
    {
        out << " and selected one event as the focal point, so its details, flow "
               "arrows and call stack are loaded";
        if(marked > 1)
        {
            out << ", with " << (marked - 1) << " more highlighted around it";
        }
    }
    else if(!targets.empty())
    {
        // Saying "moved the view" alone would let the model believe it had
        // selected events it never selected, and report that to the user.
        out << ", but none of the " << targets.size()
            << " event(s) named could be selected - check that __uuid and "
               "__trackId came from a table read on this trace";
    }
    out << ".";
    return DoneResult(out.str(), "Moved the view");
}

const AssistantToolEntry k_ui_tool_handlers[] = {
    { "offer_next_steps", ToolOfferNextSteps },
    { "show_panel", ToolShowPanel },
    { "reset_view", ToolResetView },
    { "measure", ToolMeasure },
    { "bookmark", ToolBookmark },
    { "annotate", ToolAnnotate },
    { "switch_tab", ToolSwitchTab },
    { "flow_arrows", ToolFlowArrows },
    { "goto", ToolGoto },
};

}  // namespace

// The UI half of the tool set, for StartAssistantTool to search.
AssistantToolTable
GetAssistantUiToolHandlers()
{
    AssistantToolTable table;
    table.entries = k_ui_tool_handlers;
    table.count   = sizeof(k_ui_tool_handlers) / sizeof(k_ui_tool_handlers[0]);
    return table;
}

}  // namespace View
}  // namespace RocProfVis
