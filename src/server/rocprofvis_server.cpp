// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_server.h"
#include "protocol/rocprofvis_protocol.h"
#include "protocol/rocprofvis_protocol_frame.h"
#include "rocprofvis_server_session.h"

#include "httplib.h"
#include "json.h"
#include "spdlog/spdlog.h"

#include <random>
#include <vector>

namespace RocProfVis
{
namespace Server
{

namespace
{

// A burst of large responses can keep one write waiting while the client
// works through the previous ones; httplib's 5s default drops the client.
constexpr time_t                SEND_TIMEOUT_SECONDS = 120;
constexpr std::chrono::seconds  REAPER_INTERVAL(1);
constexpr size_t                SESSION_ID_BYTES = 16;

std::string
random_hex(size_t bytes)
{
    std::random_device                      device;
    std::uniform_int_distribution<uint32_t> distribution(0, 255);
    std::string                             text;
    char                                    digits[3];
    for(size_t i = 0; i < bytes; i++)
    {
        std::snprintf(digits, sizeof(digits), "%02x", distribution(device));
        text += digits;
    }
    return text;
}

// Compares every byte, so the time taken does not reveal a matching prefix.
bool
tokens_match(const std::string& a, const std::string& b)
{
    unsigned char difference = a.size() == b.size() ? 0 : 1;
    for(size_t i = 0; i < a.size(); i++)
    {
        difference |= static_cast<unsigned char>(a[i] ^ (i < b.size() ? b[i] : 0));
    }
    return difference == 0;
}

void
reject(httplib::ws::WebSocket& socket, const std::string& error, bool auth_required)
{
    jt::Json welcome;
    welcome["type"]     = View::MSG_WELCOME;
    welcome["protocol"] = static_cast<long long>(View::PROTOCOL_VERSION);
    welcome["error"]    = error;
    if(auth_required)
    {
        welcome["auth_required"] = true;
    }
    std::string frame = View::encode_frame(welcome);
    socket.send(frame.data(), frame.size());
    socket.close();
}

}  // namespace

Server::Server(const server_options_t& options)
: m_options(options)
, m_port(0)
, m_stopping(false)
, m_stop_requested(false)
{
#ifdef CPPHTTPLIB_SSL_ENABLED
    if(!m_options.tls_certificate.empty() || !m_options.tls_key.empty())
    {
        m_http = std::make_unique<httplib::SSLServer>(m_options.tls_certificate.c_str(),
                                                      m_options.tls_key.c_str());
    }
    else
#endif
    {
        m_http = std::make_unique<httplib::Server>();
    }
    m_http->set_write_timeout(SEND_TIMEOUT_SECONDS, 0);
    // httplib's default, SO_REUSEPORT, lets a second server bind a port one is
    // already serving and take half its connections; this fails it instead.
    m_http->set_socket_options([](socket_t sock) {
        int enable = 1;
#ifdef _WIN32
        setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&enable),
                   sizeof(enable));
#else
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
#endif
    });
    m_http->WebSocket(View::PROTOCOL_WEBSOCKET_PATH,
                      [this](const httplib::Request& request, httplib::ws::WebSocket& socket) {
                          HandleConnection(request, socket);
                      });
    m_http->Get("/", [](const httplib::Request&, httplib::Response& response) {
        response.set_content("roc-optiq-server: connect a roc-optiq client to ws://<host>:<port>" +
                                 std::string(View::PROTOCOL_WEBSOCKET_PATH) + "\n",
                             "text/plain");
    });
    m_reaper = std::thread([this]() {
        std::unique_lock<std::mutex> lock(m_reaper_mutex);
        while(!m_stopping)
        {
            m_reaper_condition.wait_for(lock, REAPER_INTERVAL);
            if(!m_stopping)
            {
                lock.unlock();
                if(m_stop_requested)
                {
                    Stop();
                }
                else
                {
                    ReapSessions();
                }
                lock.lock();
            }
        }
    });
}

Server::~Server()
{
    Stop();
    if(m_reaper.joinable())
    {
        m_reaper.join();
    }
    ShutdownSessions();
}

bool
Server::IsValid() const
{
    return m_http && m_http->is_valid();
}

bool
Server::Bind()
{
    if(!IsValid())
    {
        m_port = -1;
    }
    else if(m_options.port == 0)
    {
        m_port = m_http->bind_to_any_port(m_options.host);
    }
    else if(m_http->bind_to_port(m_options.host, m_options.port))
    {
        m_port = m_options.port;
    }
    else
    {
        m_port = -1;
    }
    return m_port > 0;
}

bool
Server::Listen()
{
    return m_http->listen_after_bind();
}

void
Server::Stop()
{
    {
        std::lock_guard<std::mutex> lock(m_reaper_mutex);
        m_stopping = true;
    }
    m_reaper_condition.notify_all();
    // httplib waits for every connection's handler, and an attached one only
    // returns once its socket closes.
    std::vector<std::shared_ptr<ServerSession>> sessions;
    {
        std::lock_guard<std::mutex> lock(m_sessions_mutex);
        for(std::pair<const std::string, std::shared_ptr<ServerSession>>& session : m_sessions)
        {
            sessions.push_back(session.second);
        }
    }
    for(const std::shared_ptr<ServerSession>& session : sessions)
    {
        session->CloseConnection();
    }
    if(m_http)
    {
        m_http->stop();
    }
}

void
Server::RequestStop()
{
    m_stop_requested = true;
}

int
Server::GetPort() const
{
    return m_port;
}

void
Server::HandleConnection(const httplib::Request& request, httplib::ws::WebSocket& socket)
{
    // Browsers name the page a WebSocket comes from and native clients send no
    // Origin. Without a token, any site the user visits could reach this
    // server through their browser and read every trace.
    if(m_options.token.empty() && request.has_header("Origin"))
    {
        spdlog::warn("Rejected a WebSocket from a page at {}", request.get_header_value("Origin"));
        reject(socket, "Web pages cannot connect to a server running with --no-auth.", false);
        return;
    }
    std::string payload;
    jt::Json    hello;
    if(socket.read(payload) == httplib::ws::Fail || !View::decode_frame(payload, hello) ||
       View::json_get_string(hello, "type") != View::MSG_HELLO)
    {
        reject(socket, "Expected a hello message.", false);
        return;
    }

    const int64_t client_protocol = View::json_get_int64(hello, "protocol");
    if(client_protocol != View::PROTOCOL_VERSION)
    {
        reject(socket,
               "The client speaks protocol " + std::to_string(client_protocol) +
                   " but this server speaks " + std::to_string(View::PROTOCOL_VERSION) +
                   ". Use matching roc-optiq and roc-optiq-server builds.",
               false);
        return;
    }
    if(!Authenticate(hello))
    {
        spdlog::warn("Rejected a client from {} without the access token", request.remote_addr);
        reject(socket,
               "This server needs its access token. roc-optiq-server prints it when it "
               "starts, and saves it in its config directory as " +
                   View::server_token_file_name(m_port) + ".",
               true);
        return;
    }

    std::shared_ptr<ServerSession> session = FindResumableSession(hello);
    const bool                     resumed = session != nullptr;
    if(!session)
    {
        session = CreateSession();
    }
    spdlog::info("Client '{}' from {}:{} {} session {}", View::json_get_string(hello, "client"),
                 request.remote_addr, request.remote_port, resumed ? "resumed" : "started",
                 session->GetId());

    const bool tls        = !m_options.tls_certificate.empty();
    uint64_t   attachment = session->Attach(
        [&socket, tls](const std::string& frame) {
            if(!tls || frame.size() <= View::MAX_TLS_FRAME_SIZE)
            {
                return socket.send(frame.data(), frame.size());
            }
            for(const std::string& piece : View::split_frame(frame, View::MAX_TLS_FRAME_SIZE))
            {
                if(!socket.send(piece.data(), piece.size()))
                {
                    return false;
                }
            }
            return true;
        },
        [&socket]() { socket.close(); }, resumed, View::json_get_uint64(hello, "last_seq"));
    while(socket.read(payload) != httplib::ws::Fail)
    {
        session->OnFrame(std::move(payload));
        payload.clear();
    }
    session->Detach(attachment);
    if(!session->HasEnded())
    {
        spdlog::info("Connection from {}:{} closed; session {} kept for {}s",
                     request.remote_addr, request.remote_port, session->GetId(),
                     m_options.session_timeout_seconds);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_sessions_mutex);
        auto                        it = m_sessions.find(session->GetId());
        if(it != m_sessions.end() && it->second == session)
        {
            m_sessions.erase(it);
        }
    }
    spdlog::info("Client {}:{} disconnected; session {} ended", request.remote_addr,
                 request.remote_port, session->GetId());
    session->Shutdown();
}

bool
Server::Authenticate(jt::Json& hello) const
{
    return m_options.token.empty() ||
           tokens_match(View::json_get_string(hello, "token"), m_options.token);
}

std::shared_ptr<ServerSession>
Server::FindResumableSession(jt::Json& hello)
{
    std::shared_ptr<ServerSession> result;
    std::shared_ptr<ServerSession> stale;
    const std::string              id = View::json_get_string(hello, "session");
    if(!id.empty())
    {
        std::lock_guard<std::mutex> lock(m_sessions_mutex);
        auto                        it = m_sessions.find(id);
        if(it != m_sessions.end())
        {
            if(it->second->CanResume(View::json_get_uint64(hello, "last_seq")))
            {
                result = it->second;
            }
            else
            {
                // It missed more than the replay buffer holds; start it afresh.
                stale = it->second;
                m_sessions.erase(it);
            }
        }
    }
    if(stale)
    {
        spdlog::warn("Session {} can no longer be resumed", stale->GetId());
        stale->Shutdown();
    }
    return result;
}

std::shared_ptr<ServerSession>
Server::CreateSession()
{
    session_options_t session_options;
    session_options.config_path      = m_options.config_path;
    session_options.upload_directory = m_options.upload_directory;
    session_options.max_upload_bytes = m_options.max_upload_bytes;
    session_options.allow_profiler   = m_options.allow_profiler;
    session_options.allow_scripts    = m_options.allow_scripts;
    std::shared_ptr<ServerSession> session =
        std::make_shared<ServerSession>(random_hex(SESSION_ID_BYTES), session_options);
    std::lock_guard<std::mutex> lock(m_sessions_mutex);
    m_sessions[session->GetId()] = session;
    return session;
}

void
Server::ReapSessions()
{
    std::vector<std::shared_ptr<ServerSession>> expired;
    {
        std::lock_guard<std::mutex> lock(m_sessions_mutex);
        for(auto it = m_sessions.begin(); it != m_sessions.end();)
        {
            if(it->second->IsExpired(std::chrono::seconds(m_options.session_timeout_seconds)))
            {
                expired.push_back(it->second);
                it = m_sessions.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    for(const std::shared_ptr<ServerSession>& session : expired)
    {
        spdlog::info("Session {} expired; closing its traces", session->GetId());
        session->Shutdown();
    }
}

void
Server::ShutdownSessions()
{
    std::map<std::string, std::shared_ptr<ServerSession>> sessions;
    {
        std::lock_guard<std::mutex> lock(m_sessions_mutex);
        sessions.swap(m_sessions);
    }
    for(std::pair<const std::string, std::shared_ptr<ServerSession>>& session : sessions)
    {
        session.second->Shutdown();
    }
}

}  // namespace Server
}  // namespace RocProfVis
