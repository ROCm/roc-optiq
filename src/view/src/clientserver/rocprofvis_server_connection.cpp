// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_server_connection.h"
#include "protocol/rocprofvis_data_channel.h"
#include "rocprofvis_controller_enums.h"
#include "protocol/rocprofvis_protocol.h"
#include "protocol/rocprofvis_protocol_frame.h"
#include "protocol/rocprofvis_protocol_uri.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_version.h"

#include "httplib.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr time_t CONNECT_TIMEOUT_SECONDS   = 5;
constexpr time_t HANDSHAKE_TIMEOUT_SECONDS = 10;
// Long enough for a request to wait behind a burst of large responses.
constexpr time_t SEND_TIMEOUT_SECONDS = 120;
// Opening only identifies the file; the load itself is asynchronous.
constexpr std::chrono::seconds OPEN_TRACE_TIMEOUT(60);
constexpr std::chrono::seconds UPLOAD_READY_TIMEOUT(60);
// The server has to flush and check the whole file before it answers.
constexpr std::chrono::minutes UPLOAD_DONE_TIMEOUT(10);
// Matches the server's default session timeout: past it there is nothing left
// to resume.
constexpr std::chrono::minutes RECONNECT_GIVE_UP(10);
constexpr std::chrono::seconds RECONNECT_MAX_BACKOFF(10);
// What this side keeps to send again after a resume.
constexpr size_t SENT_BUFFER_BYTES = 64ull * 1024 * 1024;

std::mutex&
registry_mutex()
{
    static std::mutex s_mutex;
    return s_mutex;
}

std::vector<std::shared_ptr<ServerConnection>>&
registry()
{
    static std::vector<std::shared_ptr<ServerConnection>> s_connections;
    return s_connections;
}

std::string
token_key(const std::string& host, int port, bool tls)
{
    return make_server_address(host, port) + (tls ? "/tls" : "");
}

std::mutex&
token_cache_mutex()
{
    static std::mutex s_mutex;
    return s_mutex;
}

std::map<std::string, std::string>&
token_cache()
{
    static std::map<std::string, std::string> s_tokens;
    return s_tokens;
}

std::string
read_first_line(const std::filesystem::path& path)
{
    std::ifstream file(path);
    std::string   line;
    std::getline(file, line);
    while(!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
    {
        line.pop_back();
    }
    return line;
}

}  // namespace

/**
 * @brief The IDataChannel of one trace on a ServerConnection.
 */
class ServerTraceChannel : public IDataChannel
{
public:
    ServerTraceChannel(std::shared_ptr<ServerConnection> connection, uint32_t trace_id)
    : m_connection(std::move(connection))
    , m_trace_id(trace_id)
    , m_closed(false)
    {}

    ~ServerTraceChannel() override { Close(); }

    bool Send(jt::Json& message) override
    {
        bool result = false;
        if(!m_closed)
        {
            message["trace"] = static_cast<long long>(m_trace_id);
            result           = m_connection->SendMessage(message);
        }
        return result;
    }

    bool Receive(jt::Json& message) override
    {
        std::lock_guard<std::mutex> lock(m_inbox_mutex);
        bool                        result = !m_inbox.empty();
        if(result)
        {
            message = std::move(m_inbox.front());
            m_inbox.pop_front();
        }
        return result;
    }

    void ReceiveFile(uint64_t request_id, const std::string& local_path) override
    {
        m_connection->ExpectFile(m_trace_id, request_id, local_path);
    }

    // Requests sent while the connection is resuming queue until it has.
    bool IsConnected() const override
    {
        return !m_closed && m_connection->GetState() != ServerConnectionState::kLost;
    }

    void Close() override
    {
        if(!m_closed.exchange(true))
        {
            jt::Json message;
            message["type"]  = MSG_CLOSE_TRACE;
            message["trace"] = static_cast<long long>(m_trace_id);
            m_connection->SendMessage(message);
            m_connection->RemoveChannel(m_trace_id);
        }
    }

    // Called from the connection's reader thread.
    void Deliver(jt::Json& message)
    {
        std::lock_guard<std::mutex> lock(m_inbox_mutex);
        m_inbox.push_back(std::move(message));
    }

    // The server never opened the trace, so there is nothing to close there.
    void Abandon()
    {
        m_closed = true;
        m_connection->RemoveChannel(m_trace_id);
    }

private:
    std::shared_ptr<ServerConnection> m_connection;
    uint32_t                          m_trace_id;
    std::atomic<bool>                 m_closed;
    std::mutex                        m_inbox_mutex;
    std::deque<jt::Json>              m_inbox;
};

std::shared_ptr<ServerConnection>
ServerConnection::Get(const std::string& host, int port, const ServerConnectionOptions& options,
                      std::string& error)
{
    auto find_live = [&host, port, &options]() -> std::shared_ptr<ServerConnection> {
        std::vector<std::shared_ptr<ServerConnection>>& connections = registry();
        for(auto it = connections.begin(); it != connections.end();)
        {
            if((*it)->GetState() == ServerConnectionState::kLost)
            {
                it = connections.erase(it);
            }
            else if((*it)->GetHost() == host && (*it)->GetPort() == port &&
                    (*it)->IsTls() == options.tls)
            {
                return *it;
            }
            else
            {
                ++it;
            }
        }
        return nullptr;
    };
    {
        std::lock_guard<std::mutex> lock(registry_mutex());
        if(std::shared_ptr<ServerConnection> live = find_live())
        {
            return live;
        }
    }

    // Connected outside the lock: the status bar lists connections every few frames.
    std::shared_ptr<ServerConnection> connection(new ServerConnection(host, port, options));
    if(!connection->Start(error))
    {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(registry_mutex());
    if(std::shared_ptr<ServerConnection> live = find_live())
    {
        // Another open connected to the same server meanwhile.
        connection->Disconnect();
        return live;
    }
    registry().insert(registry().begin(), connection);
    return connection;
}

std::vector<std::shared_ptr<ServerConnection>>
ServerConnection::GetAll()
{
    std::lock_guard<std::mutex>                    lock(registry_mutex());
    std::vector<std::shared_ptr<ServerConnection>> live;
    for(const std::shared_ptr<ServerConnection>& connection : registry())
    {
        if(connection->GetState() != ServerConnectionState::kLost)
        {
            live.push_back(connection);
        }
    }
    return live;
}

void
ServerConnection::DisconnectAll()
{
    std::vector<std::shared_ptr<ServerConnection>> connections;
    {
        std::lock_guard<std::mutex> lock(registry_mutex());
        connections.swap(registry());
    }
    for(const std::shared_ptr<ServerConnection>& connection : connections)
    {
        connection->Disconnect();
    }
}

void
ServerConnection::SetToken(const std::string& host, int port, bool tls, const std::string& token)
{
    std::lock_guard<std::mutex> lock(token_cache_mutex());
    token_cache()[token_key(host, port, tls)] = token;
}

ServerConnection::ServerConnection(const std::string& host, int port,
                                   const ServerConnectionOptions& options)
: m_host(host)
, m_port(port)
, m_options(options)
, m_last_sent_seq(0)
, m_sent_bytes(0)
, m_state(ServerConnectionState::kLost)
, m_stopping(false)
, m_last_received_seq(0)
, m_next_trace_id(1)
, m_next_listing_id(1)
, m_next_upload_id(1)
, m_next_run_id(1)
{}

ServerConnection::~ServerConnection() { Disconnect(); }

std::string
ServerConnection::ResolveToken() const
{
    if(!m_options.token.empty())
    {
        return m_options.token;
    }
    {
        std::lock_guard<std::mutex> lock(token_cache_mutex());
        auto it = token_cache().find(token_key(m_host, m_port, m_options.tls));
        if(it != token_cache().end())
        {
            return it->second;
        }
    }
    const char* environment = std::getenv(SERVER_TOKEN_ENV);
    if(environment && *environment)
    {
        return environment;
    }
    std::string token;
    if(is_loopback_host(m_host))
    {
        token = read_first_line(std::filesystem::path(get_application_config_path(false)) /
                                server_token_file_name(m_port));
    }
    return token;
}

bool
ServerConnection::Start(std::string& error)
{
    m_token = ResolveToken();
    if(!OpenSocket(error))
    {
        return false;
    }
    m_state  = ServerConnectionState::kConnected;
    m_reader = std::thread(&ServerConnection::ReadLoop, this);

    std::shared_ptr<jt::Json> welcome;
    if(SendHello())
    {
        std::unique_lock<std::mutex> lock(m_open_mutex);
        m_open_condition.wait_for(lock, std::chrono::seconds(HANDSHAKE_TIMEOUT_SECONDS),
                                  [this]() { return m_welcome != nullptr || !IsConnected(); });
        welcome = m_welcome;
    }
    bool resumed = false;
    if(!welcome)
    {
        error = "The server at " + GetAddress() + " did not answer as a roc-optiq-server";
    }
    else if(ApplyWelcome(*welcome, error, resumed))
    {
        SetToken(m_host, m_port, m_options.tls, m_token);
        spdlog::info("Connected to {} at {}{}", m_server_version, GetAddress(),
                     m_options.tls ? " (TLS)" : "");
        return true;
    }
    Disconnect();
    return false;
}

bool
ServerConnection::OpenSocket(std::string& error)
{
    const std::string url = std::string(m_options.tls ? "wss://" : "ws://") + GetAddress() +
                            PROTOCOL_WEBSOCKET_PATH;
    std::unique_ptr<httplib::ws::WebSocketClient> socket =
        std::make_unique<httplib::ws::WebSocketClient>(url);
    if(!socket->is_valid())
    {
        error = "Not a valid server address: " + GetAddress();
        return false;
    }
    if(m_options.tls)
    {
#ifdef CPPHTTPLIB_SSL_ENABLED
        std::string ca_file     = m_options.ca_file;
        const char* environment = std::getenv(SERVER_CA_ENV);
        if(ca_file.empty() && environment && *environment)
        {
            ca_file = environment;
        }
        if(!ca_file.empty())
        {
            socket->set_ca_cert_path(ca_file);
        }
        socket->enable_server_certificate_verification(m_options.verify_certificate);
        socket->enable_server_hostname_verification(m_options.verify_certificate);
#else
        error = "This build of roc-optiq cannot connect with TLS";
        return false;
#endif
    }
    socket->set_connection_timeout(CONNECT_TIMEOUT_SECONDS, 0);
    // Fixed when the socket is created. A response can take as long as the
    // slowest query, and both ends ping well inside this while idle.
    socket->set_read_timeout(CPPHTTPLIB_WEBSOCKET_READ_TIMEOUT_SECOND, 0);
    socket->set_write_timeout(SEND_TIMEOUT_SECONDS, 0);
    httplib::ws::Result connected = socket->connect();
    if(!connected)
    {
        error = "Could not connect to roc-optiq-server at " + GetAddress() + " (" +
                httplib::to_string(connected.error()) + ")";
        return false;
    }
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    m_socket = std::move(socket);
    return true;
}

bool
ServerConnection::SendHello()
{
    jt::Json hello;
    hello["type"]     = MSG_HELLO;
    hello["protocol"] = static_cast<long long>(PROTOCOL_VERSION);
    hello["client"]   = "roc-optiq " + std::to_string(ROCPROFVIS_VERSION_MAJOR) + "." +
                      std::to_string(ROCPROFVIS_VERSION_MINOR) + "." +
                      std::to_string(ROCPROFVIS_VERSION_PATCH);
    if(!m_token.empty())
    {
        hello["token"] = m_token;
    }
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    if(!m_session.empty())
    {
        hello["session"]  = m_session;
        hello["last_seq"] = json_from_uint64(m_last_received_seq);
    }
    // Not numbered: it belongs to this connection, not to the session's stream.
    std::string frame = encode_frame(hello);
    return m_socket && m_socket->send(frame.data(), frame.size());
}

bool
ServerConnection::ApplyWelcome(jt::Json& welcome, std::string& error, bool& resumed)
{
    if(welcome.contains("error"))
    {
        error = json_get_string(welcome, "error");
        return false;
    }
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    m_server_version = json_get_string(welcome, "server");
    m_home_directory = json_get_string(welcome, "home");
    if(welcome.contains("features"))
    {
        jt::Json& features  = welcome["features"];
        m_features.upload   = json_get_bool(features, "upload");
        m_features.profiler = json_get_bool(features, "profiler");
        m_features.compare  = json_get_bool(features, "compare");
        m_features.scripts  = json_get_bool(features, "scripts");
    }
    resumed = json_get_bool(welcome, "resumed");
    m_session = json_get_string(welcome, "session");
    if(resumed)
    {
        // Send again whatever the server had not received when the socket died.
        const uint64_t client_seq = json_get_uint64(welcome, "client_seq");
        while(!m_sent.empty() && m_sent.front().seq <= client_seq)
        {
            m_sent_bytes -= m_sent.front().frame.size();
            m_sent.pop_front();
        }
        for(const SentFrame& frame : m_sent)
        {
            m_socket->send(frame.frame.data(), frame.frame.size());
        }
        spdlog::info("Resumed the session with {}, {} messages sent again", GetAddress(),
                     m_sent.size());
    }
    else
    {
        m_sent.clear();
        m_sent_bytes        = 0;
        m_last_sent_seq     = 0;
        m_last_received_seq = 0;
    }
    // Under the lock, so nothing sent meanwhile is only queued and never resent.
    m_state = ServerConnectionState::kConnected;
    return true;
}

void
ServerConnection::ReadLoop()
{
    while(true)
    {
        httplib::ws::WebSocketClient* socket = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_socket_mutex);
            socket = m_socket.get();
        }
        std::string payload;
        while(socket && socket->read(payload) != httplib::ws::Fail)
        {
            HandleFrame(payload);
            payload.clear();
        }
        m_assembler.Reset();
        bool has_session = false;
        {
            std::lock_guard<std::mutex> lock(m_socket_mutex);
            has_session = !m_session.empty();
        }
        if(m_stopping)
        {
            break;
        }
        if(has_session)
        {
            m_state = ServerConnectionState::kReconnecting;
            spdlog::warn("Lost the connection to {}, reconnecting", GetAddress());
            FailInterruptedTransfers();
            if(Reconnect())
            {
                continue;
            }
        }
        if(!m_stopping)
        {
            spdlog::warn("Disconnected from {}", GetAddress());
            m_state = ServerConnectionState::kLost;
            EndSession();
        }
        break;
    }
}

bool
ServerConnection::Reconnect()
{
    const auto           deadline = std::chrono::steady_clock::now() + RECONNECT_GIVE_UP;
    std::chrono::seconds backoff(1);
    while(!m_stopping && std::chrono::steady_clock::now() < deadline)
    {
        {
            std::unique_lock<std::mutex> lock(m_stop_mutex);
            m_stop_condition.wait_for(lock, backoff, [this]() { return m_stopping.load(); });
        }
        if(m_stopping)
        {
            break;
        }
        backoff = std::min(backoff * 2, RECONNECT_MAX_BACKOFF);

        std::string error;
        std::string payload;
        jt::Json    welcome;
        if(!OpenSocket(error) || !SendHello() ||
           m_socket->read(payload) == httplib::ws::Fail || !decode_frame(payload, welcome) ||
           json_get_string(welcome, "type") != MSG_WELCOME)
        {
            spdlog::debug("Reconnecting to {} failed: {}", GetAddress(), error);
            continue;
        }
        bool resumed = false;
        if(!ApplyWelcome(welcome, error, resumed))
        {
            spdlog::error("{} refused the reconnection: {}", GetAddress(), error);
            return false;
        }
        if(!resumed)
        {
            spdlog::warn("{} no longer has this client's session; its traces must be "
                         "opened again",
                         GetAddress());
            EndSession();
        }
        m_open_condition.notify_all();
        spdlog::info("Reconnected to {}", GetAddress());
        return true;
    }
    return false;
}

void
ServerConnection::EndSession()
{
    // Channels, open waits, listings and transfers of the old session all end here.
    std::vector<std::shared_ptr<ServerTraceChannel>> channels;
    {
        std::lock_guard<std::mutex> lock(m_channels_mutex);
        for(const std::pair<const uint32_t, std::weak_ptr<ServerTraceChannel>>& entry : m_channels)
        {
            if(std::shared_ptr<ServerTraceChannel> channel = entry.second.lock())
            {
                channels.push_back(channel);
            }
        }
        m_channels.clear();
    }
    for(const std::shared_ptr<ServerTraceChannel>& channel : channels)
    {
        jt::Json message;
        message["type"] = MSG_DISCONNECTED;
        channel->Deliver(message);
    }
    {
        std::lock_guard<std::mutex> lock(m_listing_mutex);
        for(std::pair<const uint64_t, ServerDirectoryListing>& listing : m_listings)
        {
            listing.second.ok    = false;
            listing.second.error = "Lost the connection to " + GetAddress();
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_runs_mutex);
        for(std::pair<const uint64_t, ServerProfilerRunStatus>& run : m_runs)
        {
            if(run.second.state == kRPVProfilerStateRunning ||
               run.second.state == kRPVProfilerStateIdle)
            {
                run.second.state = kRPVProfilerStateFailed;
                run.second.error = "Lost the connection to " + GetAddress();
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_files_mutex);
        for(std::pair<const std::pair<uint32_t, uint64_t>, FileTransfer>& file : m_files)
        {
            file.second.stream.close();
            std::remove(file.second.path.c_str());
        }
        m_files.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_uploads_mutex);
        for(std::pair<const uint64_t, std::unique_ptr<Upload>>& upload : m_uploads)
        {
            upload.second->cancelled = true;
        }
    }
    m_open_condition.notify_all();
    m_uploads_condition.notify_all();
}

void
ServerConnection::FailInterruptedTransfers()
{
    // Chunks are not replayed, so a file that was mid-transfer is incomplete.
    std::lock_guard<std::mutex> lock(m_files_mutex);
    for(std::pair<const std::pair<uint32_t, uint64_t>, FileTransfer>& file : m_files)
    {
        if(file.second.begun && !file.second.done)
        {
            file.second.ok = false;
        }
    }
    m_uploads_condition.notify_all();
}

ServerConnectionState
ServerConnection::GetState() const
{
    return m_state;
}

bool
ServerConnection::IsConnected() const
{
    return m_state == ServerConnectionState::kConnected;
}

const std::string&
ServerConnection::GetHost() const
{
    return m_host;
}

int
ServerConnection::GetPort() const
{
    return m_port;
}

bool
ServerConnection::IsTls() const
{
    return m_options.tls;
}

std::string
ServerConnection::GetAddress() const
{
    return make_server_address(m_host, m_port);
}

const std::string&
ServerConnection::GetServerVersion() const
{
    return m_server_version;
}

const std::string&
ServerConnection::GetHomeDirectory() const
{
    return m_home_directory;
}

ServerFeatures
ServerConnection::GetFeatures() const
{
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    return m_features;
}

std::shared_ptr<IDataChannel>
ServerConnection::OpenTrace(const std::vector<std::string>& paths, bool& is_compute,
                            std::string& error)
{
    if(m_state == ServerConnectionState::kLost || paths.empty())
    {
        error = "Not connected to " + GetAddress();
        return nullptr;
    }

    std::shared_ptr<ServerTraceChannel> channel;
    uint32_t                            trace_id = 0;
    {
        std::lock_guard<std::mutex> lock(m_channels_mutex);
        trace_id = m_next_trace_id++;
        channel  = std::make_shared<ServerTraceChannel>(shared_from_this(), trace_id);
        // Registered before the open is sent, so the load's first progress
        // messages have somewhere to queue.
        m_channels[trace_id] = channel;
    }

    jt::Json message;
    message["type"]  = MSG_OPEN_TRACE;
    message["trace"] = static_cast<long long>(trace_id);
    message["path"]  = paths.front();
    if(paths.size() > 1)
    {
        jt::Json compare;
        compare.setArray();
        for(const std::string& path : paths)
        {
            compare.getArray().emplace_back(path);
        }
        message["compare"] = std::move(compare);
    }
    if(!SendMessage(message))
    {
        channel->Abandon();
        error = "Lost the connection to " + GetAddress();
        return nullptr;
    }

    std::shared_ptr<jt::Json> reply;
    {
        std::unique_lock<std::mutex> lock(m_open_mutex);
        // The channel disappears when the session ends before the server answered.
        m_open_condition.wait_for(lock, OPEN_TRACE_TIMEOUT, [this, trace_id]() {
            if(m_state == ServerConnectionState::kLost || m_open_replies.count(trace_id) > 0)
            {
                return true;
            }
            std::lock_guard<std::mutex> channels_lock(m_channels_mutex);
            return m_channels.count(trace_id) == 0;
        });
        auto it = m_open_replies.find(trace_id);
        if(it != m_open_replies.end())
        {
            reply = it->second;
            m_open_replies.erase(it);
        }
    }

    if(!reply)
    {
        channel->Abandon();
        error = m_state != ServerConnectionState::kLost ? "The server did not answer in time"
                                                         : "Lost the connection to " + GetAddress();
        return nullptr;
    }
    if(!json_get_bool(*reply, "ok"))
    {
        channel->Abandon();
        error = json_get_string(*reply, "error");
        return nullptr;
    }
    is_compute = json_get_bool(*reply, "compute");
    return channel;
}

uint64_t
ServerConnection::RequestDirectoryListing(const std::string& path)
{
    uint64_t listing_id = 0;
    {
        std::lock_guard<std::mutex> lock(m_listing_mutex);
        listing_id = m_next_listing_id++;
    }
    jt::Json message;
    message["type"]    = MSG_LIST_DIRECTORY;
    message["listing"] = json_from_uint64(listing_id);
    message["path"]    = path;
    if(!SendMessage(message))
    {
        std::lock_guard<std::mutex> lock(m_listing_mutex);
        ServerDirectoryListing&     listing = m_listings[listing_id];
        listing.ok                          = false;
        listing.path                        = path;
        listing.error                       = "Not connected to " + GetAddress();
    }
    return listing_id;
}

bool
ServerConnection::TakeDirectoryListing(uint64_t listing_id, ServerDirectoryListing& listing)
{
    std::lock_guard<std::mutex> lock(m_listing_mutex);
    auto                        it = m_listings.find(listing_id);
    bool                        result = it != m_listings.end();
    if(result)
    {
        listing = std::move(it->second);
        m_listings.erase(it);
    }
    return result;
}

uint64_t
ServerConnection::StartUpload(const std::string& local_path)
{
    std::lock_guard<std::mutex> lock(m_uploads_mutex);
    const uint64_t              upload_id = m_next_upload_id++;
    std::unique_ptr<Upload>     upload    = std::make_unique<Upload>();
    upload->local_path                    = local_path;
    std::error_code error;
    upload->status.total = std::filesystem::file_size(local_path, error);
    if(error)
    {
        upload->status.state = ServerUploadStatus::State::kFailed;
        upload->status.error = "Cannot read " + local_path;
    }
    Upload* started = upload.get();
    m_uploads[upload_id] = std::move(upload);
    if(!error)
    {
        started->worker = std::thread(&ServerConnection::RunUpload, this, upload_id);
    }
    return upload_id;
}

bool
ServerConnection::GetUploadStatus(uint64_t upload_id, ServerUploadStatus& status)
{
    std::lock_guard<std::mutex> lock(m_uploads_mutex);
    auto                        it     = m_uploads.find(upload_id);
    bool                        result = it != m_uploads.end();
    if(result)
    {
        status = it->second->status;
    }
    return result;
}

void
ServerConnection::CancelUpload(uint64_t upload_id)
{
    std::lock_guard<std::mutex> lock(m_uploads_mutex);
    auto                        it = m_uploads.find(upload_id);
    if(it != m_uploads.end())
    {
        it->second->cancelled = true;
    }
    m_uploads_condition.notify_all();
}

void
ServerConnection::RunUpload(uint64_t upload_id)
{
    Upload* upload = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_uploads_mutex);
        upload = m_uploads.at(upload_id).get();
    }
    auto fail = [this, upload](const std::string& error) {
        std::lock_guard<std::mutex> lock(m_uploads_mutex);
        upload->status.state = ServerUploadStatus::State::kFailed;
        upload->status.error = error;
        spdlog::warn("Upload of {} failed: {}", upload->local_path, error);
    };

    std::ifstream file(upload->local_path, std::ios::binary);
    if(!file.good())
    {
        fail("Cannot read " + upload->local_path);
        return;
    }

    jt::Json begin;
    begin["type"]   = MSG_UPLOAD_BEGIN;
    begin["upload"] = json_from_uint64(upload_id);
    begin["name"]   = std::filesystem::path(upload->local_path).filename().string();
    begin["size"]   = json_from_uint64(upload->status.total);
    if(!SendMessage(begin))
    {
        fail("Not connected to " + GetAddress());
        return;
    }
    std::shared_ptr<jt::Json> ready;
    {
        std::unique_lock<std::mutex> lock(m_uploads_mutex);
        m_uploads_condition.wait_for(lock, UPLOAD_READY_TIMEOUT, [this, upload]() {
            return upload->ready != nullptr || upload->cancelled || !IsConnected();
        });
        ready = upload->ready;
    }
    if(!ready || !json_get_bool(*ready, "ok"))
    {
        fail(ready ? json_get_string(*ready, "error") : "The server did not accept the upload");
        return;
    }

    std::vector<char> buffer(FILE_CHUNK_SIZE);
    uint64_t          sent = 0;
    while(sent < upload->status.total)
    {
        if(upload->cancelled || !IsConnected())
        {
            jt::Json cancel;
            cancel["type"]   = MSG_UPLOAD_CANCEL;
            cancel["upload"] = json_from_uint64(upload_id);
            SendMessage(cancel);
            fail(upload->cancelled ? "Cancelled" : "Lost the connection during the upload");
            return;
        }
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = file.gcount();
        if(count <= 0 ||
           !SendRawFrame(encode_upload_chunk(upload_id, buffer.data(), static_cast<size_t>(count))))
        {
            fail(count <= 0 ? "Could not read " + upload->local_path
                            : "Lost the connection during the upload");
            return;
        }
        sent += static_cast<uint64_t>(count);
        std::lock_guard<std::mutex> lock(m_uploads_mutex);
        upload->status.sent = sent;
    }

    jt::Json end;
    end["type"]   = MSG_UPLOAD_END;
    end["upload"] = json_from_uint64(upload_id);
    SendMessage(end);
    std::shared_ptr<jt::Json> done;
    {
        std::unique_lock<std::mutex> lock(m_uploads_mutex);
        m_uploads_condition.wait_for(lock, UPLOAD_DONE_TIMEOUT, [this, upload]() {
            return upload->done != nullptr || m_state == ServerConnectionState::kLost;
        });
        done = upload->done;
    }
    if(!done || !json_get_bool(*done, "ok"))
    {
        fail(done ? json_get_string(*done, "error") : "The server did not confirm the upload");
        return;
    }
    std::lock_guard<std::mutex> lock(m_uploads_mutex);
    upload->status.state       = ServerUploadStatus::State::kDone;
    upload->status.server_path = json_get_string(*done, "path");
    spdlog::info("Uploaded {} to {}", upload->local_path, upload->status.server_path);
}

uint64_t
ServerConnection::StartProfilerRun(jt::Json& launch)
{
    uint64_t run_id = 0;
    {
        std::lock_guard<std::mutex> lock(m_runs_mutex);
        run_id                = m_next_run_id++;
        m_runs[run_id].state  = kRPVProfilerStateRunning;
    }
    launch["type"] = MSG_PROFILER_LAUNCH;
    launch["run"]  = json_from_uint64(run_id);
    if(!SendMessage(launch))
    {
        std::lock_guard<std::mutex> lock(m_runs_mutex);
        m_runs[run_id].state = kRPVProfilerStateFailed;
        m_runs[run_id].error = "Not connected to " + GetAddress();
    }
    return run_id;
}

bool
ServerConnection::GetProfilerRunStatus(uint64_t run_id, ServerProfilerRunStatus& status)
{
    std::lock_guard<std::mutex> lock(m_runs_mutex);
    auto                        it     = m_runs.find(run_id);
    bool                        result = it != m_runs.end();
    if(result)
    {
        status = it->second;
    }
    return result;
}

void
ServerConnection::CancelProfilerRun(uint64_t run_id)
{
    jt::Json message;
    message["type"] = MSG_PROFILER_CANCEL;
    message["run"]  = json_from_uint64(run_id);
    SendMessage(message);
}

void
ServerConnection::ForgetProfilerRun(uint64_t run_id)
{
    std::lock_guard<std::mutex> lock(m_runs_mutex);
    m_runs.erase(run_id);
}

void
ServerConnection::Disconnect()
{
    {
        std::lock_guard<std::mutex> lock(m_stop_mutex);
        m_stopping = true;
    }
    m_stop_condition.notify_all();
    if(m_state == ServerConnectionState::kConnected)
    {
        // Lets the server close this client's traces now rather than keep them
        // for a resume that will not come.
        jt::Json bye;
        bye["type"] = MSG_BYE;
        SendMessage(bye);
    }
    {
        std::lock_guard<std::mutex> lock(m_socket_mutex);
        if(m_socket)
        {
            m_socket->close();
        }
    }
    if(m_reader.joinable())
    {
        // The last channel can be released on the reader thread itself, when it
        // was delivering to it; that thread is already on its way out.
        if(m_reader.get_id() == std::this_thread::get_id())
        {
            m_reader.detach();
        }
        else
        {
            m_reader.join();
        }
    }
    std::vector<std::thread> workers;
    {
        std::lock_guard<std::mutex> lock(m_uploads_mutex);
        for(std::pair<const uint64_t, std::unique_ptr<Upload>>& upload : m_uploads)
        {
            upload.second->cancelled = true;
            if(upload.second->worker.joinable())
            {
                workers.push_back(std::move(upload.second->worker));
            }
        }
    }
    m_uploads_condition.notify_all();
    for(std::thread& worker : workers)
    {
        worker.join();
    }
    if(m_state.exchange(ServerConnectionState::kLost) != ServerConnectionState::kLost)
    {
        spdlog::info("Disconnected from {}", GetAddress());
        EndSession();
    }
}

void
ServerConnection::DropConnection()
{
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    if(m_socket)
    {
        m_socket->close();
    }
}

void
ServerConnection::HandleFrame(std::string& payload)
{
    if(payload.empty() || !m_assembler.Add(payload))
    {
        return;
    }
    if(payload[0] == FRAME_TAG_FILE_CHUNK)
    {
        HandleFileChunk(payload);
        return;
    }

    jt::Json message;
    if(!decode_frame(payload, message))
    {
        return;
    }
    const uint64_t seq = json_get_uint64(message, "seq");
    if(seq > m_last_received_seq)
    {
        m_last_received_seq = seq;
    }
    std::string    type     = json_get_string(message, "type");
    const uint32_t trace_id = static_cast<uint32_t>(json_get_uint64(message, "trace"));

    if(type == MSG_WELCOME)
    {
        {
            std::lock_guard<std::mutex> lock(m_open_mutex);
            m_welcome = std::make_shared<jt::Json>(std::move(message));
        }
        m_open_condition.notify_all();
    }
    else if(type == MSG_TRACE_OPENED)
    {
        {
            std::lock_guard<std::mutex> lock(m_open_mutex);
            m_open_replies[trace_id] = std::make_shared<jt::Json>(std::move(message));
        }
        m_open_condition.notify_all();
    }
    else if(type == MSG_DIRECTORY)
    {
        ServerDirectoryListing listing;
        listing.ok     = json_get_bool(message, "ok");
        listing.path   = json_get_string(message, "path");
        listing.parent = json_get_string(message, "parent");
        listing.error  = json_get_string(message, "error");
        for(jt::Json& entry : json_get_array(message, "entries"))
        {
            listing.entries.push_back({ json_get_string(entry, "name"),
                                        json_get_bool(entry, "directory"),
                                        json_get_uint64(entry, "size") });
        }
        std::lock_guard<std::mutex> lock(m_listing_mutex);
        m_listings[json_get_uint64(message, "listing")] = std::move(listing);
    }
    else if(type == MSG_UPLOAD_READY || type == MSG_UPLOAD_DONE)
    {
        {
            std::lock_guard<std::mutex> lock(m_uploads_mutex);
            auto it = m_uploads.find(json_get_uint64(message, "upload"));
            if(it != m_uploads.end())
            {
                (type == MSG_UPLOAD_READY ? it->second->ready : it->second->done) =
                    std::make_shared<jt::Json>(std::move(message));
            }
        }
        m_uploads_condition.notify_all();
    }
    else if(type == MSG_PROFILER_OUTPUT || type == MSG_PROFILER_STATE)
    {
        std::lock_guard<std::mutex> lock(m_runs_mutex);
        auto                        it = m_runs.find(json_get_uint64(message, "run"));
        if(it != m_runs.end())
        {
            if(type == MSG_PROFILER_OUTPUT)
            {
                it->second.output += json_get_string(message, "text");
            }
            else
            {
                it->second.state     = static_cast<int32_t>(json_get_int64(message, "state"));
                it->second.exit_code = static_cast<int32_t>(json_get_int64(message, "exit_code"));
                it->second.error     = json_get_string(message, "error");
            }
        }
    }
    else if(type == MSG_FILE_BEGIN || type == MSG_FILE_END)
    {
        std::lock_guard<std::mutex> lock(m_files_mutex);
        auto it = m_files.find({ trace_id, json_get_uint64(message, "id") });
        if(it != m_files.end())
        {
            FileTransfer& transfer = it->second;
            if(type == MSG_FILE_BEGIN)
            {
                transfer.stream.open(transfer.path, std::ios::binary | std::ios::trunc);
                transfer.expected_size = json_get_uint64(message, "size");
                transfer.written       = 0;
                transfer.begun         = true;
                transfer.ok            = transfer.stream.good();
            }
            else
            {
                transfer.stream.close();
                transfer.ok   = transfer.ok && json_get_bool(message, "ok") &&
                              !transfer.stream.fail() &&
                              transfer.written == transfer.expected_size;
                transfer.done = true;
            }
        }
    }
    else if(type == MSG_RESPONSE)
    {
        {
            std::lock_guard<std::mutex> lock(m_files_mutex);
            auto it = m_files.find({ trace_id, json_get_uint64(message, "id") });
            if(it != m_files.end())
            {
                const bool file_ok = it->second.ok && it->second.done;
                if(!file_ok)
                {
                    it->second.stream.close();
                    std::remove(it->second.path.c_str());
                }
                message["file_ok"] = file_ok;
                m_files.erase(it);
            }
        }
        DeliverToTrace(trace_id, message);
    }
    else if(message.contains("trace"))
    {
        DeliverToTrace(trace_id, message);
    }
    else if(type == MSG_ERROR)
    {
        spdlog::error("Server {}: {}", GetAddress(), json_get_string(message, "message"));
    }
}

void
ServerConnection::HandleFileChunk(const std::string& payload)
{
    uint32_t    trace_id   = 0;
    uint64_t    request_id = 0;
    const char* data       = nullptr;
    size_t      size       = 0;
    if(!decode_file_chunk(payload, trace_id, request_id, data, size))
    {
        return;
    }
    std::lock_guard<std::mutex> lock(m_files_mutex);
    auto                        it = m_files.find({ trace_id, request_id });
    if(it != m_files.end() && it->second.ok)
    {
        it->second.stream.write(data, static_cast<std::streamsize>(size));
        it->second.written += size;
        it->second.ok = it->second.stream.good();
    }
}

void
ServerConnection::DeliverToTrace(uint32_t trace_id, jt::Json& message)
{
    std::shared_ptr<ServerTraceChannel> channel;
    {
        std::lock_guard<std::mutex> lock(m_channels_mutex);
        auto                        it = m_channels.find(trace_id);
        if(it != m_channels.end())
        {
            channel = it->second.lock();
        }
    }
    if(channel)
    {
        channel->Deliver(message);
    }
}

bool
ServerConnection::SendMessage(jt::Json& message)
{
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    if(m_state == ServerConnectionState::kLost)
    {
        return false;
    }
    message["seq"]    = json_from_uint64(++m_last_sent_seq);
    std::string frame = encode_frame(message);
    // While resuming the frame only queues; it goes out once the session is back.
    if(m_state == ServerConnectionState::kConnected && m_socket)
    {
        m_socket->send(frame.data(), frame.size());
    }
    m_sent_bytes += frame.size();
    m_sent.push_back({ m_last_sent_seq, std::move(frame) });
    while(m_sent_bytes > SENT_BUFFER_BYTES && m_sent.size() > 1)
    {
        m_sent_bytes -= m_sent.front().frame.size();
        m_sent.pop_front();
    }
    return true;
}

bool
ServerConnection::SendRawFrame(const std::string& frame)
{
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    return m_state == ServerConnectionState::kConnected && m_socket &&
           m_socket->send(frame.data(), frame.size());
}

void
ServerConnection::ExpectFile(uint32_t trace_id, uint64_t request_id, const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_files_mutex);
    FileTransfer&               transfer = m_files[{ trace_id, request_id }];
    transfer.path                        = path;
    transfer.expected_size               = 0;
    transfer.written                     = 0;
    transfer.begun                       = false;
    transfer.ok                          = false;
    transfer.done                        = false;
}

void
ServerConnection::RemoveChannel(uint32_t trace_id)
{
    std::lock_guard<std::mutex> lock(m_channels_mutex);
    m_channels.erase(trace_id);
}

}  // namespace View
}  // namespace RocProfVis
