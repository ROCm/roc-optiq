// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_data_provider_json.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace httplib
{
class Server;
}  // namespace httplib

namespace RocProfVis
{
namespace Server
{

/*
 * Serves DataProviderJson over HTTP. The HTTP library answers on its own worker
 * threads, but the provider and its models belong to the thread that updates
 * them, so every route queues its work for that thread and waits for the
 * answer. Whoever owns the provider drains the queue between updates.
 *
 * A route for data the provider loads on demand (track data, tables, events,
 * the summary, metrics) issues the fetch and answers once the provider has
 * finished it, so a client only ever sees complete results.
 */
class TraceServer
{
public:
    // allowed_origin, when set, is sent as Access-Control-Allow-Origin so a page
    // served from that origin can read the API. web_root, when set, is a folder
    // served at / (the built web app).
    TraceServer(View::DataProvider& provider, const std::string& allowed_origin,
                const std::string& web_root);
    ~TraceServer();

    // Binds host:port and starts answering on a background thread. This is a
    // one-shot server; repeated calls return false.
    bool Start(const std::string& host, uint16_t port);
    // Answers every queued request with 503, refuses new ones, and stops
    // listening.
    void Stop();

    // Provider thread: starts loading trace_path in place of the current trace.
    bool Open(const std::string& trace_path, std::string& error_out);

    // Provider thread: runs the work the HTTP threads queued and answers the
    // requests whose fetches have finished.
    void RunPendingRequests();
    // Provider thread: sleeps until work is queued or timeout passes.
    void WaitForRequests(std::chrono::milliseconds timeout);
    // True when the accept loop stopped without Stop() asking it to.
    bool ListenerFailed() const;

private:
    struct Reply
    {
        int32_t  status;
        jt::Json body;
    };

    // What a route reads from its request, copied so the queued work never
    // touches a request whose HTTP thread has stopped waiting.
    struct RouteArgs
    {
        std::vector<std::string>           captures;
        std::map<std::string, std::string> params;
        jt::Json                           body;

        // The named query value, or fallback when the request has none.
        std::string Param(const std::string& name,
                          const std::string& fallback = "") const;
    };

    // A route's work on the provider thread. A route that needs a fetch sets
    // start, which issues it and returns false while the provider cannot take
    // it yet, and ready, which returns true once it has finished; reply then
    // reads the result.
    struct Job
    {
        std::function<Reply()> reply;
        std::function<bool()>  start;
        std::function<bool()>  ready;
    };

    struct Task
    {
        std::function<Job()>                  make_job;
        std::shared_ptr<std::promise<Reply>>  reply;
        std::shared_ptr<std::atomic<bool>>    cancelled;
        std::chrono::steady_clock::time_point queued_at;
        Job                                   job;
        bool                                  made    = false;
        bool                                  started = false;
    };

    using Handler = std::function<Job(View::DataProvider&, const RouteArgs&)>;

    enum class Method
    {
        kGet,
        kPost
    };

    void RegisterRoutes();
    void RegisterSystemRoutes();
    void RegisterComputeRoutes();
    bool IsExpectedHost(const std::string& host) const;
    // Registers a route whose handler runs on the provider thread. A POST
    // route takes a JSON body.
    void Route(Method method, const std::string& pattern, Handler handler);
    // Wraps a handler so it only runs once a trace of the given kind is ready.
    Handler For(View::DataProviderJson::TraceKind kind, Handler handler) const;
    // Moves a task on as far as it can go; true once it has been answered.
    bool  Advance(Task& task);
    Reply OnProviderThread(std::function<Job()> make_job);

    static Job   Now(Reply reply);
    static Reply Ok(jt::Json body);
    static Reply Error(int32_t status, const std::string& message);

    View::DataProvider&              m_provider;
    std::string                      m_allowed_origin;
    std::string                      m_web_root;
    std::vector<std::string>         m_accepted_hosts;
    std::unique_ptr<httplib::Server> m_http;
    std::thread                      m_listener;
    std::atomic<bool>                m_listener_failed;
    std::mutex                       m_mutex;
    std::condition_variable          m_task_queued;
    std::deque<Task>                 m_tasks;
    bool                             m_start_attempted;
    bool                             m_stopping;

    // Provider thread only.
    std::list<Task>                       m_active;
    View::DataProviderJson::TraceKind     m_trace_kind;
    std::unordered_map<uint64_t, uint8_t> m_track_groups;
    std::map<uint64_t, std::string>       m_table_errors;
    bool                                  m_save_succeeded;
    bool                                  m_cleanup_succeeded;
};

}  // namespace Server
}  // namespace RocProfVis
