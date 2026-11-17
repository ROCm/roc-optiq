// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The tools that change Optiq rather than read it. Every body here goes through
// OptiqActions and none of them touch a request id. All but one answer in the
// same call: switch_tab waits for the tab bar to bring a newly chosen trace
// forward. Adding a capability belongs in rocprofvis_ai_actions.h as one method,
// not as wiring inside a tool here.
#include "rocprofvis_ai_tools_internal.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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
// A chosen trace comes forward within a couple of frames. This only bounds a
// switch that never lands, such as one to a tab closed in the meantime.
constexpr uint32_t ASSISTANT_TAB_SWITCH_TIMEOUT_SECONDS = 5;
// Notes are saved with the project, so what one turn can leave behind is
// bounded: a heading stays a heading, the body stays something a sticky note
// can show, and a turn that has gone wrong cannot paper the timeline.
constexpr size_t ASSISTANT_MAX_NOTE_TITLE_CHARS = 120;
constexpr size_t ASSISTANT_MAX_NOTE_TEXT_CHARS  = 2000;
constexpr size_t ASSISTANT_MAX_NOTES_PER_TURN   = 5;

/*
 * What the turn in progress may still do through these tools.
 *
 * The prompt tells the model to leave notes and change traces only when the
 * user asked, but names and arguments in a trace are text someone else wrote,
 * and a model that takes them for instructions will not honour a prompt. These
 * are the limits that hold regardless. The panel starts each turn through
 * BeginAssistantUiTurn; until it has done so once nothing is known about the
 * turn, and nothing is restricted.
 */
struct UiTurnState
{
    bool   tracked              = false;
    // The user's own message asked for a tab change, or named another trace.
    bool   tab_switch_requested = false;
    size_t notes_pinned         = 0;
};

UiTurnState g_ui_turn;

// Every UI-facing thing a tool does goes through here.
OptiqActions
Actions(const AssistantToolContext& context)
{
    return OptiqActions(context.data_provider, context.timeline_selection,
                        context.trace_view);
}

// True when word appears in text as a word of its own, so that "tab" is not
// found inside "table". Both are expected in lower case.
bool
ContainsWord(const std::string& text, const std::string& word)
{
    size_t at = text.find(word);
    while(at != std::string::npos)
    {
        const size_t end = at + word.size();
        const bool   starts =
            at == 0 || std::isalnum(static_cast<unsigned char>(text[at - 1])) == 0;
        const bool ends = end >= text.size() ||
                          std::isalnum(static_cast<unsigned char>(text[end])) == 0;
        if(starts && ends)
        {
            return true;
        }
        at = text.find(word, at + 1);
    }
    return false;
}

// Whether what the user typed asks to change tabs: it says so in as many
// words, or it names an open trace other than the one in front.
bool
AsksForTabSwitch(const std::string& user_message)
{
    const std::string text = Core::String::to_lower_copy(user_message);
    // "Go to the other trace" asks for the same thing without either word.
    if(ContainsWord(text, "tab") || ContainsWord(text, "tabs") ||
       ContainsWord(text, "switch") || text.find("other trace") != std::string::npos ||
       text.find("other run") != std::string::npos)
    {
        return true;
    }

    // Listing the tabs needs nothing from a trace, so no context is required.
    const OptiqActions actions(nullptr, nullptr, nullptr);
    const std::string  active = Core::String::to_lower_copy(actions.ActiveTab());
    for(const std::string& tab : actions.ListTabs())
    {
        const std::string label = Core::String::to_lower_copy(tab);
        if(!label.empty() && label != active && text.find(label) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

// Cuts model-written text to max_chars and marks the cut. Backs up to the start
// of a character first: a note is saved into the project file, and half a
// UTF-8 sequence there would be read back as garbage.
std::string
TrimNoteText(std::string text, size_t max_chars)
{
    if(text.size() <= max_chars)
    {
        return text;
    }
    size_t cut = max_chars;
    while(cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
    {
        --cut;
    }
    text.resize(cut);
    text += "...";
    return text;
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
    const double      time_ns  = JsonUtils::GetDouble(args, "time_ns", -1.0);
    const std::string raw_text = JsonUtils::GetString(args, "text", "");
    const std::string title    = TrimNoteText(JsonUtils::GetString(args, "title", ""),
                                              ASSISTANT_MAX_NOTE_TITLE_CHARS);
    const std::string text     = TrimNoteText(raw_text, ASSISTANT_MAX_NOTE_TEXT_CHARS);
    if(time_ns < 0.0 || title.empty() || text.empty())
    {
        return DoneResult("annotate needs time_ns, title, and text.",
                          "Missing arguments");
    }
    if(g_ui_turn.tracked && g_ui_turn.notes_pinned >= ASSISTANT_MAX_NOTES_PER_TURN)
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
    if(!Actions(context).AddNote(time_ns, title, text, view_start, view_end,
                                 JsonU64(args, "track_id", INVALID_UINT64_INDEX)))
    {
        return DoneResult("Could not pin a note on this trace.", "Note failed");
    }
    ++g_ui_turn.notes_pinned;

    std::ostringstream out;
    out << "Pinned a note titled \"" << title << "\" at "
        << nanosecond_to_formatted_str(time_ns, time_format, true) << ".";
    if(raw_text.size() > ASSISTANT_MAX_NOTE_TEXT_CHARS)
    {
        out << " Its text was too long for a note and was cut at "
            << ASSISTANT_MAX_NOTE_TEXT_CHARS << " characters.";
    }
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

    // The other open traces are the user's business until they bring one up.
    // A call nobody asked for learns only about the trace it is already on,
    // and further down is refused the switch, because every tool after a
    // switch reads the new trace and sends what it reads to the endpoint.
    const bool other_traces = !g_ui_turn.tracked || g_ui_turn.tab_switch_requested;

    std::ostringstream out;
    out << "trace_tabs:";
    if(other_traces)
    {
        for(const std::string& tab : trace_tabs)
        {
            out << "\n  " << tab;
        }
    }
    else
    {
        out << "\n  " << actions.ActiveTab()
            << "\n  (any other open trace is listed only when the user asks to "
               "change tabs)";
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
    std::string selected_label;
    if(actions.SelectAnalysisTab(name, selected_label))
    {
        return DoneResult("Opened the " + selected_label + " tab in the details panel.",
                          "Switched tabs");
    }
    // Refused whether or not the name matches a trace, so the reply cannot be
    // used to find out which traces are open.
    if(!other_traces)
    {
        out << "\"" << name
            << "\" is not a details panel tab. Switching to another trace needs "
               "the user to ask for it, so tell the user which trace you would "
               "switch to and why.\n";
        return DoneResult(out.str(), "Tab switch not requested");
    }
    // The trace in front changes only when the tab bar next draws, so the call
    // waits for that instead of answering now: the panel re-pins the turn and
    // drops the rest of the batch by the trace in front.
    std::string selected_id;
    if(actions.SelectTab(name, selected_id, selected_label))
    {
        AssistantToolStartResult result;
        result.pending          = true;
        result.started_fetch    = true;
        result.fetch.kind       = AssistantFetchKind::kTabSwitch;
        result.fetch.project_id = selected_id;
        result.content          = "Switched to trace " + selected_label + ".";
        result.status_line      = "Switching tabs...";
        result.timeout_seconds  = ASSISTANT_TAB_SWITCH_TIMEOUT_SECONDS;
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
        JsonU64(args, "track_id",
                JsonU64(args, "__trackId", TimelineSelection::INVALID_SELECTION_ID));
    const uint64_t event_uuid =
        JsonU64(args, "event_uuid",
                JsonU64(args, "event_id", TimelineSelection::INVALID_SELECTION_ID));
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

    // goto is the one writer of the selection the data tools default to, so a
    // window outside the trace must not be stored: every later read would come
    // back empty and read as "nothing happened here".
    if(has_range)
    {
        const TimelineModel& timeline = context.data_provider->DataModel().GetTimeline();
        const double         trace_start = timeline.GetStartTime();
        const double         trace_end   = timeline.GetEndTime();
        if(trace_end > trace_start)
        {
            if(end_ns <= trace_start || start_ns >= trace_end)
            {
                if(targets.empty())
                {
                    const TimeFormat fmt = SettingsManager::GetInstance()
                                               .GetUserSettings()
                                               .unit_settings.time_format;
                    return DoneResult(
                        "That range is outside this trace, which runs " +
                            nanosecond_to_formatted_str(trace_start, fmt, true) +
                            " .. " + nanosecond_to_formatted_str(trace_end, fmt, true) +
                            ". Re-read the range from the data rather than reusing "
                            "one from an earlier trace or turn.",
                        "goto failed");
                }
                // The named events supply the window instead.
                has_range = false;
            }
            else
            {
                start_ns = std::max(start_ns, trace_start);
                end_ns   = std::min(end_ns, trace_end);
            }
        }
    }

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

// Starts a turn's allowance for the tools above, from what the user typed.
void
BeginAssistantUiTurn(const std::string& user_message)
{
    g_ui_turn                      = UiTurnState{};
    g_ui_turn.tracked              = true;
    g_ui_turn.tab_switch_requested = AsksForTabSwitch(user_message);
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
