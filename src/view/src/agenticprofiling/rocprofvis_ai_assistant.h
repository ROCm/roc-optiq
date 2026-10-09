// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_ai_client.h"
#include "rocprofvis_ai_tools.h"
#include "rocprofvis_widget.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

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

    // Toolbar control shared by the system and compute toolbars.
    static void RenderToolbarButton();

private:
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
    };

    // One tool parked until the fetches behind it land.
    struct FetchWait
    {
        bool active = false;
        // False when the tool piggybacked on a fetch already in flight: the rows
        // that land answer someone else's query, so the tool must run again.
        bool started_fetch = false;
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

    AssistantPanel();
    ~AssistantPanel() override;

    void AppendLine(Speaker speaker, const std::string& text);
    void AppendChart(uint64_t track_id);
    void RenderActivityChart(uint64_t track_id);
    void RenderHeaderCard();
    void RenderTranscript();
    void RenderMessageCard(size_t index, const ChatLine& line);
    void RenderComposer();
    // Explain this view on an empty chat, or the model's offered follow-ups
    // after a turn.
    void RenderSuggestedActions();
    void RenderSplitter();
    // Transient progress text, kept outside the transcript so it disappears
    // with the spinner when the turn ends.
    void SetStatus(const std::string& text);
    // Serializes the transcript to a Markdown string, shared by copy and export.
    std::string BuildTranscriptText() const;
    void        CopyTranscript();
    void        ExportTranscript();
    void SendCurrentInput(bool explain_view);
    void ResetTurn();
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
    // Answers every call left in the round without running it, once the trace
    // in front is no longer the one the round was written for.
    void AbandonBatchForNewTrace(const std::string& reason);
    void AppendToolReply(const AssistantToolCall& call, const std::string& content);
    void ContinueAfterTools();
    void BeginQueuedTurn();
    bool TryStartSummaryWarmup(const std::string& question);
    bool AnyFetchPending(const AssistantToolContext& context) const;
    // Makes the trace in front the one this turn is about. True when it is the
    // other kind from the one pinned before, which means another tool set.
    bool                 PinTurnTrace();
    AssistantToolContext MakeToolContext() const;
    std::string          CurrentProjectId() const;
    std::string          RestartHint(bool kind_changed) const;
    std::string          BuildUserPrompt(const std::string& question,
                                         bool               include_briefing) const;
    bool                 NeedsBriefing();
    void                 TrimConversation();
    bool                 Busy() const;

    static AssistantPanel* s_instance;

    bool  m_visible;
    bool  m_scroll_to_bottom;
    float m_dock_width;
    Phase m_phase;

    std::string m_input;
    std::string m_status;
    // Measured height of the composer block, so the transcript reserves exactly
    // the right amount instead of guessing and overflowing.
    float                 m_composer_height;
    std::vector<ChatLine> m_lines;

    std::string                    m_queued_question;
    std::vector<AssistantMessage>  m_conversation;
    std::vector<AssistantToolCall> m_pending_calls;
    size_t                         m_next_call_index;
    uint32_t                       m_tool_round;
    // Set when the tool budget runs out: the next request goes out without
    // tools, so the model has to write up what it already gathered.
    bool     m_force_final;
    // Keeps contention retries on the first wait's timeout deadline.
    uint32_t m_fetch_retries;
    // The trace this turn is about, which is what catches the user switching
    // tabs mid-investigation. Moves only through PinTurnTrace.
    std::string m_turn_project_id;
    // Which kind of trace that is, which picks the prompt body and the tool
    // schema. Pinned with the project rather than read per round, so the tool
    // set changes only at the point the model is told the trace changed, and
    // the schema a round goes out with always matches the tools the dispatcher
    // will run for it.
    bool        m_turn_is_compute;
    // The trace the briefing in the conversation describes. A follow-up about
    // the same trace does not repeat it; a different one does.
    std::string m_briefed_project_id;
    FetchWait   m_fetch_wait;
    // Clickable follow-ups from offer_next_steps. Cleared on a new turn.
    std::vector<std::string> m_next_steps;

    std::shared_ptr<AssistantChatCall> m_call;
    std::future<AssistantChatResult>   m_pending;
};

}  // namespace View
}  // namespace RocProfVis
