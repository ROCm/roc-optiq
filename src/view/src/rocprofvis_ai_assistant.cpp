// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_assistant.h"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cstdio>
#include <future>
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

constexpr float  ASSISTANT_DEFAULT_WIDTH  = 470.0f;
constexpr float  ASSISTANT_MIN_WIDTH      = 320.0f;
constexpr float  ASSISTANT_MAX_WIDTH      = 900.0f;
constexpr float  ASSISTANT_SPLITTER_WIDTH = 5.0f;
// A docked column is short on width, so padding is tighter than a floating
// dialog would use and every row earns its height.
constexpr ImVec2 ASSISTANT_WINDOW_PADDING = ImVec2(8.0f, 8.0f);
constexpr ImVec2 ASSISTANT_CARD_PADDING   = ImVec2(9.0f, 7.0f);
constexpr float  ASSISTANT_SEND_WIDTH     = 76.0f;
constexpr float  ASSISTANT_DOT_RADIUS     = 2.5f;
constexpr int    ASSISTANT_DOT_COUNT      = 3;
constexpr float  ASSISTANT_DOT_SPACING    = 4.0f;
constexpr float  ASSISTANT_DOT_SPEED      = 5.0f;
// The assistant is expected to chain tools without asking, so this budget has to
// cover a whole self-directed investigation, not a single lookup.
constexpr uint32_t ASSISTANT_MAX_TOOL_ROUNDS = 20;
constexpr int    ASSISTANT_FETCH_TIMEOUT_SECONDS = 45;
constexpr size_t ASSISTANT_CHART_MAX_BINS     = 64;
constexpr size_t ASSISTANT_CHART_MIN_BINS     = 16;
constexpr float  ASSISTANT_CHART_PX_PER_BIN   = 5.0f;
constexpr size_t ASSISTANT_CHART_ROWS         = 5;
constexpr float  ASSISTANT_CHART_HEIGHT       = 38.0f;
constexpr float  ASSISTANT_CHART_ROW_HEIGHT   = 8.0f;
constexpr float  ASSISTANT_CHART_ROW_GAP      = 2.0f;
constexpr float  ASSISTANT_CHART_MIN_ALPHA    = 0.12f;

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
    "as which of two workloads they care about.\n"

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
    "annotate, bookmark, measure, reset_view.\n"
    "You can drive every control on the trace toolbar. goto takes zoom=true when "
    "the range is too small to read unzoomed. measure drops the two pins on a span "
    "so the user sees the duration you are quoting. bookmark saves the current "
    "view to a numbered slot, so save one before you move them somewhere else. "
    "annotate pins a note that is saved with the project. reset_view zooms back "
    "out. Do these instead of describing them.\n"
    "You drive the app, not just read it. show_panel opens and closes the "
    "minimap, histogram, topology (the left navbar/tree), details, summary, log, "
    "and toolbar. switch_tab moves between open traces. flow_arrows controls the "
    "arrows linking an event to what it launched or waited on. Never tell the "
    "user you cannot change the interface: try the tool, and only say so if it "
    "reports back that it failed.\n"
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

    "FINISHING: lead with the single most important finding in one sentence, then "
    "the evidence you gathered, then what you would change. Length should match "
    "what you actually found, not a fixed template. End on the finding or the "
    "recommendation. Do not end on an offer.";

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
, m_request_generation(0)
, m_metrics_client_id(IdGenerator::GetInstance().GenerateId())
{
    m_widget_name = GenUniqueName("AssistantPanel");
}

void
AssistantPanel::Show()
{
    m_visible = true;
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

void
AssistantPanel::AppendChart(uint64_t track_id)
{
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
        ImGui::TextDisabled("Track %llu, whole trace",
                            static_cast<unsigned long long>(track_id));
        return;
    }

    ImGui::TextDisabled("Whole trace");

    const std::vector<AssistantActivityRow> rows =
        GetAssistantActivityRows(context, bin_count, ASSISTANT_CHART_ROWS);
    if(rows.empty())
    {
        return;
    }

    // Size the label gutter to the widest id actually present rather than a
    // fixed width, which would eat a chunk of a narrow column.
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

        const float cell_width = strip_width / static_cast<float>(row.bins.size());
        const float x_start    = row_origin.x + label_width;
        const float y0 =
            row_origin.y + (row_height - ASSISTANT_CHART_ROW_HEIGHT) * 0.5f;
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
            draw->AddRectFilled(ImVec2(x0, y0),
                                ImVec2(x0 + std::max(1.0f, cell_width),
                                       y0 + ASSISTANT_CHART_ROW_HEIGHT),
                                ImGui::ColorConvertFloat4ToU32(cell));
        }
        ImGui::Dummy(ImVec2(width, ASSISTANT_CHART_ROW_GAP));
    }
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

void
AssistantPanel::ResetTurn()
{
    ++m_request_generation;
    m_phase            = Phase::kIdle;
    m_pending_calls.clear();
    m_next_call_index  = 0;
    m_tool_round       = 0;
    m_force_final      = false;
    m_fetch_wait       = FetchWait();
    m_status.clear();
    m_queued_question.clear();
    m_queued_explain_view = false;
}

void
AssistantPanel::StartHttpRequest()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const AssistantSettings& assistant = settings.GetUserSettings().assistant;

    AssistantChatRequest request;
    request.endpoint_url = assistant.endpoint_url;
    request.model        = assistant.model;
    settings.GetAssistantToken(request.api_token);
    request.enable_tools = !m_force_final;

    AssistantMessage system_message;
    system_message.role    = "system";
    system_message.content = ASSISTANT_SYSTEM_PROMPT;
    request.messages.push_back(system_message);
    request.messages.insert(request.messages.end(), m_conversation.begin(),
                            m_conversation.end());

    m_phase = Phase::kHttpWait;
    SetStatus("Thinking...");
    spdlog::info("Assistant HTTP round {} ({} messages)", m_tool_round,
                 request.messages.size());

    m_pending = std::async(std::launch::async, [request]() {
        return SendAssistantChat(request);
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

    m_queued_question          = question;
    m_queued_explain_view      = explain_view;
    m_phase                    = Phase::kToolWait;
    m_fetch_wait.active        = true;
    m_fetch_wait.started_fetch = started.started_fetch;
    m_fetch_wait.request_ids   = started.request_ids;
    m_fetch_wait.fetch         = started.fetch;
    m_fetch_wait.tool_call_id.clear();
    m_fetch_wait.tool_name     = "__briefing";
    m_fetch_wait.prefix.clear();
    m_fetch_wait.started       = std::chrono::steady_clock::now();
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

    SettingsManager& settings = SettingsManager::GetInstance();
    const AssistantSettings& assistant = settings.GetUserSettings().assistant;
    if(assistant.endpoint_url.empty())
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

    ++m_request_generation;
    m_pending_calls.clear();
    m_next_call_index = 0;
    m_fetch_wait      = FetchWait();
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
    if(!result.ok)
    {
        ResetTurn();
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

    const std::string reply =
        result.reply.empty()
            ? std::string("I ran out of tool budget before I could finish that. Ask "
                          "me something more specific and I'll dig in again.")
            : result.reply;

    ResetTurn();
    AppendLine(Speaker::kAssistant, reply);

    AssistantMessage assistant_message;
    assistant_message.role    = "assistant";
    assistant_message.content = reply;
    m_conversation.push_back(assistant_message);
}

void
AssistantPanel::BeginToolQueue(const std::vector<AssistantToolCall>& calls,
                               const std::string&                    assistant_text)
{
    ++m_tool_round;
    if(m_tool_round > ASSISTANT_MAX_TOOL_ROUNDS)
    {
        AssistantMessage nudge;
        nudge.role    = "user";
        nudge.content =
            "You have used your tool budget for this question. Do not call any more "
            "tools. Write your answer now from what you already gathered.";
        m_conversation.push_back(nudge);
        m_force_final = true;
        SetStatus("Wrapping up...");
        StartHttpRequest();
        return;
    }

    AssistantMessage assistant_message;
    assistant_message.role       = "assistant";
    assistant_message.content    = assistant_text;
    assistant_message.tool_calls = calls;
    m_conversation.push_back(assistant_message);

    m_pending_calls   = calls;
    m_next_call_index = 0;
    RunNextTool();
}

void
AssistantPanel::RunNextTool()
{
    if(m_next_call_index >= m_pending_calls.size())
    {
        ContinueAfterTools();
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

    if(started.pending)
    {
        m_phase                    = Phase::kToolWait;
        m_fetch_wait.active        = true;
        m_fetch_wait.started_fetch = started.started_fetch;
        m_fetch_wait.request_ids   = started.request_ids;
        m_fetch_wait.fetch         = started.fetch;
        m_fetch_wait.tool_call_id  = call.id;
        m_fetch_wait.tool_name     = call.name;
        m_fetch_wait.prefix        = started.content;
        m_fetch_wait.started       = std::chrono::steady_clock::now();
        return;
    }

    FinishCurrentTool(started.content);
}

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

    m_fetch_wait = FetchWait();
    ++m_next_call_index;
    RunNextTool();
}

void
AssistantPanel::ContinueAfterTools()
{
    m_pending_calls.clear();
    m_next_call_index = 0;
    m_fetch_wait      = FetchWait();
    StartHttpRequest();
}

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
    if(elapsed.count() >= ASSISTANT_FETCH_TIMEOUT_SECONDS)
    {
        if(m_fetch_wait.tool_name == "__briefing")
        {
            m_fetch_wait = FetchWait();
            BeginQueuedTurn();
            return;
        }
        FinishCurrentTool("Timed out waiting for " + m_fetch_wait.tool_name + ".");
        return;
    }

    if(AnyFetchPending(context))
    {
        return;
    }

    if(m_fetch_wait.tool_name == "__briefing")
    {
        m_fetch_wait = FetchWait();
        BeginQueuedTurn();
        return;
    }

    if(!m_fetch_wait.started_fetch &&
       m_fetch_wait.fetch.kind != AssistantFetchKind::kSummary)
    {
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

void
AssistantPanel::PollPendingReply()
{
    if(m_pending.valid())
    {
        RenderScheduler::GetInstance().RequestRender();
        if(m_pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            AssistantChatResult result = m_pending.get();
            if(m_phase == Phase::kHttpWait)
            {
                HandleHttpResult(result);
            }
        }
        else if(m_phase != Phase::kToolWait)
        {
            return;
        }
    }

    if(m_phase == Phase::kToolWait)
    {
        PollToolFetch();
    }
}

void
AssistantPanel::Update()
{
    PollPendingReply();
}

void
AssistantPanel::Render()
{
    if(!m_visible)
    {
        return;
    }

    PollPendingReply();

    RenderDocked();
}

float
AssistantPanel::DockedWidth() const
{
    return m_visible ? m_dock_width + ASSISTANT_SPLITTER_WIDTH : 0.0f;
}

bool
AssistantPanel::IsVisible() const
{
    return m_visible;
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

void
AssistantPanel::RenderDocked()
{
    if(!m_visible)
    {
        return;
    }

    PollPendingReply();

    SettingsManager& settings = SettingsManager::GetInstance();

    RenderSplitter();
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ASSISTANT_WINDOW_PADDING);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
    // Only the transcript scrolls; the header and composer are pinned.
    ImGui::BeginChild("##assistant_dock", ImVec2(m_dock_width, 0.0f),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    RenderHeaderCard();
    ImGui::Spacing();
    RenderTranscript();
    RenderComposer();

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void
AssistantPanel::RenderHeaderCard()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    BeginPanelCard("##assistant_header", PanelCardTone::kFrame, ASSISTANT_CARD_PADDING,
                   true, &settings);

    const AssistantSettings& assistant = settings.GetUserSettings().assistant;

    // Title row: glyph, name, close. The endpoint lives in the tooltip rather
    // than its own line, which would cost a row of a narrow column.
    PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::TextUnformatted("Ask Optiq");
    if(ImGui::IsItemHovered())
    {
        if(assistant.endpoint_url.empty())
        {
            SetTooltipStyled("Not configured. Edit > Preferences > Assistant.");
        }
        else
        {
            SetTooltipStyled("%s\n%s",
                             assistant.model.empty() ? "(no model set)"
                                                     : assistant.model.c_str(),
                             assistant.endpoint_url.c_str());
        }
    }

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() * 0.5f);
    if(XButton("##assistant_close", "Close the assistant", &settings))
    {
        m_visible = false;
    }

    if(assistant.endpoint_url.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextWarning));
        ImGui::TextWrapped("Set the URL and key in Edit > Preferences > Assistant.");
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(Busy() || assistant.endpoint_url.empty());
    if(AccentButton("Explain this view", ImVec2(-FLT_MIN, 0.0f), &settings))
    {
        SendCurrentInput(true);
    }
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        SetTooltipStyled(
            "Runs a self-directed investigation: timeline overview, summary, top "
            "events, then drills into the worst offenders and moves the timeline "
            "to what it found.");
    }

    EndPanelCard();
}

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
            : ImGui::GetFrameHeight() * 2.0f + style.ItemSpacing.y * 3.0f +
                  ASSISTANT_CARD_PADDING.y * 2.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, PANEL_CARD_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ASSISTANT_CARD_PADDING);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
    ImGui::BeginChild("assistant_history", ImVec2(0.0f, -composer_height),
                      ImGuiChildFlags_Borders);

    if(m_lines.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextWrapped(
            "Ask about this trace, or press Explain this view. I read the timeline "
            "overview first, then dig into whatever looks worst.");
        ImGui::PopStyleColor();
    }

    for(size_t i = 0; i < m_lines.size(); ++i)
    {
        RenderMessageCard(i, m_lines[i]);
        ImGui::Spacing();
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
    ImGui::PopStyleVar(2);
}

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
    ImGui::TextWrapped("%s", line.text.c_str());
    EndPanelCard();
    ImGui::PopID();
}

void
AssistantPanel::RenderComposer()
{
    SettingsManager& settings = SettingsManager::GetInstance();

    // Measured from here to the end of the card, so RenderTranscript can reserve
    // the exact space this block needs on the next frame.
    const float start_y = ImGui::GetCursorPosY();
    ImGui::Spacing();

    BeginPanelCard("##assistant_composer", PanelCardTone::kFrame, ASSISTANT_CARD_PADDING,
                   true, &settings);

    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_None;
    if(Busy())
    {
        input_flags |= ImGuiInputTextFlags_ReadOnly;
    }
    ImGui::SetNextItemWidth(-1.0f);
    InputTextStringWithHint("##assistant_input",
                            Busy() ? "Working..." : "Ask a follow-up...", m_input,
                            input_flags);
    const bool submitted = ImGui::IsItemFocused() &&
                           ImGui::IsKeyPressed(ImGuiKey_Enter) &&
                           !ImGui::GetIO().KeyShift && !Busy();

    ImGui::BeginDisabled(Busy() || m_input.empty());
    const bool send = AccentButton("Send", ImVec2(ASSISTANT_SEND_WIDTH, 0.0f), &settings);
    ImGui::EndDisabled();

    // Square, matching the Send button's height so the two sit on one baseline.
    const float icon_size = ImGui::GetFrameHeight();
    ImGui::SameLine();
    ImGui::BeginDisabled(m_lines.empty() && m_input.empty());
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
