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
// Same padding the remote/profiler dialogs use. The old 8px gutter made a
// 320px column feel like a squeezed inspector instead of a chat surface.
constexpr ImVec2 ASSISTANT_WINDOW_PADDING = ImVec2(14.0f, 12.0f);
constexpr ImVec2 ASSISTANT_CARD_PADDING   = ImVec2(14.0f, 10.0f);
constexpr float  ASSISTANT_SEND_WIDTH     = 80.0f;
constexpr float  ASSISTANT_DOT_RADIUS     = 2.5f;
constexpr int    ASSISTANT_DOT_COUNT      = 3;
constexpr float  ASSISTANT_DOT_SPACING    = 4.0f;
constexpr float  ASSISTANT_DOT_SPEED      = 5.0f;
// The assistant is expected to chain tools without asking, so this budget has to
// cover a whole self-directed investigation, not a single lookup.
constexpr uint32_t ASSISTANT_MAX_TOOL_ROUNDS = 20;
// How many times one tool may re-run after piggybacking on someone else's fetch.
// Without a cap, a request id that stays busy would spin the tool forever.
constexpr uint32_t ASSISTANT_MAX_FETCH_RETRIES     = 2;
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
    "it. The user may never have opened a profiler before.\n"

    "VOICE: talk to the user, do not lecture them. Warm, direct, confident, plain "
    "English, second person. Explain a piece of jargon in half a sentence the "
    "first time you use it, then move on. Vary your sentence length. Use headings "
    "and bullets only when you are genuinely listing data; explain findings in "
    "prose. Do not open with a greeting or a definition of what a trace is.\n"

    "AUTONOMY: investigate on your own across as many tool calls as it takes "
    "before you answer. Never ask permission to run a tool. Never end a turn by "
    "offering to look at something: if it is worth looking at, look at it now, in "
    "this same turn, and report what you found. Phrases like 'Would you like me "
    "to' or 'I recommend looking at' are failures. Chase the thing yourself, and "
    "only stop when you can name a likely cause rather than just a symptom. Ask "
    "the user something only when it is a decision that is genuinely theirs, such "
    "as which of two workloads they care about. Follow-ups the user might want "
    "next go in offer_next_steps, never in the prose.\n"

    "ALWAYS call at least one tool before you answer. The briefing only carries "
    "topology and headline totals; it never contains kernel names, per-kernel "
    "times, or event rows, so answering from the briefing alone means you are "
    "guessing. Never state a kernel name, duration, or count that did not come "
    "back from a tool in this conversation.\n"
    "Start every investigation with trace_overview. It is free, needs no database "
    "query, and tells you which time window and which tracks matter. Then pass its "
    "busiest_window_ns and track_id values into the other tools so they look at the "
    "interesting part of the trace instead of all of it.\n"
    "A good investigation looks like: trace_overview, then get_summary, then "
    "top_events(category=dispatch) scoped to the busiest window, then the outliers "
    "with kernel_instances sorted by duration, then event_details on the worst "
    "__uuid, and track_statistics on the queue it ran on. Do all of that before "
    "you write your answer, not after.\n"
    "Tools: trace_overview, get_summary, top_events, kernel_instances, "
    "kernel_metrics, list_tracks, search_events, track_events, track_samples, "
    "event_details, track_statistics, goto, show_panel, switch_tab, flow_arrows, "
    "annotate, bookmark, measure, reset_view, offer_next_steps.\n"
    "UI you may change on your own: goto with zoom=true and the __trackId/__uuid "
    "of the events behind the claim, so the timeline is sitting on the problem "
    "and those events are selected when the user looks up. Do that before you "
    "write the answer.\n"
    "Do not call annotate, bookmark, measure, show_panel, switch_tab, "
    "flow_arrows, or reset_view unless the user asked for that action in this "
    "turn. Never pin a note, save a bookmark, drop measurement pins, toggle "
    "panels, switch tabs, or change flow arrows as part of an investigation.\n"
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
    "guessing. When you name a window or an outlier in your answer, call goto with "
    "that range and the __trackId/__uuid of the events behind the claim, so the "
    "timeline is sitting on it and those events are lit up when the user looks up. "
    "Do that before you write the answer, not instead of writing it.\n"

    "FINISHING: interpret, do not recite. The user can already see the numbers; "
    "what they cannot see is what those numbers mean for this workload, so say "
    "which one is the problem, why it is a problem, and what it points at. A "
    "duration is only worth quoting when you say what it should have been. Lead "
    "with the single most important finding in one sentence, then "
    "the evidence you gathered, then what you would change. Length should match "
    "what you actually found, not a fixed template. End on the finding or the "
    "recommendation. Do not end on an offer. Call offer_next_steps as your last "
    "tool, then write the answer in the response after it: two or three short "
    "follow-ups the user can click, most useful first, each a complete thing "
    "they would type, under 80 characters. Never put the written answer in the "
    "same response as a tool call, and do not list those same options in the "
    "prose.";

}  // namespace

AssistantPanel* AssistantPanel::s_instance = nullptr;

// The panel, created on first use the way the other global overlays are.
AssistantPanel*
AssistantPanel::GetInstance()
{
    if(s_instance == nullptr)
    {
        s_instance = new AssistantPanel();
    }
    return s_instance;
}

// Tears the panel down. AppWindow calls this on the way out.
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

// Flips the panel open or closed, for the toolbar button and the View menu.
void
AssistantPanel::ToggleVisible()
{
    m_visible = !m_visible;
}

// The visibility flag itself, so ImGui::MenuItem can tick and toggle it.
bool*
AssistantPanel::VisiblePtr()
{
    return &m_visible;
}

// True while a turn is running, which is what disables the composer.
bool
AssistantPanel::Busy() const
{
    return m_phase != Phase::kIdle || m_pending.valid();
}

// Adds a transcript line and scrolls to it.
void
AssistantPanel::AppendLine(Speaker speaker, const std::string& text)
{
    ChatLine line;
    line.speaker = speaker;
    line.text    = text;
    m_lines.push_back(line);
    m_scroll_to_bottom = true;
}

// Adds a transcript entry that draws the activity strip for one track, or the
// whole trace when track_id is invalid. The chart reads live model data, so a
// second one for the same track would be a pixel-identical copy: the model is
// told to open every investigation with trace_overview, which would otherwise
// stack one card per turn.
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

// Draws the same two things the timeline shows: the histogram strip (summed
// event density over time) and, under it, the busiest minimap rows.
void
AssistantPanel::RenderActivityChart(uint64_t track_id)
{
    const AssistantToolContext context = MakeToolContext();
    if(context.data_provider == nullptr)
    {
        return;
    }

    // Bin count follows the column width so the bars stay legible when the dock
    // is dragged narrow instead of collapsing into a smear.
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

    // Size the label gutter to the widest id actually present rather than a
    // fixed width, which would eat a chunk of a narrow column. The ids are worth
    // the room: they are what the model cites in its answer.
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

    // The rows butt up against each other so they read as one strip. Default
    // item spacing would leave a panel-coloured band between every pair.
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

        // Fill the lane first, so idle stretches read as gaps in a strip rather
        // than as holes punched through to the panel behind it.
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

// Replaces the working line under the transcript.
void
AssistantPanel::SetStatus(const std::string& text)
{
    m_status           = text;
    m_scroll_to_bottom = true;
}

// The "Ask Optiq" button, shared by the system and compute toolbars.
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

// Resolves what the tools are allowed to touch, from whichever trace is in front.
// Rebuilt on every call rather than cached, so a closed tab cannot leave a tool
// holding a dead provider.
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

// Identifies the trace in front, so a tab change mid-turn can be spotted.
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

// Wraps the question in the briefing, which is what orients the model on a trace
// it has not seen yet.
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
        out << "Explain this view to a novice. Call trace_overview, then get_summary "
               "and top_events, so the explanation uses real kernel names and "
               "numbers.\n";
    }
    return out.str();
}

// Drops everything about the current turn, including any request in flight.
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

// Posts the conversation so far to the configured route, on a worker thread.
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
    request.endpoint_url          = endpoint.endpoint_url;
    request.model                 = endpoint.model;
    request.auth_header           = endpoint.auth_header;
    request.auth_prefix           = endpoint.auth_prefix;
    request.use_legacy_max_tokens = endpoint.use_legacy_max_tokens;
    settings.GetAssistantToken(provider->name, request.api_token);
    request.enable_tools = !m_force_final;

    // Choosing the next tool is a lookup; the round that writes the answer is
    // the one worth paying for.
    AssistantMessage system_message;
    system_message.role = "system";
    system_message.content = ASSISTANT_SYSTEM_PROMPT;
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

// Sends the question that was held back while the summary preloaded.
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

// Loads the summary before the first question when it is still empty, so the
// briefing carries real numbers instead of zeros. Returns false when there is
// nothing to preload.
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

// Starts a turn from the composer, or from the "Explain this view" button.
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

// Takes the model's reply: either run the tools it asked for, or print the answer.
void
AssistantPanel::HandleHttpResult(const AssistantChatResult& result)
{
    // We walked away from this one, so there is nothing to say about it. Reset
    // anyway, so a turn can never be left mid-phase with nothing in flight.
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

    // It has stopped reaching for tools, so this is its answer - written on the
    // cheap reasoning budget the tool rounds run at. Throw that draft away and
    // ask once more with the budget turned up, now that every number it needs is
    // already in the conversation.
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

// Asks for the write-up itself: no tools, and the reasoning budget raised. This
// is the only round whose prose the user ever reads.
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

// Runs the next queued tool, or goes back to the model when the queue is empty.
void
AssistantPanel::RunNextTool()
{
    if(m_next_call_index >= m_pending_calls.size())
    {
        ContinueAfterTools();
        return;
    }

    // Tools read whichever trace is in front, so a tab change between calls
    // would quietly move the investigation to a different trace. Say so rather
    // than letting the model mix the two sets of numbers.
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

// Parks the turn until the fetches a tool just queued have landed.
void
AssistantPanel::BeginFetchWait(const AssistantToolStartResult& started,
                               const std::string& tool_call_id,
                               const std::string& tool_name, bool warmup)
{
    m_phase                    = Phase::kToolWait;
    m_fetch_wait.active        = true;
    m_fetch_wait.started_fetch = started.started_fetch;
    m_fetch_wait.warmup        = warmup;
    m_fetch_wait.request_ids   = started.request_ids;
    m_fetch_wait.fetch         = started.fetch;
    m_fetch_wait.tool_call_id  = tool_call_id;
    m_fetch_wait.tool_name     = tool_name;
    m_fetch_wait.prefix        = started.content;
    m_fetch_wait.started       = std::chrono::steady_clock::now();
}

// Hands one tool's output back to the model and moves on to the next.
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

// Every tool has answered, so ask the model what it makes of them.
void
AssistantPanel::ContinueAfterTools()
{
    m_pending_calls.clear();
    m_next_call_index = 0;
    m_fetch_retries   = 0;
    m_fetch_wait      = FetchWait();
    StartHttpRequest();
}

// True while any request the waiting tool depends on is still outstanding.
bool
AssistantPanel::AnyFetchPending(const AssistantToolContext& context) const
{
    for(uint64_t request_id : m_fetch_wait.request_ids)
    {
        if(context.data_provider->IsRequestPending(request_id))
        {
            return true;
        }
    }
    return false;
}

// Checks on the fetches the current tool is waiting for, and formats them once
// they land.
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
        FinishCurrentTool("The trace closed while a tool was running.");
        return;
    }

    const std::chrono::seconds elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_fetch_wait.started);
    const bool pending   = AnyFetchPending(context);
    const bool timed_out = elapsed.count() >= ASSISTANT_FETCH_TIMEOUT_SECONDS;
    if(pending && !timed_out)
    {
        return;
    }

    // The warmup has no tool call to answer; it exists to fill the briefing, so
    // whether it landed or timed out the queued question goes out now.
    if(m_fetch_wait.warmup)
    {
        m_fetch_wait = FetchWait();
        BeginQueuedTurn();
        return;
    }

    if(pending)
    {
        FinishCurrentTool("Timed out waiting for " + m_fetch_wait.tool_name + ".");
        return;
    }

    // The rows that landed answer someone else's query, so run the tool again to
    // issue its own. Bounded, in case that request id never goes quiet.
    if(!m_fetch_wait.started_fetch &&
       m_fetch_wait.fetch.kind != AssistantFetchKind::kSummary &&
       m_fetch_retries < ASSISTANT_MAX_FETCH_RETRIES)
    {
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

// Advances the turn once a frame: first the HTTP reply, then any fetch a tool is
// waiting on. Tools only ever run from here, never from Render(), so they cannot
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

// Width to reserve on the right of the main view, splitter included.
float
AssistantPanel::DockedWidth() const
{
    return m_visible ? m_dock_width + ASSISTANT_SPLITTER_WIDTH : 0.0f;
}

// A drag handle between the main view and the panel, matching how the topology
// sidebar is resized on the other side of the window.
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

// Draws the panel as a column on the right: header, transcript, composer.
void
AssistantPanel::RenderDocked()
{
    if(!m_visible)
    {
        return;
    }

    // A session that started before the min width went up would otherwise keep
    // the old squeezed column until the splitter was dragged.
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

// The title row and the configuration warning.
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

// The scrolling conversation, with the working indicator pinned under it.
void
AssistantPanel::RenderTranscript()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    // Use last frame's measured composer height. The estimate below only runs on
    // the very first frame, before there is anything to measure.
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

    // The working indicator is drawn from live state, not stored in the
    // transcript, so it cannot outlive the turn that spawned it.
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

// Draws one transcript entry: a message, an error notice, or an activity chart.
void
AssistantPanel::RenderMessageCard(size_t index, const ChatLine& line)
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    ImGui::PushID(static_cast<int>(index));

    if(line.speaker == Speaker::kStatus)
    {
        // A notice that outlived the turn, such as a failed request. No spinner:
        // nothing is still running.
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

// The input row: text box, Send, and the button that clears the conversation.
void
AssistantPanel::RenderComposer()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    // Measured from here to the end of the card, so RenderTranscript can reserve
    // the exact space this block needs on the next frame.
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

    // EndChild has already advanced the cursor past one ItemSpacing, which the
    // transcript's reservation has to include as well.
    m_composer_height = ImGui::GetCursorPosY() - start_y +
                        settings.GetDefaultStyle().ItemSpacing.y;

    if(send || submitted)
    {
        SendCurrentInput(false);
    }
}

}  // namespace View
}  // namespace RocProfVis
