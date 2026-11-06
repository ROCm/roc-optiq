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
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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
 * them, so every route queues its read for that thread and waits for the
 * answer. Whoever owns the provider drains the queue between updates.
 */
class TraceServer
{
public:
    // allowed_origin, when set, is sent as Access-Control-Allow-Origin so a page
    // served from that origin can read the API.
    TraceServer(View::DataProvider&               provider,
                View::DataProviderJson::TraceKind trace_kind,
                const std::string&                allowed_origin);
    ~TraceServer();

    // Binds host:port and starts answering on a background thread. This is a
    // one-shot server; repeated calls return false.
    bool Start(const std::string& host, uint16_t port);
    // Answers every queued request with 503, refuses new ones, and stops
    // listening.
    void Stop();

    // Provider thread: runs the reads the HTTP threads queued.
    void RunPendingRequests();
    // Provider thread: sleeps until a read is queued or timeout passes.
    void WaitForRequests(std::chrono::milliseconds timeout);
    // True when the accept loop stopped without Stop() asking it to.
    bool ListenerFailed() const;

private:
    struct Reply
    {
        int32_t  status;
        jt::Json body;
    };

    // What a route reads from its request, copied so the queued read never
    // touches a request whose HTTP thread has stopped waiting.
    struct RouteArgs
    {
        std::vector<std::string>           captures;
        std::map<std::string, std::string> params;
    };

    struct Task
    {
        std::function<Reply()>               work;
        std::shared_ptr<std::promise<Reply>> reply;
        std::shared_ptr<std::atomic<bool>>   cancelled;
    };

    void RegisterRoutes();
    bool IsExpectedHost(const std::string& host) const;
    // Registers a GET route whose handler runs on the provider thread.
    void  Route(const std::string&                                          pattern,
                std::function<Reply(View::DataProvider&, const RouteArgs&)> handler);
    Reply OnProviderThread(std::function<Reply()> work);

    static Reply Ok(jt::Json body);
    static Reply Error(int32_t status, const std::string& message);

    View::DataProvider&               m_provider;
    View::DataProviderJson::TraceKind m_trace_kind;
    std::string                       m_allowed_origin;
    std::string                       m_expected_host;
    std::string                       m_expected_authority;
    std::unique_ptr<httplib::Server>  m_http;
    std::thread                       m_listener;
    std::atomic<bool>                 m_listener_failed;
    std::mutex                        m_mutex;
    std::condition_variable           m_task_queued;
    std::deque<Task>                  m_tasks;
    bool                              m_start_attempted;
    bool                              m_stopping;
};

}  // namespace Server
}  // namespace RocProfVis
