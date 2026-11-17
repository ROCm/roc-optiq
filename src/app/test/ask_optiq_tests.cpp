// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Ask Optiq, end to end: the real panel runs real tools against the sample
// trace, and an in-process endpoint (ask_optiq_mock_endpoint.h) plays the
// model. Every request the panel sends is recorded, so a test reads back the
// exact tool replies the model would have seen.
//
// The golden tests pin those replies: each tool runs with fixed arguments, the
// whole set runs twice to prove the output does not depend on what ran
// before, and the result is compared with src/app/test/golden/ask_optiq.
// Set ROCPROFVIS_UPDATE_GOLDEN=1 to rewrite the files after an intended change,
// and ROCPROFVIS_GOLDEN_DIR to read them from somewhere other than the
// working directory the CTest entries use.

#include "app_tests.h"
#include "ask_optiq_mock_endpoint.h"

#include "imgui.h"
#include "imgui_te_context.h"
#include "imgui_te_engine.h"
#include "json.h"

#include "rocprofvis_appwindow.h"
#include "rocprofvis_project.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_view_test_access.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace RocProfVis::View;
using RocProfVis::AppTest::Answer;
using RocProfVis::AppTest::Calls;
using RocProfVis::AppTest::Failure;
using RocProfVis::AppTest::MockChatEndpoint;
using RocProfVis::AppTest::MockReply;
using RocProfVis::AppTest::MockToolCall;

namespace
{

constexpr const char* SYSTEM_SAMPLE  = "rocpd-transpose.db";
constexpr const char* COMPUTE_SAMPLE = "rocprof_compute_23ed6f36.db";
constexpr const char* BRIEFING       = "Briefing for the current Optiq view:";
constexpr const char* NUDGE          = "Do not call any more tools";

#define AO_CHECK(expr)                                                                   \
    do                                                                                   \
    {                                                                                    \
        const bool ask_optiq_ok_ = static_cast<bool>(expr);                              \
        if(!ask_optiq_ok_)                                                               \
        {                                                                                \
            std::cout << "[ask_optiq] " << ctx->Test->Name << ":" << __LINE__            \
                      << " check failed: " #expr "\n";                                   \
        }                                                                                \
        IM_CHECK_NO_RET(ask_optiq_ok_);                                                  \
    } while(0)

bool
Has(const std::string& text, const std::string& needle)
{
    return text.find(needle) != std::string::npos;
}

double
Seconds(std::chrono::steady_clock::time_point since)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - since).count();
}

AssistantPanelTestPeer
Panel()
{
    return AssistantPanelTestPeer{ *AssistantPanel::GetInstance() };
}

// Runs frames until the turn is over. max_retries, when given, records the most
// re-runs any one tool needed along the way.
bool
WaitIdle(ImGuiTestContext* ctx, double seconds = 60.0, uint32_t* max_retries = nullptr)
{
    AssistantPanelTestPeer peer  = Panel();
    const auto             start = std::chrono::steady_clock::now();
    ctx->Yield();
    while(peer.Busy())
    {
        if(max_retries != nullptr)
        {
            *max_retries = std::max(*max_retries, peer.FetchRetries());
        }
        if(Seconds(start) > seconds)
        {
            return false;
        }
        ctx->Yield();
    }
    return true;
}

// One request as the endpoint received it.
struct SentRequest
{
    jt::Json body;

    bool HasTools() { return body.contains("tools"); }

    std::vector<jt::Json>& Messages() { return body["messages"].getArray(); }

    static std::string Content(jt::Json& message)
    {
        return message.contains("content") && message["content"].isString()
                   ? message["content"].getString()
                   : std::string();
    }

    std::string LastUser()
    {
        std::vector<jt::Json>& messages = Messages();
        for(auto it = messages.rbegin(); it != messages.rend(); ++it)
        {
            if((*it)["role"].getString() == "user") return Content(*it);
        }
        return std::string();
    }

    bool AnyContains(const std::string& needle)
    {
        for(jt::Json& message : Messages())
        {
            if(Has(Content(message), needle)) return true;
        }
        return false;
    }

    bool AnyQuestionStartsWith(const std::string& prefix)
    {
        for(jt::Json& message : Messages())
        {
            if(message["role"].getString() == "user" && Content(message).rfind(prefix, 0) == 0)
                return true;
        }
        return false;
    }

    // The tool replies this request added, in call order. A write-up nudge
    // after them is skipped.
    std::vector<std::pair<std::string, std::string>> NewToolReplies()
    {
        std::vector<jt::Json>& messages = Messages();
        size_t                 end      = messages.size();
        if(end > 0 && messages[end - 1]["role"].getString() == "user" &&
           Has(Content(messages[end - 1]), NUDGE))
        {
            --end;
        }
        size_t first = end;
        while(first > 0 && messages[first - 1]["role"].getString() == "tool")
        {
            --first;
        }
        std::vector<std::pair<std::string, std::string>> replies;
        for(size_t i = first; i < end; ++i)
        {
            replies.emplace_back(messages[i]["name"].getString(), Content(messages[i]));
        }
        return replies;
    }

    std::string ToolReply(const std::string& tool, int nth = 0)
    {
        int seen = 0;
        for(const std::pair<std::string, std::string>& reply : NewToolReplies())
        {
            if(reply.first == tool && seen++ == nth) return reply.second;
        }
        return std::string();
    }

    std::vector<std::string> ToolNames()
    {
        std::vector<std::string> names;
        if(!HasTools()) return names;
        for(jt::Json& tool : body["tools"].getArray())
        {
            names.push_back(tool["function"]["name"].getString());
        }
        return names;
    }
};

std::vector<SentRequest>
Sent()
{
    std::vector<SentRequest> sent;
    for(const std::string& raw : MockChatEndpoint::Instance().Requests())
    {
        std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(raw);
        SentRequest                           request;
        if(parsed.first == jt::Json::success)
        {
            request.body = parsed.second;
        }
        sent.push_back(request);
    }
    return sent;
}

// Points Ask Optiq at the mock endpoint for one test, with a provider name no
// key is stored under, a fixed time format, and no timeline selection, then
// puts all of it back.
class AskOptiqFixture
{
public:
    AskOptiqFixture(ImGuiTestContext* ctx, bool want_compute = false)
    : m_ctx(ctx)
    {
        SettingsManager& settings = SettingsManager::GetInstance();
        UserSettings&    user     = settings.GetUserSettings();
        m_saved_assistant         = user.assistant;
        m_saved_time_format       = user.unit_settings.time_format;
        m_saved_visible           = *AssistantPanel::GetInstance()->VisiblePtr();

        MockChatEndpoint& endpoint = MockChatEndpoint::Instance();
        IM_CHECK_NO_RET(endpoint.Running());
        if(!endpoint.Running())
        {
            return;
        }
        UseUrl(endpoint.Url());
        user.unit_settings.time_format                   = TimeFormat::kTimecode;
        *AssistantPanel::GetInstance()->VisiblePtr() = true;
        Panel().Clear();
        endpoint.Script({});

        m_project = AppWindow::GetInstance()->GetCurrentProject();
        IM_CHECK_NO_RET(m_project != nullptr);
        if(m_project == nullptr)
        {
            return;
        }
        m_view         = dynamic_cast<TraceView*>(m_project->GetView().get());
        const bool is_compute = m_project->GetTraceType() == Project::Compute;
        if(want_compute != is_compute)
        {
            ctx->LogWarning("SKIP: needs a %s trace", want_compute ? "compute" : "system");
            return;
        }
        if(m_view != nullptr)
        {
            std::shared_ptr<TimelineSelection> selection = m_view->GetTimelineSelection();
            selection->ClearTimeRange();
            selection->UnselectAllTracks();
            selection->UnselectAllEvents();
        }
        ctx->Yield(3);
        m_ready = true;
    }

    ~AskOptiqFixture()
    {
        Panel().Clear();
        MockChatEndpoint::Instance().LimitBody(0);
        if(m_view != nullptr)
        {
            std::shared_ptr<TimelineSelection> selection = m_view->GetTimelineSelection();
            selection->ClearTimeRange();
            selection->UnselectAllTracks();
            selection->UnselectAllEvents();
        }
        UserSettings& user             = SettingsManager::GetInstance().GetUserSettings();
        user.assistant                 = m_saved_assistant;
        user.unit_settings.time_format = m_saved_time_format;
        *AssistantPanel::GetInstance()->VisiblePtr() = m_saved_visible;
        m_ctx->Yield(2);
    }

    bool          Ready() const { return m_ready; }
    bool          OnSample(const char* sample) const
    {
        return m_project != nullptr &&
               std::filesystem::path(m_project->GetID()).filename() == sample;
    }

    void UseUrl(const std::string& url)
    {
        AssistantProvider provider;
        provider.name         = "ask-optiq-tests";
        provider.endpoint_url = url;
        provider.model        = "mock-model";
        UserSettings& user    = SettingsManager::GetInstance().GetUserSettings();
        user.assistant.providers = { provider };
        user.assistant.active    = 0;
    }

private:
    ImGuiTestContext* m_ctx;
    AssistantSettings m_saved_assistant;
    TimeFormat        m_saved_time_format = TimeFormat::kTimecode;
    bool              m_saved_visible     = false;
    Project*          m_project           = nullptr;
    TraceView*        m_view              = nullptr;
    bool              m_ready             = false;
};

// ---------------------------------------------------------------- goldens

struct GoldenCase
{
    const char* name;
    const char* tool;
    const char* args;
};

// Read tools on rocpd-transpose.db. Track ids and uuids are that file's.
const GoldenCase SYSTEM_GOLDEN[] = {
    { "trace_overview", "trace_overview", "{}" },
    { "trace_overview_8_bins", "trace_overview", R"({"bins":8})" },
    { "trace_overview_queue_5", "trace_overview", R"({"track_id":5})" },
    { "get_summary", "get_summary", "{}" },
    { "get_summary_counter_stats", "get_summary_counter_stats", "{}" },
    { "get_summary_counter_stats_page", "get_summary_counter_stats", R"({"limit":3,"offset":2})" },
    { "get_summary_event_metrics_kernel", "get_summary_event_metrics",
      R"({"category":"kernel","name":"transpose_a(int*, int*, int, int) [clone .kd]"})" },
    { "get_summary_event_metrics_memory", "get_summary_event_metrics",
      R"({"category":"memory","name":"MEMORY_COPY_HOST_TO_DEVICE"})" },
    { "get_summary_event_metrics_thread", "get_summary_event_metrics",
      R"({"category":"thread","name":"hipLaunchKernel"})" },
    { "list_tracks", "list_tracks", "{}" },
    { "top_events_dispatch", "top_events", R"({"category":"dispatch"})" },
    { "top_events_memory_copy", "top_events", R"({"category":"memory_copy"})" },
    { "top_events_memory_alloc", "top_events", R"({"category":"memory_alloc"})" },
    { "top_events_instrumented", "top_events", R"({"category":"instrumented","limit":10})" },
    { "top_events_sampled", "top_events", R"({"category":"sampled","limit":10})" },
    { "top_events_by_invocations", "top_events",
      R"({"category":"dispatch","sort_by":"Invocations","sort_order":"asc"})" },
    { "top_events_queue_5", "top_events", R"({"category":"dispatch","track_ids":[5]})" },
    { "top_events_window", "top_events",
      R"({"category":"dispatch","start_ns":1000000000,"end_ns":2000000000})" },
    { "top_events_filtered", "top_events",
      R"({"category":"instrumented","filters":[{"column":"name","op":"starts_with","value":"hipMemcpy"}]})" },
    { "event_instances_kernel", "event_instances",
      R"({"category":"kernel","name":"transpose_a(int*, int*, int, int) [clone .kd]","limit":5})" },
    { "event_instances_memory", "event_instances",
      R"({"category":"memory","name":"MEMORY_COPY_HOST_TO_DEVICE","limit":5})" },
    { "track_events_queue_5_page_2", "track_events", R"({"track_ids":[5],"limit":5,"offset":5})" },
    { "track_events_longest", "track_events",
      R"({"track_ids":[5,6],"sort_by":"duration","sort_order":"desc","limit":5})" },
    { "track_events_filtered", "track_events",
      R"({"track_ids":[1],"filters":[{"column":"name","op":"=","value":"hipLaunchKernel"}],"limit":5})" },
    { "track_events_grouped", "track_events", R"({"track_ids":[1],"group_by":"name","limit":10})" },
    { "track_events_window", "track_events",
      R"({"track_ids":[5],"start_ns":1000000000,"end_ns":1100000000,"limit":5})" },
    { "search_events_hipmemcpy", "search_events", R"({"terms":["hipMemcpy"],"limit":5})" },
    { "search_events_transpose", "search_events", R"({"terms":["transpose"],"limit":5})" },
    { "search_events_all_terms", "search_events",
      R"({"terms":["hip","Memcpy"],"combine":"all","limit":5})" },
    { "search_events_equals", "search_events",
      R"({"terms":["hipLaunchKernel"],"match":"equals","limit":5})" },
    { "search_events_memory_copy", "search_events",
      R"({"terms":["MEMORY_COPY"],"categories":["memory_copy"],"limit":5})" },
    { "event_details_launch_with_track", "event_details",
      R"({"event_uuid":"1152921504606846991","track_id":1})" },
    { "track_statistics_queue_5", "track_statistics", R"({"track_id":5})" },
    { "track_statistics_copy_queue", "track_statistics", R"({"track_id":9})" },
    { "track_statistics_counter", "track_statistics", R"({"track_id":24})" },
    { "track_statistics_window", "track_statistics",
      R"({"track_id":5,"start_ns":1000000000,"end_ns":2000000000})" },
    { "error_unknown_category", "top_events", R"({"category":"bogus"})" },
    { "error_track_id_text", "track_events", R"({"track_ids":["five"]})" },
    { "error_unknown_filter_column", "track_events",
      R"({"filters":[{"column":"nope","op":"=","value":"1"}]})" },
    { "error_statistics_on_thread_track", "track_statistics", R"({"track_id":1})" },
    { "error_unknown_track", "track_statistics", R"({"track_id":99999})" },
    { "error_unknown_event", "event_details", R"({"event_uuid":"123"})" },
    { "error_search_without_terms", "search_events", "{}" },
    { "error_arguments_not_object", "list_tracks", "[1,2]" },
};

#ifdef ROCPROFVIS_ENABLE_AGENTIC_COMPUTE
// Read tools on rocprof_compute_23ed6f36.db.
const GoldenCase COMPUTE_GOLDEN[] = {
    { "compute_overview", "compute_overview", "{}" },
    { "compute_overview_workload_1", "compute_overview", R"({"workload_id":1})" },
    { "compute_overview_unknown_workload", "compute_overview", R"({"workload_id":999})" },
    { "list_kernels", "list_kernels", "{}" },
    { "list_kernels_first_2", "list_kernels", R"({"limit":2})" },
    { "list_kernels_page_2", "list_kernels", R"({"limit":2,"offset":2})" },
    { "list_kernels_name_contains", "list_kernels", R"({"name_contains":"rocprim"})" },
    { "list_kernels_by_invocations", "list_kernels",
      R"({"sort_by":"invocations","sort_order":"asc"})" },
    { "kernel_summary_4", "kernel_summary", R"({"kernel_id":4})" },
    { "kernel_summary_by_name", "kernel_summary", R"({"kernel_name":"sobol"})" },
    { "list_metrics", "list_metrics", "{}" },
    { "list_metrics_search", "list_metrics", R"({"search":"VALU"})" },
    { "list_metrics_describe", "list_metrics", R"({"describe":["2.1.0","2.1.11"]})" },
    { "kernel_roofline_ceilings", "kernel_roofline", "{}" },
    { "kernel_roofline_4", "kernel_roofline", R"({"kernel_id":4})" },
    { "kernel_triage_4", "kernel_triage", R"({"kernel_id":4})" },
    { "kernel_triage_7", "kernel_triage", R"({"kernel_id":7})" },
    { "get_metrics_4", "get_metrics", R"({"kernel_id":4,"metrics":["2.1.0","2.1.11"]})" },
    { "get_metrics_workload", "get_metrics", R"({"metrics":["2.1.0"],"scope":"workload"})" },
    { "kernel_pc_samples_4", "kernel_pc_samples", R"({"kernel_id":4})" },
    { "error_kernel_not_named", "kernel_summary", "{}" },
    { "error_unknown_kernel", "kernel_summary", R"({"kernel_id":999})" },
    { "error_unknown_metric", "get_metrics", R"({"kernel_id":4,"metrics":["99.9.9"]})" },
    { "error_system_tool_on_compute", "list_tracks", "{}" },
};
#endif

constexpr size_t GOLDEN_BATCH = 10;

std::filesystem::path
GoldenDir()
{
    const char* dir = std::getenv("ROCPROFVIS_GOLDEN_DIR");
    return dir != nullptr ? std::filesystem::path(dir)
                          : std::filesystem::path("src/app/test/golden/ask_optiq");
}

bool
UpdatingGolden()
{
    const char* value = std::getenv("ROCPROFVIS_UPDATE_GOLDEN");
    return value != nullptr && std::string(value) == "1";
}

std::string
ReadText(const std::filesystem::path& path)
{
    std::ifstream      file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    std::string result = text.str();
    result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());
    return result;
}

void
WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream file(path, std::ios::binary);
    file << text;
}

std::string
GoldenText(const GoldenCase& golden, const std::string& output)
{
    return std::string("tool: ") + golden.tool + "\nargs: " + golden.args + "\n\n" + output +
           "\n";
}

// The first line on which two texts differ, 1-based, or 0 when they match.
size_t
FirstDifference(const std::string& expected, const std::string& actual,
                std::string& expected_line, std::string& actual_line)
{
    std::istringstream a(expected);
    std::istringstream b(actual);
    for(size_t line = 1;; ++line)
    {
        const bool more_a = static_cast<bool>(std::getline(a, expected_line));
        const bool more_b = static_cast<bool>(std::getline(b, actual_line));
        if(!more_a && !more_b) return 0;
        if(!more_a) expected_line = "<end of file>";
        if(!more_b) actual_line = "<end of file>";
        if(!more_a || !more_b || expected_line != actual_line) return line;
    }
}

// Runs the cases through the panel, a batch of tool calls per turn, and
// returns each tool reply in case order.
std::vector<std::string>
RunGoldenCases(ImGuiTestContext* ctx, const GoldenCase* cases, size_t count)
{
    std::vector<std::string> outputs;
    AssistantPanelTestPeer   peer = Panel();
    for(size_t begin = 0; begin < count; begin += GOLDEN_BATCH)
    {
        const size_t              end = std::min(count, begin + GOLDEN_BATCH);
        std::vector<MockToolCall> calls;
        for(size_t i = begin; i < end; ++i)
        {
            calls.push_back({ cases[i].tool, cases[i].args });
        }
        MockChatEndpoint::Instance().Script({ Calls(calls), Answer("ok"), Answer("done") });
        peer.Clear();
        peer.Send("golden batch");
        AO_CHECK(WaitIdle(ctx, 180.0));
        std::vector<SentRequest> sent = Sent();
        std::vector<std::pair<std::string, std::string>> replies;
        if(sent.size() >= 2)
        {
            replies = sent[1].NewToolReplies();
        }
        AO_CHECK(replies.size() == end - begin);
        for(size_t i = begin; i < end; ++i)
        {
            const size_t index = i - begin;
            outputs.push_back(index < replies.size() ? replies[index].second
                                                     : std::string("<no reply>"));
        }
    }
    return outputs;
}

// Runs every case twice and compares both passes with each other and with the
// golden files. The briefing for the trace is pinned beside the tools.
void
CheckGoldens(ImGuiTestContext* ctx, const char* group, const char* sample,
             const GoldenCase* cases, size_t count, bool compute)
{
    AskOptiqFixture fx(ctx, compute);
    if(!fx.Ready()) return;
    if(!fx.OnSample(sample))
    {
        ctx->LogWarning("SKIP: these goldens are for %s", sample);
        return;
    }

    const std::vector<std::string> first  = RunGoldenCases(ctx, cases, count);
    const std::vector<std::string> second = RunGoldenCases(ctx, cases, count);
    const std::string briefing = BuildAssistantBriefing(Panel().ToolContext());

    const std::filesystem::path actual_dir =
        std::filesystem::temp_directory_path() / "roc-optiq-golden" / group;
    for(size_t i = 0; i < count; ++i)
    {
        if(first[i] != second[i])
        {
            WriteText(actual_dir / (std::string(cases[i].name) + ".first.txt"), first[i]);
            WriteText(actual_dir / (std::string(cases[i].name) + ".second.txt"), second[i]);
            std::cout << "[ask_optiq] " << group << "/" << cases[i].name
                      << " gave a different reply the second time; both are in "
                      << actual_dir.string() << "\n";
        }
        AO_CHECK(first[i] == second[i]);
    }

    std::vector<std::pair<std::string, std::string>> expected;
    for(size_t i = 0; i < count; ++i)
    {
        expected.emplace_back(cases[i].name, GoldenText(cases[i], first[i]));
    }
    expected.emplace_back("briefing", "briefing\n\n" + briefing + "\n");

    const std::filesystem::path dir = GoldenDir() / group;
    if(UpdatingGolden())
    {
        for(const std::pair<std::string, std::string>& file : expected)
        {
            WriteText(dir / (file.first + ".txt"), file.second);
        }
        ctx->LogWarning("Rewrote %d golden files in %s", static_cast<int>(expected.size()),
                        dir.string().c_str());
        return;
    }
    if(!std::filesystem::exists(dir))
    {
        ctx->LogWarning("SKIP golden comparison: %s not found (run from the source "
                        "root, or set ROCPROFVIS_GOLDEN_DIR)",
                        dir.string().c_str());
        return;
    }
    for(const std::pair<std::string, std::string>& file : expected)
    {
        const std::filesystem::path path = dir / (file.first + ".txt");
        const std::string golden = std::filesystem::exists(path) ? ReadText(path) : std::string();
        std::string       expected_line;
        std::string       actual_line;
        const size_t line = FirstDifference(golden, file.second, expected_line, actual_line);
        if(line != 0)
        {
            const std::filesystem::path actual = actual_dir / (file.first + ".txt");
            WriteText(actual, file.second);
            std::cout << "[ask_optiq] golden mismatch " << group << "/" << file.first
                      << " at line " << line << "\n  expected: " << expected_line
                      << "\n  actual:   " << actual_line << "\n  actual output: "
                      << actual.string() << "\n";
        }
        AO_CHECK(line == 0);
    }
}

void
RegisterGoldenTests(ImGuiTestEngine* e)
{
    ImGuiTest* t = IM_REGISTER_TEST(e, "ask_optiq", "golden_system_tools");
    t->TestFunc  = [](ImGuiTestContext* ctx) {
        CheckGoldens(ctx, "system", SYSTEM_SAMPLE, SYSTEM_GOLDEN,
                     sizeof(SYSTEM_GOLDEN) / sizeof(SYSTEM_GOLDEN[0]), false);
    };

#ifdef ROCPROFVIS_ENABLE_AGENTIC_COMPUTE
    t           = IM_REGISTER_TEST(e, "ask_optiq", "golden_compute_tools");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        CheckGoldens(ctx, "compute", COMPUTE_SAMPLE, COMPUTE_GOLDEN,
                     sizeof(COMPUTE_GOLDEN) / sizeof(COMPUTE_GOLDEN[0]), true);
    };
#endif
}

// ------------------------------------------------------------- turn flow

void
RegisterTurnTests(ImGuiTestEngine* e)
{
    ImGuiTest* t = nullptr;

    // With a sibling call in the batch the model still gets its tools-on round.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "next_steps_with_sibling_keeps_round");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script(
            { Calls({ { "list_tracks", "{}" }, { "offer_next_steps", R"({"steps":["A"]})" } }),
              Answer("draft that is discarded"), Answer("Final two.") });
        peer.Send("q");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 3);
        if(sent.size() == 3)
        {
            AO_CHECK(sent[1].HasTools());
            AO_CHECK(!sent[2].HasTools());
        }
        AO_CHECK(peer.LastAnswer() == "Final two.");
        AO_CHECK(peer.NextSteps().size() == 1);
    };

    // A failed offer_next_steps takes no shortcut.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "invalid_next_steps_takes_no_shortcut");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script(
            { Calls({ { "offer_next_steps", "{}" } }), Answer("draft"), Answer("Final three.") });
        peer.Send("q");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 3);
        if(sent.size() == 3)
        {
            AO_CHECK(sent[1].HasTools());
            AO_CHECK(!Has(sent[1].ToolReply("offer_next_steps"), "Offered next steps"));
        }
        AO_CHECK(peer.NextSteps().empty());
        AO_CHECK(peer.LastAnswer() == "Final three.");
    };

    // A model that never stops calling tools is made to write after the budget.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "tool_round_budget_forces_answer");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        std::vector<MockReply> script;
        for(int round = 0; round < 21; ++round)
        {
            script.push_back(Calls({ { "get_summary_counter_stats", R"({"limit":1})" } }));
        }
        script.push_back(Answer("Forced answer."));
        MockChatEndpoint::Instance().Script(script);
        peer.Send("never stop");
        AO_CHECK(WaitIdle(ctx, 120.0));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 22);
        if(!sent.empty())
        {
            AO_CHECK(!sent.back().HasTools());
            AO_CHECK(Has(sent.back().LastUser(), NUDGE));
        }
        AO_CHECK(peer.LastAnswer() == "Forced answer.");
    };

    // Tool calls on the tools-off round are ignored rather than run. With no
    // prose beside them, the turn ends on the empty-answer notice.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "tool_calls_after_nudge_ignored");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script({ Calls({ { "list_tracks", "{}" } }), Answer("draft"),
                                              Calls({ { "list_tracks", "{}" } }) });
        peer.Send("q");
        AO_CHECK(WaitIdle(ctx));
        AO_CHECK(Sent().size() == 3);
        AO_CHECK(Has(peer.LastNotice(), "The model returned an empty answer."));
        AO_CHECK(!peer.Busy());
    };

    // A tool named inline in the harmony format is still run.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "harmony_inline_call_runs");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script(
            { Answer("<|channel|>commentary to=functions.list_tracks <|constrain|>json<|message|>{}"),
              Answer("done"), Answer("final") });
        peer.Send("q");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 3);
        if(sent.size() >= 2)
        {
            AO_CHECK(Has(sent[1].ToolReply("list_tracks"), "track_id="));
        }
        AO_CHECK(peer.LastAnswer() == "final");
    };

    // Unknown tools and arguments that are not an object are answered, not run.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "unknown_tool_and_bad_arguments");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script(
            { Calls({ { "no_such_tool", "{}" },
                      { "list_tracks", "[1,2]" },
                      { "top_events", "{not json" } }),
              Answer("done"), Answer("final") });
        peer.Send("q");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 3);
        if(sent.size() >= 2)
        {
            AO_CHECK(Has(sent[1].ToolReply("no_such_tool"), "Unknown tool \"no_such_tool\""));
            AO_CHECK(Has(sent[1].ToolReply("list_tracks"), "were not a JSON object"));
            AO_CHECK(Has(sent[1].ToolReply("top_events"), "were not a JSON object"));
        }
    };

    // A reply naming more tools than one round runs is cut to the cap.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "tool_batch_capped");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer    peer = Panel();
        std::vector<MockToolCall> calls(20, { "get_summary_counter_stats", R"({"limit":1})" });
        MockChatEndpoint::Instance().Script({ Calls(calls), Answer("done"), Answer("final") });
        peer.Send("q");
        AO_CHECK(WaitIdle(ctx, 120.0));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 3);
        if(sent.size() >= 2)
        {
            AO_CHECK(sent[1].NewToolReplies().size() == 16);
        }
    };

    // Explain this view sends the overview request for the kind of trace.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "explain_this_view");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script({ Answer("Overview.") });
        peer.Explain();
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 1);
        if(sent.size() == 1)
        {
            AO_CHECK(Has(sent[0].LastUser(), "Explain this view. Call trace_overview and get_summary"));
            AO_CHECK(sent[0].AnyQuestionStartsWith(BRIEFING));
        }
        AO_CHECK(peer.LastUser() == "Explain this view");
        AO_CHECK(peer.LastAnswer() == "Overview.");
    };

    // Old large tool replies are compacted first, small ones are spared, and the
    // newest replies are never touched.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "history_compaction");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer         peer         = Panel();
        std::vector<AssistantMessage>& conversation = peer.Conversation();
        AssistantMessage               question;
        question.role    = "user";
        question.content = std::string(BRIEFING) + "\nkind: system_trace\n\nUser question:\nold\n";
        conversation.push_back(question);
        AssistantMessage call;
        call.role = "assistant";
        for(int i = 0; i < 12; ++i)
        {
            call.tool_calls.push_back({ "c" + std::to_string(i), "list_tracks", "{}" });
        }
        conversation.push_back(call);
        for(int i = 0; i < 12; ++i)
        {
            AssistantMessage reply;
            reply.role         = "tool";
            reply.name         = "list_tracks";
            reply.tool_call_id = "c" + std::to_string(i);
            reply.content      = std::string(i < 4 ? 40000 : 500, static_cast<char>('a' + i));
            conversation.push_back(reply);
        }
        AssistantMessage answer;
        answer.role    = "assistant";
        answer.content = "old answer";
        conversation.push_back(answer);
        peer.BriefedProject() = peer.CurrentProject();

        MockChatEndpoint::Instance().Script({ Answer("compacted") });
        peer.Send("new question");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 1);
        if(sent.size() != 1) return;
        size_t compacted = 0;
        size_t kept_large = 0;
        size_t kept_small = 0;
        for(jt::Json& message : sent[0].Messages())
        {
            if(message["role"].getString() != "tool") continue;
            const std::string content = SentRequest::Content(message);
            if(Has(content, "earlier result omitted")) ++compacted;
            else if(content.size() == 40000) ++kept_large;
            else if(content.size() == 500) ++kept_small;
        }
        AO_CHECK(compacted >= 1);
        AO_CHECK(compacted + kept_large == 4);
        AO_CHECK(kept_small == 8);
    };

}

// ------------------------------------------------------------- compute

#ifdef ROCPROFVIS_ENABLE_AGENTIC_COMPUTE
void
RegisterComputeTests(ImGuiTestEngine* e)
{
    ImGuiTest* t = nullptr;

    // A compute turn is offered the compute tools only, and a system tool is
    // turned away by name.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "compute_turn_uses_compute_tools");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx, true);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script(
            { Calls({ { "list_tracks", "{}" }, { "compute_overview", "{}" } }), Answer("done"),
              Answer("final") });
        peer.Send("compute");
        AO_CHECK(WaitIdle(ctx, 60.0));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 3);
        if(sent.size() < 2) return;
        std::vector<std::string> names = sent[0].ToolNames();
        auto offered = [&](const char* tool) {
            return std::find(names.begin(), names.end(), tool) != names.end();
        };
        AO_CHECK(offered("compute_overview"));
        AO_CHECK(offered("kernel_triage"));
        AO_CHECK(!offered("trace_overview"));
        AO_CHECK(!offered("goto"));
        AO_CHECK(Has(sent[1].ToolReply("list_tracks"), "Unknown tool \"list_tracks\""));
        AO_CHECK(Has(sent[1].ToolReply("compute_overview"), "workload_count:"));
        AO_CHECK(sent[0].AnyQuestionStartsWith(BRIEFING));
        AO_CHECK(sent[0].AnyContains("kind: compute_trace"));
    };

    // Explain this view on a compute trace names the free compute tools.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "compute_explain_this_view");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx, true);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script({ Answer("Compute overview.") });
        peer.Explain();
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 1);
        if(sent.size() == 1)
        {
            AO_CHECK(Has(sent[0].LastUser(), "Explain this view. Call compute_overview"));
        }
    };
}
#endif

}  // namespace

void
RegisterAskOptiqTests(ImGuiTestEngine* e)
{
    RegisterGoldenTests(e);
    RegisterTurnTests(e);
#ifdef ROCPROFVIS_ENABLE_AGENTIC_COMPUTE
    RegisterComputeTests(e);
#endif
}
