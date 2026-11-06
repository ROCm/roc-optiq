// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_trace_server.h"

#include <cctype>
#include <charconv>
#include <exception>
#include <limits>
#include <system_error>
#include <utility>

#include "httplib.h"
#include "spdlog/spdlog.h"

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

constexpr int32_t     HTTP_OK                   = 200;
constexpr int32_t     HTTP_NO_CONTENT           = 204;
constexpr int32_t     HTTP_BAD_REQUEST          = 400;
constexpr int32_t     HTTP_NOT_FOUND            = 404;
constexpr int32_t     HTTP_INTERNAL_ERROR       = 500;
constexpr int32_t     HTTP_NOT_IMPLEMENTED      = 501;
constexpr int32_t     HTTP_UNAVAILABLE          = 503;
constexpr const char* JSON_CONTENT_TYPE         = "application/json";
constexpr size_t      HTTP_WORKER_COUNT         = 8;
constexpr size_t      HTTP_QUEUED_REQUEST_LIMIT = 32;

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

static std::string
ascii_lower(const std::string& text)
{
    std::string lower = text;
    for(char& character : lower)
    {
        character =
            static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return lower;
}

static bool
is_loopback_address(const std::string& host)
{
    const std::string normalized = ascii_lower(host);
    return normalized == "localhost" || normalized == "::1" ||
           normalized.rfind("127.", 0) == 0;
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

TraceServer::TraceServer(View::DataProvider&               provider,
                         View::DataProviderJson::TraceKind trace_kind,
                         const std::string&                allowed_origin)
: m_provider(provider)
, m_trace_kind(trace_kind)
, m_allowed_origin(allowed_origin)
, m_http(std::make_unique<httplib::Server>())
, m_listener_failed(false)
, m_start_attempted(false)
, m_stopping(false)
{
    m_http->new_task_queue = []() {
        return new httplib::ThreadPool(HTTP_WORKER_COUNT, HTTP_WORKER_COUNT,
                                       HTTP_QUEUED_REQUEST_LIMIT);
    };
    RegisterRoutes();
}

TraceServer::~TraceServer() { Stop(); }

bool
TraceServer::Start(const std::string& host, uint16_t port)
{
    if(!is_loopback_address(host))
    {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(m_start_attempted || m_stopping)
        {
            return false;
        }
        m_start_attempted = true;
    }

    std::string expected_host = ascii_lower(host);
    m_expected_host           = expected_host.find(':') == std::string::npos
                                    ? expected_host
                                    : "[" + expected_host + "]";
    m_expected_authority      = m_expected_host + ":" + std::to_string(port);

    bool bound = m_http->bind_to_port(host, port);
    if(bound)
    {
        try
        {
            m_listener = std::thread([this]() {
                bool listened = false;
                try
                {
                    listened = m_http->listen_after_bind();
                } catch(const std::exception& error)
                {
                    spdlog::error("HTTP listener failed: {}", error.what());
                } catch(...)
                {
                    spdlog::error("HTTP listener failed with an unknown error.");
                }
                if(!listened)
                {
                    m_http->decommission();
                    m_listener_failed.store(true);
                    m_task_queued.notify_all();
                }
            });
        } catch(const std::exception& error)
        {
            spdlog::error("Could not start the HTTP listener thread: {}", error.what());
            m_http->stop();
            bound = false;
        }
        if(bound)
        {
            m_http->wait_until_ready();
            if(!m_http->is_running())
            {
                m_listener_failed.store(true);
                if(m_listener.joinable())
                {
                    m_listener.join();
                }
                bound = false;
            }
        }
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
    m_task_queued.notify_all();
    for(Task& task : abandoned)
    {
        if(!task.cancelled->load())
        {
            task.reply->set_value(
                Error(HTTP_UNAVAILABLE, "The server is shutting down."));
        }
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
        if(task.cancelled->load())
        {
            continue;
        }

        Reply reply = Error(HTTP_INTERNAL_ERROR, "The request could not be completed.");
        try
        {
            reply = task.work();
        } catch(const std::exception& error)
        {
            spdlog::error("Trace server provider task failed: {}", error.what());
        } catch(...)
        {
            spdlog::error("Trace server provider task failed with an unknown error.");
        }
        if(!task.cancelled->load())
        {
            task.reply->set_value(std::move(reply));
        }
    }
}

void
TraceServer::WaitForRequests(std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_task_queued.wait_for(lock, timeout, [this]() {
        return m_stopping || m_listener_failed.load() || !m_tasks.empty();
    });
}

bool
TraceServer::ListenerFailed() const
{
    return m_listener_failed.load();
}

TraceServer::Reply
TraceServer::OnProviderThread(std::function<Reply()> work)
{
    std::shared_ptr<std::promise<Reply>> promise =
        std::make_shared<std::promise<Reply>>();
    std::shared_ptr<std::atomic<bool>> cancelled =
        std::make_shared<std::atomic<bool>>(false);
    std::future<Reply> reply  = promise->get_future();
    bool               queued = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(!m_stopping)
        {
            m_tasks.push_back(Task{ std::move(work), promise, cancelled });
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
            cancelled->store(true);
            result =
                Error(HTTP_UNAVAILABLE, "The trace provider did not answer in time.");
        }
    }
    return result;
}

bool
TraceServer::IsExpectedHost(const std::string& host) const
{
    const std::string normalized = ascii_lower(host);
    return normalized == m_expected_host || normalized == m_expected_authority;
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
    m_http->set_pre_routing_handler(
        [this](const httplib::Request& request, httplib::Response& response) {
            if(request.get_header_value_count("Host") == 1 &&
               IsExpectedHost(request.get_header_value("Host")))
            {
                return httplib::Server::HandlerResponse::Unhandled;
            }

            Reply reply     = Error(HTTP_BAD_REQUEST, "Unexpected Host header.");
            response.status = reply.status;
            response.set_content(reply.body.toString(), JSON_CONTENT_TYPE);
            return httplib::Server::HandlerResponse::Handled;
        });
    m_http->set_exception_handler([](const httplib::Request&, httplib::Response& response,
                                     std::exception_ptr error) {
        try
        {
            if(error)
            {
                std::rethrow_exception(error);
            }
        } catch(const std::exception& exception)
        {
            spdlog::error("Trace server request failed: {}", exception.what());
        } catch(...)
        {
            spdlog::error("Trace server request failed with an unknown error.");
        }
        Reply reply = Error(HTTP_INTERNAL_ERROR, "The request could not be completed.");
        response.status = reply.status;
        response.set_content(reply.body.toString(), JSON_CONTENT_TYPE);
    });
    m_http->set_post_routing_handler(
        [this](const httplib::Request&, httplib::Response& response) {
            response.set_header("Cache-Control", "no-store");
            if(!m_allowed_origin.empty())
            {
                response.set_header("Access-Control-Allow-Origin", m_allowed_origin);
                response.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
                response.set_header("Access-Control-Allow-Headers", "Content-Type");
                response.set_header("Vary", "Origin");
            }
        });
    m_http->Options(R"(/api/.*)",
                    [](const httplib::Request&, httplib::Response& response) {
                        response.status = HTTP_NO_CONTENT;
                    });
    // Requests that match no route still get a JSON body.
    m_http->set_error_handler([](const httplib::Request&, httplib::Response& response) {
        if(response.body.empty())
        {
            jt::Json body;
            body["error"] = httplib::status_message(response.status);
            response.set_content(body.toString(), JSON_CONTENT_TYPE);
        }
    });

    Route("/api/status", [this](View::DataProvider& provider, const RouteArgs&) {
        return Ok(View::DataProviderJson::Status(provider, m_trace_kind));
    });
    Route("/api/snapshot", [this](View::DataProvider& provider, const RouteArgs&) {
        return Ok(View::DataProviderJson::Snapshot(provider, m_trace_kind));
    });
    if(m_trace_kind == View::DataProviderJson::TraceKind::kSystem)
    {
        Route("/api/timeline", [](View::DataProvider& provider, const RouteArgs&) {
            return Ok(View::DataProviderJson::Timeline(provider.DataModel()));
        });
        Route("/api/histogram", [](View::DataProvider& provider, const RouteArgs&) {
            return Ok(
                View::DataProviderJson::Histogram(provider.DataModel().GetTimeline()));
        });
        Route("/api/minimap", [](View::DataProvider& provider, const RouteArgs&) {
            return Ok(
                View::DataProviderJson::Minimap(provider.DataModel().GetTimeline()));
        });
        Route("/api/topology", [](View::DataProvider& provider, const RouteArgs&) {
            return Ok(
                View::DataProviderJson::Topology(provider.DataModel().GetTopology()));
        });
        Route("/api/summary", [](View::DataProvider& provider, const RouteArgs&) {
            jt::Json summary = View::DataProviderJson::Summary(provider.DataModel());
            return summary.isNull()
                       ? Error(HTTP_NOT_IMPLEMENTED,
                               "Summary fetching is not available through HTTP yet.")
                       : Ok(std::move(summary));
        });
        Route(R"(/api/tracks/(\d+))", [](View::DataProvider& provider,
                                         const RouteArgs&    args) {
            uint64_t track_id = 0;
            jt::Json track;
            if(parse_uint64(args.captures[0], track_id))
            {
                track = View::DataProviderJson::Track(provider.DataModel(), track_id);
            }
            return track.isNull() ? Error(HTTP_NOT_FOUND, "No track with that id.")
                                  : Ok(std::move(track));
        });
        Route(R"(/api/tracks/(\d+)/data)", [](View::DataProvider& provider,
                                              const RouteArgs&    args) {
            const View::TimelineModel& timeline = provider.DataModel().GetTimeline();
            uint64_t                   track_id = 0;
            Reply reply = Error(HTTP_NOT_FOUND, "No track with that id.");
            if(parse_uint64(args.captures[0], track_id) && timeline.GetTrack(track_id))
            {
                jt::Json data = View::DataProviderJson::TrackData(timeline, track_id);
                reply =
                    data.isNull()
                        ? Error(HTTP_NOT_IMPLEMENTED,
                                "Track-data fetching is not available through HTTP yet.")
                        : Ok(std::move(data));
            }
            return reply;
        });
        Route(R"(/api/tables/([a-z0-9_]+))", [](View::DataProvider& provider,
                                                const RouteArgs&    args) {
            View::TableType type = View::TableType::__kTableTypeCount;
            if(!View::DataProviderJson::TableTypeFromName(args.captures[0], type))
            {
                return Error(HTTP_NOT_FOUND, "No table with that name.");
            }
            jt::Json table = View::DataProviderJson::Table(provider.DataModel(), type);
            return table.isNull()
                       ? Error(HTTP_NOT_IMPLEMENTED,
                               "Table fetching is not available through HTTP yet.")
                       : Ok(std::move(table));
        });
        Route(R"(/api/events/(\d+))", [](View::DataProvider& provider,
                                         const RouteArgs&    args) {
            uint64_t uuid = 0;
            if(!parse_uint64(args.captures[0], uuid))
            {
                return Error(HTTP_BAD_REQUEST, "An event uuid is a decimal uint64.");
            }
            jt::Json event =
                View::DataProviderJson::Event(provider.DataModel().GetEvents(), uuid);
            return event.isNull()
                       ? Error(HTTP_NOT_IMPLEMENTED,
                               "Event fetching is not available through HTTP yet.")
                       : Ok(std::move(event));
        });
    }
    else
    {
        Route("/api/compute", [](View::DataProvider& provider, const RouteArgs&) {
            return Ok(View::DataProviderJson::Compute(provider.ComputeModel()));
        });
        Route("/api/compute/kernel_table", [](View::DataProvider& provider,
                                              const RouteArgs&) {
            jt::Json table = View::DataProviderJson::KernelSelectionTable(
                provider.ComputeModel().GetKernelSelectionTable());
            return table.isNull()
                       ? Error(HTTP_NOT_IMPLEMENTED,
                               "Kernel-table fetching is not available through HTTP yet.")
                       : Ok(std::move(table));
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
                  if(!kernel)
                  {
                      return Error(HTTP_NOT_FOUND,
                                   "No kernel with that id in that workload.");
                  }
                  const View::PcSamplingData& data = kernel->pc_sampling_data;
                  const bool                  loaded =
                      !data.code_objects.empty() || !data.source_files.empty() ||
                      data.sample_states_loaded || data.stall_details_loaded;
                  return loaded
                             ? Ok(View::DataProviderJson::PcSampling(data))
                             : Error(HTTP_NOT_IMPLEMENTED, "PC-sampling fetching is not "
                                                           "available through HTTP yet.");
              });
        Route(R"(/api/compute/kernels/(\d+)/metrics)", [](View::DataProvider& provider,
                                                          const RouteArgs&    args) {
            uint32_t kernel_id = 0;
            uint64_t client_id = 0;
            auto     client    = args.params.find("client_id");
            bool     valid =
                parse_uint32(args.captures[0], kernel_id) &&
                (client == args.params.end() || parse_uint64(client->second, client_id));
            if(!valid)
            {
                return Error(HTTP_BAD_REQUEST,
                             "kernel id and client_id are uint values.");
            }
            jt::Json metrics = View::DataProviderJson::KernelMetrics(
                provider.ComputeModel(), client_id, kernel_id);
            return metrics.isNull()
                       ? Error(HTTP_NOT_IMPLEMENTED,
                               "Metric fetching is not available through HTTP yet.")
                       : Ok(std::move(metrics));
        });
    }
}

}  // namespace Server
}  // namespace RocProfVis
