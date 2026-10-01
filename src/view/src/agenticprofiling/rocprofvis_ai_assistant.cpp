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

#include "icons/rocprovfis_icon_defines.h"
#include "model/rocprofvis_summary_model.h"
#include "rocprofvis_ai_prompts.h"
#include "rocprofvis_ai_tool_schema.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_project.h"
#include "rocprofvis_render_scheduler.h"
#include "rocprofvis_root_view.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_trace_view.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"

namespace RocProfVis
{
namespace View
{

namespace
{

// Only the starting width lives here, because the constructor seeds the member
// with it. The rest of the panel geometry belongs to the drawing half, in
// rocprofvis_ai_assistant_render.cpp.
constexpr float  ASSISTANT_DEFAULT_WIDTH  = 520.0f;
// Covers a whole self-directed investigation, not a single lookup.
constexpr uint32_t ASSISTANT_MAX_TOOL_ROUNDS = 20;
// How much of the conversation travels upstream. Every round re-sends all of
// it, so this bounds what a long session costs per round instead of letting the
// bill grow with the square of its length. Comfortably more than one full tool
// budget, so a single turn is never cut short by it.
constexpr size_t   ASSISTANT_MAX_CONVERSATION_MESSAGES = 60;
// The same bill measured the way it is actually charged. A message count
// cannot bound one investigation: that is a single question followed by round
// after round of tool replies, so it never reaches a second user message to cut
// on, and one reply alone may be ASSISTANT_MAX_RESULT_CHARS wide. Roughly four
// characters to the token.
constexpr size_t   ASSISTANT_MAX_CONVERSATION_CHARS = 120000;
// Tool replies newer than this are never compacted, however long the history
// has run. These are the rows the answer is about to be written from.
constexpr size_t   ASSISTANT_RECENT_TOOL_REPLIES = 8;
// Stands in for a tool reply dropped from the history. Says plainly that the
// rows can be had again, so a model that still needs them asks rather than
// invents.
constexpr const char* ASSISTANT_COMPACTED_TOOL_REPLY =
    "(earlier result omitted to save room - call this tool again if you still "
    "need these rows)";
// How long a tool waits for its rows. Sized for a query over a large trace
// rather than a small one: cutting a live query off and telling the model it
// timed out is worse than making the user wait, because the model answers
// around the gap instead of reporting it. Past this the user is better served
// by being told the query is too broad than by a spinner, and Clear cancels it
// at any point.
constexpr int      ASSISTANT_FETCH_TIMEOUT_SECONDS = 350;
// When a fetch passes this, the status starts counting up. Long enough that an
// ordinary query never shows a timer, short enough that a slow one is visibly
// working rather than stuck.
constexpr int64_t  ASSISTANT_SLOW_FETCH_NOTICE_SECONDS = 5;

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
, m_next_call_index(0)
, m_tool_round(0)
, m_force_final(false)
, m_fetch_retries(0)
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
AssistantPanel::SetStatus(const std::string& text)
{
    m_status           = text;
    m_scroll_to_bottom = true;
}

// Resolves what the tools may touch, from whichever trace is in front. Rebuilt
// per call, so a closed tab cannot leave a tool holding a dead provider.
AssistantToolContext
AssistantPanel::MakeToolContext() const
{
    AssistantToolContext context;

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
    context.is_compute = project->GetTraceType() == Project::Compute;
    RootView* root_view = dynamic_cast<RootView*>(project->GetView().get());
    if(root_view != nullptr)
    {
        context.data_provider = root_view->GetDataProvider();
    }
    if(project->GetTraceType() == Project::System)
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

    TrimConversation();

    AssistantMessage system_message;
    system_message.role    = "system";
    system_message.content = AssistantSystemPrompt();
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
    user_message.content = BuildUserPrompt(m_queued_question, NeedsBriefing());
    m_conversation.push_back(user_message);
    m_queued_question.clear();
    StartHttpRequest();
}

// Preloads an empty summary so the briefing carries real numbers, not zeros.
// False when there is nothing to preload.
bool
AssistantPanel::TryStartSummaryWarmup(const std::string& question)
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

    m_queued_question = question;
    BeginFetchWait(started, "get_summary", true);
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
    if(TryStartSummaryWarmup(question))
    {
        return;
    }

    AssistantMessage user_message;
    user_message.role    = "user";
    user_message.content = BuildUserPrompt(question, NeedsBriefing());
    m_conversation.push_back(user_message);
    StartHttpRequest();
}

/*
 * Whether this turn should carry the briefing again.
 *
 * The briefing is topology, selection and summary headline for the trace in
 * front. Repeating it on every follow-up costs its length once per turn for the
 * rest of the session, and puts several copies in context that disagree about
 * the selection - the model then has to guess which is current. Send it when
 * there is no conversation to refer back to, or when the trace it described is
 * no longer the one being asked about.
 */
bool
AssistantPanel::NeedsBriefing()
{
    const std::string project_id = CurrentProjectId();
    if(!m_conversation.empty() && project_id == m_briefed_project_id)
    {
        return false;
    }
    m_briefed_project_id = project_id;
    return true;
}

namespace
{

// What one message costs to send, near enough to budget with.
size_t
MessageChars(const AssistantMessage& message)
{
    size_t chars = message.role.size() + message.name.size() + message.content.size();
    for(const AssistantToolCall& call : message.tool_calls)
    {
        chars += call.name.size() + call.arguments.size();
    }
    return chars;
}

}  // namespace

/*
 * Bounds what the transcript costs to re-send, in two passes.
 *
 * Every round re-sends the whole conversation, so an investigation that runs
 * for many tool rounds pays for its own history again each time.
 *
 * Whole earlier exchanges go first, which is the cheapest thing to lose. Those
 * cuts land only on a user message: an assistant message carrying tool_calls
 * and the tool replies answering it have to travel together, and an orphan of
 * either kind is rejected by the endpoint. They also stop at the start of the
 * live turn, whose replies are the evidence the answer is about to be written
 * from. If no safe cut exists the history is left alone - growing is better
 * than sending something malformed, or something gutted.
 *
 * That pass alone cannot bound one investigation, which is a single question
 * followed by round after round of tool replies and so never reaches a second
 * user message to cut on. The second pass shrinks the oldest tool replies in
 * place instead: the rounds that followed have already read those numbers, and
 * keeping the message keeps its tool_call_id paired with the call that asked
 * for it, which is what dropping it outright would break.
 */
void
AssistantPanel::TrimConversation()
{
    if(m_conversation.size() > ASSISTANT_MAX_CONVERSATION_MESSAGES)
    {
        // Where the live investigation begins. Everything from here on is what
        // the answer is about to be written from, so the drop pass may not
        // reach into it however long it has run - only whole earlier exchanges
        // are cheap enough to lose.
        //
        // Finding it means the last user message, except on the final round:
        // BeginFinalAnswer appends a "write it up now" nudge in the user role,
        // and that nudge is the only later user message a single investigation
        // ever produces. Cutting to it dropped the question and every tool
        // reply, and asked the model to write up numbers that were no longer
        // there - which it rightly refused to do.
        size_t       turn_start = 0;
        size_t       user_seen  = 0;
        const size_t wanted     = m_force_final ? 2 : 1;
        for(size_t i = m_conversation.size(); i-- > 0;)
        {
            if(m_conversation[i].role == "user" && ++user_seen == wanted)
            {
                turn_start = i;
                break;
            }
        }

        size_t drop = m_conversation.size() - ASSISTANT_MAX_CONVERSATION_MESSAGES;
        while(drop < turn_start && m_conversation[drop].role != "user")
        {
            ++drop;
        }
        if(drop > 0 && drop < turn_start)
        {
            spdlog::info("Assistant history trimmed: dropping {} of {} messages", drop,
                         m_conversation.size());
            m_conversation.erase(m_conversation.begin(),
                                 m_conversation.begin() + static_cast<ptrdiff_t>(drop));
        }
    }

    size_t total = 0;
    for(const AssistantMessage& message : m_conversation)
    {
        total += MessageChars(message);
    }
    if(total <= ASSISTANT_MAX_CONVERSATION_CHARS)
    {
        return;
    }

    // Walk back over the newest replies to find where the protected tail
    // starts, so compaction can then run oldest-first.
    size_t protected_from = m_conversation.size();
    size_t recent         = 0;
    while(protected_from > 0 && recent < ASSISTANT_RECENT_TOOL_REPLIES)
    {
        --protected_from;
        if(m_conversation[protected_from].role == "tool")
        {
            ++recent;
        }
    }

    const std::string note      = ASSISTANT_COMPACTED_TOOL_REPLY;
    size_t            compacted = 0;
    for(size_t i = 0; i < protected_from && total > ASSISTANT_MAX_CONVERSATION_CHARS; ++i)
    {
        AssistantMessage& message = m_conversation[i];
        if(message.role != "tool" || message.content.size() <= note.size())
        {
            continue;
        }
        total -= message.content.size() - note.size();
        message.content = note;
        ++compacted;
    }
    if(compacted > 0)
    {
        spdlog::info("Assistant history compacted: {} older tool replies, {} chars left",
                     compacted, total);
    }
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
        BeginFetchWait(started, call.name, false);
        return;
    }

    FinishCurrentTool(started.content);
}

void
AssistantPanel::BeginFetchWait(const AssistantToolStartResult& started,
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

    // The whole exchange, verbatim, so an answer can be checked against what
    // the model was actually given rather than against what it says it read.
    // Nothing else records this: the transcript shows only the final prose, and
    // these rows are dropped when the turn ends. At debug level, so a normal
    // session does not pay for it - raise the log level to inspect a turn.
    spdlog::debug("Assistant tool {} args: {}", call.name,
                  call.arguments.empty() ? "{}" : call.arguments);
    spdlog::debug("Assistant tool {} result:\n{}", call.name, tool_message.content);

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
    // Nothing was queried yet: this tool is waiting on the trace itself, so the
    // provider's own state is what says whether the wait is over.
    if(m_fetch_wait.fetch.kind == AssistantFetchKind::kTraceLoading)
    {
        return context.data_provider->GetState() != ProviderState::kReady;
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
        // Say how long, so a slow query reads as working rather than frozen.
        if(elapsed.count() >= ASSISTANT_SLOW_FETCH_NOTICE_SECONDS &&
           !m_fetch_wait.tool_name.empty())
        {
            SetStatus(AssistantToolStatusLabel(m_fetch_wait.tool_name) + " (" +
                      std::to_string(elapsed.count()) + "s)");
        }
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

    // The trace never finished opening. Say that rather than naming the tool,
    // which did not get as far as running.
    if(pending && m_fetch_wait.fetch.kind == AssistantFetchKind::kTraceLoading)
    {
        FinishCurrentTool("The trace is still loading after a long wait, so "
                          "nothing could be read. Tell the user the trace is "
                          "still opening rather than answering without it.");
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

    // The trace is ready now, so run the tool that was parked before it could
    // query anything. The clock starts again for the query: retries are zeroed
    // so BeginFetchWait stamps a fresh deadline, because waiting out a long
    // load must not leave the query it was blocking with no time to run.
    if(m_fetch_wait.fetch.kind == AssistantFetchKind::kTraceLoading)
    {
        m_fetch_retries     = 0;
        m_fetch_wait        = FetchWait();
        RunNextTool();
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

}  // namespace View
}  // namespace RocProfVis
