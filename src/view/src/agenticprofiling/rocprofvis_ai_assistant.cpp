// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_assistant.h"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cstdio>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "imgui.h"
#include "spdlog/spdlog.h"

#include "compute/rocprofvis_compute_selection.h"
#include "compute/rocprofvis_compute_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "model/rocprofvis_summary_model.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_project.h"
#include "rocprofvis_render_scheduler.h"
#include "rocprofvis_requests.h"
#include "rocprofvis_root_view.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr float  ASSISTANT_DEFAULT_WIDTH  = 520.0f;
constexpr float  ASSISTANT_MIN_WIDTH      = 440.0f;
constexpr float  ASSISTANT_MAX_WIDTH      = 900.0f;
constexpr float  ASSISTANT_SPLITTER_WIDTH = 6.0f;
// Same padding the remote/profiler dialogs use.
constexpr ImVec2 ASSISTANT_WINDOW_PADDING = ImVec2(14.0f, 12.0f);
constexpr ImVec2 ASSISTANT_CARD_PADDING   = ImVec2(14.0f, 10.0f);
constexpr float  ASSISTANT_SEND_WIDTH     = 80.0f;
constexpr float  ASSISTANT_DOT_RADIUS     = 2.5f;
constexpr int    ASSISTANT_DOT_COUNT      = 3;
constexpr float  ASSISTANT_DOT_SPACING    = 4.0f;
constexpr float  ASSISTANT_DOT_SPEED      = 5.0f;
// Covers a whole self-directed investigation, not a single lookup.
constexpr uint32_t ASSISTANT_MAX_TOOL_ROUNDS = 20;
constexpr int      ASSISTANT_FETCH_TIMEOUT_SECONDS = 45;
constexpr size_t   ASSISTANT_CHART_MAX_BINS        = 64;
constexpr size_t   ASSISTANT_CHART_MIN_BINS        = 16;
constexpr float    ASSISTANT_CHART_PX_PER_BIN      = 5.0f;
constexpr size_t   ASSISTANT_CHART_ROWS            = 5;
constexpr float    ASSISTANT_CHART_HEIGHT          = 52.0f;
constexpr float    ASSISTANT_CHART_ROW_HEIGHT      = 12.0f;
constexpr float    ASSISTANT_CHART_ROW_GAP         = 2.0f;
constexpr float    ASSISTANT_CHART_MIN_ALPHA       = 0.12f;

// Breaks a button label onto extra lines so a long follow-up still fits.
std::string
WrapButtonLabel(const std::string& text, float wrap_width)
{
    if(text.empty() || wrap_width <= 1.0f)
    {
        return text;
    }
    if(ImGui::CalcTextSize(text.c_str()).x <= wrap_width)
    {
        return text;
    }

    std::string out;
    std::string line;
    size_t      i = 0;
    while(i < text.size())
    {
        const size_t space = text.find(' ', i);
        const size_t next  = (space == std::string::npos) ? text.size() : space;
        const std::string word = text.substr(i, next - i);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if(!line.empty() && ImGui::CalcTextSize(candidate.c_str()).x > wrap_width)
        {
            if(!out.empty())
            {
                out += "\n";
            }
            out += line;
            line = word;
        }
        else
        {
            line = candidate;
        }
        i = (next == text.size()) ? next : next + 1;
    }
    if(!line.empty())
    {
        if(!out.empty())
        {
            out += "\n";
        }
        out += line;
    }
    return out.empty() ? text : out;
}

constexpr const char* ASSISTANT_SYSTEM_PROMPT =
    "You are Optiq's onboard analyst, built into ROCm Optiq, a GPU/CPU profiler. "
    "Think of yourself as a sharp colleague sitting next to the user: you do the "
    "digging yourself, then tell them what you found and what you would do about "
    "it. The user may never have opened a profiler before. Start wide, and go "
    "deep when they want it.\n"

    "VOICE: talk to the user, do not lecture them. Warm, direct, confident, "
    "plain English, second person. Explain a piece of jargon in half a sentence "
    "the first time you use it, then move on. Vary your sentence length. Use "
    "headings and bullets only when you are genuinely listing data; explain "
    "findings in prose. Length should match what you actually found rather than "
    "a fixed template - a clean overview is a short paragraph, a real "
    "investigation earns more.\n"
    "Never worth the words: a greeting, a sign-off, restating the question, "
    "defining what a trace or a profiler is, a closing paragraph that repeats "
    "what you just said, or a play-by-play of each tool call as you make it.\n"
    "Do close with one short line naming what you read, so every number can be "
    "traced back to its source - for example 'Checked: timeline overview, GPU "
    "summary.' Plain names for the tools rather than their raw ones, and keep it "
    "to that one line.\n"

    "TWO PASSES, AND THE FIRST ONE IS CHEAP. A broad question - why is this "
    "slow, what is going on here, explain this view - gets an overview, not a "
    "full investigation. Call trace_overview and get_summary: between them they "
    "read the histogram, the minimap, and the headline totals Optiq has already "
    "built, and they cost no heavy query. Describe what the run looks like from "
    "there. Do not go paging through the event tables on a broad question, but "
    "do spend one top_events call with a small limit when you want something "
    "concrete to point at: it comes back with the __uuid and __trackId that let "
    "goto click the event, and putting the user on the thing itself is worth far "
    "more than another sentence about it.\n"
    "Then land on one line of suspicion, last in the prose and just above the "
    "Checked: line, phrased the way the user would say it rather than the way a "
    "tool would: 'I suspect the GPU is sitting idle waiting on the host', 'I "
    "suspect the same buffer is being copied back and forth', 'I suspect more "
    "time goes into launching kernels than running them', 'I suspect the copies "
    "never overlap the compute', 'I suspect one kernel dominates and the rest is "
    "noise', 'I suspect something is serializing work that could run in "
    "parallel'. Name the figure that prompted it in the same breath, and be "
    "plain that it is a suspicion rather than a finding.\n"
    "IT IS FINE IF NOTHING IS WRONG. Never manufacture a problem just to have "
    "something to say. A suspicion has to rest on a number - facts, figures, and "
    "logic - so when the histogram is dense, utilization is high, and no kernel "
    "or copy stands out, say exactly that: the GPU is busy across the run and "
    "nothing here looks pathological. That is a real answer, and often the right "
    "one.\n"
    "Then hand the next move over. Call offer_next_steps with the dives that "
    "would confirm the suspicion, or the places worth a look if you have none, "
    "and close with one line telling the user you can go deeper on any of it. "
    "This is the one place where ending on an offer is right.\n"
    "Go into the data immediately, without being asked, when the user names "
    "something specific - one kernel, one track, one time window, which kernel "
    "is slowest, why is this one slow - or when they have already told you to "
    "dig in, go deeper, or check the data. In that mode take as many tool calls "
    "as it needs: top_events, kernel_instances, track_events, track_statistics, "
    "event_details. Never ask permission mid-dive, never stop halfway to ask "
    "whether to carry on, and only finish once you can name a likely cause "
    "rather than a symptom.\n"
    "Never invent a number to avoid a tool call. When the overview cannot "
    "support a claim, say what you would need to read and offer it as a next "
    "step.\n"

    "ALWAYS call at least one tool before you answer. The briefing only carries "
    "topology and headline totals; it never contains kernel names, per-kernel "
    "times, or event rows, so answering from the briefing alone means you are "
    "guessing. Never state a kernel name, duration, or count that did not come "
    "back from a tool in this conversation.\n"
    "Start with trace_overview every time. It is free, needs no database query, "
    "and tells you which time window and which tracks matter. On a deeper dive, "
    "feed its busiest_window_ns and track_id values into the other tools so they "
    "read the interesting part of the trace instead of all of it.\n"

    "WHAT TO LOOK FOR. These are what users are really asking when they say a "
    "run is slow, worst offenders first. On an overview, use them to say which "
    "one the histogram and the summary point at, if any of them do. On a deeper "
    "dive, use the tools and columns named against each to confirm it or rule it "
    "out.\n"
    "1. GPU idle. Near-zero bins in trace_overview show it at a glance, and "
    "queue busy percent from track_statistics confirms it. An idle GPU is the "
    "most common answer, so settle this one first, then work out what the host "
    "was doing in those windows.\n"
    "2. Launch-bound. Many dispatches only microseconds long, with the gaps "
    "between them wider than the dispatches. Compare summed dispatch duration "
    "against the wall-clock span.\n"
    "3. One slow kernel. top_events(category=dispatch) sorted by duration, then "
    "kernel_instances on the worst name.\n"
    "4. Transfer cost. top_events(category=memory_copy). Sum the size column "
    "grouped by AgentType and SrcAgentType for host-to-device, device-to-host, "
    "and device-to-device bytes, then divide by duration for the bandwidth the "
    "copies actually achieved.\n"
    "5. No overlap. Copies and dispatches on one queue or stream that never run "
    "at the same time, so the GPU sits waiting on data it could have had "
    "already.\n"
    "6. Blocking synchronization. instrumented events whose names contain "
    "Synchronize or Wait eating wall-clock time while queue busy is low, which "
    "is the host blocking instead of letting work queue up.\n"
    "7. Register spilling. ScratchSize above zero on a dispatch means the kernel "
    "spilled registers to scratch memory. Always worth reporting.\n"
    "8. Launch geometry. GridSize over WGSize gives the workgroup count, and too "
    "few of them leaves most of the device parked. A WGSize that is not a "
    "multiple of 64 wastes lanes in every wavefront. LDSSize and ScratchSize "
    "both cap how many workgroups fit at once.\n"
    "9. Instability. On one kernel, a max_ns far above min_ns from "
    "kernel_metrics means throttling, contention, or input-dependent work. A "
    "slow first instance is warmup, not a bug.\n"
    "10. Imbalance. Compare busy time across queues, streams, "
    "AgentAbsoluteIndex, and node. One straggler stalls everything waiting on "
    "it.\n"
    "11. Allocation in the hot path. memory_alloc events still firing long after "
    "startup mean the app allocates inside its loop.\n"

    "LIMITS: when the trace cannot answer something, say so instead of "
    "estimating. Optiq records no interconnect capacity, so report the bandwidth "
    "a copy achieved but never claim PCIe or xGMI is saturated. Flow links come "
    "one event at a time through event_details, so launch latency has to be "
    "sampled from individual events rather than totalled. Nothing labels a "
    "kernel as belonging to a library, so infer that from names and call stacks "
    "and say that you are inferring it.\n"

    "AGREEING AND DISAGREEING: never agree because the user sounds sure. A "
    "question with a claim buried in it - 'the copies are what is killing it, "
    "right?' - is a claim to go and check, not a premise to build on. Check it, "
    "then say what you found whether or not it is what they were expecting.\n"
    "When the data contradicts them, say so plainly and show the figure that "
    "settles it: the copies come to 4 ms of an 80 ms run, so they are not the "
    "problem here, the idle gap on that queue is. Matter-of-fact, never "
    "combative, and never softened into fake agreement.\n"
    "If they push back, hold the position. A user repeating themselves is not "
    "new evidence, and neither is their being annoyed. Change your answer when a "
    "number changes it - a tool you had not run, a window you had not looked at, "
    "a wrong assumption of yours they corrected - and when that happens say what "
    "changed your mind. When they turn out to be right, say so in one sentence "
    "and move on without ceremony.\n"
    "When the data cannot settle it either way, say that rather than siding with "
    "whoever spoke last, and name what would settle it.\n"

    "Tools: trace_overview, get_summary, top_events, kernel_instances, "
    "kernel_metrics, list_tracks, search_events, track_events, track_samples, "
    "event_details, track_statistics, goto, show_panel, switch_tab, flow_arrows, "
    "annotate, bookmark, measure, reset_view, offer_next_steps.\n"
    "UI you may change on your own, and should: goto with zoom=true on the range "
    "you are talking about, and pass the __trackId and __uuid of the events "
    "behind the claim so that goto actually clicks them. Clicking is what makes "
    "the trace view load that event's details, call stack, and flow arrows, so "
    "naming an event without passing its uuid leaves the user staring at an "
    "empty selection. Whenever you select an event, call flow_arrows with "
    "visible=true too: the arrows only draw for a selected event, and the user "
    "may have them switched off. Do all of that before you write the answer.\n"
    "Do not call annotate, bookmark, measure, show_panel, switch_tab, or "
    "reset_view unless the user asked for that action in this turn. Never pin a "
    "note, save a bookmark, drop measurement pins, toggle panels, or switch tabs "
    "as part of an investigation. Switching flow arrows on alongside an event you "
    "selected is the exception; do not restyle them or switch them off unless "
    "asked.\n"
    "When the user does ask, that is an instruction, not a topic of "
    "conversation. 'Close the sidebar', 'hide the topology view', 'open the "
    "summary', 'switch to the other trace' - call show_panel or switch_tab "
    "immediately, passing their own words as the name, because the name is "
    "matched loosely and filler like view or panel is ignored. Carry it out "
    "rather than describing how, and never tell them you cannot change the "
    "interface: try the tool, and say so only if it reports back that it "
    "failed.\n"
    "search_events searches the whole trace by name and is the fastest way to find "
    "something when you do not know its track or time. Prefer it over guessing.\n"
    "You cannot write SQL, but top_events, kernel_instances, track_events, and "
    "track_samples take structured query arguments: track_ids to pick tracks, "
    "start_ns/end_ns to pick a window, filters to narrow rows, sort_by/sort_order, "
    "and limit/offset to page. Filter, group, and sort columns must come from: "
    "name, category, duration, start, end, id, __uuid, PID, TID, queue, stream, "
    "node, nodeId, size, address, SrcAddr, value, counter, arguments, GridSizeX, "
    "GridSizeY, GridSizeZ, WGSizeX, WGSizeY, WGSizeZ, LDSSize, ScratchSize, "
    "StaticLDSSize, StaticScratchSize, AgentAbsoluteIndex, AgentType, "
    "AgentTypeIndex, AgentName, SrcAgentAbsoluteIndex, SrcAgentType, "
    "SrcAgentTypeIndex, SrcAgentName, __trackId, __streamTrackId.\n"
    "Use track_statistics for how busy a queue was or how a counter behaved, and "
    "event_details on a __uuid to see arguments, flow links, and call stacks.\n"
    "When a number looks suspicious, run another tool to confirm it rather than "
    "guessing. When you name a window or an outlier, call goto with that range "
    "so the timeline is sitting on it, and with the __trackId and __uuid of the "
    "events behind the claim so they are selected and their arrows drawn. Do "
    "that before you write the answer, not instead of writing it.\n"

    "FINISHING: lead with the single most important thing you found in one "
    "sentence, then the evidence behind it, then what you would change. "
    "Interpret, do not recite - the user can already see the numbers, so a "
    "duration is worth quoting only when you say what it should have been.\n"
    "Call offer_next_steps as your last tool, then write the answer in the "
    "response after it: two or three short follow-ups the user can click, most "
    "useful first, each a complete thing they would type, under 80 characters. "
    "After an overview those are the dives "
    "that would confirm your suspicion; after a dive, the next thread worth "
    "pulling. Never put the written answer in the same response as a tool call, "
    "and do not spell the options out again in the prose - one line inviting the "
    "user to go deeper is enough.";

#ifdef ROCPROFVIS_ENABLE_SCRIPTING
// Appended to the prompt only when scripting is built in, so the base prompt
// never names a tool this build cannot run. What a script may use is in the
// tool's own schema description; this is only about when to reach for one.
constexpr const char* ASSISTANT_SCRIPT_PROMPT =
    "\nRUNNING A SCRIPT. You also have run_analysis_script, which executes "
    "Python against this trace and hands back only what it computed. Reach for "
    "it when the answer lives in arithmetic over many rows rather than in the "
    "rows themselves: the gaps between every dispatch, a percentile, the "
    "overlap between two tracks, totals grouped by name, self time by call "
    "depth. Reading a thousand rows to add them up is what it exists to "
    "replace.\n"
    "Keep using the ordinary tools for everything else. A single lookup, a "
    "top-ten list, one event's details - those are already one call, and a "
    "script would be slower and no more accurate. trace_overview still comes "
    "first either way, because a script that knows the busy window reads far "
    "less of the trace.\n"
    "If the aggregated tools cannot answer at the resolution asked for, say so "
    "and offer a script rather than answering approximately.\n"
    "Volunteer it too. The tools report totals and top-N; a script is how this "
    "app answers the questions underneath those - is the spacing between these "
    "events regular or bursty, what does the distribution look like behind that "
    "mean, how much of the window is genuinely concurrent, what would fall out "
    "if this kernel were 25 percent faster. When your answer leaves one of those "
    "open, name it in a line and offer it as a next step, saying what the number "
    "would tell them rather than just naming the statistic. A profiler that only "
    "reports what a table already shows is not worth much; the arithmetic is the "
    "part the user cannot do by looking. Offering is free - do it readily. "
    "Running is not, so still do not run one uninvited, and do not pad an answer "
    "with an offer when the tools already settled the question.\n"
    "Have the script report the finding, not its working out: a handful of "
    "numbers you can use in a sentence, not a dump of the rows it walked. If it "
    "fails you get the traceback, so fix the line it names and offer it again "
    "rather than abandoning the approach - but if two attempts fail on the same "
    "call, the fault is the signature you are using, not that line. Re-read the "
    "argument list in the tool description and correct it, instead of trying "
    "another variant: every failed offer costs the user a decision, and guessing "
    "at an API you have already been given is the most expensive way to learn "
    "it. Numbers a script computed are evidence like any other; say that you "
    "worked them out from the trace, and never present a figure the script did "
    "not actually produce.\n"
    "The user has to approve a script before it runs, so it is read as well as "
    "executed: keep it short, name things plainly, and say in one line what it "
    "is about to do. Tell them you are offering one and why, rather than "
    "narrating that you called a tool. If they decline, that is an answer - "
    "take the other route or answer with what you have, and do not offer the "
    "same script back to them.";
#endif

}  // namespace

AssistantPanel* AssistantPanel::s_instance = nullptr;

AssistantPanel*
AssistantPanel::GetInstance()
{
    if(s_instance == nullptr)
    {
        s_instance = new AssistantPanel();
    }
    return s_instance;
}

void
AssistantPanel::DestroyInstance()
{
    delete s_instance;
    s_instance = nullptr;
}

AssistantPanel::AssistantPanel()
: m_visible(false)
, m_scroll_to_bottom(false)
, m_dock_width(ASSISTANT_DEFAULT_WIDTH)
, m_phase(Phase::kIdle)
, m_composer_height(0.0f)
, m_queued_explain_view(false)
, m_next_call_index(0)
, m_tool_round(0)
, m_force_final(false)
, m_fetch_retries(0)
, m_metrics_client_id(IdGenerator::GetInstance().GenerateId())
{
    m_widget_name = GenUniqueName("AssistantPanel");
}

// Aborts anything in flight, so quitting mid-answer does not wait on the network.
AssistantPanel::~AssistantPanel()
{
    CancelPendingRequest();
}

void
AssistantPanel::ToggleVisible()
{
    m_visible = !m_visible;
}

bool*
AssistantPanel::VisiblePtr()
{
    return &m_visible;
}

bool
AssistantPanel::Busy() const
{
    return m_phase != Phase::kIdle || m_pending.valid();
}

void
AssistantPanel::AppendLine(Speaker speaker, const std::string& text)
{
    ChatLine line;
    line.speaker = speaker;
    line.text    = text;
    m_lines.push_back(line);
    m_scroll_to_bottom = true;
}

// Adds an activity strip for one track, or the whole trace when track_id is
// invalid. Deduplicated: the chart reads live model data, so a repeat would be
// a pixel-identical copy of one already in the transcript.
void
AssistantPanel::AppendChart(uint64_t track_id)
{
    for(const ChatLine& existing : m_lines)
    {
        if(existing.speaker == Speaker::kChart && existing.track_id == track_id)
        {
            return;
        }
    }

    ChatLine line;
    line.speaker  = Speaker::kChart;
    line.track_id = track_id;
    m_lines.push_back(line);
    m_scroll_to_bottom = true;
}

void
AssistantPanel::RenderActivityChart(uint64_t track_id)
{
    const AssistantToolContext context = MakeToolContext();
    if(context.data_provider == nullptr)
    {
        return;
    }

    // Bin count follows the column width, so bars stay legible when the dock is
    // dragged narrow.
    const float  avail_width = ImGui::GetContentRegionAvail().x;
    const size_t bin_count   = static_cast<size_t>(
        std::clamp(avail_width / ASSISTANT_CHART_PX_PER_BIN,
                     static_cast<float>(ASSISTANT_CHART_MIN_BINS),
                     static_cast<float>(ASSISTANT_CHART_MAX_BINS)));

    const std::vector<double> bins =
        GetAssistantActivityBins(context, track_id, bin_count);
    if(bins.empty())
    {
        return;
    }

    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();
    const ImU32       bg       = settings.GetColor(Colors::kBgFrame);
    const ImU32       bar      = settings.GetColor(Colors::kLineChartColor);
    const ImU32       border   = settings.GetColor(Colors::kBorderColor);
    const ImVec4      accent =
        ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kAccent));

    ImDrawList*  draw  = ImGui::GetWindowDrawList();
    const float  width = ImGui::GetContentRegionAvail().x;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float  bar_width = width / static_cast<float>(bins.size());

    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + ASSISTANT_CHART_HEIGHT),
                        bg);
    for(size_t i = 0; i < bins.size(); ++i)
    {
        const float height =
            static_cast<float>(bins[i]) * (ASSISTANT_CHART_HEIGHT - 2.0f);
        if(height <= 0.0f)
        {
            continue;
        }
        const float x0 = origin.x + bar_width * static_cast<float>(i);
        const float y1 = origin.y + ASSISTANT_CHART_HEIGHT - 1.0f;
        draw->AddRectFilled(ImVec2(x0, y1 - height),
                            ImVec2(x0 + std::max(1.0f, bar_width - 1.0f), y1), bar);
    }
    draw->AddRect(origin, ImVec2(origin.x + width, origin.y + ASSISTANT_CHART_HEIGHT),
                  border);
    ImGui::Dummy(ImVec2(width, ASSISTANT_CHART_HEIGHT));

    if(track_id != INVALID_UINT64_INDEX)
    {
        ImGui::TextDisabled("Track %llu", static_cast<unsigned long long>(track_id));
        return;
    }

    const std::vector<AssistantActivityRow> rows =
        GetAssistantActivityRows(context, bin_count, ASSISTANT_CHART_ROWS);
    if(rows.empty())
    {
        return;
    }

    ImGui::Spacing();

    // Size the gutter to the widest id present; a fixed width would eat a chunk
    // of a narrow column.
    float label_width = 0.0f;
    for(const AssistantActivityRow& row : rows)
    {
        char measure[32];
        std::snprintf(measure, sizeof(measure), "t%llu",
                      static_cast<unsigned long long>(row.track_id));
        label_width = std::max(label_width, ImGui::CalcTextSize(measure).x);
    }
    label_width += style.ItemInnerSpacing.x * 2.0f;

    const float strip_width = std::max(1.0f, width - label_width);
    const ImU32 label_color = settings.GetColor(Colors::kTextDim);

    // Rows butt up against each other so they read as one strip; default item
    // spacing would band them.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(0.0f, ASSISTANT_CHART_ROW_GAP));
    for(const AssistantActivityRow& row : rows)
    {
        if(row.bins.empty())
        {
            continue;
        }

        const ImVec2 row_origin = ImGui::GetCursorScreenPos();
        const float  row_height =
            std::max(ASSISTANT_CHART_ROW_HEIGHT, ImGui::GetTextLineHeight());

        ImGui::PushID(static_cast<int>(row.track_id));
        ImGui::InvisibleButton("##minimap_row", ImVec2(width, row_height));
        ImGui::PopID();
        if(ImGui::IsItemHovered())
        {
            SetTooltipStyled("%s", row.name.c_str());
        }

        char label[32];
        std::snprintf(label, sizeof(label), "t%llu",
                      static_cast<unsigned long long>(row.track_id));
        draw->AddText(row_origin, label_color, label);

        // Fill the lane first, so idle stretches read as gaps rather than holes.
        const float x_start = row_origin.x + label_width;
        draw->AddRectFilled(ImVec2(x_start, row_origin.y),
                            ImVec2(x_start + strip_width, row_origin.y + row_height),
                            bg);

        const float cell_width = strip_width / static_cast<float>(row.bins.size());
        for(size_t i = 0; i < row.bins.size(); ++i)
        {
            const float value = static_cast<float>(row.bins[i]);
            if(value <= 0.0f)
            {
                continue;
            }
            ImVec4 cell = accent;
            cell.w = ASSISTANT_CHART_MIN_ALPHA + value * (1.0f - ASSISTANT_CHART_MIN_ALPHA);
            const float x0 = x_start + cell_width * static_cast<float>(i);
            draw->AddRectFilled(ImVec2(x0, row_origin.y),
                                ImVec2(x0 + std::max(1.0f, cell_width),
                                       row_origin.y + row_height),
                                ImGui::ColorConvertFloat4ToU32(cell));
        }
    }
    ImGui::PopStyleVar();

    ImGui::TextDisabled("Busiest tracks. Hover for names.");
}

void
AssistantPanel::SetStatus(const std::string& text)
{
    m_status           = text;
    m_scroll_to_bottom = true;
}

void
AssistantPanel::RenderToolbarButton()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kBgFrame)));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered,
        ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kButtonHovered)));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonActive,
        ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kButtonActive)));
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kTextMain)));
    if(ImGui::Button("Ask Optiq"))
    {
        GetInstance()->ToggleVisible();
    }
    ImGui::PopStyleColor(4);
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Open the assistant. Configure the URL and key in "
                         "Edit > Preferences > Assistant.");
    }
}

// Resolves what the tools may touch, from whichever trace is in front. Rebuilt
// per call, so a closed tab cannot leave a tool holding a dead provider.
AssistantToolContext
AssistantPanel::MakeToolContext() const
{
    AssistantToolContext context;
    context.metrics_client_id = m_metrics_client_id;

    AppWindow* app = AppWindow::GetInstance();
    if(app == nullptr)
    {
        return context;
    }
    Project* project = app->GetCurrentProject();
    if(project == nullptr)
    {
        return context;
    }

    context.trace_name = project->GetName();
    RootView* root_view = dynamic_cast<RootView*>(project->GetView().get());
    if(root_view != nullptr)
    {
        context.data_provider = root_view->GetDataProvider();
    }
    if(project->GetTraceType() == Project::Compute)
    {
        context.is_compute = true;
        ComputeView* compute_view = dynamic_cast<ComputeView*>(project->GetView().get());
        if(compute_view != nullptr && compute_view->GetComputeSelection())
        {
            context.compute_selection = compute_view->GetComputeSelection().get();
        }
    }
    else if(project->GetTraceType() == Project::System)
    {
        TraceView* trace_view = dynamic_cast<TraceView*>(project->GetView().get());
        context.trace_view    = trace_view;
        if(trace_view != nullptr && trace_view->GetTimelineSelection())
        {
            context.timeline_selection = trace_view->GetTimelineSelection().get();
        }
    }
    return context;
}

std::string
AssistantPanel::CurrentProjectId() const
{
    AppWindow* app = AppWindow::GetInstance();
    if(app == nullptr || app->GetCurrentProject() == nullptr)
    {
        return std::string();
    }
    return app->GetCurrentProject()->GetID();
}

std::string
AssistantPanel::BuildUserPrompt(const std::string& question, bool include_briefing) const
{
    std::ostringstream out;
    if(include_briefing)
    {
        out << "Briefing for the current Optiq view:\n";
        out << BuildAssistantBriefing(MakeToolContext());
        out << "\n";
    }
    if(!question.empty())
    {
        out << "User question:\n" << question << "\n";
    }
    else
    {
        out << "Explain this view. Call trace_overview and get_summary so the "
               "explanation uses real kernel names and numbers, keep it to the "
               "overview pass, and offer the deeper dives as next steps.\n";
    }
    return out.str();
}

void
AssistantPanel::ResetTurn()
{
    CancelPendingRequest();
    m_phase           = Phase::kIdle;
    m_pending_calls.clear();
    m_next_call_index = 0;
    m_tool_round      = 0;
    m_force_final     = false;
    m_fetch_retries   = 0;
    m_fetch_wait      = FetchWait();
    m_status.clear();
    m_queued_question.clear();
    m_queued_explain_view = false;
}

// Abandons the reply we are waiting on. Cancel() closes the socket, so this
// returns immediately instead of blocking out the endpoint's read timeout.
void
AssistantPanel::CancelPendingRequest()
{
    if(m_call != nullptr)
    {
        m_call->Cancel();
    }
    if(m_pending.valid())
    {
        m_pending.wait();
        m_pending = std::future<AssistantChatResult>();
    }
    m_call.reset();
}

void
AssistantPanel::StartHttpRequest()
{
    SettingsManager&         settings = SettingsManager::GetInstance();
    const AssistantProvider* provider = settings.GetActiveAssistantProvider();
    if(provider == nullptr)
    {
        ResetTurn();
        AppendLine(Speaker::kStatus,
                   "Set the URL, model, and key in Edit > Preferences > Assistant.");
        return;
    }

    AssistantProvider endpoint = *provider;
    ApplyAssistantEndpointDefaults(endpoint);

    AssistantChatRequest request;
    request.endpoint_url = endpoint.endpoint_url;
    request.model        = endpoint.model;
    settings.GetAssistantToken(endpoint.name, request.api_token);
    request.enable_tools = !m_force_final;

    AssistantMessage system_message;
    system_message.role = "system";
    system_message.content = ASSISTANT_SYSTEM_PROMPT;
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    system_message.content += ASSISTANT_SCRIPT_PROMPT;
#endif
    request.messages.push_back(system_message);
    request.messages.insert(request.messages.end(), m_conversation.begin(),
                            m_conversation.end());

    m_phase = Phase::kHttpWait;
    SetStatus("Thinking...");
    spdlog::info("Assistant HTTP round {} ({} messages)", m_tool_round,
                 request.messages.size());

    m_call    = std::make_shared<AssistantChatCall>();
    m_pending = std::async(std::launch::async, [call = m_call, request]() {
        return call->Send(request);
    });
}

void
AssistantPanel::BeginQueuedTurn()
{
    AssistantMessage user_message;
    user_message.role    = "user";
    user_message.content = BuildUserPrompt(m_queued_question, true);
    m_conversation.push_back(user_message);
    m_queued_question.clear();
    m_queued_explain_view = false;
    StartHttpRequest();
}

// Preloads an empty summary so the briefing carries real numbers, not zeros.
// False when there is nothing to preload.
bool
AssistantPanel::TryStartSummaryWarmup(const std::string& question, bool explain_view)
{
    const AssistantToolContext context = MakeToolContext();
    if(context.data_provider == nullptr || context.is_compute)
    {
        return false;
    }
    if(context.data_provider->GetState() != ProviderState::kReady)
    {
        return false;
    }
    const SummaryInfo::GPUMetrics& gpu =
        context.data_provider->DataModel().GetSummary().GetSummaryData().gpu;
    if(!gpu.top_kernels.empty() || gpu.kernel_exec_time_total > 0.0)
    {
        return false;
    }

    const AssistantToolStartResult started =
        StartAssistantTool(context, "get_summary", "{}");
    if(!started.pending)
    {
        return false;
    }

    m_queued_question     = question;
    m_queued_explain_view = explain_view;
    BeginFetchWait(started, std::string(), "get_summary", true);
    SetStatus("Loading summary...");
    return true;
}

void
AssistantPanel::SendCurrentInput(bool explain_view)
{
    if(Busy())
    {
        return;
    }

    SettingsManager&         settings = SettingsManager::GetInstance();
    const AssistantProvider* provider = settings.GetActiveAssistantProvider();
    if(provider == nullptr || provider->endpoint_url.empty())
    {
        AppendLine(Speaker::kStatus,
                   "Set the assistant URL in Edit > Preferences > Assistant.");
        NotificationManager::GetInstance().Show(
            "Assistant URL is not set", NotificationLevel::Warning);
        return;
    }

    std::string question = m_input;
    if(!explain_view && question.empty())
    {
        return;
    }

    m_next_steps.clear();

    const std::string display =
        explain_view ? (question.empty() ? std::string("Explain this view") : question)
                     : question;
    AppendLine(Speaker::kUser, display);
    m_input.clear();

    if(explain_view)
    {
        m_conversation.clear();
        m_tool_round  = 0;
        m_force_final = false;
    }

    m_pending_calls.clear();
    m_next_call_index = 0;
    m_fetch_retries   = 0;
    m_fetch_wait      = FetchWait();
    m_turn_project_id = CurrentProjectId();
    if(TryStartSummaryWarmup(question, explain_view))
    {
        return;
    }

    AssistantMessage user_message;
    user_message.role    = "user";
    user_message.content = BuildUserPrompt(question, true);
    m_conversation.push_back(user_message);
    StartHttpRequest();
}

void
AssistantPanel::HandleHttpResult(const AssistantChatResult& result)
{
    // Nothing to report on a request we abandoned, but still reset so a turn is
    // never left mid-phase with nothing in flight.
    if(result.cancelled)
    {
        ResetTurn();
        m_next_steps.clear();
        return;
    }

    if(!result.ok)
    {
        ResetTurn();
        m_next_steps.clear();
        AppendLine(Speaker::kStatus, result.error);
        NotificationManager::GetInstance().Show(result.error, NotificationLevel::Error);
        return;
    }

    // Once the budget nudge has gone out, ignore any further tool calls so a
    // model that keeps reaching for them cannot loop.
    if(!result.tool_calls.empty() && !m_force_final)
    {
        BeginToolQueue(result.tool_calls, result.reply);
        return;
    }

    // It stopped reaching for tools, but that draft was written with the tool
    // schema still in the request. Discard it and ask once more with tools off,
    // so the model writes prose instead of weighing up another call.
    if(!m_force_final && m_tool_round > 0)
    {
        BeginFinalAnswer();
        return;
    }

    const std::string reply =
        result.reply.empty()
            ? std::string("I gathered the data but could not put an answer together. "
                          "Ask me something more specific and I'll dig in again.")
            : result.reply;

    ResetTurn();
    AppendLine(Speaker::kAssistant, reply);

    AssistantMessage assistant_message;
    assistant_message.role    = "assistant";
    assistant_message.content = reply;
    m_conversation.push_back(assistant_message);
}

// Queues one round of tool calls, or gives up on gathering once the budget for
// it is gone.
void
AssistantPanel::BeginToolQueue(const std::vector<AssistantToolCall>& calls,
                               const std::string&                    assistant_text)
{
    ++m_tool_round;
    if(m_tool_round > ASSISTANT_MAX_TOOL_ROUNDS)
    {
        BeginFinalAnswer();
        return;
    }

    AssistantMessage assistant_message;
    assistant_message.role       = "assistant";
    assistant_message.content    = assistant_text;
    assistant_message.tool_calls = calls;
    m_conversation.push_back(assistant_message);

    m_pending_calls   = calls;
    m_next_call_index = 0;
    m_fetch_retries   = 0;
    RunNextTool();
}

// Asks for the write-up with tools off. The only round whose prose the user
// ever reads.
void
AssistantPanel::BeginFinalAnswer()
{
    AssistantMessage nudge;
    nudge.role    = "user";
    nudge.content =
        "Do not call any more tools. Write your answer now, reading the numbers you "
        "already gathered and saying what they mean for this workload.";
    m_conversation.push_back(nudge);
    m_force_final = true;
    SetStatus("Working out what it means...");
    StartHttpRequest();
}

void
AssistantPanel::RunNextTool()
{
    if(m_next_call_index >= m_pending_calls.size())
    {
        ContinueAfterTools();
        return;
    }

    // Tools read whichever trace is in front, so a tab change mid-queue would
    // silently mix two traces' numbers. Say so instead.
    const std::string project_id = CurrentProjectId();
    if(project_id != m_turn_project_id)
    {
        m_turn_project_id = project_id;
        FinishCurrentTool("The trace in front changed, so everything you gathered "
                          "before this belongs to a different trace. Call "
                          "trace_overview to start again on this one.");
        return;
    }

    const AssistantToolCall& call = m_pending_calls[m_next_call_index];
    SetStatus(AssistantToolStatusLabel(call.name));
    spdlog::info("Assistant tool {} ({})", call.name, call.id);

    const AssistantToolStartResult started =
        StartAssistantTool(MakeToolContext(), call.name, call.arguments);
    if(!started.status_line.empty())
    {
        SetStatus(started.status_line);
    }
    if(started.chart)
    {
        AppendChart(started.chart_track_id);
    }
    if(started.set_next_steps)
    {
        m_next_steps = started.next_steps;
    }

    if(started.pending)
    {
        BeginFetchWait(started, call.id, call.name, false);
        return;
    }

    FinishCurrentTool(started.content);
}

void
AssistantPanel::BeginFetchWait(const AssistantToolStartResult& started,
                               const std::string& tool_call_id,
                               const std::string& tool_name, bool warmup)
{
    const std::chrono::steady_clock::time_point started_at =
        m_fetch_retries > 0 && !warmup ? m_fetch_wait.started
                                        : std::chrono::steady_clock::now();
    m_phase                    = Phase::kToolWait;
    m_fetch_wait.active        = true;
    m_fetch_wait.started_fetch = started.started_fetch;
    m_fetch_wait.warmup        = warmup;
    m_fetch_wait.request_ids   = started.request_ids;
    m_fetch_wait.fetch         = started.fetch;
    m_fetch_wait.tool_call_id  = tool_call_id;
    m_fetch_wait.tool_name     = tool_name;
    m_fetch_wait.prefix        = started.content;
    m_fetch_wait.started       = started_at;
    m_fetch_wait.timeout_seconds = started.timeout_seconds;
}

// Hands one tool's output back to the model and moves on to the next. Warmup
// never arrives here: it has no tool call to answer, so PollToolFetch takes
// every exit itself rather than let an empty queue look like "tools done".
void
AssistantPanel::FinishCurrentTool(const std::string& content)
{
    if(m_next_call_index >= m_pending_calls.size())
    {
        ContinueAfterTools();
        return;
    }

    const AssistantToolCall& call = m_pending_calls[m_next_call_index];
    AssistantMessage tool_message;
    tool_message.role         = "tool";
    tool_message.name         = call.name;
    tool_message.tool_call_id = call.id;
    tool_message.content      = content.empty() ? "(empty)" : content;
    m_conversation.push_back(tool_message);

    // switch_tab is the model changing traces on purpose, so re-pin here rather
    // than reporting its own move back to it on the next call.
    m_turn_project_id = CurrentProjectId();
    m_fetch_wait      = FetchWait();
    m_fetch_retries   = 0;
    ++m_next_call_index;
    RunNextTool();
}

void
AssistantPanel::ContinueAfterTools()
{
    m_pending_calls.clear();
    m_next_call_index = 0;
    m_fetch_retries   = 0;
    m_fetch_wait      = FetchWait();
    StartHttpRequest();
}

bool
AssistantPanel::AnyFetchPending(const AssistantToolContext& context) const
{
    // A script waits on the user reading it, then on the run they approved.
    // Neither has a request id, so the tool answers for itself.
    if(m_fetch_wait.fetch.kind == AssistantFetchKind::kScript)
    {
        return AssistantScriptFetchPending(context);
    }
    for(uint64_t request_id : m_fetch_wait.request_ids)
    {
        if(context.data_provider->IsRequestPending(request_id))
        {
            return true;
        }
    }
    return false;
}

void
AssistantPanel::PollToolFetch()
{
    if(m_phase != Phase::kToolWait || !m_fetch_wait.active)
    {
        return;
    }

    RenderScheduler::GetInstance().RequestRender();

    const AssistantToolContext context = MakeToolContext();
    if(context.data_provider == nullptr)
    {
        // Warmup has no pending tool call, so FinishCurrentTool would wrongly
        // treat the empty queue as "tools done" and start an HTTP turn.
        if(m_fetch_wait.warmup)
        {
            ResetTurn();
            AppendLine(Speaker::kStatus,
                       "The trace closed before the summary finished loading.");
            return;
        }
        FinishCurrentTool("The trace closed while a tool was running.");
        return;
    }

    if(!m_fetch_wait.warmup && CurrentProjectId() != m_turn_project_id)
    {
        FinishCurrentTool("The trace in front changed, so this tool's pending "
                          "data belongs to a different trace. Run it again on "
                          "the current trace.");
        return;
    }

    const std::chrono::seconds elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_fetch_wait.started);
    const int64_t deadline = m_fetch_wait.timeout_seconds > 0
                                 ? static_cast<int64_t>(m_fetch_wait.timeout_seconds)
                                 : ASSISTANT_FETCH_TIMEOUT_SECONDS;
    const bool pending   = AnyFetchPending(context);
    const bool timed_out = elapsed.count() >= deadline;
    if(pending && !timed_out)
    {
        return;
    }

    // The warmup has no tool call to answer, so the queued question goes out
    // whether it landed or timed out — but only on the same trace it started on.
    if(m_fetch_wait.warmup)
    {
        m_fetch_wait = FetchWait();
        if(CurrentProjectId() != m_turn_project_id)
        {
            ResetTurn();
            AppendLine(Speaker::kStatus,
                       "The trace in front changed before the summary finished "
                       "loading. Ask again on this trace.");
            return;
        }
        BeginQueuedTurn();
        return;
    }

    // A script is answered by its own tool even when the wait runs out: only
    // that side knows whether the user never replied or the run was abandoned,
    // and it has an outstanding offer to clear either way.
    if(pending && m_fetch_wait.fetch.kind != AssistantFetchKind::kScript)
    {
        FinishCurrentTool("Timed out waiting for " + m_fetch_wait.tool_name + ".");
        return;
    }

    // The rows that landed answer someone else's query, so never format them as
    // this tool's result. Re-run until we issue our own query, bounded by the
    // original wait deadline even though BeginFetchWait runs again.
    if(!m_fetch_wait.started_fetch &&
       m_fetch_wait.fetch.kind != AssistantFetchKind::kSummary)
    {
        if(timed_out)
        {
            FinishCurrentTool("Timed out waiting to run " + m_fetch_wait.tool_name +
                              " because its shared data request remained busy.");
            return;
        }

        ++m_fetch_retries;
        m_fetch_wait.active = false;
        RunNextTool();
        return;
    }

    std::string body = FinishAssistantFetch(context, m_fetch_wait.fetch);
    if(!m_fetch_wait.prefix.empty())
    {
        body = m_fetch_wait.prefix + body;
    }
    FinishCurrentTool(body);
}

// Advances the turn once a frame: the HTTP reply first, then any fetch a tool
// is waiting on. Tools run only from here, never Render(), so they cannot
// reorder panels halfway through the frame that draws them.
void
AssistantPanel::Update()
{
    if(m_pending.valid())
    {
        RenderScheduler::GetInstance().RequestRender();
        if(m_pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            return;
        }
        const AssistantChatResult result = m_pending.get();
        m_call.reset();
        HandleHttpResult(result);
        return;
    }

    PollToolFetch();
}

// Kept for the RocWidget contract; AppWindow draws the panel through RenderDocked.
void
AssistantPanel::Render()
{
    RenderDocked();
}

float
AssistantPanel::DockedWidth() const
{
    return m_visible ? m_dock_width + ASSISTANT_SPLITTER_WIDTH : 0.0f;
}

// Drag handle between the main view and the panel, matching the topology
// sidebar on the other side of the window.
void
AssistantPanel::RenderSplitter()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const ImVec2     origin   = ImGui::GetCursorScreenPos();
    const float      height   = ImGui::GetContentRegionAvail().y;

    ImGui::InvisibleButton("##assistant_splitter",
                           ImVec2(ASSISTANT_SPLITTER_WIDTH, height));
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();
    if(hovered || active)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if(active)
    {
        // Dragging left widens the panel, so the delta is negated.
        m_dock_width = std::clamp(m_dock_width - ImGui::GetIO().MouseDelta.x,
                                  ASSISTANT_MIN_WIDTH, ASSISTANT_MAX_WIDTH);
    }

    ImGui::GetWindowDrawList()->AddRectFilled(
        origin, ImVec2(origin.x + ASSISTANT_SPLITTER_WIDTH, origin.y + height),
        settings.GetColor(hovered || active ? Colors::kAccent
                                            : Colors::kSplitterColor));
}

void
AssistantPanel::RenderDocked()
{
    if(!m_visible)
    {
        return;
    }

    // A width persisted below the current minimum would otherwise stick until
    // the splitter was dragged.
    m_dock_width = std::max(m_dock_width, ASSISTANT_MIN_WIDTH);

    SettingsManager& settings = SettingsManager::GetInstance();

    RenderSplitter();
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ASSISTANT_WINDOW_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
    // Only the transcript scrolls; the header and composer are pinned.
    ImGui::BeginChild("##assistant_dock", ImVec2(m_dock_width, 0.0f),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    RenderHeaderCard();
    RenderTranscript();
    RenderComposer();

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void
AssistantPanel::RenderHeaderCard()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    BeginPanelCard("##assistant_header", PanelCardTone::kFrame, ASSISTANT_CARD_PADDING,
                   true, &settings);

    const AssistantProvider* provider = settings.GetActiveAssistantProvider();
    const bool configured = provider != nullptr && !provider->endpoint_url.empty();

    const float close_width = ImGui::GetFrameHeight();
    ImGui::BeginGroup();
    PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::BeginGroup();
    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
    ImGui::TextUnformatted("Ask Optiq");
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    if(!configured)
    {
        ImGui::TextUnformatted("Not configured");
    }
    else if(!provider->model.empty())
    {
        ImGui::TextUnformatted(provider->model.c_str());
    }
    else
    {
        ImGui::TextUnformatted("Ready");
    }
    ImGui::PopStyleColor();
    ImGui::EndGroup();
    ImGui::EndGroup();
    if(ImGui::IsItemHovered())
    {
        if(!configured)
        {
            SetTooltipStyled("Not configured. Edit > Preferences > Assistant.");
        }
        else
        {
            SetTooltipStyled("%s\n%s",
                             provider->model.empty() ? "(no model set)"
                                                     : provider->model.c_str(),
                             provider->endpoint_url.c_str());
        }
    }

    ImGui::SameLine(0.0f, 0.0f);
    const float leftover = ImGui::GetContentRegionAvail().x - close_width;
    if(leftover > 0.0f)
    {
        ImGui::Dummy(ImVec2(leftover, 0.0f));
        ImGui::SameLine(0.0f, 0.0f);
    }
    if(XButton("##assistant_close", "Close the assistant", &settings))
    {
        m_visible = false;
    }

    if(!configured)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextWarning));
        ImGui::TextWrapped("Set the URL, model, and key in Edit > Preferences > Assistant.");
        ImGui::PopStyleColor();
    }

    EndPanelCard();
}

void
AssistantPanel::RenderTranscript()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    // Last frame's measured composer height; the estimate is only for frame one.
    const float composer_height =
        m_composer_height > 0.0f
            ? m_composer_height
            : ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f +
                  ASSISTANT_CARD_PADDING.y * 2.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, PANEL_CARD_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ASSISTANT_CARD_PADDING);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 12.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
    ImGui::BeginChild("assistant_history", ImVec2(0.0f, -composer_height),
                      ImGuiChildFlags_Borders);

    if(m_lines.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
        ImGui::TextWrapped("Ask about this trace, or press Explain this view below.");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextWrapped(
            "I read the timeline overview first, then dig into whatever looks worst.");
        ImGui::PopStyleColor();
    }

    for(size_t i = 0; i < m_lines.size(); ++i)
    {
        RenderMessageCard(i, m_lines[i]);
    }

    // Drawn from live state rather than stored, so it cannot outlive its turn.
    if(Busy() && !m_status.empty())
    {
        RenderLoadingIndicatorDots(ASSISTANT_DOT_RADIUS, ASSISTANT_DOT_COUNT,
                                   ASSISTANT_DOT_SPACING,
                                   settings.GetColor(Colors::kAccent),
                                   ASSISTANT_DOT_SPEED);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x * 2.0f);
        PanelFieldLabel(m_status.c_str(), false, &settings);
    }

    if(m_scroll_to_bottom)
    {
        ImGui::SetScrollHereY(1.0f);
        m_scroll_to_bottom = false;
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

void
AssistantPanel::RenderMessageCard(size_t index, const ChatLine& line)
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    ImGui::PushID(static_cast<int>(index));

    if(line.speaker == Speaker::kStatus)
    {
        // A notice that outlived its turn, such as a failed request.
        PanelIcon(ICON_X_CIRCLED, Colors::kTextError, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextError));
        ImGui::TextWrapped("%s", line.text.c_str());
        ImGui::PopStyleColor();
        ImGui::PopID();
        return;
    }

    if(line.speaker == Speaker::kChart)
    {
        BeginPanelCard("##chart_card", PanelCardTone::kPanel, ASSISTANT_CARD_PADDING,
                       true, &settings);
        PanelIcon(ICON_CHART_BAR, Colors::kAccent, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        PanelFieldLabel("Timeline overview", false, &settings);
        RenderActivityChart(line.track_id);
        EndPanelCard();
        ImGui::PopID();
        return;
    }

    const bool user = line.speaker == Speaker::kUser;
    BeginPanelCard("##message_card",
                   user ? PanelCardTone::kFrame : PanelCardTone::kPanel,
                   ASSISTANT_CARD_PADDING, true, &settings);
    if(user)
    {
        PanelFieldLabel("You", false, &settings);
    }
    else
    {
        PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        PanelFieldLabel("Optiq", false, &settings);
    }
    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextWrapped("%s", line.text.c_str());
    ImGui::PopTextWrapPos();
    EndPanelCard();
    ImGui::PopID();
}

// Stacked follow-ups under the transcript: Explain this view on an empty chat,
// or the model's offered next steps after a turn.
void
AssistantPanel::RenderSuggestedActions()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const AssistantProvider* provider = settings.GetActiveAssistantProvider();
    const bool configured =
        provider != nullptr && !provider->endpoint_url.empty();

    const bool show_explain = m_lines.empty() && m_next_steps.empty() && !Busy();
    if(!show_explain && m_next_steps.empty())
    {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    ImGui::BeginDisabled(Busy() || !configured);

    if(show_explain)
    {
        if(AccentButton("Explain this view", ImVec2(-FLT_MIN, 0.0f), &settings))
        {
            SendCurrentInput(true);
        }
        ImGui::EndDisabled();
        ImGui::PopStyleVar(2);
        if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            SetTooltipStyled(
                "Runs a self-directed investigation: timeline overview, summary, top "
                "events, then drills into the worst offenders and moves the timeline "
                "to what it found.");
        }
        ImGui::Spacing();
        return;
    }

    const float wrap_width = std::max(
        1.0f, ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2.0f);
    int clicked = -1;
    for(size_t i = 0; i < m_next_steps.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        const std::string label =
            WrapButtonLabel(std::to_string(i + 1) + ". " + m_next_steps[i], wrap_width);
        bool pressed = false;
        if(i == 0)
        {
            pressed = AccentButton(label.c_str(), ImVec2(-FLT_MIN, 0.0f), &settings);
        }
        else
        {
            pressed = ColoredButton(label.c_str(), settings.GetColor(Colors::kButton),
                                    settings.GetColor(Colors::kButtonHovered),
                                    settings.GetColor(Colors::kButtonActive),
                                    settings.GetColor(Colors::kTextMain), nullptr,
                                    ImVec2(-FLT_MIN, 0.0f));
        }
        if(pressed)
        {
            clicked = static_cast<int>(i);
        }
        ImGui::PopID();
    }

    ImGui::EndDisabled();
    ImGui::PopStyleVar(2);

    if(clicked >= 0)
    {
        m_input = m_next_steps[static_cast<size_t>(clicked)];
        SendCurrentInput(false);
    }

    ImGui::Spacing();
}

void
AssistantPanel::RenderComposer()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    // Measured to the end of the card, so RenderTranscript can reserve exactly
    // this much on the next frame.
    const float start_y = ImGui::GetCursorPosY();

    BeginPanelCard("##assistant_composer", PanelCardTone::kFrame, ASSISTANT_CARD_PADDING,
                   true, &settings);

    RenderSuggestedActions();

    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_None;
    if(Busy())
    {
        input_flags |= ImGuiInputTextFlags_ReadOnly;
    }

    const float gap       = style.ItemInnerSpacing.x;
    const float icon_size = ImGui::GetFrameHeight();
    const float input_width =
        std::max(80.0f, ImGui::GetContentRegionAvail().x - ASSISTANT_SEND_WIDTH -
                            icon_size - gap * 2.0f);

    ImGui::SetNextItemWidth(input_width);
    InputTextStringWithHint("##assistant_input",
                            Busy() ? "Working..." : "Ask about this trace...", m_input,
                            input_flags);
    const bool submitted = ImGui::IsItemFocused() &&
                           ImGui::IsKeyPressed(ImGuiKey_Enter) &&
                           !ImGui::GetIO().KeyShift && !Busy();

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(Busy() || m_input.empty());
    const bool send = AccentButton("Send", ImVec2(ASSISTANT_SEND_WIDTH, 0.0f), &settings);
    ImGui::EndDisabled();

    ImGui::SameLine(0.0f, gap);
    ImGui::BeginDisabled(m_lines.empty() && m_input.empty() && !Busy());
    if(IconButton(ICON_TRASH_CAN, settings.GetFontManager().GetFont(FontType::kIcon),
                  ImVec2(icon_size, icon_size), "Clear the conversation", false,
                  ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                  settings.GetColor(Colors::kButtonHovered),
                  settings.GetColor(Colors::kButtonActive)))
    {
        ResetTurn();
        m_lines.clear();
        m_input.clear();
        m_conversation.clear();
        m_next_steps.clear();
    }
    ImGui::EndDisabled();

    EndPanelCard();

    // EndChild already advanced the cursor past one ItemSpacing, which the
    // transcript's reservation has to include too.
    m_composer_height = ImGui::GetCursorPosY() - start_y +
                        settings.GetDefaultStyle().ItemSpacing.y;

    if(send || submitted)
    {
        SendCurrentInput(false);
    }
}

}  // namespace View
}  // namespace RocProfVis
