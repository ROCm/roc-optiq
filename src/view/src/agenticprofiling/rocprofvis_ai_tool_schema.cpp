// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_tool_schema.h"

#include <sstream>
#include <string>
#include <vector>

#include "rocprofvis_ai_actions.h"
#include "rocprofvis_ai_tool_query.h"

namespace RocProfVis
{
namespace View
{

namespace
{

struct AssistantToolLabel
{
    const char* name;
    const char* status;
};

// Every tool the model can call, with the line the panel shows while it runs.
// One list keeps the status text and the "unknown tool" reply in step with what
// BuildAssistantToolsJson registers.
const AssistantToolLabel ASSISTANT_TOOL_LABELS[] = {
    { "trace_overview", "Reading the timeline overview..." },
    { "get_summary", "Loading summary..." },
    { "top_events", "Querying top events..." },
    { "kernel_instances", "Loading kernel dispatches..." },
    { "kernel_metrics", "Loading kernel metrics..." },
    { "list_tracks", "Listing tracks..." },
    { "search_events", "Searching the trace..." },
    { "track_events", "Reading track events..." },
    { "track_samples", "Reading counter samples..." },
    { "event_details", "Loading event details..." },
    { "track_statistics", "Computing track statistics..." },
    { "goto", "Moving the view..." },
    { "show_panel", "Opening a panel..." },
    { "switch_tab", "Switching tabs..." },
    { "flow_arrows", "Adjusting flow arrows..." },
    { "annotate", "Leaving a note..." },
    { "bookmark", "Working with bookmarks..." },
    { "measure", "Measuring..." },
    { "reset_view", "Resetting the view..." },
    { "offer_next_steps", "Offering next steps..." },
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    { "run_analysis_script", "Running an analysis script..." },
#endif
};

// Builds a JSON string array, for the enum of a tool parameter.
jt::Json
MakeStringEnum(const std::vector<const char*>& values)
{
    jt::Json array;
    for(size_t i = 0; i < values.size(); ++i)
    {
        array[i] = values[i];
    }
    return array;
}

// Registers one tool in the schema array the model receives.
void
AddTool(jt::Json& tools, size_t index, const char* name, const char* description,
        jt::Json parameters)
{
    tools[index]["type"]                    = "function";
    tools[index]["function"]["name"]        = name;
    tools[index]["function"]["description"] = description;
    tools[index]["function"]["parameters"]  = parameters;
}

// Starts an empty object schema, which every tool's parameters build on.
jt::Json
ObjectParams()
{
    jt::Json params;
    params["type"] = "object";
    params["properties"].setObject();
    return params;
}

// Adds one typed, described property to a parameter schema.
void
AddParam(jt::Json& params, const char* name, const char* type, const char* description)
{
    params["properties"][name]["type"]        = type;
    params["properties"][name]["description"] = description;
}

// The window and tracks a table tool reads from.
void
AddScopeParams(jt::Json& params)
{
    AddParam(params, "start_ns", "number",
             "Window start in nanoseconds. Defaults to the user's selected range, "
             "or the whole trace.");
    AddParam(params, "end_ns", "number", "Window end in nanoseconds.");
    AddParam(params, "track_ids", "array",
             "Track ids from list_tracks. Defaults to every track that carries the "
             "requested events.");
    params["properties"]["track_ids"]["items"]["type"] = "integer";
}

// The structured stand-in for a SQL query: filters, sort, paging, grouping.
void
AddQueryParams(jt::Json& params)
{
    jt::Json filter_item = ObjectParams();
    AddParam(filter_item, "column", "string",
             "Column name. A bad name comes back with the allowed list.");
    AddParam(filter_item, "op", "string",
             "=, !=, <, <=, >, >=, contains, or starts_with.");
    filter_item["properties"]["value"]["description"] =
        "String or number to compare against.";
    filter_item["required"][0] = "column";
    filter_item["required"][1] = "value";

    AddParam(params, "filters", "array",
             "Conditions combined with AND, e.g. "
             "[{\"column\":\"duration\",\"op\":\">=\",\"value\":5000}].");
    params["properties"]["filters"]["items"] = filter_item;

    AddParam(params, "sort_by", "string",
             "Column name to sort by. Call once without it to see the columns.");
    AddParam(params, "sort_order", "string", "asc or desc.");
    params["properties"]["sort_order"]["enum"] = MakeStringEnum({ "asc", "desc" });
    AddParam(params, "limit", "integer", "Rows to return (default 20, max 200).");
    AddParam(params, "offset", "integer",
             "Rows to skip. Use with limit to page through a big result.");
    AddParam(params, "group_by", "string",
             "Optional column to aggregate rows by, e.g. name.");
}

}  // namespace

// Every registered tool name, for telling the model when it invents one.
std::string
AssistantToolNameList()
{
    std::ostringstream out;
    bool               first = true;
    for(const AssistantToolLabel& label : ASSISTANT_TOOL_LABELS)
    {
        if(!first)
        {
            out << ", ";
        }
        first = false;
        out << label.name;
    }
    return out.str();
}

// Builds the tool schema sent with every request. The descriptions here are
// the only instructions the model gets about what each tool is for.
jt::Json
BuildAssistantToolsJson()
{
    jt::Json tools;

    jt::Json summary_params = ObjectParams();
    AddTool(tools, 0, "get_summary",
            "Load named GPU summary stats and the top kernels by execution time. "
            "Call this first when the briefing is empty or kernel_exec_time_total_ns is 0.",
            summary_params);

    jt::Json top_params = ObjectParams();
    AddParam(top_params, "category", "string", "Event family to rank by duration.");
    top_params["properties"]["category"]["enum"] =
        MakeStringEnum({ "dispatch", "memory_copy", "memory_alloc", "instrumented",
                         "sampled" });
    AddScopeParams(top_params);
    AddQueryParams(top_params);
    top_params["required"][0] = "category";
    AddTool(tools, 1, "top_events",
            "Rank the hottest events of one category, optionally narrowed to named "
            "tracks, an explicit time window, and column filters. Uses the same "
            "analysis tables as View > Top Events. Does not run raw SQL.",
            top_params);

    jt::Json inst_params = ObjectParams();
    AddParam(inst_params, "kernel_name", "string",
             "Exact or unique kernel name from get_summary / top_events.");
    AddScopeParams(inst_params);
    AddQueryParams(inst_params);
    inst_params["required"][0] = "kernel_name";
    AddTool(tools, 2, "kernel_instances",
            "List individual GPU dispatches for one kernel name, with timestamps "
            "and track ids that goto can use.",
            inst_params);

    jt::Json metrics_params = ObjectParams();
    AddParam(metrics_params, "kernel_name", "string",
             "Kernel name (compute traces) or summary kernel name (system traces).");
    AddParam(metrics_params, "kernel_id", "integer",
             "Kernel id from get_summary, instead of the name, on a compute trace.");
    AddParam(metrics_params, "metric_name", "string",
             "Optional substring of a hardware metric to fetch on a compute trace.");
    AddTool(tools, 3, "kernel_metrics",
            "Return duration/call stats for a kernel. On compute traces, can also "
            "fetch named hardware metrics. On system traces, returns summary stats.",
            metrics_params);

    jt::Json tracks_params = ObjectParams();
    AddTool(tools, 4, "list_tracks",
            "List timeline tracks with ids, names, and event families. Use before goto "
            "if you need a track_id.",
            tracks_params);

    jt::Json goto_params = ObjectParams();
    AddParam(goto_params, "start_ns", "number", "Range start in nanoseconds.");
    AddParam(goto_params, "end_ns", "number", "Range end in nanoseconds.");
    AddParam(goto_params, "track_id", "integer",
             "__trackId of the event. Pass it whenever you pass event_uuid; without "
             "it the flow arrows cannot be drawn.");
    AddParam(goto_params, "event_uuid", "integer",
             "__uuid of the event to select. Selecting it expands its flow arrows "
             "and call stack, the same as clicking it.");
    AddParam(goto_params, "kernel_name", "string",
             "On compute traces, select this kernel in the UI.");
    AddParam(goto_params, "kernel_id", "integer",
             "On compute traces, select this kernel id instead of the name.");

    jt::Json goto_event_item = ObjectParams();
    AddParam(goto_event_item, "track_id", "integer", "Track the event lives on.");
    AddParam(goto_event_item, "event_uuid", "integer", "__uuid of the event.");
    goto_event_item["required"][0] = "event_uuid";
    AddParam(goto_params, "events", "array",
             "Events to light up on the timeline, from the __trackId and __uuid "
             "columns. They stay highlighted while the user reads, and the first "
             "one gets selected so its flow arrows and call stack expand.");
    goto_params["properties"]["events"]["items"] = goto_event_item;
    AddParam(goto_params, "zoom", "boolean",
             "Also zoom the timeline to the range, not just select it. Use this "
             "when the range is small enough that the user would have to zoom in "
             "by hand.");

    AddTool(tools, 5, "goto",
            "Point the UI at what you are talking about: move the timeline to a "
            "range, highlight the specific events behind your claim, and expand "
            "the first one's connecting arrows. Call this on your own whenever you "
            "name a window or an outlier, so the user sees exactly what you saw. "
            "A range on its own only scrolls the view - pass event_uuid and "
            "track_id, or nothing gets highlighted and no arrows appear. Do not "
            "use it to pin notes or change other UI.",
            goto_params);

    jt::Json track_events_params = ObjectParams();
    AddScopeParams(track_events_params);
    AddQueryParams(track_events_params);
    AddTool(tools, 6, "track_events",
            "Read raw events off specific timeline tracks. This is the closest thing "
            "to querying the database: pick tracks with track_ids, a window with "
            "start_ns/end_ns, narrow rows with filters, then sort and page. Returns "
            "the same rows as the Event Table tab.",
            track_events_params);

    jt::Json track_samples_params = ObjectParams();
    AddScopeParams(track_samples_params);
    AddQueryParams(track_samples_params);
    AddTool(tools, 7, "track_samples",
            "Read counter/PMC samples off specific counter tracks over a window. "
            "Use track_ids from list_tracks where type=Counter. Same filtering, "
            "sorting, and paging as track_events.",
            track_samples_params);

    jt::Json event_details_params = ObjectParams();
    AddParam(event_details_params, "event_uuid", "integer",
             "Event uuid from the __uuid column of track_events, top_events, or "
             "kernel_instances.");
    AddParam(event_details_params, "track_id", "integer",
             "Optional owning track id, which fills in the event name and duration.");
    event_details_params["required"][0] = "event_uuid";
    AddTool(tools, 8, "event_details",
            "Drill into one event: its arguments, extended data, flow links to and "
            "from other events, and its call stack. Use after track_events or "
            "top_events gives you a __uuid.",
            event_details_params);

    jt::Json track_stats_params = ObjectParams();
    AddParam(track_stats_params, "track_id", "integer",
             "Queue track (returns utilization) or counter track (returns "
             "min/max/mean/stddev). From list_tracks.");
    AddParam(track_stats_params, "start_ns", "number",
             "Window start in nanoseconds. Defaults to the selection or whole trace.");
    AddParam(track_stats_params, "end_ns", "number", "Window end in nanoseconds.");
    track_stats_params["required"][0] = "track_id";
    AddTool(tools, 9, "track_statistics",
            "Compute statistics for one track over a window: queue busy percentage "
            "for a queue track, or min/max/mean/standard deviation for a counter "
            "track. Answers \"how busy was this GPU\" and \"how hot did it get\".",
            track_stats_params);

    jt::Json search_params = ObjectParams();
    AddParam(search_params, "terms", "array",
             "Names to search for, e.g. [\"gemm\", \"memcpy\"].");
    search_params["properties"]["terms"]["items"]["type"] = "string";
    AddParam(search_params, "match", "string",
             "contains (default, substring) or equals (exact name).");
    search_params["properties"]["match"]["enum"] =
        MakeStringEnum({ "contains", "equals" });
    AddParam(search_params, "combine", "string",
             "all (default, an event must match every term) or any.");
    search_params["properties"]["combine"]["enum"] = MakeStringEnum({ "all", "any" });
    AddParam(search_params, "include_category", "boolean",
             "Also match against event category names. Default false.");
    AddParam(search_params, "categories", "array",
             "Limit to these event families. Defaults to all of them.");
    search_params["properties"]["categories"]["items"]["type"] = "string";
    search_params["properties"]["categories"]["items"]["enum"] =
        MakeStringEnum({ "dispatch", "memory_copy", "memory_alloc", "instrumented",
                         "sampled" });
    AddParam(search_params, "start_ns", "number",
             "Window start in nanoseconds. Defaults to the whole trace.");
    AddParam(search_params, "end_ns", "number", "Window end in nanoseconds.");
    AddQueryParams(search_params);
    search_params["required"][0] = "terms";
    AddTool(tools, 10, "search_events",
            "Search the whole trace by event name, the same as the search box in "
            "the toolbar. Use this when you know part of a name but not which track "
            "or when it ran. Returns matching events with track ids, timestamps, and "
            "__uuid values you can pass to goto or event_details.",
            search_params);

    jt::Json overview_params = ObjectParams();
    AddParam(overview_params, "track_id", "integer",
             "Optional: profile one track instead of the whole trace.");
    AddParam(overview_params, "bins", "integer",
             "Time buckets to report (default 32).");
    AddTool(tools, 11, "trace_overview",
            "Preliminary analysis. Reads the timeline histogram and minimap that "
            "Optiq already built: where activity is concentrated in time, the "
            "busiest and quietest windows as real nanosecond ranges, total event "
            "count, and which tracks own the most events. Costs no database query. "
            "Call this FIRST so later tools can target a window and a track instead "
            "of scanning the whole trace.",
            overview_params);

    jt::Json panel_params = ObjectParams();
    AddParam(panel_params, "panel", "string",
             ("Which panel: " + OptiqActions::PanelNameList()).c_str());
    AddParam(panel_params, "visible", "boolean",
             "true to open, false to close. Defaults to true.");
    panel_params["required"][0] = "panel";
    AddTool(tools, 12, "show_panel",
            "Open or close one of Optiq's panels, the same as the View menu. Only "
            "call this when the user asked you to show or hide that panel.",
            panel_params);

    jt::Json tab_params = ObjectParams();
    AddParam(tab_params, "name", "string",
             "Tab to switch to. Part of the name is enough. Omit to list every "
             "tab that is available.");
    AddTool(tools, 13, "switch_tab",
            "Switch tabs among open traces or the details panel. Only call this "
            "when the user asked you to change tabs. Call with no name to list "
            "what is available.",
            tab_params);

    jt::Json flow_params = ObjectParams();
    AddParam(flow_params, "visible", "boolean",
             "Show or hide the flow arrows. Defaults to true.");
    AddParam(flow_params, "style", "string",
             "fan draws every link from the selected event; chain follows the "
             "sequence end to end.");
    flow_params["properties"]["style"]["enum"] = MakeStringEnum({ "fan", "chain" });
    AddTool(tools, 14, "flow_arrows",
            "Show, hide, or restyle the arrows that connect an event to the events "
            "it launched or waited on. Selecting an event with goto expands that "
            "event's arrows, but only while arrows are switched on, so pair this "
            "with goto using visible=true whenever you select an event - the user "
            "may have turned them off. Wait to be asked only before restyling them "
            "or switching them off.",
            flow_params);

    jt::Json note_params = ObjectParams();
    AddParam(note_params, "time_ns", "number",
             "Absolute timestamp to pin the note at, in nanoseconds. Take it "
             "from data you read this turn, not from an earlier window.");
    AddParam(note_params, "title", "string", "Short heading, a few words.");
    AddParam(note_params, "text", "string",
             "What you found here and why it matters.");
    AddParam(note_params, "track_id", "integer",
             "Optional track to attach the note to.");
    note_params["required"][0] = "time_ns";
    note_params["required"][1] = "title";
    note_params["required"][2] = "text";
    AddTool(tools, 15, "annotate",
            "Pin a sticky note on the timeline. Notes are saved with the project. "
            "Only call this when the user asked you to leave a note.\n"
            "The note goes at an absolute timestamp, and the user can pan, zoom "
            "or change the selection between your turns - so a number you "
            "worked out earlier may no longer be where they are looking. The "
            "reply tells you where the note actually landed and whether that is "
            "outside the current selection; if it is, either say so plainly or "
            "call goto for that range and annotate again. Do not describe a "
            "note as marking the selected interval unless the reply confirms "
            "it does.",
            note_params);

    jt::Json bookmark_params = ObjectParams();
    AddParam(bookmark_params, "action", "string",
             "save the current view, go to a saved one, remove one, or list them.");
    bookmark_params["properties"]["action"]["enum"] =
        MakeStringEnum({ "save", "goto", "remove", "list" });
    AddParam(bookmark_params, "slot", "integer", "Bookmark slot, 0 to 9.");
    AddTool(tools, 16, "bookmark",
            "Save, jump to, remove, or list numbered zoom bookmarks. Only call "
            "this when the user asked you to work with bookmarks.",
            bookmark_params);

    jt::Json measure_params = ObjectParams();
    AddParam(measure_params, "start_ns", "number", "Span start in nanoseconds.");
    AddParam(measure_params, "end_ns", "number", "Span end in nanoseconds.");
    AddParam(measure_params, "clear", "boolean",
             "Set true to clear the measurement instead of taking one.");
    AddTool(tools, 17, "measure",
            "Drop the two measurement pins on a span so the toolbar shows its "
            "duration. Only call this when the user asked you to measure a span.",
            measure_params);

    jt::Json reset_params = ObjectParams();
    AddTool(tools, 18, "reset_view",
            "Zoom the timeline back out to the whole trace. Only call this when "
            "the user asked you to reset the view.",
            reset_params);

    jt::Json next_params = ObjectParams();
    AddParam(next_params, "steps", "array",
             "Two or three short follow-ups the user can click, most useful first. "
             "Each is a complete thing they would type, under 80 characters.");
    next_params["properties"]["steps"]["items"]["type"] = "string";
    next_params["required"][0] = "steps";
    AddTool(tools, 19, "offer_next_steps",
            "Puts stacked buttons under the chat for what to look at next. Call it "
            "as the last tool of an investigation, then write your answer in the "
            "response after it. Do not list those same options in the prose.\n"
            "The best follow-ups are the questions your answer raised but could "
            "not settle - the distribution behind a mean, whether a cadence is "
            "regular, how much two tracks really overlap, where a total actually "
            "goes. Offer those even when answering them would need a script: the "
            "user clicking one is exactly what makes writing it worthwhile, and a "
            "question you can already answer in one call is a weak thing to "
            "offer.",
            next_params);

#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    // This description is the only account the model gets of what a script may
    // use. Anything left out of it gets invented, so it names the whole surface
    // rather than summarizing it.
    jt::Json script_params = ObjectParams();
    AddParam(script_params, "script", "string",
             "Python source, shown to the user before it runs. Report findings "
             "with optiq.result.text(...); nothing else is returned.");
    script_params["required"][0] = "script";
    AddTool(tools, 20, "run_analysis_script",
            "Offer a Python script that analyses this trace. It does not run on "
            "its own: the editor opens with your source and the user presses Run "
            "or Reject, so write it to be read as well as executed - clear names, "
            "and a comment where the intent is not obvious. You get back what it "
            "computed, or that they declined.\n"
            "Use it when the answer needs arithmetic across many rows - gaps "
            "between events, totals, percentiles, overlap between two tracks, "
            "per-name rollups, call-depth analysis - because one script costs a "
            "fraction of paging the same rows back through the other tools. For a "
            "single lookup the other tools are cheaper.\n"
            "Available inside the script, and nothing else: optiq.selection.tracks, "
            "optiq.selection.start, optiq.selection.end, optiq.trace.tracks, and "
            "optiq.result.text(str), which is the only way to report anything. A "
            "track has id, type, name, sub_name, min_time, max_time, num_entries, "
            "and events(start=None, end=None). An event has id, start, end, level "
            "(nesting depth, 0 on samples), name, category, and value (counter "
            "reading, None on interval events). optiq.table() opens a private query "
            "table whose only method is fetch, returning a list of dicts keyed by "
            "column name. Constants: optiq.TRACK_TYPE_EVENTS, "
            "optiq.TRACK_TYPE_SAMPLES, optiq.SORT_ASCENDING, "
            "optiq.SORT_DESCENDING.\n"
            "fetch takes twelve keyword arguments and nothing else. In order: "
            "tracks (list of Track), start (float), end (float), where (str), "
            "filter (str), group (str), group_columns (str), sort_column (int - a "
            "zero-based column index, NOT a column name), sort_order (int - use "
            "optiq.SORT_ASCENDING or optiq.SORT_DESCENDING), start_index (int), "
            "count (int), type ('events' or 'samples'). Always call it with "
            "keyword arguments: passing positionally shifts every later argument "
            "because filter and group_columns sit in the middle, and an int "
            "parameter that receives a string fails with 'an integer is "
            "required'. track.events takes only start and end.\n"
            "Exact types, because guessing these is what fails first. tracks= "
            "takes Track objects straight from optiq.trace.tracks or "
            "optiq.selection.tracks, never their ids - a list of ints raises "
            "TypeError. where=, group= and sort_column= are single strings or "
            "None, never lists or dicts. fetch also drops any track whose type "
            "does not match type=, so passing sample tracks with type='events' "
            "ends in 'fetch requires at least one matching track'. Row values are "
            "typed per column and a column you expect to be a number is often "
            "str, so call float(...) or int(...) before any arithmetic rather "
            "than adding a cell directly. track.name, track.sub_name, event.name "
            "and event.category are free-form strings that came out of this "
            "trace, and none of them is guaranteed to equal the label shown in "
            "the UI or the name list_tracks gave you. Never write a script whose "
            "first act is an equality test against a name you assumed: print the "
            "handful of names you can see, match loosely, and guard the result, "
            "because [t for t in optiq.trace.tracks if t.name == 'Queue 1'][0] is "
            "an IndexError the moment that guess is wrong, and an empty match "
            "looks exactly like a track with no events.\n"
            "One counting trap. The same GPU work is usually visible on more "
            "than one track, so walking every track and concatenating events "
            "counts each dispatch several times - a total that comes out a clean "
            "multiple of the expected one is this, not a discovery. Work on a "
            "single track, or deduplicate on event id, and check the total "
            "against get_summary or top_events before you report it: if a script "
            "disagrees with the aggregated tools, the script is wrong until you "
            "have shown otherwise.\n"
            "The Python is ordinary and most of it works: def, class, "
            "dataclasses, comprehensions, generators, lambda, f-strings, "
            "try/except, and print(...), which writes a line to the result just "
            "like optiq.result.text. math, statistics, json, re, itertools, "
            "functools, operator, collections, heapq, decimal, fractions, "
            "dataclasses, typing, enum, datetime, textwrap and string are "
            "already imported - use them without an import line.\n"
            "What is not there: open, os, sys, subprocess, pathlib, numpy, "
            "pandas, and any other third-party package; also eval, exec, "
            "getattr, globals, input and compile. Importing anything outside the "
            "list above raises ImportError. There is no file, network, or shell "
            "access of any kind, so do not write a script that saves, loads, or "
            "downloads - report the numbers instead.\n"
            "A script is stopped after 30 seconds, so narrow the window and "
            "compute rather than walking every event in the trace. If it raises, "
            "the traceback comes back to you naming the line: fix that line and "
            "offer it again.",
            script_params);
#endif

    return tools;
}

// The line the panel shows under the transcript while a tool runs.
std::string
AssistantToolStatusLabel(const std::string& tool_name)
{
    for(const AssistantToolLabel& label : ASSISTANT_TOOL_LABELS)
    {
        if(tool_name == label.name)
        {
            return label.status;
        }
    }
    return "Using " + tool_name + "...";
}

}  // namespace View
}  // namespace RocProfVis
