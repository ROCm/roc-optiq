// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace httplib
{
class Server;
namespace ws
{
class WebSocket;
}
struct Request;
}  // namespace httplib

namespace jt
{
class Json;
}

namespace RocProfVis
{
namespace Server
{

class ServerSession;

struct server_options_t
{
    // Loopback by default; reach a remote server through an SSH tunnel or TLS.
    std::string host;
    // 0 picks a free port, reported by Server::GetPort().
    int         port = 0;
    // Where controllers keep their per-trace metadata, as the app's config path.
    std::string config_path;
    // Clients must send this in their hello. Empty accepts any client.
    std::string token;
    // Both set: serve wss:// with this PEM certificate and key.
    std::string tls_certificate;
    std::string tls_key;
    // Where uploaded traces are written. Empty refuses uploads.
    std::string upload_directory;
    // Largest upload accepted, 0 for no limit.
    uint64_t    max_upload_bytes = 0;
    // How long the traces of a client that dropped are kept for it to resume.
    uint32_t    session_timeout_seconds = 600;
    bool        allow_profiler          = false;
    // Clients may run Python analysis scripts, as this server's user.
    bool        allow_scripts           = false;
};

/**
 * @brief The roc-optiq-server endpoint: authenticates each WebSocket and binds
 * it to a ServerSession, new or resumed.
 *
 * Sessions outlive their connections: a client that drops keeps its open
 * traces for session_timeout_seconds and picks them up again by reconnecting
 * with its session id.
 */
class Server
{
public:
    explicit Server(const server_options_t& options);
    ~Server();

    Server(const Server&)            = delete;
    Server& operator=(const Server&) = delete;

    // False when the TLS certificate or key could not be loaded.
    bool IsValid() const;
    bool Bind();
    // Serves clients until Stop() is called. False if listening failed.
    bool Listen();
    // Closes every connection and stops listening.
    void Stop();
    // Stop() within a second, from another thread. Safe in a signal handler.
    void RequestStop();
    int  GetPort() const;

private:
    void HandleConnection(const httplib::Request& request, httplib::ws::WebSocket& socket);
    bool Authenticate(jt::Json& hello) const;
    std::shared_ptr<ServerSession> FindResumableSession(jt::Json& hello);
    std::shared_ptr<ServerSession> CreateSession();
    void ReapSessions();
    void ShutdownSessions();

    server_options_t                 m_options;
    std::unique_ptr<httplib::Server> m_http;
    int                              m_port;

    std::mutex                                            m_sessions_mutex;
    std::map<std::string, std::shared_ptr<ServerSession>> m_sessions;

    std::thread             m_reaper;
    std::mutex              m_reaper_mutex;
    std::condition_variable m_reaper_condition;
    bool                    m_stopping;
    std::atomic<bool>       m_stop_requested;
};

}  // namespace Server
}  // namespace RocProfVis
