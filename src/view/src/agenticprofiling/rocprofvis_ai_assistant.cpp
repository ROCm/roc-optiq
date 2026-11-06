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
// ~AssistantPanel() destroys every conversation's TextSelects, which needs the
// complete type.
#include "textselect.hpp"

#include "rocprofvis_ai_prompts.h"
#include "rocprofvis_ai_tool_schema.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_project.h"
#include "rocprofvis_render_scheduler.h"
#include "rocprofvis_settings_manager.h"
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
, m_dock_width(ASSISTANT_DEFAULT_WIDTH)
, m_phase(Phase::kIdle)
, m_composer_height(0.0f)
, m_next_call_index(0)
, m_tool_round(0)
, m_force_final(false)
, m_stop_tool_queue(false)
, m_fetch_retries(0)
, m_tab_closed_token(EventManager::InvalidSubscriptionToken)
{
    m_widget_name = GenUniqueName("AssistantPanel");

    // A closed trace takes its conversation with it, and a turn pinned to it has
    // nothing left to read. Every tab bar sends this event for its own tabs, so
    // only the main one's count.
    m_tab_closed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTabClosed), [this](std::shared_ptr<RocEvent> event) {
            std::shared_ptr<TabEvent> tab_event =
                std::dynamic_pointer_cast<TabEvent>(event);
            AppWindow* app = AppWindow::GetInstance();
            if(tab_event != nullptr && app != nullptr &&
               tab_event->GetSourceId() == app->GetMainTabSourceName())
            {
                HandleTabClosed(tab_event->GetTabId());
            }
        });
}

// Aborts anything in flight, so quitting mid-answer does not wait on the network.
AssistantPanel::~AssistantPanel()
{
    CancelPendingRequest();
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabClosed),
                                             m_tab_closed_token);
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

AssistantPanel::Conversation&
AssistantPanel::ConversationFor(const std::string& project_id)
{
    std::unique_ptr<Conversation>& slot = m_conversations[project_id];
    if(slot == nullptr)
    {
        slot = std::make_unique<Conversation>();
    }
    return *slot;
}

AssistantPanel::Conversation&
AssistantPanel::TurnConversation()
{
    return ConversationFor(m_turn_project_id);
}

AssistantPanel::Conversation&
AssistantPanel::ViewConversation()
{
    return ConversationFor(m_view_project_id);
}

void
AssistantPanel::ResetHistory(Conversation& conversation)
{
    conversation.messages.clear();
    conversation.briefed = false;
    conversation.links_announced.clear();
}

void
AssistantPanel::HandleTabClosed(const std::string& project_id)
{
    if(Busy() && project_id == m_turn_project_id)
    {
        ResetTurn();
    }
    m_conversations.erase(project_id);
}

void
AssistantPanel::AppendLine(Speaker speaker, const std::string& text)
{
    Conversation& conversation = TurnConversation();
    ChatLine      line;
    line.speaker = speaker;
    line.text    = text;
    conversation.lines.push_back(line);
    conversation.scroll_to_bottom = true;
}

// Adds an activity strip for one track, or a whole trace when track_id is
// invalid. Deduplicated: the chart reads live model data, so a repeat would be
// a pixel-identical copy of one already in the transcript.
void
AssistantPanel::AppendChart(const std::string& project_id, uint64_t track_id)
{
    Conversation& conversation = TurnConversation();
    for(const ChatLine& existing : conversation.lines)
    {
        if(existing.speaker == Speaker::kChart && existing.track_id == track_id &&
           existing.project_id == project_id)
        {
            return;
        }
    }

    ChatLine line;
    line.speaker    = Speaker::kChart;
    line.track_id   = track_id;
    line.project_id = project_id;
    conversation.lines.push_back(line);
    conversation.scroll_to_bottom = true;
}

void
AssistantPanel::SetStatus(const std::string& text)
{
    m_status                            = text;
    TurnConversation().scroll_to_bottom = true;
}

AssistantToolContext
AssistantPanel::MakeToolContext() const
{
    return MakeAssistantToolContext(m_turn_project_id);
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
AssistantPanel::BuildUserPrompt(const std::string& question, bool include_briefing)
{
    const AssistantToolContext context      = MakeToolContext();
    Conversation&              conversation = TurnConversation();
    std::ostringstream         out;
    if(include_briefing)
    {
        out << "Briefing for the current Optiq view:\n";
        out << BuildAssistantBriefing(context);
        out << "\n";
    }

    // Links can change between turns, so the current set goes out with the
    // first message and with any message after a change. It travels in the
    // user message rather than the system prompt, which has to stay identical
    // from round to round for the endpoint's prompt cache to hold.
    const std::string links = BuildAssistantLinkBlock(context);
    if(include_briefing || links != conversation.links_announced)
    {
        if(!links.empty())
        {
            out << links << "\n";
        }
        else if(!include_briefing)
        {
            out << "linked_traces: none. The links listed earlier were removed.\n";
        }
        conversation.links_announced = links;
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
    if(m_fetch_wait.active && m_fetch_wait.owns_requests &&
       m_fetch_wait.fetch.kind != AssistantFetchKind::kScript)
    {
        const std::string          project_id = m_fetch_wait.fetch.project_id.empty()
                                                    ? m_turn_project_id
                                                    : m_fetch_wait.fetch.project_id;
        const AssistantToolContext context    = MakeAssistantToolContext(project_id);
        if(context.data_provider != nullptr)
        {
            for(uint64_t request_id : m_fetch_wait.request_ids)
            {
                context.data_provider->CancelRequest(request_id);
            }
        }
    }
    AbandonPendingRequest();
    m_phase           = Phase::kIdle;
    m_pending_calls.clear();
    m_next_call_index = 0;
    m_tool_round      = 0;
    m_force_final     = false;
    m_stop_tool_queue = false;
    m_fetch_retries   = 0;
    m_fetch_wait      = FetchWait();
    m_status.clear();
    m_queued_question.clear();
}

// Cancels without destroying the std::async future. A future's destructor may
// wait for the worker, so tab close and Clear hand it to Update() to reap.
void
AssistantPanel::AbandonPendingRequest()
{
    if(m_pending.valid())
    {
        AbandonedRequest abandoned;
        abandoned.call   = std::move(m_call);
        abandoned.result = std::move(m_pending);
        if(abandoned.call != nullptr)
        {
            abandoned.cancel = std::async(std::launch::async,
                                          [call = abandoned.call]() { call->Cancel(); });
        }
        m_abandoned.push_back(std::move(abandoned));
    }
    else if(m_call != nullptr)
    {
        // No async request owns this call, so stopping it cannot strand a
        // future whose destructor waits on the UI thread.
        m_call->Cancel();
    }
    m_call.reset();
}

void
AssistantPanel::ReapAbandonedRequests()
{
    for(std::vector<AbandonedRequest>::iterator it = m_abandoned.begin();
        it != m_abandoned.end();)
    {
        const bool cancel_ready =
            !it->cancel.valid() ||
            it->cancel.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        const bool result_ready =
            !it->result.valid() ||
            it->result.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        if(!cancel_ready || !result_ready)
        {
            RenderScheduler::GetInstance().RequestRender();
            ++it;
            continue;
        }
        if(it->cancel.valid())
        {
            it->cancel.get();
        }
        if(it->result.valid())
        {
            it->result.get();
        }
        it = m_abandoned.erase(it);
    }
}

// Final teardown may wait: every worker must finish before its owning panel is
// destroyed, and there is no UI thread left to keep responsive.
void
AssistantPanel::CancelPendingRequest()
{
    if(m_call != nullptr)
    {
        m_call->Cancel();
    }
    for(AbandonedRequest& abandoned : m_abandoned)
    {
        if(abandoned.call != nullptr && !abandoned.cancel.valid())
        {
            abandoned.call->Cancel();
        }
    }
    if(m_pending.valid())
    {
        m_pending.wait();
        m_pending = std::future<AssistantChatResult>();
    }
    for(AbandonedRequest& abandoned : m_abandoned)
    {
        if(abandoned.cancel.valid())
        {
            abandoned.cancel.wait();
        }
        if(abandoned.result.valid())
        {
            abandoned.result.wait();
        }
    }
    m_abandoned.clear();
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
    const std::vector<AssistantMessage>& history = TurnConversation().messages;
    request.messages.insert(request.messages.end(), history.begin(), history.end());

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
    TurnConversation().messages.push_back(user_message);
    m_queued_question.clear();
    StartHttpRequest();
}

// Preloads an empty summary so the briefing carries real numbers, not zeros.
// False when there is nothing to preload.
bool
AssistantPanel::TryStartSummaryWarmup(const std::string& question)
{
    const AssistantToolContext context = MakeToolContext();
    if(context.data_provider == nullptr || context.is_compute || context.is_compare)
    {
        return false;
    }
    if(context.data_provider->GetState() != ProviderState::kReady)
    {
        return false;
    }
    if(AssistantSummaryLoaded(context))
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

    // The turn belongs to the trace on screen when it is asked, and stays with
    // it whichever tab the user moves to while it runs.
    m_turn_project_id                       = m_view_project_id;
    Conversation&              conversation = TurnConversation();
    const AssistantToolContext context      = MakeToolContext();
    std::string                unavailable;
    if(context.data_provider == nullptr)
    {
        unavailable = "Open a system trace before asking Optiq about it.";
    }
    else if(context.is_compute)
    {
        unavailable = "Ask Optiq reads system traces, not compute traces.";
    }
    else if(context.is_compare)
    {
        unavailable =
            "Ask Optiq does not read a side-by-side compare tab, because its tools "
            "would add both runs together. Open the two files on their own and link "
            "them with the Link runs button instead.";
    }
    if(!unavailable.empty())
    {
        AppendLine(Speaker::kStatus, unavailable);
        NotificationManager::GetInstance().Show(unavailable, NotificationLevel::Warning);
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

    std::string question = conversation.input;
    if(!explain_view && question.empty())
    {
        return;
    }

    conversation.next_steps.clear();

    const std::string display =
        explain_view ? (question.empty() ? std::string("Explain this view") : question)
                     : question;
    AppendLine(Speaker::kUser, display);
    conversation.input.clear();

    if(explain_view)
    {
        ResetHistory(conversation);
        m_tool_round  = 0;
        m_force_final = false;
    }

    m_pending_calls.clear();
    m_next_call_index = 0;
    m_fetch_retries   = 0;
    m_fetch_wait      = FetchWait();
    if(TryStartSummaryWarmup(question))
    {
        return;
    }

    AssistantMessage user_message;
    user_message.role    = "user";
    user_message.content = BuildUserPrompt(question, NeedsBriefing());
    conversation.messages.push_back(user_message);
    StartHttpRequest();
}

/*
 * Whether this turn should carry the briefing again.
 *
 * The briefing is topology, selection and summary headline for the trace the
 * conversation belongs to. Repeating it on every follow-up costs its length once
 * per turn for the rest of the session, and puts several copies in context that
 * disagree about the selection - the model then has to guess which is current.
 * A conversation only ever covers one trace, so it is sent once, and again only
 * after the history was cleared.
 */
bool
AssistantPanel::NeedsBriefing()
{
    Conversation& conversation = TurnConversation();
    if(!conversation.messages.empty() && conversation.briefed)
    {
        return false;
    }
    conversation.briefed = true;
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
    std::vector<AssistantMessage>& history = TurnConversation().messages;
    if(history.size() > ASSISTANT_MAX_CONVERSATION_MESSAGES)
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
        for(size_t i = history.size(); i-- > 0;)
        {
            if(history[i].role == "user" && ++user_seen == wanted)
            {
                turn_start = i;
                break;
            }
        }

        size_t drop = history.size() - ASSISTANT_MAX_CONVERSATION_MESSAGES;
        while(drop < turn_start && history[drop].role != "user")
        {
            ++drop;
        }
        if(drop > 0 && drop < turn_start)
        {
            spdlog::info("Assistant history trimmed: dropping {} of {} messages", drop,
                         history.size());
            history.erase(history.begin(),
                          history.begin() + static_cast<ptrdiff_t>(drop));

            // The first user message carried the trace briefing and links. If
            // that exchange was one of the ones dropped, put a compact refresh
            // ahead of what remains so a later answer cannot attribute old
            // rows to an unidentified trace.
            const AssistantToolContext context = MakeToolContext();
            AssistantMessage           refresh;
            refresh.role    = "user";
            refresh.content = "Refreshed context for the retained conversation:\n" +
                              BuildAssistantBriefing(context);
            const std::string links = BuildAssistantLinkBlock(context);
            if(!links.empty())
            {
                refresh.content += "\n" + links;
            }
            history.insert(history.begin(), refresh);
            Conversation& conversation   = TurnConversation();
            conversation.briefed         = true;
            conversation.links_announced = links;
        }
    }

    size_t total = 0;
    for(const AssistantMessage& message : history)
    {
        total += MessageChars(message);
    }
    if(total <= ASSISTANT_MAX_CONVERSATION_CHARS)
    {
        return;
    }

    // Walk back over the newest replies to find where the protected tail
    // starts, so compaction can then run oldest-first.
    size_t protected_from = history.size();
    size_t recent         = 0;
    while(protected_from > 0 && recent < ASSISTANT_RECENT_TOOL_REPLIES)
    {
        --protected_from;
        if(history[protected_from].role == "tool")
        {
            ++recent;
        }
    }

    const std::string note      = ASSISTANT_COMPACTED_TOOL_REPLY;
    size_t            compacted = 0;
    for(size_t i = 0; i < protected_from && total > ASSISTANT_MAX_CONVERSATION_CHARS; ++i)
    {
        AssistantMessage& message = history[i];
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
        TurnConversation().next_steps.clear();
        return;
    }

    if(!result.ok)
    {
        ResetTurn();
        TurnConversation().next_steps.clear();
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
    TurnConversation().messages.push_back(assistant_message);
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
    TurnConversation().messages.push_back(assistant_message);

    m_pending_calls   = calls;
    m_next_call_index = 0;
    m_fetch_retries   = 0;
    m_stop_tool_queue = false;
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
    TurnConversation().messages.push_back(nudge);
    m_force_final = true;
    SetStatus("Working out what it means...");
    StartHttpRequest();
}

// Tools run against the turn's own trace, never whichever tab is in front, so
// a tab switch mid-queue cannot mix two traces' numbers.
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
        // The trace the tool read, which a linked read makes another one.
        AppendChart(started.fetch.project_id.empty() ? m_turn_project_id
                                                     : started.fetch.project_id,
                    started.chart_track_id);
    }
    if(started.set_next_steps)
    {
        TurnConversation().next_steps = started.next_steps;
    }
    if(started.stop_tool_queue)
    {
        m_stop_tool_queue = true;
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
    m_fetch_wait.owns_requests   = started.owns_requests;
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

    TurnConversation().messages.push_back(tool_message);

    m_fetch_wait    = FetchWait();
    m_fetch_retries = 0;
    ++m_next_call_index;
    if(m_stop_tool_queue)
    {
        while(m_next_call_index < m_pending_calls.size())
        {
            const AssistantToolCall& skipped = m_pending_calls[m_next_call_index];
            AssistantMessage         skipped_message;
            skipped_message.role         = "tool";
            skipped_message.name         = skipped.name;
            skipped_message.tool_call_id = skipped.id;
            skipped_message.content =
                "Not run because switch_tab moved the user to another trace. This "
                "conversation stays with its original trace.";
            TurnConversation().messages.push_back(skipped_message);
            ++m_next_call_index;
        }
        m_stop_tool_queue = false;
    }
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

    // The provider the requests went to: a linked trace's when the tool read
    // one, the turn's own otherwise. Request ids repeat in every provider, so
    // polling any other would report someone else's query as this one's.
    const std::string          fetch_project_id = m_fetch_wait.fetch.project_id.empty()
                                                      ? m_turn_project_id
                                                      : m_fetch_wait.fetch.project_id;
    const AssistantToolContext context = MakeAssistantToolContext(fetch_project_id);
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
        FinishCurrentTool(fetch_project_id == m_turn_project_id
                              ? "The trace closed while a tool was running."
                              : "The linked trace closed before its data arrived, so "
                                "nothing was read from it.");
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
    // whether it landed or timed out.
    if(m_fetch_wait.warmup)
    {
        m_fetch_wait = FetchWait();
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
    ReapAbandonedRequests();
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
