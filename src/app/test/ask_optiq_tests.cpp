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

#include "model/rocprofvis_tables_model.h"
#include "model/rocprofvis_timeline_model.h"
#include "model/rocprofvis_trace_data_model.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_project.h"
#include "rocprofvis_requests.h"
#include "rocprofvis_root_view.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_view_test_access.h"
#include "widgets/rocprofvis_tab_container.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <regex>
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

// Known events in rocpd-transpose.db, for the tests that need a real uuid.
constexpr const char* LAUNCH_UUID = "1152921504606846991";  // hipLaunchKernel, track 1
constexpr uint64_t    LAUNCH_TRACK = 1;

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

void
YieldSeconds(ImGuiTestContext* ctx, double seconds)
{
    const auto start = std::chrono::steady_clock::now();
    while(Seconds(start) < seconds)
    {
        ctx->Yield();
    }
}

template <typename Predicate>
bool
WaitFor(ImGuiTestContext* ctx, Predicate predicate, double seconds)
{
    const auto start = std::chrono::steady_clock::now();
    while(!predicate())
    {
        if(Seconds(start) > seconds)
        {
            return false;
        }
        ctx->Yield();
    }
    return true;
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

std::string
ArgsWindow(double start_ns, double end_ns, const std::string& extra = std::string())
{
    return "{\"start_ns\":" + nanosecond_to_str(start_ns) +
           ",\"end_ns\":" + nanosecond_to_str(end_ns) + extra + "}";
}

std::vector<double>
ColumnValues(const std::string& reply, const std::string& column)
{
    std::vector<double> values;
    const std::regex    pattern("(?:^|\\s)" + column + "=\"?(-?[0-9.]+)");
    std::istringstream  lines(reply);
    std::string         line;
    while(std::getline(lines, line))
    {
        std::smatch match;
        if(!line.empty() && std::isdigit(static_cast<unsigned char>(line[0])) &&
           std::regex_search(line, match, pattern))
        {
            values.push_back(std::stod(match[1].str()));
        }
    }
    return values;
}

bool
NonIncreasing(const std::vector<double>& values)
{
    for(size_t i = 1; i < values.size(); ++i)
    {
        if(values[i] > values[i - 1]) return false;
    }
    return true;
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
        RootView* root = dynamic_cast<RootView*>(m_project->GetView().get());
        m_data         = root != nullptr ? root->GetDataProvider() : nullptr;
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
    Project*      ProjectPtr() const { return m_project; }
    DataProvider* Data() const { return m_data; }
    TraceView*    View() const { return m_view; }
    bool          OnSample(const char* sample) const
    {
        return m_project != nullptr &&
               std::filesystem::path(m_project->GetID()).filename() == sample;
    }

    // Repoints the configured endpoint, for the tests that need a host that
    // never answers.
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
    DataProvider*     m_data              = nullptr;
    TraceView*        m_view              = nullptr;
    bool              m_ready             = false;
};

// Removes the tabs a test opened and brings the one it started on back.
class TabRestorer
{
public:
    explicit TabRestorer(ImGuiTestContext* ctx)
    : m_ctx(ctx)
    , m_tabs(AppWindowTestPeer{ *AppWindow::GetInstance() }.TabContainerPtr())
    {
        if(m_tabs == nullptr) return;
        for(const TabItem* tab : m_tabs->GetTabs()) m_start_ids.push_back(tab->m_id);
        if(const TabItem* active = m_tabs->GetActiveTab()) m_active_id = active->m_id;
    }

    ~TabRestorer()
    {
        if(m_tabs == nullptr) return;
        std::vector<std::string> opened;
        for(const TabItem* tab : m_tabs->GetTabs())
        {
            if(std::find(m_start_ids.begin(), m_start_ids.end(), tab->m_id) == m_start_ids.end())
                opened.push_back(tab->m_id);
        }
        for(const std::string& id : opened) m_tabs->RemoveTab(id);
        if(!m_active_id.empty()) m_tabs->SetActiveTab(m_active_id);
        m_ctx->Yield(3);
    }

private:
    ImGuiTestContext*        m_ctx;
    TabContainer*            m_tabs;
    std::vector<std::string> m_start_ids;
    std::string              m_active_id;
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
    { "event_instances_thread_by_start", "event_instances",
      R"({"category":"thread","name":"hipLaunchKernel","limit":5,"sort_by":"start","sort_order":"asc"})" },
    { "track_events_queue_5", "track_events", R"({"track_ids":[5],"limit":5})" },
    { "track_events_queue_5_page_2", "track_events", R"({"track_ids":[5],"limit":5,"offset":5})" },
    { "track_events_longest", "track_events",
      R"({"track_ids":[5,6],"sort_by":"duration","sort_order":"desc","limit":5})" },
    { "track_events_filtered", "track_events",
      R"({"track_ids":[1],"filters":[{"column":"name","op":"=","value":"hipLaunchKernel"}],"limit":5})" },
    { "track_events_grouped", "track_events", R"({"track_ids":[1],"group_by":"name","limit":10})" },
    { "track_events_every_track", "track_events", R"({"limit":3})" },
    { "track_events_window", "track_events",
      R"({"track_ids":[5],"start_ns":1000000000,"end_ns":1100000000,"limit":5})" },
    { "track_samples_gfx_busy", "track_samples", R"({"track_ids":[24],"limit":5})" },
    { "track_samples_grouped", "track_samples", R"({"group_by":"counter","limit":10})" },
    { "track_samples_highest", "track_samples",
      R"({"track_ids":[24],"sort_by":"value","sort_order":"desc","limit":5})" },
    { "search_events_hipmemcpy", "search_events", R"({"terms":["hipMemcpy"],"limit":5})" },
    { "search_events_transpose", "search_events", R"({"terms":["transpose"],"limit":5})" },
    { "search_events_all_terms", "search_events",
      R"({"terms":["hip","Memcpy"],"combine":"all","limit":5})" },
    { "search_events_equals", "search_events",
      R"({"terms":["hipLaunchKernel"],"match":"equals","limit":5})" },
    { "search_events_memory_copy", "search_events",
      R"({"terms":["MEMORY_COPY"],"categories":["memory_copy"],"limit":5})" },
    { "event_details_launch", "event_details", R"({"event_uuid":"1152921504606846991"})" },
    { "event_details_launch_with_track", "event_details",
      R"({"event_uuid":"1152921504606846991","track_id":1})" },
    { "event_details_kernel", "event_details", R"({"event_uuid":"2305843009213693957"})" },
    { "event_details_copy", "event_details", R"({"event_uuid":"4611686018427387905"})" },
    { "track_statistics_queue_5", "track_statistics", R"({"track_id":5})" },
    { "track_statistics_copy_queue", "track_statistics", R"({"track_id":9})" },
    { "track_statistics_counter", "track_statistics", R"({"track_id":24})" },
    { "track_statistics_window", "track_statistics",
      R"({"track_id":5,"start_ns":1000000000,"end_ns":2000000000})" },
    { "error_unknown_category", "top_events", R"({"category":"bogus"})" },
    { "error_track_id_text", "track_events", R"({"track_ids":["five"]})" },
    { "error_uuid_filter", "track_events",
      R"({"filters":[{"column":"__uuid","op":"=","value":"1"}]})" },
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

    // A lone offer_next_steps goes straight to the tools-off round, and the
    // write-up nudge is gone from the history before the next turn.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "next_steps_shortcut_and_nudge_removed");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script(
            { Calls({ { "trace_overview", "{}" }, { "get_summary", "{}" } }),
              Calls({ { "offer_next_steps", R"({"steps":["Look at the copies","Check the busiest queue"]})" } }),
              Answer("Final answer one.") });
        peer.Send("What is slow here?");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 3);
        if(sent.size() == 3)
        {
            AO_CHECK(sent[0].HasTools());
            AO_CHECK(sent[1].HasTools());
            AO_CHECK(!sent[2].HasTools());
            AO_CHECK(Has(sent[2].LastUser(), NUDGE));
            AO_CHECK(sent[0].AnyQuestionStartsWith(BRIEFING));
            AO_CHECK(Has(sent[2].ToolReply("offer_next_steps"), "Offered next steps"));
        }
        AO_CHECK(peer.LastAnswer() == "Final answer one.");
        AO_CHECK(peer.NextSteps().size() == 2);
        AO_CHECK(peer.ChartCount() == 1);
        bool nudge_left = false;
        for(const AssistantMessage& message : peer.Conversation())
            nudge_left = nudge_left || Has(message.content, NUDGE);
        AO_CHECK(!nudge_left);

        MockChatEndpoint::Instance().Script({ Answer("Follow-up answer.") });
        peer.Send("And after that?");
        AO_CHECK(WaitIdle(ctx));
        sent = Sent();
        AO_CHECK(sent.size() == 1);
        if(sent.size() == 1)
        {
            AO_CHECK(sent[0].HasTools());
            AO_CHECK(!sent[0].AnyContains(NUDGE));
            AO_CHECK(sent[0].LastUser().rfind(BRIEFING, 0) != 0);
        }
        AO_CHECK(peer.LastAnswer() == "Follow-up answer.");
    };

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

    // finish_reason=length is flagged under the answer; a normal stop is not.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "truncated_answer_flagged");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script({ Answer("Partial answer that stops", "length") });
        peer.Send("q");
        AO_CHECK(WaitIdle(ctx));
        AO_CHECK(peer.LastAnswer() == "Partial answer that stops");
        AO_CHECK(Has(peer.LastNotice(), "cut off at the model's output limit"));
        const std::string transcript = peer.Transcript();
        AO_CHECK(Has(transcript, "## You\n\nq"));
        AO_CHECK(Has(transcript, "## Optiq\n\nPartial answer that stops"));
        AO_CHECK(Has(transcript, "> Notice: The answer was cut off"));

        peer.Clear();
        MockChatEndpoint::Instance().Script({ Answer("Complete answer.") });
        peer.Send("q2");
        AO_CHECK(WaitIdle(ctx));
        AO_CHECK(peer.LastAnswer() == "Complete answer.");
        AO_CHECK(peer.LastNotice().empty());
    };

    // The trim drops the exchange that carried the briefing; the request going
    // out then still carries one.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "briefing_survives_trim");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer         peer         = Panel();
        std::vector<AssistantMessage>& conversation = peer.Conversation();
        AssistantMessage               message;
        message.role    = "user";
        message.content = std::string(BRIEFING) + "\nkind: system_trace\n\nUser question:\nold\n";
        conversation.push_back(message);
        message.role    = "assistant";
        message.content = "old answer";
        conversation.push_back(message);
        for(int i = 0; i < 29; ++i)
        {
            message.role    = "user";
            message.content = "User question:\nfiller " + std::to_string(i) + "\n";
            conversation.push_back(message);
            message.role    = "assistant";
            message.content = "ok " + std::to_string(i);
            conversation.push_back(message);
        }
        peer.BriefedProject() = peer.CurrentProject();

        MockChatEndpoint::Instance().Script({ Answer("a1"), Answer("a2") });
        peer.Send("q1");
        AO_CHECK(WaitIdle(ctx));
        peer.Send("q2");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 2);
        if(sent.size() == 2)
        {
            AO_CHECK(sent[0].AnyQuestionStartsWith(BRIEFING));
            AO_CHECK(sent[1].AnyQuestionStartsWith(BRIEFING));
            AO_CHECK(sent[0].Messages().size() <= 61);
        }
    };

    // The write-up round fails: the nudge still leaves, and the next turn goes
    // out with tools and every tool reply still behind its call.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "failed_final_round_drops_nudge");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script(
            { Calls({ { "list_tracks", "{}" } }), Answer("draft"),
              Failure(500, R"({"error":{"message":"boom"}})") });
        peer.Send("q1");
        AO_CHECK(WaitIdle(ctx));
        AO_CHECK(peer.LastNotice() == "The endpoint returned HTTP 500: boom");
        bool nudge_left = false;
        for(const AssistantMessage& message : peer.Conversation())
            nudge_left = nudge_left || Has(message.content, NUDGE);
        AO_CHECK(!nudge_left);

        MockChatEndpoint::Instance().Script({ Answer("Recovered answer.") });
        peer.Send("q2");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 1);
        if(sent.size() == 1)
        {
            AO_CHECK(sent[0].HasTools());
            AO_CHECK(!sent[0].AnyContains(NUDGE));
            std::vector<jt::Json>& messages = sent[0].Messages();
            bool                   paired   = true;
            for(size_t i = 0; i < messages.size(); ++i)
            {
                if(messages[i]["role"].getString() != "tool") continue;
                size_t j = i;
                while(j > 0 && messages[j - 1]["role"].getString() == "tool") --j;
                paired = paired && j > 0 &&
                         messages[j - 1]["role"].getString() == "assistant" &&
                         messages[j - 1].contains("tool_calls");
            }
            AO_CHECK(paired);
        }
        AO_CHECK(peer.LastAnswer() == "Recovered answer.");
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

    // Null arguments read as none, and an "error": null beside a good reply is
    // not an error.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "null_arguments_and_null_error");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script(
            { Failure(200, R"({"error":null,"choices":[{"index":0,"finish_reason":"tool_calls",)"
                           R"("message":{"role":"assistant","content":null,"tool_calls":[)"
                           R"({"id":"n1","type":"function","function":{"name":"list_tracks",)"
                           R"("arguments":null}}]}}]})"),
              Answer("done"),
              Failure(200, R"({"error":null,"choices":[{"index":0,"finish_reason":"stop",)"
                           R"("message":{"role":"assistant","content":"final"}}]})") });
        peer.Send("q");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 3);
        if(sent.size() >= 2)
        {
            AO_CHECK(Has(sent[1].ToolReply("list_tracks"), "track_id="));
        }
        AO_CHECK(peer.LastAnswer() == "final");
        AO_CHECK(peer.LastNotice().empty());
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

    // The message cap drops whole earlier exchanges and leaves no tool reply
    // without its call.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "message_cap_keeps_tool_pairs");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer         peer         = Panel();
        std::vector<AssistantMessage>& conversation = peer.Conversation();
        for(int i = 0; i < 30; ++i)
        {
            AssistantMessage question;
            question.role    = "user";
            question.content = (i == 0 ? std::string(BRIEFING) + "\n" : std::string()) +
                               "User question:\nq" + std::to_string(i) + "\n";
            conversation.push_back(question);
            AssistantMessage call;
            call.role = "assistant";
            call.tool_calls.push_back({ "p" + std::to_string(i), "list_tracks", "{}" });
            conversation.push_back(call);
            AssistantMessage reply;
            reply.role         = "tool";
            reply.name         = "list_tracks";
            reply.tool_call_id = "p" + std::to_string(i);
            reply.content      = "rows";
            conversation.push_back(reply);
        }
        peer.BriefedProject() = peer.CurrentProject();
        MockChatEndpoint::Instance().Script({ Answer("trimmed") });
        peer.Send("latest");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 1);
        if(sent.size() != 1) return;
        std::vector<jt::Json>& messages = sent[0].Messages();
        AO_CHECK(messages.size() <= 62);
        AO_CHECK(messages.size() > 1 && messages[1]["role"].getString() == "user");
        bool paired = true;
        for(size_t i = 0; i < messages.size(); ++i)
        {
            if(messages[i]["role"].getString() != "tool") continue;
            paired = paired && i > 0 && messages[i - 1].contains("tool_calls");
        }
        AO_CHECK(paired);
        AO_CHECK(sent[0].AnyQuestionStartsWith(BRIEFING));
    };
}

// ------------------------------------------------------------- transport

#ifndef _WIN32
std::atomic<int> g_sigpipes{ 0 };

void
CountSigpipe(int)
{
    ++g_sigpipes;
}
#endif

void
RegisterTransportTests(ImGuiTestEngine* e)
{
    ImGuiTest* t = nullptr;

    // Transient failures are retried within the turn; others are reported with
    // the HTTP status first.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "retry_and_error_text");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer     = Panel();
        MockChatEndpoint&      endpoint = MockChatEndpoint::Instance();

        endpoint.Script({ Failure(503, R"({"error":{"message":"overloaded"}})", "1"),
                          Answer("After retry.") });
        peer.Send("q");
        AO_CHECK(WaitIdle(ctx, 20.0));
        AO_CHECK(peer.LastAnswer() == "After retry.");
        AO_CHECK(peer.LastNotice().empty());
        AO_CHECK(Sent().size() == 2);

        peer.Clear();
        MockReply dropped;
        dropped.drop = true;
        endpoint.Script({ dropped, Answer("After a dropped connection.") });
        peer.Send("q");
        AO_CHECK(WaitIdle(ctx, 20.0));
        AO_CHECK(peer.LastAnswer() == "After a dropped connection.");
        AO_CHECK(Sent().size() == 2);

        struct Case
        {
            int         status;
            const char* body;
            const char* notice;
        };
        const Case cases[] = {
            { 401, R"({"error":{"message":"Invalid key"}})", "The endpoint returned HTTP 401: Invalid key" },
            { 500, R"({"error":{"message":"boom"}})", "The endpoint returned HTTP 500: boom" },
            { 404, "Not Found", "The endpoint returned HTTP 404. Requested http://127.0.0.1:" },
        };
        for(const Case& c : cases)
        {
            peer.Clear();
            endpoint.Script({ Failure(c.status, c.body) });
            peer.Send("q");
            AO_CHECK(WaitIdle(ctx, 20.0));
            AO_CHECK(Has(peer.LastNotice(), c.notice));
            AO_CHECK(Sent().size() == 1);
        }
    };

    // A server that resets the connection while the request is still being
    // written fails the request; it must not raise SIGPIPE.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "reset_during_upload_survives");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().LimitBody(4096);
        MockChatEndpoint::Instance().Script({ Answer("never") });
#ifndef _WIN32
        g_sigpipes                          = 0;
        void (*previous)(int)               = std::signal(SIGPIPE, CountSigpipe);
#endif
        peer.Send(std::string(2 * 1024 * 1024, 'x'));
        AO_CHECK(WaitIdle(ctx, 30.0));
#ifndef _WIN32
        std::signal(SIGPIPE, previous);
        AO_CHECK(g_sigpipes == 0);
#endif
        AO_CHECK(Has(peer.LastNotice(), "request failed") || Has(peer.LastNotice(), "HTTP 413"));
        AO_CHECK(peer.LastAnswer().empty());
    };

    // Clear while a connect hangs returns at once; the cancel finishes in the
    // background and Update() lets it go.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "clear_during_connect_does_not_block");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        // TEST-NET-1: reserved for documentation, so packets to it are dropped.
        fx.UseUrl("http://192.0.2.1:9/v1");
        peer.Send("q");
        AO_CHECK(WaitFor(ctx, [&]() { return peer.InHttpWait(); }, 30.0));
        YieldSeconds(ctx, 0.5);
        if(!peer.Busy())
        {
            ctx->LogWarning("SKIP: the connect failed at once, so there is no hang to clear");
            return;
        }
        const auto start = std::chrono::steady_clock::now();
        peer.Clear();
        const double blocked = Seconds(start);
        std::cout << "[ask_optiq] Clear during a hung connect took " << blocked << " s\n";
        AO_CHECK(blocked < 0.25);
        AO_CHECK(!peer.Busy());
        AO_CHECK(peer.AbandonedRequests() == 1);
        AO_CHECK(WaitFor(ctx, [&]() { return peer.AbandonedRequests() == 0; }, 30.0));
    };
}

// ------------------------------------------------------------- data tools

void
RegisterDataToolTests(ImGuiTestEngine* e)
{
    ImGuiTest* t = nullptr;

    // window_ns leads every table read and names where the window came from;
    // the tracks line appears only when tracks were left out.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "table_reads_lead_with_window");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer     = Panel();
        const TimelineModel&   timeline = fx.Data()->DataModel().GetTimeline();
        const double           start    = timeline.GetStartTime();
        const double           end      = timeline.GetEndTime();
        const double           q1       = start + (end - start) * 0.25;
        const double           q3       = start + (end - start) * 0.75;

        MockChatEndpoint::Instance().Script(
            { Calls({ { "top_events", R"({"category":"dispatch","limit":3})" },
                      { "track_samples", R"({"limit":2})" },
                      { "search_events", R"({"terms":["hip"],"limit":2})" },
                      { "track_events", ArgsWindow(q3, end + 1e9, ",\"limit\":2") },
                      { "track_events", ArgsWindow(q1, q3, ",\"limit\":2") },
                      { "track_events", ArgsWindow(end + 1e9, end + 2e9, ",\"limit\":2") },
                      { "track_events", "{\"start_ns\":" + nanosecond_to_str(q1) + "}" } }),
              Answer("done"), Answer("final") });
        peer.Send("window lines");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        AO_CHECK(sent.size() == 3);
        if(sent.size() >= 2)
        {
            for(const char* tool : { "top_events", "track_samples", "search_events" })
            {
                const std::string reply = sent[1].ToolReply(tool);
                AO_CHECK(reply.rfind("window_ns: ", 0) == 0);
                AO_CHECK(Has(reply.substr(0, reply.find('\n')), "(whole trace)"));
            }
            AO_CHECK(Has(sent[1].ToolReply("track_events", 0), "(requested, clamped to the trace)"));
            AO_CHECK(Has(sent[1].ToolReply("track_events", 1), "(requested)"));
            AO_CHECK(Has(sent[1].ToolReply("track_events", 2),
                         "(whole trace - the start_ns/end_ns you passed were incomplete or "
                         "outside the trace and were ignored)"));
            AO_CHECK(Has(sent[1].ToolReply("track_events", 3), "were ignored)"));
        }

        std::vector<uint64_t> dispatch;
        for(const TrackInfo* track : timeline.GetTrackList())
        {
            if(track != nullptr && track->track_type == kRPVControllerTrackTypeEvents &&
               track->operation_types.count(kRocProfVisDmOperationDispatch) > 0)
                dispatch.push_back(track->id);
        }
        std::shared_ptr<TimelineSelection> selection = fx.View()->GetTimelineSelection();
        selection->SelectTimeRange(q1, q3);
        for(int pass = 0; pass < 2 && dispatch.size() > 1; ++pass)
        {
            selection->UnselectAllTracks();
            if(pass == 0) selection->SetSelectedTracks(dispatch);
            else selection->SetSelectedTracks({ dispatch[0] });
            ctx->Yield(3);
            peer.Clear();
            MockChatEndpoint::Instance().Script(
                { Calls({ { "top_events", R"({"category":"dispatch","limit":3})" } }),
                  Answer("done"), Answer("final") });
            peer.Send("with a selection");
            AO_CHECK(WaitIdle(ctx));
            sent = Sent();
            if(sent.size() < 2) continue;
            const std::string reply = sent[1].ToolReply("top_events");
            AO_CHECK(Has(reply, "(timeline selection - goto sets this"));
            if(pass == 0) AO_CHECK(!Has(reply, "tracks:"));
            else
                AO_CHECK(Has(reply, "tracks: the 1 selected, of " + std::to_string(dispatch.size()) +
                                        " in all - pass track_ids to read the others"));
        }
    };

    // A sort column guessed from a stale header is corrected by one re-run, and
    // a sort_by that names no column says what the rows are sorted by.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "sort_rerun_and_unknown_sort");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        for(int pass = 0; pass < 2; ++pass)
        {
            if(pass == 0)
                fx.Data()->DataModel().GetTables().ClearTable(TableType::kAssistantEventTable);
            peer.Clear();
            MockChatEndpoint::Instance().Script(
                { Calls({ { "track_events", R"({"sort_by":"duration","sort_order":"desc","limit":8})" } }),
                  Answer("done"), Answer("final") });
            uint32_t max_retries = 0;
            peer.Send("sorted");
            AO_CHECK(WaitIdle(ctx, 60.0, &max_retries));
            std::vector<SentRequest> sent = Sent();
            if(sent.size() < 2) continue;
            const std::string   reply     = sent[1].ToolReply("track_events");
            std::vector<double> durations = ColumnValues(reply, "duration");
            AO_CHECK(durations.size() >= 2);
            AO_CHECK(NonIncreasing(durations));
            AO_CHECK(!Has(reply, "note: sort_by"));
            AO_CHECK(max_retries == (pass == 0 ? 1u : 0u));
        }

        fx.Data()->DataModel().GetTables().ClearTable(TableType::kAssistantTopEventsTable);
        peer.Clear();
        MockChatEndpoint::Instance().Script(
            { Calls({ { "track_events", R"({"sort_by":"bogus","limit":3})" },
                      { "top_events", R"({"category":"dispatch","limit":6})" } }),
              Answer("done"), Answer("final") });
        peer.Send("unknown sort");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        if(sent.size() >= 2)
        {
            AO_CHECK(Has(sent[1].ToolReply("track_events"),
                         "note: sort_by=\"bogus\" is not a column of this result; rows are sorted by"));
            std::vector<double> totals = ColumnValues(sent[1].ToolReply("top_events"), "DurationTotal");
            AO_CHECK(totals.size() >= 2);
            AO_CHECK(NonIncreasing(totals));
        }
    };

    // A read whose reply leaves no params behind is reported as failed, never
    // formatted from rows an earlier read left in the slot.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "failed_read_reported_as_failed");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script(
            { Calls({ { "track_events", R"({"limit":4})" } }), Answer("done"), Answer("final") });
        peer.Send("read that fails");
        bool       nulled = false;
        const auto start  = std::chrono::steady_clock::now();
        // The reply lands in one frame's tab update and is formatted in the
        // next frame's panel update; dropping the params in between stands in
        // for a reply that failed.
        while(peer.Busy() && Seconds(start) < 30.0)
        {
            if(!nulled && peer.WaitingOn(AssistantFetchKind::kDataTable) &&
               !fx.Data()->IsRequestPending(DataProvider::ASSISTANT_EVENT_TABLE_REQUEST_ID))
            {
                fx.Data()->DataModel().GetTables().SetTableParams(TableType::kAssistantEventTable,
                                                                  nullptr);
                nulled = true;
            }
            ctx->Yield();
        }
        AO_CHECK(nulled);
        std::vector<SentRequest> sent = Sent();
        if(sent.size() >= 2)
        {
            const std::string reply = sent[1].ToolReply("track_events");
            AO_CHECK(reply.rfind("window_ns: ", 0) == 0);
            AO_CHECK(Has(reply, "The query failed, so no rows were read."));
            AO_CHECK(!Has(reply, "columns:"));
        }
    };

    // A track_statistics result dropped by a range change in the same batch is
    // fetched again rather than reported as not ready.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "track_statistics_after_goto");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        if(!fx.OnSample(SYSTEM_SAMPLE))
        {
            ctx->LogWarning("SKIP: uses track ids from %s", SYSTEM_SAMPLE);
            return;
        }
        AssistantPanelTestPeer peer     = Panel();
        const TimelineModel&   timeline = fx.Data()->DataModel().GetTimeline();
        const double           start    = timeline.GetStartTime();
        const double           end      = timeline.GetEndTime();
        MockChatEndpoint::Instance().Script(
            { Calls({ { "goto", ArgsWindow(start + (end - start) * 0.25, start + (end - start) * 0.75) },
                      { "track_statistics", R"({"track_id":5})" },
                      { "track_statistics", R"({"track_id":24})" } }),
              Answer("done"), Answer("final") });
        peer.Send("stats");
        AO_CHECK(WaitIdle(ctx, 90.0));
        std::vector<SentRequest> sent = Sent();
        if(sent.size() >= 2)
        {
            for(int i = 0; i < 2; ++i)
            {
                const std::string reply = sent[1].ToolReply("track_statistics", i);
                AO_CHECK(reply.rfind("window_ns: ", 0) == 0);
                AO_CHECK(!Has(reply, "not ready"));
                AO_CHECK(!Has(reply, "Timed out"));
            }
        }
    };

    // While the UI loads a table of the same controller type, the tool waits for
    // that table instead of re-running every frame, and then reads real rows.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "waits_on_busy_ui_table");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        std::vector<uint64_t>  event_tracks;
        for(const TrackInfo* track : fx.Data()->DataModel().GetTimeline().GetTrackList())
        {
            if(track != nullptr && track->track_type == kRPVControllerTrackTypeEvents)
                event_tracks.push_back(track->id);
        }
        const TimelineModel& timeline = fx.Data()->DataModel().GetTimeline();
        MockChatEndpoint::Instance().Script(
            { Calls({ { "track_events", R"({"limit":3})" } }), Answer("done"), Answer("final") });
        peer.Send("busy UI table");
        constexpr int kBusyFrames      = 40;
        int           tool_frames      = 0;
        uint32_t      busy_max_retries = 0;
        const auto    start            = std::chrono::steady_clock::now();
        while(peer.Busy() && Seconds(start) < 60.0)
        {
            if(peer.InToolWait()) ++tool_frames;
            if(tool_frames < kBusyFrames)
            {
                if(!fx.Data()->IsTableRequestPending(kRPVControllerTableTypeEvents))
                {
                    fx.Data()->FetchTable(TrackTableRequestParams(
                        kRPVControllerTableTypeEvents, event_tracks, timeline.GetStartTime(),
                        timeline.GetEndTime(), nullptr, "", "", "", 0, 2000, 0,
                        kRPVControllerSortOrderAscending));
                }
                busy_max_retries = std::max(busy_max_retries, peer.FetchRetries());
            }
            ctx->Yield();
        }
        AO_CHECK(busy_max_retries <= 1);
        std::vector<SentRequest> sent = Sent();
        if(sent.size() >= 2)
        {
            const std::string reply = sent[1].ToolReply("track_events");
            AO_CHECK(Has(reply, "columns:"));
            AO_CHECK(!Has(reply, "Could not queue"));
        }
    };
}

// ------------------------------------------------------------- UI tools

void
RegisterUiToolTests(ImGuiTestEngine* e)
{
    ImGuiTest* t = nullptr;

    // goto refuses a window outside the trace, clamps a partial one, and lets
    // named events supply the window when it drops one.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "goto_range_validation");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        if(!fx.OnSample(SYSTEM_SAMPLE))
        {
            ctx->LogWarning("SKIP: uses an event uuid from %s", SYSTEM_SAMPLE);
            return;
        }
        AssistantPanelTestPeer             peer      = Panel();
        std::shared_ptr<TimelineSelection> selection = fx.View()->GetTimelineSelection();
        const TimelineModel&               timeline  = fx.Data()->DataModel().GetTimeline();
        const double                       start     = timeline.GetStartTime();
        const double                       end       = timeline.GetEndTime();
        auto run = [&](const std::string& args) {
            peer.Clear();
            MockChatEndpoint::Instance().Script(
                { Calls({ { "goto", args } }), Answer("done"), Answer("final") });
            peer.Send("show me");
            WaitIdle(ctx);
            std::vector<SentRequest> sent = Sent();
            return sent.size() >= 2 ? sent[1].ToolReply("goto") : std::string("<no reply>");
        };

        std::string reply = run(ArgsWindow(end + 1e9, end + 2e9));
        AO_CHECK(Has(reply, "That range is outside this trace, whose window_ns is " +
                                nanosecond_to_str(start) + " .. " + nanosecond_to_str(end) + "."));
        AO_CHECK(!selection->HasValidTimeRangeSelection());

        reply = run(ArgsWindow(start + (end - start) * 0.75, end + 1e9));
        AO_CHECK(Has(reply, "Moved the timeline to"));
        double selected_start = 0.0;
        double selected_end   = 0.0;
        AO_CHECK(selection->GetSelectedTimeRange(selected_start, selected_end));
        AO_CHECK(selected_start > start);
        AO_CHECK(std::abs(selected_end - end) <= 1.0);

        selection->ClearTimeRange();
        reply = run(ArgsWindow(end + 1e9, end + 2e9,
                               std::string(",\"events\":[{\"__uuid\":\"") + LAUNCH_UUID +
                                   "\",\"__trackId\":" + std::to_string(LAUNCH_TRACK) + "}]"));
        AO_CHECK(Has(reply, "selected one event as the focal point"));
        AO_CHECK(Has(reply, "the range you gave is outside this trace"));
        AO_CHECK(!Has(reply, "no window was given"));
        AO_CHECK(selection->HasSelectedEvents());

        selection->UnselectAllEvents();
        selection->ClearTimeRange();
        reply = run(std::string("{\"event_uuid\":\"") + LAUNCH_UUID + "\",\"__trackId\":" +
                    std::to_string(LAUNCH_TRACK) + "}");
        AO_CHECK(Has(reply, "selected one event as the focal point"));
        selection->UnselectAllEvents();
        selection->ClearTimeRange();
    };

    // event_details answers from a uuid alone, flow links included.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "event_details_without_track_id");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        if(!fx.OnSample(SYSTEM_SAMPLE))
        {
            ctx->LogWarning("SKIP: uses an event uuid from %s", SYSTEM_SAMPLE);
            return;
        }
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script(
            { Calls({ { "event_details", std::string("{\"event_uuid\":\"") + LAUNCH_UUID + "\"}" } }),
              Answer("done"), Answer("final") });
        peer.Send("details");
        AO_CHECK(WaitIdle(ctx));
        std::vector<SentRequest> sent = Sent();
        if(sent.size() >= 2)
        {
            const std::string reply = sent[1].ToolReply("event_details");
            AO_CHECK(Has(reply, "name: hipLaunchKernel"));
            AO_CHECK(Has(reply, "flow:"));
            AO_CHECK(!Has(reply, "No details came back"));
        }
    };

    // switch_tab reaches the other trace when two tabs share a file name.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "switch_tab_between_same_named_traces");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        namespace fs = std::filesystem;
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AppWindow*        app      = AppWindow::GetInstance();
        const std::string original = fx.ProjectPtr()->GetID();
        const fs::path    dup_dir  = fs::temp_directory_path() / "roc-optiq-ask-optiq-dup";
        std::error_code   error;
        // Opening the copy files it under recent files, and can push the
        // user's oldest entry off the end of a full list.
        std::list<std::string>& recent =
            SettingsManager::GetInstance().GetInternalSettings().recent_files;
        const std::list<std::string> saved_recent = recent;
        {
            TabRestorer tabs(ctx);
            fs::create_directories(dup_dir, error);
            const fs::path dup = dup_dir / fs::path(original).filename();
            fs::copy_file(original, dup, fs::copy_options::overwrite_existing, error);
            AO_CHECK(!error);
            const std::string dup_id = fs::weakly_canonical(dup).string();
            app->OpenFile(dup.string());
            const bool loaded = WaitFor(ctx, [&]() {
                Project* project = app->GetProject(dup_id);
                if(project == nullptr) return false;
                RootView* root = dynamic_cast<RootView*>(project->GetView().get());
                return root != nullptr && root->GetDataProvider() != nullptr &&
                       root->GetDataProvider()->GetState() == ProviderState::kReady;
            }, 90.0);
            AO_CHECK(loaded);
            if(loaded)
            {
                ctx->Yield(10);
                AssistantPanelTestPeer peer = Panel();
                const std::string      name = fs::path(original).filename().string();
                for(int hop = 0; hop < 2; ++hop)
                {
                    const std::string before = app->GetCurrentProject()->GetID();
                    const std::string expect = before == original ? dup_id : original;
                    peer.Clear();
                    MockChatEndpoint::Instance().Script(
                        { Calls({ { "switch_tab", "{\"name\":\"" + name + "\"}" } }),
                          Answer("done"), Answer("final") });
                    peer.Send("switch to the other " + name + " tab");
                    AO_CHECK(WaitIdle(ctx));
                    AO_CHECK(app->GetCurrentProject()->GetID() == expect);
                    std::vector<SentRequest> sent = Sent();
                    if(sent.size() >= 2)
                    {
                        AO_CHECK(Has(sent[1].ToolReply("switch_tab"),
                                     "Switched to trace " + name + " (tab "));
                    }
                }
                peer.Clear();
            }
        }
        recent = saved_recent;
        fs::remove_all(dup_dir, error);
    };

    // Clicking a next step sends it and keeps what the user was typing.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "next_step_click_keeps_draft");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script(
            { Calls({ { "offer_next_steps", R"({"steps":["Step one","Step two"]})" } }),
              Answer("Final with steps.") });
        peer.Send("give me steps");
        AO_CHECK(WaitIdle(ctx));
        AO_CHECK(peer.NextSteps().size() == 2);
        ctx->Yield(5);
        ctx->SetRef("Main Window");
        ctx->ItemInput("**/##assistant_input");
        ctx->KeyCharsReplace("my draft question");
        ctx->Yield(2);
        AO_CHECK(peer.Input() == "my draft question");

        MockChatEndpoint::Instance().Script({ Answer("Answer to step one.") });
        ctx->ItemClick("**/1. Step one");
        AO_CHECK(WaitIdle(ctx));
        AO_CHECK(peer.LastUser() == "Step one");
        AO_CHECK(peer.LastAnswer() == "Answer to step one.");
        AO_CHECK(peer.Input() == "my draft question");
        std::vector<SentRequest> sent = Sent();
        if(!sent.empty()) AO_CHECK(Has(sent[0].LastUser(), "Step one"));
    };

    // Numpad Enter sends like Enter.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "numpad_enter_sends");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer = Panel();
        MockChatEndpoint::Instance().Script({ Answer("Numpad answer.") });
        ctx->Yield(3);
        ctx->SetRef("Main Window");
        ctx->ItemInput("**/##assistant_input");
        ctx->KeyCharsReplace("numpad question");
        ctx->KeyPress(ImGuiKey_KeypadEnter);
        AO_CHECK(WaitIdle(ctx));
        AO_CHECK(peer.LastUser() == "numpad question");
        AO_CHECK(peer.LastAnswer() == "Numpad answer.");
    };

#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    // A long wait keeps the tool's own status without pinning the transcript,
    // and clearing takes back a pending script offer.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "script_offer_withdrawn_on_clear");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer     = Panel();
        AnalysisView*          analysis = TraceViewTestPeer{ *fx.View() }.AnalysisViewPtr();
        AO_CHECK(analysis != nullptr);
        if(analysis == nullptr) return;
        std::shared_ptr<ScriptEditor> editor = analysis->GetScriptEditor();
        AO_CHECK(editor != nullptr);
        if(!editor) return;

        MockChatEndpoint::Instance().Script(
            { Calls({ { "run_analysis_script",
                        R"JSON({"script":"optiq.result.text('hi')"})JSON" } }) });
        peer.Send("run a script");
        AO_CHECK(WaitFor(ctx, [&]() { return peer.WaitingOn(AssistantFetchKind::kScript); }, 30.0));
        AO_CHECK(editor->ProposalState() == ScriptApproval::kPending);

        // Hidden, so nothing consumes a scroll request and any SetStatus shows.
        *AssistantPanel::GetInstance()->VisiblePtr() = false;
        ctx->Yield(2);
        peer.ScrollToBottom() = false;
        YieldSeconds(ctx, 6.5);
        AO_CHECK(peer.Status().rfind("Waiting for you to review a script...", 0) == 0);
        AO_CHECK(Has(peer.Status(), "s)"));
        AO_CHECK(!peer.ScrollToBottom());
        *AssistantPanel::GetInstance()->VisiblePtr() = true;
        ctx->Yield(2);

        peer.Clear();
        ctx->Yield(3);
        AO_CHECK(editor->ProposalState() != ScriptApproval::kPending);
        AO_CHECK(!peer.Busy());
        AO_CHECK(Sent().size() == 1);
    };

    // Stopping an approved run is reported as the user's choice, not a failure.
    t           = IM_REGISTER_TEST(e, "ask_optiq", "script_stop_is_rejection");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        AskOptiqFixture fx(ctx);
        if(!fx.Ready()) return;
        AssistantPanelTestPeer peer     = Panel();
        AnalysisView*          analysis = TraceViewTestPeer{ *fx.View() }.AnalysisViewPtr();
        std::shared_ptr<ScriptEditor> editor = analysis ? analysis->GetScriptEditor() : nullptr;
        AO_CHECK(editor != nullptr);
        if(!editor) return;
        MockChatEndpoint::Instance().Script(
            { Calls({ { "run_analysis_script",
                        R"JSON({"script":"n = 0\nfor i in range(400000000):\n    n += i\noptiq.result.text(str(n))"})JSON" } }),
              Answer("noted"), Answer("final") });
        peer.Send("long script");
        AO_CHECK(WaitFor(ctx, [&]() {
            return peer.WaitingOn(AssistantFetchKind::kScript) &&
                   editor->ProposalState() == ScriptApproval::kPending;
        }, 30.0));
        ScriptEditorTestPeer{ *editor }.Run();
        AO_CHECK(WaitFor(ctx, [&]() { return editor->ProposalState() == ScriptApproval::kRunning; },
                         15.0));
        YieldSeconds(ctx, 1.0);
        ScriptEditorTestPeer{ *editor }.Cancel();
        AO_CHECK(WaitIdle(ctx, 60.0));
        std::vector<SentRequest> sent = Sent();
        if(sent.size() >= 2)
        {
            const std::string reply = sent[1].ToolReply("run_analysis_script");
            AO_CHECK(Has(reply, "stopped it before it finished"));
            AO_CHECK(!Has(reply, "The script failed"));
        }
        YieldSeconds(ctx, 3.0);
    };
#endif
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
    RegisterTransportTests(e);
    RegisterDataToolTests(e);
    RegisterUiToolTests(e);
#ifdef ROCPROFVIS_ENABLE_AGENTIC_COMPUTE
    RegisterComputeTests(e);
#endif
}
