// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace RocProfVis
{
namespace Server
{

class ServerTrace;
class ServerProfilerRun;

struct session_options_t
{
    std::string config_path;
    std::string upload_directory;
    uint64_t    max_upload_bytes = 0;
    bool        allow_profiler   = false;
    bool        allow_scripts    = false;
};

/**
 * @brief One client and everything it has open on the server.
 *
 * A session outlives the connection that created it. Its worker keeps running
 * while no connection is attached, and every message it sends is kept in a
 * bounded replay buffer, so a client that reconnects receives what it missed.
 * Messages are queued by the connection's reader thread and handled, together
 * with every trace's DataProvider, on the session's worker thread, so a trace
 * is only ever touched by one thread.
 */
class ServerSession
{
public:
    // Sends one binary frame to the attached client.
    using SendFunction  = std::function<bool(const std::string& frame)>;
    // Closes the attached connection, when another one takes the session over.
    using CloseFunction = std::function<void()>;

    ServerSession(const std::string& id, const session_options_t& options);
    ~ServerSession();

    ServerSession(const ServerSession&)            = delete;
    ServerSession& operator=(const ServerSession&) = delete;

    const std::string& GetId() const;

    // True when a client that has received up to last_seq can be brought up to
    // date from the replay buffer.
    bool CanResume(uint64_t last_seq);
    /*
     * Binds a connection, sends it the welcome and, when resuming, every message
     * after last_seq. A connection already attached is closed. Returns the
     * attachment, which Detach() takes.
     */
    uint64_t Attach(SendFunction send, CloseFunction close, bool resumed, uint64_t last_seq);
    // Releases the connection, unless another one has attached since.
    void     Detach(uint64_t attachment);
    // Closes the attached connection, if any; its handler then detaches.
    void     CloseConnection();
    bool     IsExpired(std::chrono::seconds timeout);
    // The client said bye: nothing will resume this session.
    bool     HasEnded() const;

    // Queues one frame from the client. Called from the reader thread.
    void OnFrame(std::string&& payload);

    // Stops the worker and releases every trace, upload and profiler run.
    void Shutdown();

private:
    struct InboxEntry
    {
        jt::Json    message;
        std::string chunk;
    };

    struct Upload
    {
        std::string   path;
        std::ofstream stream;
        uint64_t      expected_size;
        uint64_t      received;
        bool          ok;
    };

    struct ReplayFrame
    {
        uint64_t    seq;
        std::string frame;
    };

    void Run();
    void HandleMessage(jt::Json& message);
    void HandleOpenTrace(jt::Json& message);
    void HandleListDirectory(jt::Json& message);
    void HandleUploadBegin(jt::Json& message);
    void HandleUploadChunk(const std::string& frame);
    void HandleUploadEnd(jt::Json& message, bool cancel);
    void HandleProfilerLaunch(jt::Json& message);
    void SendWelcome(bool resumed);
    ServerTrace* FindTrace(uint32_t trace_id);
    bool         Send(jt::Json& message);
    bool         SendTraceMessage(uint32_t trace_id, jt::Json& message);
    bool SendFileChunk(uint32_t trace_id, uint64_t request_id, const char* data, size_t size);
    void DiscardUploads();

    std::string       m_id;
    session_options_t m_options;

    // Guards the attached connection and the outgoing sequence.
    std::mutex              m_send_mutex;
    SendFunction            m_send;
    CloseFunction           m_close;
    uint64_t                m_attachment;
    uint64_t                m_last_seq;
    std::deque<ReplayFrame> m_replay;
    size_t                  m_replay_bytes;
    std::chrono::steady_clock::time_point m_detached_since;

    std::mutex              m_inbox_mutex;
    std::condition_variable m_inbox_condition;
    std::deque<InboxEntry>  m_inbox;
    // Highest client seq queued; a message at or below it is a resend.
    uint64_t                m_last_client_seq;
    bool                    m_stopping;
    std::atomic<bool>       m_ended;
    std::thread             m_worker;

    // Worker thread only.
    std::map<uint32_t, std::unique_ptr<ServerTrace>>       m_traces;
    std::map<uint64_t, Upload>                             m_uploads;
    std::map<uint64_t, std::unique_ptr<ServerProfilerRun>> m_profiler_runs;
    bool                                                   m_busy;
};

}  // namespace Server
}  // namespace RocProfVis
