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
#include <utility>
#include <vector>

#include "imgui.h"
#include "spdlog/spdlog.h"
// ~AssistantPanel() destroys every conversation's TextSelects, which needs the
// complete type.
#include "textselect.hpp"

#include "rocprofvis_ai_actions.h"
#include "rocprofvis_ai_prompts.h"
#include "rocprofvis_ai_tool_schema.h"
#include "rocprofvis_ai_tools_internal.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_project.h"
#include "rocprofvis_render_scheduler.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
#    include "widgets/rocprofvis_script_editor.h"
#endif

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
// on, and one reply alone may be ASSISTANT_MAX_RESULT_CHARS wide.
constexpr size_t   ASSISTANT_MAX_CONVERSATION_CHARS = 120000;
// The ratio the history budget was sized with, and the one the context pill
// uses to turn characters into tokens.
constexpr size_t ASSISTANT_CHARS_PER_TOKEN = 4;
// Tool replies newer than this are never compacted, however long the history
// has run. These are the rows the answer is about to be written from.
constexpr size_t   ASSISTANT_RECENT_TOOL_REPLIES = 8;
// Replies this small are spared until the large ones are gone: they save
// little, and are the overview and summary every later claim leans on.
constexpr size_t   ASSISTANT_COMPACT_SMALL_CHARS = 4000;
// Stands in for a tool reply dropped from the history. Says plainly that the
// rows can be had again, so a model that still needs them asks rather than
// invents.
constexpr const char* ASSISTANT_COMPACTED_TOOL_REPLY =
    "(earlier result omitted to save room - call this tool again if you still "
    "need these rows)";
// Answers a queued call that never ran because switch_tab, earlier in the same
// round, moved the user to another trace.
constexpr const char* ASSISTANT_SKIPPED_TOOL_REPLY =
    "Not run because switch_tab moved the user to another trace. This "
    "conversation stays with its original trace.";
// Answers the call in flight, and those queued behind it, when the user stops
// the turn. The endpoint rejects a history in which a call has no reply.
constexpr const char* ASSISTANT_STOPPED_TOOL_REPLY =
    "Not finished: the user stopped this turn.";
// Closes a stopped turn in the history, so the next question is not read as a
// follow-up to one the user gave up on.
constexpr const char* ASSISTANT_STOPPED_REPLY =
    "(The user stopped this turn before an answer was written.)";
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
// How often a read asks again after the UI dropped what it was waiting for - a
// range change voiding track statistics, a change of selection clearing an
// event - so one that keeps being dropped answers well before the deadline.
constexpr uint32_t ASSISTANT_MAX_DROPPED_RERUNS = 4;

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
, m_turn_is_compute(false)
, m_tab_closed_token(EventManager::InvalidSubscriptionToken)
, m_summary_event_token(EventManager::InvalidSubscriptionToken)
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

    m_summary_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kSummaryComplete),
        [this](std::shared_ptr<RocEvent> event) {
            std::shared_ptr<SummaryEvent> summary_event =
                std::dynamic_pointer_cast<SummaryEvent>(event);
            if(summary_event != nullptr)
            {
                HandleSummaryEvent(*summary_event);
            }
        });
}

// Aborts anything in flight, so quitting mid-answer does not wait on the network.
AssistantPanel::~AssistantPanel()
{
    CancelPendingRequest();
    AssistantWipeSecret(m_turn_token);
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabClosed),
                                             m_tab_closed_token);
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kSummaryComplete),
                                             m_summary_event_token);
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
AssistantPanel::HandleSummaryEvent(const SummaryEvent& event)
{
    if(!m_fetch_wait.active)
    {
        return;
    }
    const AssistantFetchKind kind = m_fetch_wait.fetch.kind;
    if(kind != AssistantFetchKind::kSummary && kind != AssistantFetchKind::kCompareTraces)
    {
        return;
    }
    const std::string& waited =
        m_fetch_wait.fetch.project_id.empty() ? m_turn_project_id
                                              : m_fetch_wait.fetch.project_id;
    if(event.GetSourceId() != waited)
    {
        return;
    }
    m_fetch_wait.summary_settled   = true;
    m_fetch_wait.summary_succeeded = event.Succeeded();
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

// The kind moves only here, together with the project, so the prompt and
// schema a round goes out with always match the tools the dispatcher runs.
// Without compute support every turn is a system turn, and a compute trace is
// turned away before it is asked anything.
void
AssistantPanel::PinTurnTrace()
{
    m_turn_project_id = m_view_project_id;
#ifdef ROCPROFVIS_ENABLE_AGENTIC_COMPUTE
    m_turn_is_compute = MakeToolContext().is_compute;
#endif
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
    else if(m_turn_is_compute)
    {
        // Names the free tools, so the button never spends a metric query the
        // user did not ask for. kernel_roofline follows compute_overview
        // because, with several kernels, it needs the id that call hands back.
        // Keyed on the pinned kind rather than the trace's, because these are
        // the names in the schema the turn goes out with.
        out << "Explain this view. Call compute_overview, then kernel_roofline with "
               "the kernel_id of the kernel that owns the most time, so the "
               "explanation uses real kernel names and numbers. Stay on the tools "
               "that need no query, and offer the deeper dives as next steps.\n";
    }
    else
    {
        out << "Explain this view. Call trace_overview and get_summary so the "
               "explanation uses real kernel names and numbers, keep it to the "
               "overview pass, and offer the deeper dives as next steps.\n";
    }
    return out.str();
}

// Lets go of the script a turn was waiting on when it ends with nobody left to
// hear the outcome. The offer sits on the turn's own trace, whichever tab is
// in front.
void
AssistantPanel::WithdrawScriptOffer()
{
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    if(!m_fetch_wait.active || m_fetch_wait.fetch.kind != AssistantFetchKind::kScript)
    {
        return;
    }
    const AssistantToolContext context = MakeToolContext();
    OptiqActions               actions(context.data_provider, context.timeline_selection,
                                       context.trace_view);
    if(actions.ScriptProposalState() == ScriptApproval::kPending)
    {
        // Declined rather than cleared, so the editor does not go on to present
        // the unapproved script as the user's own.
        actions.RejectScriptProposal();
    }
    else
    {
        // Already answered. A run the user approved carries on as their own,
        // so the editor must not report it as never run.
        actions.ClearScriptProposal();
    }
#endif
}

// Ends the running turn and keeps the conversation, which Clear does not.
void
AssistantPanel::StopTurn()
{
    if(!Busy())
    {
        return;
    }
    // A turn stopped while its summary preload ran never sent its question.
    const bool asked = !m_fetch_wait.warmup;
    // Only a parked tool leaves calls unanswered: a request goes out after
    // every call of its round has a reply.
    if(m_phase == Phase::kToolWait)
    {
        for(size_t i = m_next_call_index; i < m_pending_calls.size(); ++i)
        {
            AppendToolReply(m_pending_calls[i], ASSISTANT_STOPPED_TOOL_REPLY);
        }
    }
    ResetTurn();
    Conversation& conversation = TurnConversation();
    conversation.next_steps.clear();
    if(asked)
    {
        AssistantMessage stopped;
        stopped.role    = "assistant";
        stopped.content = ASSISTANT_STOPPED_REPLY;
        conversation.messages.push_back(stopped);
    }
    AppendLine(Speaker::kStatus, "Stopped before an answer was written.");
}

// Cancels the requests the parked tool started itself, on the provider they
// went to. A wait behind someone else's request leaves that request alone.
void
AssistantPanel::CancelOwnedFetch()
{
    if(!m_fetch_wait.active || !m_fetch_wait.owns_requests ||
       m_fetch_wait.fetch.kind == AssistantFetchKind::kScript)
    {
        return;
    }
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
    // The track's pill shares this slot and may by now own the request under
    // the same id, so the cancel must not leave it waiting.
    if(m_fetch_wait.fetch.kind == AssistantFetchKind::kTrackStatistics)
    {
        ReleaseAssistantTrackStatistics(context, m_fetch_wait.fetch.track_id);
    }
}

void
AssistantPanel::ResetTurn()
{
    WithdrawScriptOffer();
    CancelOwnedFetch();
    AbandonPendingRequest();
    ResetAssistantPcRead();
    ResetAssistantScriptOffers();
    // A request still unwinding reads its own copy of these, never the members.
    m_turn_endpoint_url.clear();
    m_turn_model.clear();
    AssistantWipeSecret(m_turn_token);
    // Left in, the write-up nudge tells every later turn not to call tools.
    if(m_force_final)
    {
        std::vector<AssistantMessage>& history = TurnConversation().messages;
        if(!history.empty() && history.back().role == "user")
        {
            history.pop_back();
        }
    }
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
// Cancel() itself waits out a connect in progress, so it gets a thread too.
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
    // Every round goes to the endpoint SendCurrentInput copied, never to the
    // live settings, which Preferences edits in place: a later round would
    // otherwise post the key to a half-typed URL.
    if(m_turn_endpoint_url.empty())
    {
        ResetTurn();
        AppendLine(Speaker::kStatus,
                   "Set the URL, model, and key in Edit > Preferences > Assistant.");
        return;
    }

    AssistantChatRequest request;
    request.endpoint_url  = m_turn_endpoint_url;
    request.model         = m_turn_model;
    request.api_token     = m_turn_token;
    request.enable_tools  = !m_force_final;
    request.compute_tools = m_turn_is_compute;

    TrimConversation();

    AssistantMessage system_message;
    system_message.role    = "system";
    system_message.content = AssistantSystemPrompt(m_turn_is_compute);
    request.messages.push_back(system_message);
    const std::vector<AssistantMessage>& history = TurnConversation().messages;
    request.messages.insert(request.messages.end(), history.begin(), history.end());

    m_phase = Phase::kHttpWait;
    SetStatus("Thinking...");
    spdlog::info("Assistant HTTP round {} ({} messages)", m_tool_round,
                 request.messages.size());

    // The worker owns the request's copy of the key and wipes it after the reply.
    m_call    = std::make_shared<AssistantChatCall>();
    m_pending = std::async(std::launch::async,
                           [call = m_call, request = std::move(request)]() mutable {
                               AssistantChatResult result = call->Send(request);
                               AssistantWipeSecret(request.api_token);
                               return result;
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
//
// A compute trace needs no warmup at all: its kernel list and metric catalogue
// are loaded with the file, so the compute briefing already has real counts to
// report and compute_overview answers without a query.
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
    PinTurnTrace();
    Conversation&              conversation = TurnConversation();
    const AssistantToolContext context      = MakeToolContext();
    std::string                unavailable;
    if(context.data_provider == nullptr)
    {
        unavailable = "Open a trace before asking Optiq about it.";
    }
#ifndef ROCPROFVIS_ENABLE_AGENTIC_COMPUTE
    else if(context.is_compute)
    {
        unavailable = "This build of Ask Optiq reads system traces, not compute traces.";
    }
#endif
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

    // Fixes where this turn's requests go, and with which key. Taken here so
    // it also covers a turn that waits on the summary warmup first. Nothing
    // below returns without starting the turn, so ResetTurn wipes this copy.
    AssistantProvider endpoint = *provider;
    ApplyAssistantEndpointDefaults(endpoint);
    m_turn_endpoint_url = endpoint.endpoint_url;
    m_turn_model        = endpoint.model;
    settings.GetAssistantToken(endpoint.name, m_turn_token);
    // A key pasted into the question would sit in the transcript, and from
    // there go into an export or onto the clipboard.
    AssistantRedactKey(m_turn_token, question);

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
    // Starts this turn's allowance of pinned notes.
    BeginAssistantUiTurn();

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

size_t
TokensFromChars(size_t chars)
{
    return (chars + ASSISTANT_CHARS_PER_TOKEN - 1) / ASSISTANT_CHARS_PER_TOKEN;
}

// The standing prompt and the tool schema do not change for a trace kind, so
// their sizes are measured once. Rebuilding either on every frame would copy
// the whole prompt just to count it.
size_t
StandingPromptChars(bool is_compute)
{
    static size_t chars[2] = { 0, 0 };
    static bool   ready[2] = { false, false };
    const size_t  index    = is_compute ? 1 : 0;
    if(!ready[index])
    {
        chars[index] = AssistantSystemPrompt(is_compute).size();
        ready[index] = true;
    }
    return chars[index];
}

size_t
ToolSchemaChars(bool is_compute)
{
    static size_t chars[2] = { 0, 0 };
    static bool   ready[2] = { false, false };
    const size_t  index    = is_compute ? 1 : 0;
    if(!ready[index])
    {
        chars[index] = BuildAssistantToolsJson(is_compute).toString().size();
        ready[index] = true;
    }
    return chars[index];
}

}  // namespace

AssistantPanel::ContextLoad
AssistantPanel::MeasureContext() const
{
    // The pill follows the conversation on screen, even while another trace's
    // turn is running. Only that conversation's active write-up omits tools.
    const bool viewing_turn =
        Busy() && m_view_project_id == m_turn_project_id;
    const bool is_compute =
        viewing_turn ? m_turn_is_compute
                     : MakeAssistantToolContext(m_view_project_id).is_compute;
    // The write-up round goes out with the schema omitted. Every other round,
    // including the one a new question is about to start, carries it.
    const bool   include_tools  = !viewing_turn || !m_force_final;
    const size_t overhead_chars = StandingPromptChars(is_compute) +
                                  (include_tools ? ToolSchemaChars(is_compute) : 0);

    ContextLoad load;
    load.history_budget_chars = ASSISTANT_MAX_CONVERSATION_CHARS;
    const std::unordered_map<std::string, std::unique_ptr<Conversation>>::const_iterator
        conversation = m_conversations.find(m_view_project_id);
    if(conversation != m_conversations.end())
    {
        for(const AssistantMessage& message : conversation->second->messages)
        {
            load.history_chars += MessageChars(message);
        }
    }
    load.history_tokens        = TokensFromChars(load.history_chars);
    load.history_budget_tokens = TokensFromChars(load.history_budget_chars);
    load.overhead_tokens       = TokensFromChars(overhead_chars);
    return load;
}

void
AssistantPanel::NoteTokenUsage(const AssistantChatResult& result)
{
    if(!result.has_usage)
    {
        return;
    }
    Conversation& conversation              = TurnConversation();
    conversation.usage_reported             = true;
    conversation.last_prompt_tokens         = result.prompt_tokens;
    conversation.last_completion_tokens     = result.completion_tokens;
    conversation.last_cached_known          = result.has_cached_tokens;
    conversation.last_cached_tokens         = result.cached_tokens;
    conversation.session_prompt_tokens     += result.prompt_tokens;
    conversation.session_completion_tokens += result.completion_tokens;
    ++conversation.session_rounds;
}

void
AssistantPanel::ClearTokenUsage(Conversation& conversation)
{
    conversation.usage_reported            = false;
    conversation.last_prompt_tokens        = 0;
    conversation.last_completion_tokens    = 0;
    conversation.last_cached_known         = false;
    conversation.last_cached_tokens        = 0;
    conversation.session_prompt_tokens     = 0;
    conversation.session_completion_tokens = 0;
    conversation.session_rounds            = 0;
}

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
    // Large replies first, then every reply the note is shorter than.
    const size_t tiers[] = { ASSISTANT_COMPACT_SMALL_CHARS, note.size() };
    for(const size_t spared_up_to : tiers)
    {
        for(size_t i = 0;
            i < protected_from && total > ASSISTANT_MAX_CONVERSATION_CHARS; ++i)
        {
            AssistantMessage& message = history[i];
            if(message.role != "tool" || message.content.size() <= spared_up_to)
            {
                continue;
            }
            total -= message.content.size() - note.size();
            message.content = note;
            ++compacted;
        }
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
    // A length-limit error can still name what was spent. Record it before any
    // path below resets the turn.
    NoteTokenUsage(result);

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
    // Say so, or a write-up that stops short reads as the whole thing.
    if(result.finish_reason == "length")
    {
        AppendLine(Speaker::kStatus,
                   "The answer was cut off at the model's output limit. Ask it to "
                   "continue, or ask something narrower.");
    }

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
    // A re-run is the same call still waiting, so only the first run is logged
    // and scrolls the transcript down to the status.
    m_status = AssistantToolStatusLabel(call.name);
    if(m_fetch_retries == 0)
    {
        TurnConversation().scroll_to_bottom = true;
        spdlog::info("Assistant tool {} ({})", call.name, call.id);
    }

    const AssistantToolStartResult started =
        StartAssistantTool(MakeToolContext(), call.name, call.arguments);
    if(!started.status_line.empty())
    {
        m_status = started.status_line;
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
        // Called alone, it is the model's last step before writing, and the
        // prose of a tools-on round is discarded anyway (see HandleHttpResult),
        // so go straight to the tools-off round. With sibling calls the model
        // has not yet read their results. ResetTurn clears the queue.
        if(m_pending_calls.size() == 1)
        {
            AppendToolReply(call, started.content);
            BeginFinalAnswer();
            return;
        }
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
    m_fetch_wait.stats_dropped = false;
    m_fetch_wait.owns_requests = started.owns_requests;
    m_fetch_wait.warmup        = warmup;
    m_fetch_wait.request_ids   = started.request_ids;
    m_fetch_wait.fetch         = started.fetch;
    m_fetch_wait.tool_name     = tool_name;
    m_fetch_wait.prefix        = started.content;
    m_fetch_wait.status        = started.status_line.empty()
                                     ? AssistantToolStatusLabel(tool_name)
                                     : started.status_line;
    m_fetch_wait.started       = started_at;
    m_fetch_wait.timeout_seconds = started.timeout_seconds;
}

// Hands one tool's output back to the model and moves on to the next. Warmup
// never arrives here: it has no tool call to answer, so PollToolFetch takes
// every exit itself rather than let an empty queue look like "tools done".
void
AssistantPanel::FinishCurrentTool(const std::string& content)
{
    // kernel_pc_samples re-enters through RunNextTool, never here, so this runs
    // only once its call is over.
    ResetAssistantPcRead();

    if(m_next_call_index >= m_pending_calls.size())
    {
        ContinueAfterTools();
        return;
    }
    AppendToolReply(m_pending_calls[m_next_call_index], content);

    m_fetch_wait    = FetchWait();
    m_fetch_retries = 0;
    ++m_next_call_index;
    // The endpoint still needs a reply to every call, so the rest of a batch
    // that switch_tab ended is answered without running.
    if(m_stop_tool_queue)
    {
        while(m_next_call_index < m_pending_calls.size())
        {
            AppendToolReply(m_pending_calls[m_next_call_index],
                            ASSISTANT_SKIPPED_TOOL_REPLY);
            ++m_next_call_index;
        }
        m_stop_tool_queue = false;
    }
    RunNextTool();
}

void
AssistantPanel::AppendToolReply(const AssistantToolCall& call, const std::string& content)
{
    AssistantMessage tool_message;
    tool_message.role         = "tool";
    tool_message.name         = call.name;
    tool_message.tool_call_id = call.id;
    tool_message.content      = content.empty() ? "(empty)" : content;
    // A script the user approved runs unsandboxed and can print the key, and a
    // trace can have recorded it. Blanked before the reply is logged or kept,
    // the way the client blanks one a provider echoes.
    AssistantRedactKey(m_turn_token, tool_message.content);

    // The whole exchange, as the model was given it, so an answer can be
    // checked against what it actually read rather than what it says it read.
    // Nothing else records this: the transcript shows only the final prose, and
    // these rows are dropped when the turn ends. At debug level, so a normal
    // session does not pay for it - raise the log level to inspect a turn.
    spdlog::debug("Assistant tool {} args: {}", call.name,
                  call.arguments.empty() ? "{}" : call.arguments);
    spdlog::debug("Assistant tool {} result:\n{}", call.name, tool_message.content);

    TurnConversation().messages.push_back(tool_message);
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
    // Refused because the UI was loading a table of the same type. That load is
    // what has to finish: the tool's own request id is idle, and polling it
    // would re-run the tool every frame.
    if(m_fetch_wait.fetch.waits_for_ui_table &&
       context.data_provider->IsTableRequestPending(m_fetch_wait.fetch.ui_table_type))
    {
        return true;
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

    // A range change voids a statistics read in flight, and a drawn track's
    // pill then replaces it under the same request id. Noted while it shows,
    // or the pill's numbers land as this tool's.
    if(m_fetch_wait.fetch.kind == AssistantFetchKind::kTrackStatistics &&
       AssistantTrackStatisticsDropped(context, m_fetch_wait.fetch.track_id, pending))
    {
        m_fetch_wait.stats_dropped = true;
    }

    if(pending && !timed_out)
    {
        // Say how long, so a slow query reads as working rather than frozen.
        // Assigned directly: every frame, SetStatus would pin the transcript
        // to the bottom for the whole wait.
        if(elapsed.count() >= ASSISTANT_SLOW_FETCH_NOTICE_SECONDS &&
           !m_fetch_wait.tool_name.empty())
        {
            m_status =
                m_fetch_wait.status + " (" + std::to_string(elapsed.count()) + "s)";
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

    // The summary request this wait was parked on finished without writing one.
    // Reporting it here stops compare_traces from requesting that same summary
    // again on the re-entry that follows a completed wait.
    if(m_fetch_wait.summary_settled && !m_fetch_wait.summary_succeeded &&
       (m_fetch_wait.fetch.kind == AssistantFetchKind::kSummary ||
        m_fetch_wait.fetch.kind == AssistantFetchKind::kCompareTraces))
    {
        const std::string name =
            context.trace_name.empty() ? std::string("this trace") : context.trace_name;
        if(m_fetch_wait.fetch.kind == AssistantFetchKind::kCompareTraces)
        {
            FinishCurrentTool(
                "The summary of " + name +
                " failed to load, so the runs cannot be compared. Call get_summary on "
                "that trace to retry, or use top_events on each trace instead.");
        }
        else
        {
            FinishCurrentTool("The summary of " + name +
                              " failed to load. Use top_events or track_events instead.");
        }
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
        // A table read holds a slot nothing but the assistant uses. Left to
        // run, a query that outlived its deadline keeps that slot, and the
        // narrower one the model tries next waits behind it.
        const AssistantFetchKind kind = m_fetch_wait.fetch.kind;
        if(m_fetch_wait.owns_requests && (kind == AssistantFetchKind::kTopEvents ||
                                          kind == AssistantFetchKind::kEventInstances ||
                                          kind == AssistantFetchKind::kDataTable))
        {
            CancelOwnedFetch();
            FinishCurrentTool(m_fetch_wait.prefix + "Timed out waiting for " +
                              m_fetch_wait.tool_name +
                              ", so its query was cancelled. Narrow the window, the "
                              "tracks or the filters and call it again.");
            return;
        }
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

    // Each side of a comparison loads its own summary. A finished one is a new
    // wait, not another try of the same one, so the clock starts again for
    // whichever side comes next. Keeping the original deadline would leave a
    // slow baseline's target almost no time, and a summary that landed as the
    // deadline passed would be reported as still busy.
    if(m_fetch_wait.fetch.kind == AssistantFetchKind::kCompareTraces)
    {
        m_fetch_retries = 0;
        m_fetch_wait    = FetchWait();
        RunNextTool();
        return;
    }

    // The rows that landed answer someone else's query, so never format them as
    // this tool's result. Re-run until we issue our own query, bounded by the
    // original wait deadline even though BeginFetchWait runs again.
    const bool not_our_rows = !m_fetch_wait.started_fetch &&
                              m_fetch_wait.fetch.kind != AssistantFetchKind::kSummary;
    if(not_our_rows && timed_out)
    {
        // kernel_pc_samples parks every layer this way, its own reads included,
        // so here the slot was not busy: a layer landed, but past the deadline.
        if(m_fetch_wait.fetch.kind == AssistantFetchKind::kPcSampling)
        {
            FinishCurrentTool("kernel_pc_samples ran out of time reading the PC "
                              "samples. The layers it did read are kept, so calling "
                              "it again carries on from there.");
            return;
        }
        FinishCurrentTool("Timed out waiting to run " + m_fetch_wait.tool_name +
                          " because its shared data request remained busy.");
        return;
    }

    // Our own rows are asked for again too, inside the same deadline, when the
    // UI took them away while they were in flight - a range change dropped the
    // statistics, or a change of selection cleared the event, and either is
    // often the model's own goto - or when the sort column was resolved
    // against the header an earlier query left behind.
    const bool dropped = m_fetch_wait.stats_dropped ||
                         AssistantEventDetailsDropped(context, m_fetch_wait.fetch);
    if(not_our_rows ||
       (!timed_out && ((dropped && m_fetch_retries < ASSISTANT_MAX_DROPPED_RERUNS) ||
                       AssistantFetchNeedsRerun(context, m_fetch_wait.fetch))))
    {
        ++m_fetch_retries;
        m_fetch_wait.active = false;
        RunNextTool();
        return;
    }

    // Out of re-runs or time. Anything the slot holds now is a pill's, for its
    // own window.
    if(m_fetch_wait.stats_dropped)
    {
        FinishCurrentTool(m_fetch_wait.prefix +
                          "The track statistics were dropped before they could be "
                          "read: the selection or view changed while the query ran, "
                          "or it failed. Call track_statistics again.");
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
