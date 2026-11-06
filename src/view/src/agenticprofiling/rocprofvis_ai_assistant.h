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
    };

    // One tool parked until the fetches behind it land.
    struct FetchWait
    {
        bool active = false;
        // False when the tool piggybacked on a fetch already in flight: the rows
        // that land answer someone else's query, so the tool must run again.
        bool started_fetch = false;
        // This turn started the request ids, so Clear/tab close may cancel them.
        bool owns_requests = false;
        // The summary preload, which has no tool call to answer and resumes the
        // queued turn instead.
        bool                  warmup = false;
        std::vector<uint64_t> request_ids;
        AssistantFetchState   fetch;
        std::string           tool_name;
        std::string           prefix;
        std::chrono::steady_clock::time_point started;
        // Zero takes the default fetch deadline. A tool waiting on the user
        // rather than on a query sets its own, much longer.
        uint32_t              timeout_seconds = 0;
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

    void AppendLine(Speaker speaker, const std::string& text);
    void        AppendChart(const std::string& project_id, uint64_t track_id);
    void        RenderActivityChart(const std::string& project_id, uint64_t track_id);
    void        RenderHeaderCard();
    void        RenderLinkMenu();
    void        RenderTranscript();
    void        RenderMessageCard(Conversation& conversation, size_t index,
                                  const ChatLine& line);
    void        RenderSelectableMessage(Conversation& conversation, size_t index,
                                        const std::string& text, Colors text_color);
    TextSelect* MessageSelect(Conversation& conversation, size_t index);
    void        RenderComposer();
    // Explain this view on an empty chat, or the model's offered follow-ups
    // after a turn.
    void RenderSuggestedActions();
    void RenderSplitter();
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
    void ContinueAfterTools();
    void BeginQueuedTurn();
    bool TryStartSummaryWarmup(const std::string& question);
    bool AnyFetchPending(const AssistantToolContext& context) const;
    // What the tools may touch on the trace the turn is pinned to.
    AssistantToolContext MakeToolContext() const;
    std::string          CurrentProjectId() const;
    std::string BuildUserPrompt(const std::string& question, bool include_briefing);
    bool                 NeedsBriefing();
    void                 TrimConversation();
    bool                 Busy() const;

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
    // Set when the tool budget runs out: the next request goes out without
    // tools, so the model has to write up what it already gathered.
    bool     m_force_final;
    // A trace switch ends this batch. Every remaining call still gets a tool
    // reply, but none of its body runs against a tab that has not switched yet.
    bool m_stop_tool_queue;
    // Keeps contention retries on the first wait's timeout deadline.
    uint32_t m_fetch_retries;
    // The trace the turn belongs to. Its tools read this trace and its replies
    // go to this trace's conversation, whichever tab is in front while it runs.
    std::string m_turn_project_id;
    FetchWait   m_fetch_wait;

    EventManager::SubscriptionToken m_tab_closed_token;

    std::shared_ptr<AssistantChatCall> m_call;
    std::future<AssistantChatResult>   m_pending;
    std::vector<AbandonedRequest>      m_abandoned;
};

}  // namespace View
}  // namespace RocProfVis
