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

    void Show();
    void ToggleVisible();
    bool* VisiblePtr();
    bool  IsVisible() const;

    // Width the docked panel wants, including its splitter. Zero when closed, so
    // AppWindow can lay out the main view against it.
    float DockedWidth() const;

    void Update() override;

    // Renders as a docked column. AppWindow calls this inside the main window;
    // Render() is kept for the RocWidget contract and forwards to it.
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
        // Draws the timeline histogram and minimap from live model data rather
        // than storing a snapshot in the transcript.
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

    struct FetchWait
    {
        bool                  active        = false;
        bool                  started_fetch = false;
        std::vector<uint64_t> request_ids;
        AssistantFetchState   fetch;
        std::string           tool_call_id;
        std::string           tool_name;
        std::string           prefix;
        std::chrono::steady_clock::time_point started;
    };

    bool AnyFetchPending(const AssistantToolContext& context) const;

    AssistantPanel();
    ~AssistantPanel() override = default;

    void AppendLine(Speaker speaker, const std::string& text);
    void AppendChart(uint64_t track_id);
    void RenderActivityChart(uint64_t track_id);
    void RenderHeaderCard();
    void RenderTranscript();
    void RenderMessageCard(size_t index, const ChatLine& line);
    void RenderComposer();
    void RenderSplitter();
    // Transient "what I'm doing right now" text. Lives outside the transcript so
    // it disappears with the spinner when the turn ends.
    void SetStatus(const std::string& text);
    void SendCurrentInput(bool explain_view);
    void ResetTurn();
    void StartHttpRequest();
    void PollPendingReply();
    void PollToolFetch();
    void HandleHttpResult(const AssistantChatResult& result);
    void BeginToolQueue(const std::vector<AssistantToolCall>& calls,
                        const std::string&                    assistant_text);
    void RunNextTool();
    void FinishCurrentTool(const std::string& content);
    void ContinueAfterTools();
    void BeginQueuedTurn();
    bool TryStartSummaryWarmup(const std::string& question, bool explain_view);
    AssistantToolContext MakeToolContext() const;
    std::string          BuildUserPrompt(const std::string& question,
                                         bool               include_briefing) const;
    bool                 Busy() const;

    static AssistantPanel* s_instance;

    bool  m_visible;
    bool  m_scroll_to_bottom;
    float m_dock_width;
    Phase m_phase;

    std::string           m_input;
    std::string           m_status;
    // Measured height of the composer block, so the transcript can reserve
    // exactly the right amount instead of guessing and overflowing the window.
    float                 m_composer_height;
    std::vector<ChatLine> m_lines;

    std::string                     m_queued_question;
    bool                            m_queued_explain_view;
    std::vector<AssistantMessage>   m_conversation;
    std::vector<AssistantToolCall>  m_pending_calls;
    size_t                          m_next_call_index;
    uint32_t                        m_tool_round;
    // Set when the tool budget runs out: the next request goes out without
    // tools so the model has to write up what it already gathered.
    bool                            m_force_final;
    uint64_t                        m_request_generation;
    uint64_t                        m_metrics_client_id;
    FetchWait                       m_fetch_wait;

    std::future<AssistantChatResult> m_pending;
};

}  // namespace View
}  // namespace RocProfVis
