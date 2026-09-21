// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The scripted half of AssistantPanel: press Explain this view, ask one
// question, write down what came back. Kept beside the interactive half rather
// than inside it because nothing here belongs to a turn - it only watches one
// finish and starts the next.

#include "rocprofvis_ai_assistant.h"

#include <chrono>
#include <fstream>
#include <string>
#include <vector>

#include "json.h"
#include "spdlog/spdlog.h"

#include "rocprofvis_data_provider.h"
#include "rocprofvis_render_scheduler.h"

namespace RocProfVis
{
namespace View
{

void
AssistantPanel::StartBatch(const AssistantBatchRequest& request)
{
    if(m_batch.state == AssistantBatchState::kRunning)
    {
        spdlog::warn("Assistant batch already running; ignoring the second request");
        return;
    }

    m_batch         = BatchRun();
    m_batch.request = request;
    if(m_batch.request.timeout_seconds == 0)
    {
        m_batch.request.timeout_seconds = ASSISTANT_BATCH_DEFAULT_TIMEOUT_SECONDS;
    }
    m_batch.state   = AssistantBatchState::kRunning;
    m_batch.stage   = BatchRun::Stage::kWaitForTrace;
    m_batch.started = std::chrono::steady_clock::now();

    spdlog::info("Assistant batch started: explain_first={} timeout={}s out='{}'",
                 m_batch.request.explain_first, m_batch.request.timeout_seconds,
                 m_batch.request.output_path);
}

AssistantBatchState
AssistantPanel::BatchState() const
{
    return m_batch.state;
}

// Walks the run forward by at most one step per frame. Every wait is expressed
// as "the panel is no longer busy", so the scripted path cannot disagree with
// the interactive one about when a turn has ended.
void
AssistantPanel::UpdateBatch()
{
    if(m_batch.state != AssistantBatchState::kRunning)
    {
        return;
    }

    // The app shell sleeps until an OS event when nothing asks for a frame, and
    // a scripted run produces no events at all.
    RenderScheduler::GetInstance().RequestRender();

    const std::chrono::seconds elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - m_batch.started);
    if(elapsed.count() >= static_cast<int64_t>(m_batch.request.timeout_seconds))
    {
        FinishBatch(AssistantBatchState::kFailed,
                    "Timed out after " + std::to_string(elapsed.count()) + "s.");
        return;
    }

    switch(m_batch.stage)
    {
        case BatchRun::Stage::kWaitForTrace:
        {
            const AssistantToolContext context = MakeToolContext();
            if(context.data_provider == nullptr)
            {
                // The tab has not been built yet. Nothing to report; wait.
                return;
            }
            if(context.data_provider->GetState() == ProviderState::kError)
            {
                FinishBatch(AssistantBatchState::kFailed, "The trace failed to open.");
                return;
            }
            if(context.data_provider->GetState() != ProviderState::kReady)
            {
                return;
            }
            m_batch.stage = m_batch.request.explain_first ? BatchRun::Stage::kExplain
                                                          : BatchRun::Stage::kQuestion;
            return;
        }

        case BatchRun::Stage::kExplain:
        {
            if(Busy())
            {
                return;
            }
            m_input.clear();
            SendCurrentInput(true);
            if(!Busy())
            {
                // SendCurrentInput refuses before it starts anything when the
                // endpoint is unconfigured, and says so in the transcript.
                FinishBatch(AssistantBatchState::kFailed,
                            "Explain this view did not start. " + LastStatusText());
                return;
            }
            m_batch.stage = BatchRun::Stage::kExplainWait;
            return;
        }

        case BatchRun::Stage::kExplainWait:
        {
            if(Busy())
            {
                return;
            }
            m_batch.stage = BatchRun::Stage::kQuestion;
            return;
        }

        case BatchRun::Stage::kQuestion:
        {
            if(Busy())
            {
                return;
            }
            m_input                  = m_batch.request.question;
            m_batch.answer_watermark = m_lines.size();
            SendCurrentInput(false);
            if(!Busy())
            {
                FinishBatch(AssistantBatchState::kFailed,
                            "The question did not start. " + LastStatusText());
                return;
            }
            m_batch.stage = BatchRun::Stage::kQuestionWait;
            return;
        }

        case BatchRun::Stage::kQuestionWait:
        {
            if(Busy())
            {
                return;
            }
            // A turn ending is not a turn answering. A request that fails
            // mid-flight clears Busy() exactly like one that succeeded, so the
            // run is only done once the model has actually said something.
            if(LastAssistantText(m_batch.answer_watermark).empty())
            {
                const std::string reason = LastStatusText();
                FinishBatch(AssistantBatchState::kFailed,
                            reason.empty() ? "The turn ended without an answer." : reason);
                return;
            }
            FinishBatch(AssistantBatchState::kDone, std::string());
            return;
        }
    }
}

void
AssistantPanel::FinishBatch(AssistantBatchState state, const std::string& error)
{
    m_batch.error = error;
    m_batch.state = state;
    if(!error.empty())
    {
        spdlog::error("Assistant batch failed: {}", error);
    }

    if(!WriteBatchOutput())
    {
        m_batch.state = AssistantBatchState::kFailed;
        return;
    }
    if(state == AssistantBatchState::kDone)
    {
        spdlog::info("Assistant batch wrote '{}'", m_batch.request.output_path);
    }
}

std::string
AssistantPanel::LastAssistantText(size_t first_line) const
{
    for(size_t i = m_lines.size(); i > first_line; --i)
    {
        const ChatLine& line = m_lines[i - 1];
        if(line.speaker == Speaker::kAssistant && !line.text.empty())
        {
            return line.text;
        }
    }
    return std::string();
}

std::string
AssistantPanel::LastStatusText() const
{
    for(size_t i = m_lines.size(); i > 0; --i)
    {
        const ChatLine& line = m_lines[i - 1];
        if(line.speaker == Speaker::kStatus && !line.text.empty())
        {
            return line.text;
        }
    }
    return std::string();
}

// The answer on its own is what a reader wants; the tool calls are what tells
// you whether it got there by looking or by guessing, which is the whole point
// of capturing a run rather than a screenshot.
bool
AssistantPanel::WriteBatchOutput() const
{
    if(m_batch.request.output_path.empty())
    {
        return true;
    }

    const AssistantToolContext context = MakeToolContext();

    jt::Json out;
    out["trace"]         = context.trace_name;
    out["is_compute"]    = context.is_compute;
    out["question"]      = m_batch.request.question;
    out["explain_first"] = m_batch.request.explain_first;
    out["status"] =
        m_batch.state == AssistantBatchState::kDone ? std::string("ok") : std::string("error");
    out["error"]  = m_batch.error;
    out["answer"] = LastAssistantText(m_batch.answer_watermark);
    out["duration_seconds"] = static_cast<long long>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - m_batch.started)
            .count());

    out["tool_calls"].setArray();
    size_t call_index = 0;
    for(size_t i = 0; i < m_conversation.size(); ++i)
    {
        const std::vector<AssistantToolCall>& calls = m_conversation[i].tool_calls;
        for(size_t j = 0; j < calls.size(); ++j)
        {
            out["tool_calls"][call_index]["name"]      = calls[j].name;
            out["tool_calls"][call_index]["arguments"] = calls[j].arguments;
            // What the tool answered, which is what decides whether the model
            // read its numbers or talked around them. Long-running turns
            // compact their oldest replies, so this is what the model still had
            // in front of it rather than what the tool first returned.
            out["tool_calls"][call_index]["result"] = std::string();
            for(size_t k = i + 1; k < m_conversation.size(); ++k)
            {
                if(m_conversation[k].role == "tool" &&
                   m_conversation[k].tool_call_id == calls[j].id)
                {
                    out["tool_calls"][call_index]["result"] = m_conversation[k].content;
                    break;
                }
            }
            ++call_index;
        }
    }

    out["transcript"].setArray();
    for(size_t i = 0; i < m_lines.size(); ++i)
    {
        std::string speaker;
        switch(m_lines[i].speaker)
        {
            case Speaker::kUser: speaker = "user"; break;
            case Speaker::kAssistant: speaker = "assistant"; break;
            case Speaker::kStatus: speaker = "status"; break;
            case Speaker::kChart: speaker = "chart"; break;
        }
        out["transcript"][i]["speaker"] = speaker;
        out["transcript"][i]["text"]    = m_lines[i].text;
    }

    std::ofstream file(m_batch.request.output_path, std::ios::binary | std::ios::trunc);
    if(!file.is_open())
    {
        spdlog::error("Assistant batch could not write '{}'", m_batch.request.output_path);
        return false;
    }
    file << out.toStringPretty();
    file.close();
    return file.good();
}

}  // namespace View
}  // namespace RocProfVis
