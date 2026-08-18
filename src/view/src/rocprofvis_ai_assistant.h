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

    void Update() override;
    void Render() override;

    // Toolbar control shared by the system and compute toolbars.
    static void RenderToolbarButton();

private:
    enum class Speaker
    {
        kUser,
        kAssistant,
        kStatus
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
    };

    struct FetchWait
    {
        bool               active        = false;
        bool               started_fetch = false;
        uint64_t           request_id    = 0;
        AssistantFetchKind kind          = AssistantFetchKind::kNone;
        TableType          table_type    = TableType::kSummaryKernelTable;
        uint32_t           kernel_id     = 0;
        size_t             row_limit     = 10;
        std::string        tool_call_id;
        std::string        tool_name;
        std::string        prefix;
        std::chrono::steady_clock::time_point started;
    };

    AssistantPanel();
    ~AssistantPanel() override = default;

    void AppendLine(Speaker speaker, const std::string& text);
    void ReplaceLastStatus(const std::string& text);
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
    Phase m_phase;

    std::string           m_input;
    std::vector<ChatLine> m_lines;

    std::string                     m_queued_question;
    bool                            m_queued_explain_view;
    std::vector<AssistantMessage>   m_conversation;
    std::vector<AssistantToolCall>  m_pending_calls;
    size_t                          m_next_call_index;
    uint32_t                        m_tool_round;
    uint64_t                        m_request_generation;
    uint64_t                        m_metrics_client_id;
    FetchWait                       m_fetch_wait;

    std::future<AssistantChatResult> m_pending;
};

}  // namespace View
}  // namespace RocProfVis
