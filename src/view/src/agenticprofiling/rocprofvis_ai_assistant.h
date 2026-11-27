// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_ai_client.h"
#include "rocprofvis_ai_tools.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_widget.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class TextSelect;

namespace RocProfVis
{
namespace View
{

enum class Colors;

class AssistantPanel : public RocWidget
{
public:
    static AssistantPanel* GetInstance();
    static void            DestroyInstance();

    void  ToggleVisible();
    bool* VisiblePtr();

    // Width the docked panel wants, including its splitter. Zero when closed, so
    // AppWindow can lay out the main view against it.
    float DockedWidth() const;

    // Drives the turn forward. The only place tools run, so a tool can never
    // reorder panels in the middle of the frame that draws them.
    void Update() override;

    // Renders as a docked column, called by AppWindow inside the main window.
    // Render() is kept for the RocWidget contract and forwards here.
    void RenderDocked();
    void Render() override;

    // Toolbar control for the system trace toolbar.
    static void RenderToolbarButton();

    // The models the active endpoint lists. The settings dialog and the model
    // picker in the header share it, so both offer the same names.
    AssistantModelCatalog& ModelCatalog();
    // The widest listed name in the current face and size, for sizing either
    // picker. Measured again only when the list, the face, or the size changes.
    float ModelNameWidth();

private:
    friend struct AssistantPanelTestPeer;

    enum class Speaker
    {
        kUser,
        kAssistant,
        kStatus,
        // Drawn from live model data rather than a stored snapshot.
        kChart
    };

    enum class Phase
    {
        kIdle,
        kHttpWait,
        kToolWait
    };

    struct ChatLine
    {
        Speaker     speaker;
        std::string text;
        uint64_t    track_id = 0;  // kChart only
        // kChart only: the trace the chart draws, which a read of a linked
        // trace makes different from the conversation's own.
        std::string project_id;
        // kChart only: the strip as last cut. The minimap is built once per
        // load, so it is cut again only for another bin count or a closed tab.
        size_t                            chart_bin_count = 0;
        std::vector<double>               chart_bins;
        std::vector<AssistantActivityRow> chart_rows;
    };

    /*
     * One trace's chat. Each open trace keeps its own, so nothing said about
     * one is ever sent with a question about another, and switching tabs swaps
     * which one is shown instead of mixing them.
     */
    struct Conversation
    {
        std::vector<ChatLine> lines;
        // Parallel to lines. Text rows create one; charts do not. Cleared with
        // the transcript so a new conversation does not keep an old highlight.
        std::vector<std::unique_ptr<TextSelect>> message_select;
        std::vector<std::vector<std::string>>    message_lines;
        std::vector<AssistantMessage>            messages;
        // Clickable follow-ups from offer_next_steps. Cleared on a new turn.
        std::vector<std::string> next_steps;
        std::string              input;
        // Whether messages already carry this trace's briefing.
        bool briefed = false;
        // The linked-trace block the model was last given, so a change is
        // announced on the next turn instead of leaving it an outdated list.
        std::string links_announced;
        bool        scroll_to_bottom = false;

        // Token counts the endpoint reported for this trace's conversation.
        // Kept across the end of a turn and cleared with the conversation.
        bool usage_reported            = false;
        int  last_prompt_tokens        = 0;
        int  last_completion_tokens    = 0;
        bool last_cached_known         = false;
        int  last_cached_tokens        = 0;
        int  session_prompt_tokens     = 0;
        int  session_completion_tokens = 0;
        int  session_rounds            = 0;
    };

    // One tool parked until the fetches behind it land.
    struct FetchWait
    {
        bool active = false;
        // False when the tool piggybacked on a fetch already in flight: the rows
        // that land answer someone else's query, so the tool must run again.
        bool started_fetch = false;
        // A track_statistics read that was voided while it waited, so the
        // numbers in the shared slot are not this tool's.
        bool stats_dropped = false;
        // This turn started the request ids, so it may cancel them when it
        // ends, or when a read that holds a slot outlives its deadline.
        bool owns_requests = false;
        // The summary preload, which has no tool call to answer and resumes the
        // queued turn instead.
        bool                  warmup = false;
        std::vector<uint64_t> request_ids;
        AssistantFetchState   fetch;
        std::string           tool_name;
        std::string           prefix;
        // What the slow-wait counter is appended to: the tool's own status
        // line when it gave one.
        std::string           status;
        std::chrono::steady_clock::time_point started;
        // Zero takes the default fetch deadline. A tool waiting on the user
        // rather than on a query sets its own, much longer.
        uint32_t timeout_seconds = 0;
        // Set from SummaryEvent while parked on a summary. A failure stops this
        // wait instead of asking the provider for the same summary again.
        bool summary_settled   = false;
        bool summary_succeeded = false;
        // The user pressed Run on the script this waits on, which restarted
        // the clock: reading it and running it are two waits.
        bool script_running = false;
    };

    // A cancelled HTTP call reaped from Update() instead of joined in the tab
    // close / Clear event that cancelled it. std::future from std::async waits
    // in its destructor, which would otherwise freeze the UI on a slow socket.
    struct AbandonedRequest
    {
        std::shared_ptr<AssistantChatCall> call;
        std::future<void>                  cancel;
        std::future<AssistantChatResult>   result;
    };

    // ModelNameWidth()'s last measurement, with the list revision, face, and
    // size it was measured for.
    struct ModelWidthCache
    {
        uint64_t revision  = 0;
        ImFont*  font      = nullptr;
        float    font_size = 0.0f;
        float    width     = 0.0f;
    };

    AssistantPanel();
    ~AssistantPanel() override;

    // Created on first use, so a trace that is never asked about holds nothing.
    Conversation& ConversationFor(const std::string& project_id);
    // The conversation the turn writes to, and the one on screen.
    Conversation& TurnConversation();
    Conversation& ViewConversation();
    // Forgets what the model was told, keeping the transcript on screen.
    void ResetHistory(Conversation& conversation);
    void HandleTabClosed(const std::string& project_id);
    // A summary finished on some trace. Recorded only when this turn is parked
    // on that trace's summary, whoever started the request.
    void HandleSummaryEvent(const SummaryEvent& event);

    void AppendLine(Speaker speaker, const std::string& text);
    void        AppendChart(const std::string& project_id, uint64_t track_id);
    void        RenderActivityChart(const std::string& project_id, ChatLine& line);
    void        RenderHeaderCard();
    void        RenderLinkMenu();
    // The model, context, and token-usage pill under the title.
    void        RenderContextChip(const std::string& model, const std::string& endpoint);
    void        RenderSetupChip();
    // The list the arrow on the pill opens when the endpoint names its models.
    void        RenderModelMenu(const ImVec2& anchor, const std::string& model,
                                const std::string& endpoint);
    void        RenderTranscript();
    void        RenderMessageCard(Conversation& conversation, size_t index,
                                  ChatLine& line);
    void        RenderSelectableMessage(Conversation& conversation, size_t index,
                                        const std::string& text, Colors text_color);
    TextSelect* MessageSelect(Conversation& conversation, size_t index);
    void        RenderComposer();
    // Explain this view on an empty chat, or the model's offered follow-ups
    // after a turn.
    void RenderSuggestedActions();
    void RenderSplitter();
    // The panel width drawn: the dragged width, within what the window can spare.
    float DockWidth() const;
    // Transient progress text, kept outside the transcript so it disappears
    // with the spinner when the turn ends.
    void SetStatus(const std::string& text);
    // Serializes a transcript to a Markdown string, shared by copy and export.
    std::string BuildTranscriptText(const Conversation& conversation,
                                    const std::string&  trace_name) const;
    void        CopyTranscript();
    void        ExportTranscript();
    void        ClearConversation();
    void SendCurrentInput(bool explain_view);
    void ResetTurn();
    // The Stop button: ends the running turn and keeps the conversation.
    void StopTurn();
    // Declines a script still waiting on Run or Reject for a turn that is over,
    // and lets go of one the user already answered.
    void WithdrawScriptOffer();
    // Cancels the requests the parked tool started itself.
    void CancelOwnedFetch();
    // Non-blocking cancellation for a UI action. The future is reaped later.
    void AbandonPendingRequest();
    void ReapAbandonedRequests();
    // Blocking cancellation for final panel destruction only.
    void CancelPendingRequest();
    void StartHttpRequest();
    void PollToolFetch();
    void HandleHttpResult(const AssistantChatResult& result);
    void BeginToolQueue(const std::vector<AssistantToolCall>& calls,
                        const std::string&                    assistant_text);
    void RunNextTool();
    // Spends one extra round on the answer, with tools off so the model writes
    // prose instead of reaching for another call.
    void BeginFinalAnswer();
    void BeginFetchWait(const AssistantToolStartResult& started,
                        const std::string& tool_name, bool warmup);
    void FinishCurrentTool(const std::string& content);
    void AppendToolReply(const AssistantToolCall& call, const std::string& content);
    void ContinueAfterTools();
    void BeginQueuedTurn();
    bool TryStartSummaryWarmup(const std::string& question);
    bool AnyFetchPending(const AssistantToolContext& context) const;
    // Makes the trace on screen the one this turn is about, together with its
    // kind, which picks the prompt body and the tool schema.
    void PinTurnTrace();
    // What the tools may touch on the trace the turn is pinned to.
    AssistantToolContext MakeToolContext() const;
    // The context a tool call runs under: the trace's, with the turn's limits.
    AssistantToolContext MakeToolCallContext(const std::string& project_id);
    std::string          CurrentProjectId() const;
    std::string BuildUserPrompt(const std::string& question, bool include_briefing);
    bool                 NeedsBriefing();
    void                 TrimConversation();
    bool                 Busy() const;
    // Records a usage object from a completed response. Ignored when the
    // endpoint sent none.
    void NoteTokenUsage(const AssistantChatResult& result);
    void ClearTokenUsage(Conversation& conversation);

    // The conversation as the history trimmer budgets it, plus the standing
    // prompt and tool schema that ride along with every request. The pill
    // draws the first against its budget and reports the overhead apart, so
    // an empty chat reads as empty. Tokens are characters over four.
    struct ContextLoad
    {
        size_t history_chars         = 0;
        size_t history_budget_chars  = 0;
        size_t history_tokens        = 0;
        size_t history_budget_tokens = 0;
        size_t overhead_tokens       = 0;
    };
    ContextLoad MeasureContext() const;
    void        RenderContextTooltip(const Conversation& conversation,
                                     const ContextLoad& load, float fill,
                                     const std::string& model,
                                     const std::string& endpoint) const;

    static AssistantPanel* s_instance;

    bool  m_visible;
    float m_dock_width;
    Phase m_phase;

    std::string m_status;
    // Measured height of the composer block, so the transcript reserves exactly
    // the right amount instead of guessing and overflowing.
    float m_composer_height;

    // Every open trace's conversation, keyed by Project::GetID(). Held by
    // pointer so a TextSelect callback can keep its conversation's address.
    std::unordered_map<std::string, std::unique_ptr<Conversation>> m_conversations;
    // The trace whose conversation is on screen. Read while drawing, after the
    // tab bar has applied a switch: during Update() the active tab can still be
    // the one the user just left.
    std::string m_view_project_id;

    std::string                    m_queued_question;
    std::vector<AssistantToolCall> m_pending_calls;
    size_t                         m_next_call_index;
    uint32_t                       m_tool_round;
    // Set for the write-up round, which goes out without tools so the model
    // writes up what it gathered. A spent tool budget forces the same round.
    bool     m_force_final;
    // A trace switch ends this batch. Every remaining call still gets a tool
    // reply, but none of its body runs against a tab that has not switched yet.
    bool m_stop_tool_queue;
    // Whether the batch offered next steps, and whether everything else in it
    // was an action carried out as asked. Both true: the write-up comes next.
    bool m_batch_offered_steps;
    bool m_batch_settled;
    // Keeps contention retries on the first wait's timeout deadline.
    uint32_t m_fetch_retries;
    // The trace the turn belongs to. Its tools read this trace and its replies
    // go to this trace's conversation, whichever tab is in front while it runs.
    // Moves only through PinTurnTrace.
    std::string m_turn_project_id;
    // Which kind of trace that is, which picks the prompt body and the tool
    // schema. Pinned with the project rather than read per round, so the schema
    // a round goes out with always matches the tools the dispatcher will run
    // for it.
    bool m_turn_is_compute;
    // The endpoint, model, and key are pinned with the turn too. Preferences
    // may change while tools run, but every round in one answer must go to the
    // same host and model. Empty between turns.
    std::string m_turn_endpoint_url;
    std::string m_turn_model;
    std::string m_turn_api_token;
    // What this turn's tools have used up, handed to them with each call.
    AssistantTurnLimits m_turn_limits;
    FetchWait           m_fetch_wait;

    EventManager::SubscriptionToken m_tab_closed_token;
    EventManager::SubscriptionToken m_summary_event_token;

    std::shared_ptr<AssistantChatCall> m_call;
    std::future<AssistantChatResult>   m_pending;
    std::vector<AbandonedRequest>      m_abandoned;

    AssistantModelCatalog m_model_catalog;
    // Set once the open panel has asked for the saved endpoint's models, and
    // cleared when it closes. The settings dialog asks again whenever the
    // endpoint or key changes.
    bool            m_models_requested;
    ModelWidthCache m_model_width_cache;
};

}  // namespace View
}  // namespace RocProfVis
