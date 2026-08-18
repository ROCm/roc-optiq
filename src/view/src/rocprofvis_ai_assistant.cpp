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

constexpr float  ASSISTANT_DEFAULT_WIDTH  = 440.0f;
constexpr float  ASSISTANT_DEFAULT_HEIGHT = 520.0f;
constexpr float  ASSISTANT_MIN_WIDTH      = 320.0f;
constexpr float  ASSISTANT_MIN_HEIGHT     = 280.0f;
constexpr float  ASSISTANT_INPUT_HEIGHT   = 72.0f;
constexpr uint32_t ASSISTANT_MAX_TOOL_ROUNDS = 8;
constexpr int    ASSISTANT_FETCH_TIMEOUT_SECONDS = 45;

constexpr const char* ASSISTANT_SYSTEM_PROMPT =
    "You are Optiq Assistant, built into ROCm Optiq, a GPU/CPU profiler. "
    "Explain traces to someone who has never used a GPU profiler. "
    "You have tools: get_summary, top_events, kernel_instances, kernel_metrics, "
    "list_tracks, goto. "
    "If the briefing is empty, kernel_exec_time_total_ns is 0, or top_kernels is "
    "missing, call get_summary and top_events (category=dispatch) before answering. "
    "Use only names and numbers from the briefing or tool results. Do not invent "
    "metrics. Do not write SQL. "
    "Point at the biggest problem first, then one concrete next step. "
    "When you know a time range the user should inspect, call goto. "
    "Keep the final answer short.";

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
, m_phase(Phase::kIdle)
, m_queued_explain_view(false)
, m_next_call_index(0)
, m_tool_round(0)
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
AssistantPanel::ReplaceLastStatus(const std::string& text)
{
    if(!m_lines.empty() && m_lines.back().speaker == Speaker::kStatus)
    {
        m_lines.back().text = text;
        m_scroll_to_bottom  = true;
        return;
    }
    AppendLine(Speaker::kStatus, text);
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
        out << "Explain this view to a novice. Use tools if the briefing is incomplete.\n";
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
    m_fetch_wait       = FetchWait();
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
    request.enable_tools = true;

    AssistantMessage system_message;
    system_message.role    = "system";
    system_message.content = ASSISTANT_SYSTEM_PROMPT;
    request.messages.push_back(system_message);
    request.messages.insert(request.messages.end(), m_conversation.begin(),
                            m_conversation.end());

    m_phase = Phase::kHttpWait;
    ReplaceLastStatus("Thinking...");
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

    m_queued_question     = question;
    m_queued_explain_view = explain_view;
    m_phase               = Phase::kToolWait;
    m_fetch_wait.active   = true;
    m_fetch_wait.started_fetch = started.started_fetch;
    m_fetch_wait.request_id    = started.request_id;
    m_fetch_wait.kind          = started.fetch_kind;
    m_fetch_wait.table_type    = started.table_type;
    m_fetch_wait.kernel_id     = started.kernel_id;
    m_fetch_wait.row_limit     = started.row_limit;
    m_fetch_wait.tool_call_id.clear();
    m_fetch_wait.tool_name     = "__briefing";
    m_fetch_wait.prefix.clear();
    m_fetch_wait.started       = std::chrono::steady_clock::now();
    ReplaceLastStatus("Loading summary...");
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
        m_tool_round = 0;
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
        ReplaceLastStatus(result.error);
        NotificationManager::GetInstance().Show(result.error, NotificationLevel::Error);
        return;
    }

    if(!result.tool_calls.empty())
    {
        BeginToolQueue(result.tool_calls, result.reply);
        return;
    }

    ResetTurn();
    if(!m_lines.empty() && m_lines.back().speaker == Speaker::kStatus)
    {
        m_lines.pop_back();
    }
    AppendLine(Speaker::kAssistant, result.reply);

    AssistantMessage assistant_message;
    assistant_message.role    = "assistant";
    assistant_message.content = result.reply;
    m_conversation.push_back(assistant_message);
}

void
AssistantPanel::BeginToolQueue(const std::vector<AssistantToolCall>& calls,
                               const std::string&                    assistant_text)
{
    ++m_tool_round;
    if(m_tool_round > ASSISTANT_MAX_TOOL_ROUNDS)
    {
        ResetTurn();
        ReplaceLastStatus("Stopped after too many tool rounds. Ask a more specific question.");
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
    ReplaceLastStatus(AssistantToolStatusLabel(call.name));
    spdlog::info("Assistant tool {} ({})", call.name, call.id);

    const AssistantToolStartResult started =
        StartAssistantTool(MakeToolContext(), call.name, call.arguments);
    if(!started.status_line.empty())
    {
        ReplaceLastStatus(started.status_line);
    }

    if(started.pending)
    {
        m_phase                      = Phase::kToolWait;
        m_fetch_wait.active          = true;
        m_fetch_wait.started_fetch   = started.started_fetch;
        m_fetch_wait.request_id      = started.request_id;
        m_fetch_wait.kind            = started.fetch_kind;
        m_fetch_wait.table_type      = started.table_type;
        m_fetch_wait.kernel_id       = started.kernel_id;
        m_fetch_wait.row_limit       = started.row_limit;
        m_fetch_wait.tool_call_id    = call.id;
        m_fetch_wait.tool_name       = call.name;
        m_fetch_wait.prefix          = started.content;
        m_fetch_wait.started         = std::chrono::steady_clock::now();
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

    if(context.data_provider->IsRequestPending(m_fetch_wait.request_id))
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
       m_fetch_wait.kind != AssistantFetchKind::kSummary)
    {
        m_fetch_wait.active = false;
        RunNextTool();
        return;
    }

    std::string body = FinishAssistantFetch(context, m_fetch_wait.kind,
                                            m_fetch_wait.table_type, m_fetch_wait.kernel_id,
                                            m_fetch_wait.row_limit);
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

    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::SetNextWindowSize(ImVec2(ASSISTANT_DEFAULT_WIDTH, ASSISTANT_DEFAULT_HEIGHT),
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(ASSISTANT_MIN_WIDTH, ASSISTANT_MIN_HEIGHT),
                                        ImVec2(FLT_MAX, FLT_MAX));
    if(!ImGui::Begin("Ask Optiq", &m_visible))
    {
        ImGui::End();
        return;
    }

    const AssistantSettings& assistant = settings.GetUserSettings().assistant;
    if(assistant.endpoint_url.empty())
    {
        ImGui::TextWrapped(
            "Add the base URL and subscription key in Edit > Preferences > Assistant. "
            "Example URL: https://llm-api.amd.com/OnPrem");
    }
    else
    {
        ImGui::TextWrapped("Endpoint: %s", assistant.endpoint_url.c_str());
        if(!assistant.model.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", assistant.model.c_str());
        }
    }

    ImGui::BeginDisabled(Busy());
    if(ImGui::Button("Explain this view"))
    {
        SendCurrentInput(true);
    }
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        SetTooltipStyled(
            "Sends a briefing, then lets the model query summary, top events, "
            "kernel instances, and metrics, and jump the timeline with goto.");
    }

    ImGui::Separator();

    const float footer = ASSISTANT_INPUT_HEIGHT + ImGui::GetFrameHeightWithSpacing() * 2.0f;
    ImGui::BeginChild("assistant_history", ImVec2(0.0f, -footer),
                      ImGuiChildFlags_Borders);
    for(size_t i = 0; i < m_lines.size(); ++i)
    {
        const ChatLine& line = m_lines[i];
        Colors color         = Colors::kTextMain;
        const char* label    = "Optiq";
        if(line.speaker == Speaker::kUser)
        {
            label = "You";
            color = Colors::kAccent;
        }
        else if(line.speaker == Speaker::kStatus)
        {
            label = "Status";
            color = Colors::kTextDim;
        }
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImGui::ColorConvertU32ToFloat4(settings.GetColor(color)));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::TextWrapped("%s", line.text.c_str());
        ImGui::Spacing();
    }
    if(m_scroll_to_bottom)
    {
        ImGui::SetScrollHereY(1.0f);
        m_scroll_to_bottom = false;
    }
    ImGui::EndChild();

    ImGui::SetNextItemWidth(-1.0f);
    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_None;
    if(Busy())
    {
        input_flags |= ImGuiInputTextFlags_ReadOnly;
    }
    InputTextStringWithHint("##assistant_input", "Ask a follow-up...", m_input,
                            input_flags);
    if(ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter) &&
       !ImGui::GetIO().KeyShift && !Busy())
    {
        SendCurrentInput(false);
    }

    ImGui::BeginDisabled(Busy() || m_input.empty());
    if(ImGui::Button("Send"))
    {
        SendCurrentInput(false);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if(ImGui::Button("Clear"))
    {
        ResetTurn();
        m_lines.clear();
        m_input.clear();
        m_conversation.clear();
    }

    ImGui::End();
}

}  // namespace View
}  // namespace RocProfVis
