// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "protocol/rocprofvis_protocol_frame.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace httplib
{
namespace ws
{
class WebSocketClient;
}
}  // namespace httplib

namespace jt
{
class Json;
}

namespace RocProfVis
{
namespace View
{

class IDataChannel;
class ServerTraceChannel;

struct ServerConnectionOptions
{
    bool        tls = false;
    // Empty: the token remembered for this server, then ROCPROFVIS_SERVER_TOKEN,
    // then, for a server on this machine, the token file it saved.
    std::string token;
    // Empty: ROCPROFVIS_SERVER_CA, then the system's certificate authorities.
    std::string ca_file;
    // Off accepts any certificate, such as a self-signed one, unchecked.
    bool        verify_certificate = true;
};

enum class ServerConnectionState
{
    kConnected,
    // The socket dropped; requests queue until the session is resumed.
    kReconnecting,
    // The server is gone, or no longer has this client's session.
    kLost
};

struct ServerFeatures
{
    bool upload   = false;
    bool profiler = false;
    bool compare  = false;
    bool scripts  = false;
};

struct ServerDirectoryEntry
{
    std::string name;
    bool        is_directory;
    uint64_t    size;
};

struct ServerDirectoryListing
{
    bool                              ok = false;
    std::string                       path;
    std::string                       parent;
    std::string                       error;
    std::vector<ServerDirectoryEntry> entries;
};

struct ServerUploadStatus
{
    enum class State
    {
        kRunning,
        kDone,
        kFailed
    };
    State       state = State::kRunning;
    uint64_t    sent  = 0;
    uint64_t    total = 0;
    std::string server_path;
    std::string error;
};

struct ServerProfilerRunStatus
{
    // Everything the run has printed so far.
    std::string output;
    // A rocprofvis_profiler_state_t.
    int32_t     state     = 0;
    int32_t     exit_code = -1;
    std::string error;
};

/**
 * @brief The UI's connection to one roc-optiq-server.
 *
 * Owns the WebSocket and a reader thread that routes each incoming message to
 * the trace channel it belongs to. Connections are shared by address, so every
 * trace opened on the same server uses one socket. When the socket drops, the
 * reader reconnects and resumes the session, replaying what each side missed;
 * channels only see a disconnect when the session cannot be resumed.
 */
class ServerConnection : public std::enable_shared_from_this<ServerConnection>
{
public:
    // A connected server at host:port, reusing a live connection. Connecting
    // blocks the caller for at most a few seconds. Null, with error set, on failure.
    static std::shared_ptr<ServerConnection> Get(const std::string& host, int port,
                                                 const ServerConnectionOptions& options,
                                                 std::string&                   error);
    // Every connection not yet lost, most recent first.
    static std::vector<std::shared_ptr<ServerConnection>> GetAll();
    static void DisconnectAll();
    // Remembers the token for a server for the rest of this run, e.g. from a URI.
    static void SetToken(const std::string& host, int port, bool tls, const std::string& token);

    ~ServerConnection();

    ServerConnection(const ServerConnection&)            = delete;
    ServerConnection& operator=(const ServerConnection&) = delete;

    ServerConnectionState GetState() const;
    bool                  IsConnected() const;
    const std::string&    GetHost() const;
    int                   GetPort() const;
    bool                  IsTls() const;
    std::string           GetAddress() const;
    const std::string&    GetServerVersion() const;
    const std::string&    GetHomeDirectory() const;
    ServerFeatures        GetFeatures() const;

    /*
     * Opens a trace on the server and blocks until the server has identified it.
     * The server starts loading straight away; the channel queues what it sends.
     * @param paths: One trace, or two or more system traces to compare.
     * @param is_compute: Set to the kind of trace on success.
     * @param error: Set to a message for the user on failure.
     */
    std::shared_ptr<IDataChannel> OpenTrace(const std::vector<std::string>& paths,
                                            bool& is_compute, std::string& error);

    // Asks for a listing of path (the server's home directory when empty).
    // Collect it with TakeDirectoryListing using the returned id.
    uint64_t RequestDirectoryListing(const std::string& path);
    bool     TakeDirectoryListing(uint64_t listing_id, ServerDirectoryListing& listing);

    // Copies a local file to the server's upload folder on a worker thread.
    uint64_t StartUpload(const std::string& local_path);
    bool     GetUploadStatus(uint64_t upload_id, ServerUploadStatus& status);
    void     CancelUpload(uint64_t upload_id);

    // Runs a profiler on the server; launch carries tool, tool_directory,
    // working_directory, argv and env as in profiler_launch.
    uint64_t StartProfilerRun(jt::Json& launch);
    bool     GetProfilerRunStatus(uint64_t run_id, ServerProfilerRunStatus& status);
    void     CancelProfilerRun(uint64_t run_id);
    void     ForgetProfilerRun(uint64_t run_id);

    void Disconnect();
    // Closes the socket as a network failure would, so the connection resumes.
    void DropConnection();

private:
    friend class ServerTraceChannel;

    struct FileTransfer
    {
        std::string   path;
        std::ofstream stream;
        uint64_t      expected_size = 0;
        uint64_t      written       = 0;
        bool          begun         = false;
        bool          ok            = false;
        bool          done          = false;
    };

    struct Upload
    {
        std::string        local_path;
        ServerUploadStatus status;
        std::atomic<bool>  cancelled{ false };
        std::thread        worker;
        std::shared_ptr<jt::Json> ready;
        std::shared_ptr<jt::Json> done;
    };

    struct SentFrame
    {
        uint64_t    seq;
        std::string frame;
    };

    ServerConnection(const std::string& host, int port, const ServerConnectionOptions& options);

    bool Start(std::string& error);
    bool OpenSocket(std::string& error);
    bool SendHello();
    bool ApplyWelcome(jt::Json& welcome, std::string& error, bool& resumed);
    void ReadLoop();
    bool Reconnect();
    // Fails everything that belonged to the server's session for this client.
    void EndSession();
    void HandleFrame(std::string& payload);
    void HandleFileChunk(const std::string& payload);
    void DeliverToTrace(uint32_t trace_id, jt::Json& message);
    bool SendMessage(jt::Json& message);
    bool SendRawFrame(const std::string& frame);
    void ExpectFile(uint32_t trace_id, uint64_t request_id, const std::string& path);
    void RemoveChannel(uint32_t trace_id);
    void FailInterruptedTransfers();
    void RunUpload(uint64_t upload_id);
    std::string ResolveToken() const;

    std::string             m_host;
    int                     m_port;
    ServerConnectionOptions m_options;
    std::string             m_token;
    std::string             m_server_version;
    std::string             m_home_directory;
    ServerFeatures          m_features;

    // Guards the socket and the outgoing sequence; only the reader replaces the socket.
    mutable std::mutex                            m_socket_mutex;
    std::unique_ptr<httplib::ws::WebSocketClient> m_socket;
    uint64_t                                      m_last_sent_seq;
    std::deque<SentFrame>                         m_sent;
    size_t                                        m_sent_bytes;
    std::string                                   m_session;

    std::thread                        m_reader;
    // Reader thread only.
    FrameAssembler                     m_assembler;
    std::atomic<ServerConnectionState> m_state;
    std::atomic<bool>                  m_stopping;
    std::atomic<uint64_t>              m_last_received_seq;
    std::mutex                         m_stop_mutex;
    std::condition_variable            m_stop_condition;

    std::mutex                                               m_channels_mutex;
    std::map<uint32_t, std::weak_ptr<ServerTraceChannel>> m_channels;
    uint32_t                                                 m_next_trace_id;

    // Replies the UI thread blocks on: the handshake and each trace open.
    std::mutex                                    m_open_mutex;
    std::condition_variable                       m_open_condition;
    std::shared_ptr<jt::Json>                     m_welcome;
    std::map<uint32_t, std::shared_ptr<jt::Json>> m_open_replies;

    std::mutex                                 m_listing_mutex;
    std::map<uint64_t, ServerDirectoryListing> m_listings;
    uint64_t                                   m_next_listing_id;

    std::mutex                                              m_files_mutex;
    std::map<std::pair<uint32_t, uint64_t>, FileTransfer> m_files;

    std::mutex                                 m_uploads_mutex;
    std::condition_variable                    m_uploads_condition;
    std::map<uint64_t, std::unique_ptr<Upload>> m_uploads;
    uint64_t                                   m_next_upload_id;

    std::mutex                                  m_runs_mutex;
    std::map<uint64_t, ServerProfilerRunStatus> m_runs;
    uint64_t                                    m_next_run_id;
};

}  // namespace View
}  // namespace RocProfVis
