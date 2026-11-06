// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_trace_server.h"

#include <charconv>
#include <limits>
#include <system_error>
#include <utility>

#include "httplib.h"

#include "model/compute/rocprofvis_compute_data_model.h"
#include "model/rocprofvis_trace_data_model.h"
#include "rocprofvis_data_provider_json.h"

namespace RocProfVis
{
namespace Server
{

// Long enough for a snapshot of a large trace; short enough that a stuck
// provider thread reaches the client as an error rather than a hung request.
constexpr std::chrono::seconds PROVIDER_REPLY_TIMEOUT(30);

constexpr int32_t     HTTP_OK           = 200;
constexpr int32_t     HTTP_BAD_REQUEST  = 400;
constexpr int32_t     HTTP_NOT_FOUND    = 404;
constexpr int32_t     HTTP_UNAVAILABLE  = 503;
constexpr const char* JSON_CONTENT_TYPE = "application/json";

// A route capture or query value as a whole uint64; false for anything else.
static bool
parse_uint64(const std::string& text, uint64_t& value_out)
{
    const char*            last   = text.data() + text.size();
    std::from_chars_result result = std::from_chars(text.data(), last, value_out);
    return !text.empty() && result.ec == std::errc() && result.ptr == last;
}

static bool
parse_uint32(const std::string& text, uint32_t& value_out)
{
    uint64_t value = 0;
    bool     parsed =
        parse_uint64(text, value) && value <= std::numeric_limits<uint32_t>::max();
    if(parsed)
    {
        value_out = static_cast<uint32_t>(value);
    }
    return parsed;
}

// The kernel a /workloads/<id>/kernels/<id> route names, or nullptr.
static const View::KernelInfo*
find_kernel(View::DataProvider& provider, const std::vector<std::string>& captures)
{
    uint32_t                workload_id = 0;
    uint32_t                kernel_id   = 0;
    const View::KernelInfo* kernel      = nullptr;
    if(parse_uint32(captures[0], workload_id) && parse_uint32(captures[1], kernel_id))
    {
        kernel = provider.ComputeModel().GetKernelInfo(workload_id, kernel_id);
    }
    return kernel;
}

TraceServer::TraceServer(View::DataProvider& provider, const std::string& allowed_origin)
: m_provider(provider)
, m_allowed_origin(allowed_origin)
, m_http(std::make_unique<httplib::Server>())
, m_stopping(false)
{
    RegisterRoutes();
}

TraceServer::~TraceServer() { Stop(); }

bool
TraceServer::Start(const std::string& host, uint16_t port)
{
    bool bound = m_http->bind_to_port(host, port);
    if(bound)
    {
        m_listener = std::thread([this]() { m_http->listen_after_bind(); });
    }
    return bound;
}

void
TraceServer::Stop()
{
    std::deque<Task> abandoned;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
        abandoned.swap(m_tasks);
    }
    for(Task& task : abandoned)
    {
        task.reply->set_value(Error(HTTP_UNAVAILABLE, "The server is shutting down."));
    }
    m_http->stop();
    if(m_listener.joinable())
    {
        m_listener.join();
    }
}

void
TraceServer::RunPendingRequests()
{
    std::deque<Task> tasks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        tasks.swap(m_tasks);
    }
    for(Task& task : tasks)
    {
        task.reply->set_value(task.work());
    }
}

void
TraceServer::WaitForRequests(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_task_queued.wait_for(lock, timeout, [this]() { return !m_tasks.empty(); });
}

TraceServer::Reply
TraceServer::OnProviderThread(std::function<Reply()> work)
{
    std::shared_ptr<std::promise<Reply>> promise =
        std::make_shared<std::promise<Reply>>();
    std::future<Reply> reply  = promise->get_future();
    bool               queued = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(!m_stopping)
        {
            m_tasks.push_back(Task{ std::move(work), promise });
            queued = true;
        }
    }

    Reply result = Error(HTTP_UNAVAILABLE, "The server is shutting down.");
    if(queued)
    {
        m_task_queued.notify_one();
        if(reply.wait_for(PROVIDER_REPLY_TIMEOUT) == std::future_status::ready)
        {
            result = reply.get();
        }
        else
        {
            result =
                Error(HTTP_UNAVAILABLE, "The trace provider did not answer in time.");
        }
    }
    return result;
}

void
TraceServer::Route(const std::string&                                          pattern,
                   std::function<Reply(View::DataProvider&, const RouteArgs&)> handler)
{
    m_http->Get(pattern, [this, handler](const httplib::Request& request,
                                         httplib::Response&      response) {
        RouteArgs args;
        for(size_t i = 1; i < request.matches.size(); i++)
        {
            args.captures.push_back(request.matches[i].str());
        }
        for(const std::pair<std::string, std::string>& param : request.params)
        {
            args.params.emplace(param.first, param.second);
        }
        Reply reply = OnProviderThread(
            [this, handler, args]() { return handler(m_provider, args); });
        response.status = reply.status;
        response.set_content(request.has_param("pretty") ? reply.body.toStringPretty()
                                                         : reply.body.toString(),
                             JSON_CONTENT_TYPE);
    });
}

TraceServer::Reply
TraceServer::Ok(jt::Json body)
{
    return Reply{ HTTP_OK, std::move(body) };
}

TraceServer::Reply
TraceServer::Error(int32_t status, const std::string& message)
{
    jt::Json body;
    body["error"] = message;
    return Reply{ status, std::move(body) };
}

void
TraceServer::RegisterRoutes()
{
    if(!m_allowed_origin.empty())
    {
        m_http->set_post_routing_handler(
            [this](const httplib::Request&, httplib::Response& response) {
                response.set_header("Access-Control-Allow-Origin", m_allowed_origin);
            });
    }
    // Requests that match no route still get a JSON body.
    m_http->set_error_handler([](const httplib::Request&, httplib::Response& response) {
        if(response.body.empty())
        {
            jt::Json body;
            body["error"] = httplib::status_message(response.status);
            response.set_content(body.toString(), JSON_CONTENT_TYPE);
        }
    });

    Route("/api/status", [](View::DataProvider& provider, const RouteArgs&) {
        return Ok(View::DataProviderJson::Status(provider));
    });
    Route("/api/snapshot", [](View::DataProvider& provider, const RouteArgs&) {
        return Ok(View::DataProviderJson::Snapshot(provider));
    });
    Route("/api/timeline", [](View::DataProvider& provider, const RouteArgs&) {
        return Ok(View::DataProviderJson::Timeline(provider.DataModel()));
    });
    Route("/api/histogram", [](View::DataProvider& provider, const RouteArgs&) {
        return Ok(View::DataProviderJson::Histogram(provider.DataModel().GetTimeline()));
    });
    Route("/api/minimap", [](View::DataProvider& provider, const RouteArgs&) {
        return Ok(View::DataProviderJson::Minimap(provider.DataModel().GetTimeline()));
    });
    Route("/api/topology", [](View::DataProvider& provider, const RouteArgs&) {
        return Ok(View::DataProviderJson::Topology(provider.DataModel().GetTopology()));
    });
    Route("/api/summary", [](View::DataProvider& provider, const RouteArgs&) {
        return Ok(View::DataProviderJson::Summary(provider.DataModel()));
    });
    Route(R"(/api/tracks/(\d+))",
          [](View::DataProvider& provider, const RouteArgs& args) {
              uint64_t track_id = 0;
              jt::Json track;
              if(parse_uint64(args.captures[0], track_id))
              {
                  track = View::DataProviderJson::Track(provider.DataModel(), track_id);
              }
              return track.isNull() ? Error(HTTP_NOT_FOUND, "No track with that id.")
                                    : Ok(std::move(track));
          });
    Route(R"(/api/tracks/(\d+)/data)",
          [](View::DataProvider& provider, const RouteArgs& args) {
              const View::TimelineModel& timeline = provider.DataModel().GetTimeline();
              uint64_t                   track_id = 0;
              Reply reply = Error(HTTP_NOT_FOUND, "No track with that id.");
              if(parse_uint64(args.captures[0], track_id) && timeline.GetTrack(track_id))
              {
                  reply = Ok(View::DataProviderJson::TrackData(timeline, track_id));
              }
              return reply;
          });
    Route(R"(/api/tables/([a-z0-9_]+))",
          [](View::DataProvider& provider, const RouteArgs& args) {
              View::TableType type = View::TableType::__kTableTypeCount;
              return View::DataProviderJson::TableTypeFromName(args.captures[0], type)
                         ? Ok(View::DataProviderJson::Table(provider.DataModel(), type))
                         : Error(HTTP_NOT_FOUND, "No table with that name.");
          });
    Route(R"(/api/events/(\d+))",
          [](View::DataProvider& provider, const RouteArgs& args) {
              uint64_t uuid = 0;
              return parse_uint64(args.captures[0], uuid)
                         ? Ok(View::DataProviderJson::Event(
                               provider.DataModel().GetEvents(), uuid))
                         : Error(HTTP_BAD_REQUEST, "An event uuid is a decimal uint64.");
          });

    Route("/api/compute", [](View::DataProvider& provider, const RouteArgs&) {
        return Ok(View::DataProviderJson::Compute(provider.ComputeModel()));
    });
    Route("/api/compute/kernel_table", [](View::DataProvider& provider,
                                          const RouteArgs&) {
        return Ok(View::DataProviderJson::KernelSelectionTable(
            provider.ComputeModel().GetKernelSelectionTable()));
    });
    Route(R"(/api/compute/workloads/(\d+))",
          [](View::DataProvider& provider, const RouteArgs& args) {
              uint32_t                  workload_id = 0;
              const View::WorkloadInfo* workload    = nullptr;
              if(parse_uint32(args.captures[0], workload_id))
              {
                  workload = provider.ComputeModel().GetWorkload(workload_id);
              }
              return workload ? Ok(View::DataProviderJson::Workload(*workload))
                              : Error(HTTP_NOT_FOUND, "No workload with that id.");
          });
    Route(R"(/api/compute/workloads/(\d+)/kernels/(\d+))",
          [](View::DataProvider& provider, const RouteArgs& args) {
              const View::KernelInfo* kernel = find_kernel(provider, args.captures);
              return kernel ? Ok(View::DataProviderJson::Kernel(*kernel))
                            : Error(HTTP_NOT_FOUND,
                                    "No kernel with that id in that workload.");
          });
    Route(R"(/api/compute/workloads/(\d+)/kernels/(\d+)/pc_sampling)",
          [](View::DataProvider& provider, const RouteArgs& args) {
              const View::KernelInfo* kernel = find_kernel(provider, args.captures);
              return kernel ? Ok(View::DataProviderJson::PcSampling(
                                  kernel->pc_sampling_data))
                            : Error(HTTP_NOT_FOUND,
                                    "No kernel with that id in that workload.");
          });
    Route(R"(/api/compute/kernels/(\d+)/metrics)", [](View::DataProvider& provider,
                                                      const RouteArgs&    args) {
        uint32_t kernel_id = 0;
        uint64_t client_id = 0;
        auto     client    = args.params.find("client_id");
        bool     valid =
            parse_uint32(args.captures[0], kernel_id) &&
            (client == args.params.end() || parse_uint64(client->second, client_id));
        return valid
                   ? Ok(View::DataProviderJson::KernelMetrics(provider.ComputeModel(),
                                                              client_id, kernel_id))
                   : Error(HTTP_BAD_REQUEST, "kernel id and client_id are uint values.");
    });
}

}  // namespace Server
}  // namespace RocProfVis
