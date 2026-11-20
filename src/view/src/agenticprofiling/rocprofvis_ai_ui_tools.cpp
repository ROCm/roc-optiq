// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The tools that change Optiq rather than read it. Every body here goes through
// OptiqActions and answers in the same call, so none of them park a fetch or
// touch a request id. Adding a capability belongs in rocprofvis_ai_actions.h as
// one method, not as wiring inside a tool here.
#include "rocprofvis_ai_tools_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "json.h"

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
// Notes are saved with the project, so what one turn can leave is bounded.
constexpr size_t ASSISTANT_MAX_NOTE_TITLE_CHARS = 120;
constexpr size_t ASSISTANT_MAX_NOTE_TEXT_CHARS  = 2000;
constexpr size_t ASSISTANT_MAX_NOTES_PER_TURN   = 5;
constexpr uint64_t ASSISTANT_TAB_REF_OFFSET      = 14695981039346656037ULL;
constexpr uint64_t ASSISTANT_TAB_REF_PRIME       = 1099511628211ULL;

// A per-turn limit that holds even when text in the trace talks the model out
// of its prompt. BeginAssistantUiTurn resets it.
size_t g_notes_pinned = 0;

// Stable for one project id without sending its full path to the model. The
// mapping is recomputed from the open tabs, so closing another tab cannot
// silently redirect an earlier ref.
std::string
TabRef(const std::string& project_id)
{
    uint64_t hash = ASSISTANT_TAB_REF_OFFSET;
    for(unsigned char c : project_id)
    {
        hash ^= c;
        hash *= ASSISTANT_TAB_REF_PRIME;
    }
    std::ostringstream out;
    out << "tab_" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

// Every UI-facing thing a tool does goes through here.
OptiqActions
Actions(const AssistantToolContext& context)
{
    return OptiqActions(context.data_provider, context.timeline_selection,
                        context.trace_view);
}

// The result of an action carried out exactly as asked (see settled). A
// refusal, a listing, or an action that missed stays a plain DoneResult.
AssistantToolStartResult
SettledResult(const std::string& content, const std::string& status)
{
    AssistantToolStartResult result = DoneResult(content, status);
    result.settled                  = true;
    return result;
}

// Cuts a note or a next step to max_chars and marks the cut. A note is saved
// into the project file, and a next step is sent as a message.
std::string
TrimNoteText(std::string text, size_t max_chars)
{
    return TrimAssistantText(std::move(text), max_chars, "...");
}

// Holds a window to the trace. False when none of it lies inside.
bool
ClampToTrace(const TimelineModel& timeline, double& start_ns, double& end_ns)
{
    const double trace_start = timeline.GetStartTime();
    const double trace_end   = timeline.GetEndTime();
    if(trace_end > trace_start)
    {
        start_ns = std::max(start_ns, trace_start);
        end_ns   = std::min(end_ns, trace_end);
    }
    return end_ns > start_ns;
}

// A next step as its button shows it, which a click sends as the user's own
// words. ImGui hides what follows "##", so that and control characters go.
std::string
PrintableStep(const std::string& text)
{
    std::string step;
    step.reserve(text.size());
    for(const char c : text)
    {
        if(c == '#' && !step.empty() && step.back() == '#')
        {
            continue;
        }
        const bool control = static_cast<unsigned char>(c) < 0x20 || c == 0x7F;
        step += control ? ' ' : c;
    }
    return TrimNoteText(Core::String::trim_copy(step), ASSISTANT_MAX_NEXT_STEP_CHARS);
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
        const std::string step = PrintableStep(entry.getString());
        if(step.empty())
        {
            continue;
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
    return SettledResult(std::string(visible ? "Opened the " : "Closed the ") +
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
    return SettledResult("Zoomed back out to the whole trace.", "Reset the view");
}

AssistantToolStartResult
ToolMeasure(const AssistantToolContext& context, const jt::Json& args,
            const std::string&)
{
    OptiqActions actions = Actions(context);
    if(JsonUtils::GetBool(args, "clear", false))
    {
        actions.ClearMeasurement();
        return SettledResult("Cleared the measurement.", "Cleared measurement");
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
    return SettledResult(
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
        return SettledResult("Saved the current view to bookmark " +
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
        return SettledResult("Jumped to bookmark " + std::to_string(slot) + ".",
                             "Used a bookmark");
    }
    if(action == "remove")
    {
        if(!actions.RemoveBookmark(slot))
        {
            return DoneResult("Bookmark " + std::to_string(slot) + " is empty.",
                              "No such bookmark");
        }
        return SettledResult("Removed bookmark " + std::to_string(slot) + ".",
                             "Removed a bookmark");
    }
    return DoneResult("Unknown action. Use save, goto, remove, or list.",
                      "Bad action");
}

AssistantToolStartResult
ToolAnnotate(const AssistantToolContext& context, const jt::Json& args,
             const std::string&)
{
    const double      time_ns = JsonUtils::GetDouble(args, "time_ns", -1.0);
    const std::string title   = TrimNoteText(JsonUtils::GetString(args, "title", ""),
                                             ASSISTANT_MAX_NOTE_TITLE_CHARS);
    const std::string text    = JsonUtils::GetString(args, "text", "");
    if(time_ns < 0.0 || title.empty() || text.empty())
    {
        return DoneResult("annotate needs time_ns, title, and text.",
                          "Missing arguments");
    }
    if(g_notes_pinned >= ASSISTANT_MAX_NOTES_PER_TURN)
    {
        return DoneResult("This turn has already pinned " +
                              std::to_string(ASSISTANT_MAX_NOTES_PER_TURN) +
                              " notes, which is the most one turn may leave. Put "
                              "the rest in your answer instead.",
                          "Too many notes");
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
    if(!Actions(context).AddNote(time_ns, title,
                                 TrimNoteText(text, ASSISTANT_MAX_NOTE_TEXT_CHARS),
                                 view_start, view_end,
                                 JsonU64(args, "track_id", INVALID_UINT64_INDEX)))
    {
        return DoneResult("Could not pin a note on this trace.", "Note failed");
    }
    ++g_notes_pinned;

    std::ostringstream out;
    out << "Pinned a note titled \"" << title << "\" at "
        << nanosecond_to_formatted_str(time_ns, time_format, true) << ".";
    if(text.size() > ASSISTANT_MAX_NOTE_TEXT_CHARS)
    {
        out << " Its text was too long for a note and was cut at "
            << ASSISTANT_MAX_NOTE_TEXT_CHARS << " characters.";
    }
    const bool selected = context.timeline_selection != nullptr &&
                          context.timeline_selection->HasValidTimeRangeSelection();
    const bool outside  = selected && (time_ns < view_start || time_ns > view_end);
    if(outside)
    {
        // The user moved on, or the model reused a timestamp from an earlier
        // window. Either way the note is not where the conversation is.
        out << " Note that this is outside the selected range ("
            << nanosecond_to_formatted_str(view_start, time_format, true) << " .. "
            << nanosecond_to_formatted_str(view_end, time_format, true)
            << "), so say where the note went rather than calling it the "
               "selected interval, or call goto first and annotate again.";
    }
    AssistantToolStartResult result = DoneResult(out.str(), "Left a note");
    // A note that missed the selection may be redone after a goto.
    result.settled = !outside;
    return result;
}

AssistantToolStartResult
ToolSwitchTab(const AssistantToolContext& context, const jt::Json& args,
              const std::string&)
{
    OptiqActions                   actions       = Actions(context);
    const std::vector<std::string> trace_tabs    = actions.ListTabs();
    const std::vector<std::string> trace_tab_ids = actions.ListTabIds();
    const std::vector<std::string> analysis_tabs = actions.ListAnalysisTabs();

    std::ostringstream out;
    out << "trace_tabs:";
    for(size_t i = 0; i < trace_tabs.size(); ++i)
    {
        out << "\n  ref=" << TabRef(trace_tab_ids[i]) << " name=\"" << trace_tabs[i]
            << "\"";
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

    std::string name = JsonUtils::GetString(args, "name", "");
    if(name.empty())
    {
        return DoneResult(out.str(), "Listed tabs");
    }

    // Details tabs are the ones people usually mean by name, so try those
    // first; trace tabs are file names and rarely collide.
    std::string selected;
    if(actions.SelectAnalysisTab(name, selected))
    {
        return SettledResult("Opened the " + selected + " tab in the details panel.",
                             "Switched tabs");
    }
    // A ref is unambiguous even when two paths have the same basename. The
    // labels remain accepted when they name exactly one tab.
    std::string       target     = name;
    const std::string ref_prefix = "tab_";
    if(name.compare(0, ref_prefix.size(), ref_prefix) == 0)
    {
        const std::string* matched_id = nullptr;
        for(const std::string& tab_id : trace_tab_ids)
        {
            if(TabRef(tab_id) != name)
            {
                continue;
            }
            if(matched_id != nullptr)
            {
                matched_id = nullptr;  // A hash collision is ambiguous.
                break;
            }
            matched_id = &tab_id;
        }
        if(matched_id != nullptr)
        {
            // A file path, so it is kept apart from the name the reply quotes.
            target = *matched_id;
        }
    }
    std::string selected_id;
    if(actions.SelectTab(target, selected_id, selected))
    {
        if(selected_id == context.project_id)
        {
            return SettledResult("Trace " + selected + " is already in front.",
                                 "Switched tabs");
        }
        // The turn stays with its own trace, so say where the user went rather
        // than let the model think it can now read the other one.
        AssistantToolStartResult result =
            SettledResult("Switched the user to trace " + selected +
                              ". This conversation stays with " + context.trace_name +
                              ", so read and change nothing more here; questions about " +
                              selected + " continue in its own conversation.",
                          "Switched tabs");
        result.stop_tool_queue = true;
        return result;
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
    // With nothing selected there is nothing for them to draw from yet, which
    // the model can still put right, so that reply is not a settled one.
    if(visible && (context.timeline_selection == nullptr ||
                   !context.timeline_selection->HasSelectedEvents()))
    {
        return DoneResult(message + " They only draw for a selected event, so call "
                                    "goto with an event_uuid and track_id to see them.",
                          "Flow arrows on");
    }
    return SettledResult(message, visible ? "Flow arrows on" : "Flow arrows off");
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
        JsonU64(args, "track_id",
                JsonU64(args, "__trackId", TimelineSelection::INVALID_SELECTION_ID));
    const uint64_t event_uuid = JsonU64(
        args, "event_uuid",
        JsonU64(args, "__uuid",
                JsonU64(args, "event_id", TimelineSelection::INVALID_SELECTION_ID)));
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
                targets.emplace_back(
                    JsonU64(entry, "track_id", JsonU64(entry, "__trackId", track_id)),
                    entry_uuid);
            }
        }
    }

    // Naming one event as event_uuid and again inside events[] is the natural
    // way to write "this is the one I mean, and here is the set", so the same
    // uuid arrives twice. Left in, the second copy highlights the event the
    // first copy selected, the highlight border wins where they overlap, and
    // the focal point disappears back into the group it was supposed to stand
    // out from.
    std::vector<std::pair<uint64_t, uint64_t>> unique_targets;
    unique_targets.reserve(targets.size());
    for(const std::pair<uint64_t, uint64_t>& target : targets)
    {
        bool seen = false;
        for(const std::pair<uint64_t, uint64_t>& kept : unique_targets)
        {
            if(kept.second == target.second)
            {
                seen = true;
                break;
            }
        }
        if(!seen)
        {
            unique_targets.push_back(target);
        }
    }
    targets.swap(unique_targets);

    // isfinite is doing real work here: NaN fails all three comparisons, so
    // without it a NaN bound would be accepted as a valid range.
    bool has_range = std::isfinite(start_ns) && std::isfinite(end_ns) &&
                     start_ns >= 0.0 && end_ns > start_ns;
    // The start and end of an event with no duration, as its row prints them.
    // Zero for both is how an argument left out tends to be spelled instead.
    bool pointlike = std::isfinite(start_ns) && start_ns > 0.0 && end_ns == start_ns;

    // goto is the one writer of the selection the data tools default to, so a
    // window outside the trace must not be stored: every later read would come
    // back empty. The trace's window is reported in raw nanoseconds so the
    // model can pass it straight back.
    const TimelineModel& timeline = context.data_provider->DataModel().GetTimeline();
    const std::string    trace_window =
        "window_ns is " + nanosecond_to_str(timeline.GetStartTime()) + " .. " +
        nanosecond_to_str(timeline.GetEndTime());
    bool range_dropped = false;
    if(has_range && !ClampToTrace(timeline, start_ns, end_ns))
    {
        if(targets.empty())
        {
            return DoneResult("That range is outside this trace, whose " + trace_window +
                                  ". Re-read the range from the data rather than "
                                  "reusing one from an earlier trace or turn.",
                              "goto failed");
        }
        // The named events supply the window instead.
        has_range     = false;
        range_dropped = true;
    }

    // An event named with no window is framed by its own span, from a row read
    // or its loaded details. With neither, the click below still happens.
    if(!has_range && !targets.empty())
    {
        double found_start = 0.0;
        double found_end   = 0.0;
        bool   found       = false;
        for(const std::pair<uint64_t, uint64_t>& target : targets)
        {
            const AssistantEventSighting sighting =
                FindAssistantEvent(context, target.second);
            if(!sighting.has_span)
            {
                pointlike = pointlike || (sighting.seen && sighting.start_ns > 0.0 &&
                                          sighting.end_ns == sighting.start_ns);
                continue;
            }
            found_start = found ? std::min(found_start, sighting.start_ns)
                                : sighting.start_ns;
            found_end   = found ? std::max(found_end, sighting.end_ns) : sighting.end_ns;
            found       = true;
        }
        if(found && std::isfinite(found_start) && std::isfinite(found_end) &&
           found_start >= 0.0 && found_end > found_start &&
           ClampToTrace(timeline, found_start, found_end))
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
            "__uuid of an event to put the view on, passed as a quoted string. "
            "A 19-digit uuid sent as a number is rejected rather than rounded, "
            "since rounding it would silently select another event.",
            "goto failed");
    }

    OptiqActions actions = Actions(context);
    if(has_range)
    {
        actions.SelectRange(start_ns, end_ns);
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
        if(range_dropped)
        {
            out << ", the named events' own window, because the range you gave is "
                   "outside this trace, whose "
                << trace_window;
        }
    }
    else if(range_dropped)
    {
        out << "Left the timeline where it was, because the range you gave is outside "
               "this trace, whose "
            << trace_window
            << ", and the event's own start and end are not in the rows read last - "
               "pass them from its row";
    }
    else if(pointlike)
    {
        out << "Left the timeline where it was, because there is no span to frame: an "
               "event with no duration needs start_ns and end_ns a little either side "
               "of it";
    }
    else
    {
        out << "Left the timeline where it was, because no window was given and the "
               "event's own start and end are not in the rows read last - pass them "
               "as start_ns and end_ns from its row";
    }
    if(have_focus)
    {
        out << (has_range ? " and selected" : ". Selected") << " the event with __uuid \""
            << focus.second << "\" as the focal point";
        if(marked > 1)
        {
            out << ", with " << (marked - 1) << " more highlighted around it";
        }
        // Selecting takes any uuid. Only a row read or loaded details can
        // vouch for one, so without either nothing is promised.
        if(FindAssistantEvent(context, focus.second).seen)
        {
            out << ". Its details, flow arrows and call stack load in the details "
                   "panel";
        }
        else
        {
            out << ". Its details, flow arrows and call stack load from that __uuid, "
                   "so they stay empty if it is not an event of this trace";
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
    AssistantToolStartResult result = DoneResult(out.str(), "Moved the view");
    // Settled only when the view went where it was sent and every event named
    // was marked. Anything short of that the model may want to correct.
    result.settled = has_range && !range_dropped && marked == targets.size();
    return result;
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

/*
 * The two above that mean the same thing whatever kind of trace is in front.
 *
 * offer_next_steps only writes buttons under the chat. switch_tab reads
 * AppWindow's tab container, and the details-panel half of it already
 * null-guards the TraceView, so on a compute trace it lists the open traces and
 * switches between them without mentioning tabs that are not there.
 *
 * show_panel is deliberately absent. Six of its eight panels are persisted in
 * AppWindowSettings and apply to system traces, so on a compute tab it would
 * report having opened a panel the user cannot see - a false success is worse
 * to the model than an unknown tool, because it answers as though it worked.
 */
const AssistantToolEntry k_shared_ui_tool_handlers[] = {
    { "offer_next_steps", ToolOfferNextSteps },
    { "switch_tab", ToolSwitchTab },
};

}  // namespace

// Starts a turn's allowance for the tools above.
void
BeginAssistantUiTurn()
{
    g_notes_pinned = 0;
}

// The UI half of the tool set, for StartAssistantTool to search.
AssistantToolTable
GetAssistantUiToolHandlers()
{
    AssistantToolTable table;
    table.entries = k_ui_tool_handlers;
    table.count   = sizeof(k_ui_tool_handlers) / sizeof(k_ui_tool_handlers[0]);
    return table;
}

// The trace-kind-agnostic subset, searched on a compute trace.
AssistantToolTable
GetAssistantSharedUiToolHandlers()
{
    AssistantToolTable table;
    table.entries = k_shared_ui_tool_handlers;
    table.count = sizeof(k_shared_ui_tool_handlers) / sizeof(k_shared_ui_tool_handlers[0]);
    return table;
}

}  // namespace View
}  // namespace RocProfVis
