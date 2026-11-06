// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_server_session.h"
#include "protocol/rocprofvis_protocol.h"
#include "protocol/rocprofvis_protocol_frame.h"
#include "rocprofvis_server_trace.h"
#include "rocprofvis_version.h"
#ifdef ROCPROFVIS_ENABLE_PROFILER
#include "rocprofvis_server_profiler.h"
#endif

#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <vector>

namespace RocProfVis
{
namespace Server
{

namespace
{

// How often the worker pumps the providers while they have work in flight, and
// how long it sleeps waiting for a message when they do not.
constexpr std::chrono::milliseconds BUSY_POLL_INTERVAL(1);
constexpr std::chrono::milliseconds IDLE_POLL_INTERVAL(50);
// What a client that drops can still catch up on when it resumes.
constexpr size_t REPLAY_BUFFER_BYTES = 256ull * 1024 * 1024;

struct DirectoryEntry
{
    std::string name;
    bool        is_directory;
    uint64_t    size;
};

std::string
get_home_directory()
{
#ifdef _WIN32
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    std::string result;
    if(home && *home)
    {
        result = home;
    }
    else
    {
        std::error_code error;
        result = std::filesystem::current_path(error).string();
    }
    return result;
}

std::string
get_server_version()
{
    return std::to_string(ROCPROFVIS_VERSION_MAJOR) + "." +
           std::to_string(ROCPROFVIS_VERSION_MINOR) + "." +
           std::to_string(ROCPROFVIS_VERSION_PATCH);
}

std::string
random_directory_name()
{
    std::random_device                      device;
    std::uniform_int_distribution<uint32_t> distribution;
    char                                    buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%08x", distribution(device));
    return buffer;
}

}  // namespace

ServerSession::ServerSession(const std::string& id, const session_options_t& options)
: m_id(id)
, m_options(options)
, m_attachment(0)
, m_last_seq(0)
, m_replay_bytes(0)
, m_detached_since(std::chrono::steady_clock::now())
, m_last_client_seq(0)
, m_stopping(false)
, m_ended(false)
, m_busy(false)
{
    m_worker = std::thread(&ServerSession::Run, this);
}

ServerSession::~ServerSession() { Shutdown(); }

const std::string&
ServerSession::GetId() const
{
    return m_id;
}

bool
ServerSession::CanResume(uint64_t last_seq)
{
    std::lock_guard<std::mutex> lock(m_send_mutex);
    bool                        result = false;
    if(last_seq == m_last_seq)
    {
        result = true;
    }
    else if(last_seq < m_last_seq && !m_replay.empty())
    {
        result = m_replay.front().seq <= last_seq + 1;
    }
    return result;
}

uint64_t
ServerSession::Attach(SendFunction send, CloseFunction close, bool resumed, uint64_t last_seq)
{
    std::lock_guard<std::mutex> lock(m_send_mutex);
    // A connection the server had not noticed was dead yet. Closed under the
    // lock: its handler cannot finish Detach, and free the socket, until then.
    if(m_close)
    {
        m_close();
    }
    m_send                = std::move(send);
    m_close               = std::move(close);
    const uint64_t attachment = ++m_attachment;
    SendWelcome(resumed);
    if(resumed)
    {
        size_t replayed = 0;
        for(const ReplayFrame& frame : m_replay)
        {
            if(frame.seq > last_seq && m_send)
            {
                m_send(frame.frame);
                replayed++;
            }
        }
        spdlog::info("Session {} resumed, {} messages replayed", m_id, replayed);
    }
    return attachment;
}

void
ServerSession::Detach(uint64_t attachment)
{
    std::lock_guard<std::mutex> lock(m_send_mutex);
    if(attachment == m_attachment)
    {
        m_send           = nullptr;
        m_close          = nullptr;
        m_detached_since = std::chrono::steady_clock::now();
    }
}

void
ServerSession::CloseConnection()
{
    std::lock_guard<std::mutex> lock(m_send_mutex);
    if(m_close)
    {
        m_close();
    }
}

bool
ServerSession::IsExpired(std::chrono::seconds timeout)
{
    std::lock_guard<std::mutex> lock(m_send_mutex);
    return !m_send && std::chrono::steady_clock::now() - m_detached_since > timeout;
}

bool
ServerSession::HasEnded() const
{
    return m_ended;
}

void
ServerSession::OnFrame(std::string&& payload)
{
    InboxEntry entry;
    if(!payload.empty() && payload[0] == View::FRAME_TAG_UPLOAD_CHUNK)
    {
        entry.chunk = std::move(payload);
    }
    else if(!View::decode_frame(payload, entry.message))
    {
        spdlog::warn("Ignoring a frame that is not a protocol message");
        return;
    }
    else if(View::json_get_string(entry.message, "type") == View::MSG_BYE)
    {
        m_ended = true;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_inbox_mutex);
        if(entry.chunk.empty())
        {
            // A client that resumed sends again what it is not sure arrived.
            const uint64_t seq = View::json_get_uint64(entry.message, "seq");
            if(seq != 0 && seq <= m_last_client_seq)
            {
                return;
            }
            m_last_client_seq = std::max(m_last_client_seq, seq);
        }
        m_inbox.push_back(std::move(entry));
    }
    m_inbox_condition.notify_one();
}

void
ServerSession::Shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_inbox_mutex);
        m_stopping = true;
    }
    m_inbox_condition.notify_one();
    if(m_worker.joinable())
    {
        m_worker.join();
    }
}

void
ServerSession::Run()
{
    while(true)
    {
        std::deque<InboxEntry> entries;
        {
            std::unique_lock<std::mutex> lock(m_inbox_mutex);
            m_inbox_condition.wait_for(lock, m_busy ? BUSY_POLL_INTERVAL : IDLE_POLL_INTERVAL,
                                       [this]() { return m_stopping || !m_inbox.empty(); });
            if(m_stopping)
            {
                break;
            }
            entries.swap(m_inbox);
        }

        for(InboxEntry& entry : entries)
        {
            if(!entry.chunk.empty())
            {
                HandleUploadChunk(entry.chunk);
            }
            else
            {
                HandleMessage(entry.message);
            }
        }

        bool busy = false;
        for(std::pair<const uint32_t, std::unique_ptr<ServerTrace>>& trace : m_traces)
        {
            busy = trace.second->Update() || busy;
        }
#ifdef ROCPROFVIS_ENABLE_PROFILER
        for(auto it = m_profiler_runs.begin(); it != m_profiler_runs.end();)
        {
            if(it->second->Update())
            {
                busy = true;
                ++it;
            }
            else
            {
                it = m_profiler_runs.erase(it);
            }
        }
#endif
        m_busy = busy;
    }

    // Each trace's provider cancels and waits for its in-flight work here.
    m_profiler_runs.clear();
    m_traces.clear();
    DiscardUploads();
}

void
ServerSession::HandleMessage(jt::Json& message)
{
    std::string type = View::json_get_string(message, "type");
    if(type == View::MSG_OPEN_TRACE)
    {
        HandleOpenTrace(message);
    }
    else if(type == View::MSG_LIST_DIRECTORY)
    {
        HandleListDirectory(message);
    }
    else if(type == View::MSG_UPLOAD_BEGIN)
    {
        HandleUploadBegin(message);
    }
    else if(type == View::MSG_UPLOAD_END || type == View::MSG_UPLOAD_CANCEL)
    {
        HandleUploadEnd(message, type == View::MSG_UPLOAD_CANCEL);
    }
    else if(type == View::MSG_PROFILER_LAUNCH)
    {
        HandleProfilerLaunch(message);
    }
    else if(type == View::MSG_PROFILER_CANCEL)
    {
#ifdef ROCPROFVIS_ENABLE_PROFILER
        auto run = m_profiler_runs.find(View::json_get_uint64(message, "run"));
        if(run != m_profiler_runs.end())
        {
            run->second->Cancel();
        }
#endif
    }
    else
    {
        const uint32_t trace_id =
            static_cast<uint32_t>(View::json_get_uint64(message, "trace"));
        if(type == View::MSG_CLOSE_TRACE)
        {
            if(m_traces.erase(trace_id) > 0)
            {
                spdlog::info("Closed trace {}", trace_id);
            }
            return;
        }

        ServerTrace* trace = FindTrace(trace_id);
        if(!trace)
        {
            spdlog::warn("{} for trace {}, which is not open", type, trace_id);
        }
        else if(type == View::MSG_REQUEST)
        {
            trace->HandleRequest(View::json_get_uint64(message, "id"),
                                 View::json_get_string(message, "op"), message["params"]);
        }
        else if(type == View::MSG_CANCEL)
        {
            trace->Cancel(View::json_get_uint64(message, "id"));
        }
        else if(type == View::MSG_SET_SELECTED)
        {
            trace->SetSelected();
        }
        else
        {
            spdlog::warn("Ignoring unknown message type '{}'", type);
        }
    }
}

void
ServerSession::SendWelcome(bool resumed)
{
    jt::Json welcome;
    welcome["type"]       = View::MSG_WELCOME;
    welcome["protocol"]   = static_cast<long long>(View::PROTOCOL_VERSION);
    welcome["server"]     = "roc-optiq-server " + get_server_version();
    welcome["home"]       = get_home_directory();
    welcome["session"]    = m_id;
    welcome["resumed"]    = resumed;
    {
        std::lock_guard<std::mutex> lock(m_inbox_mutex);
        welcome["client_seq"] = View::json_from_uint64(m_last_client_seq);
    }
    jt::Json features;
    features["upload"] = !m_options.upload_directory.empty();
#ifdef ROCPROFVIS_ENABLE_PROFILER
    features["profiler"] = m_options.allow_profiler;
#else
    features["profiler"] = false;
#endif
#ifdef ROCPROFVIS_ENABLE_TRACE_COMPARE
    features["compare"] = true;
#else
    features["compare"] = false;
#endif
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    features["scripts"] = m_options.allow_scripts;
#else
    features["scripts"] = false;
#endif
    welcome["features"] = std::move(features);
    // Not numbered: it belongs to this connection, not to the session's stream.
    if(m_send)
    {
        m_send(View::encode_frame(welcome));
    }
}

void
ServerSession::HandleOpenTrace(jt::Json& message)
{
    const uint32_t trace_id = static_cast<uint32_t>(View::json_get_uint64(message, "trace"));
    std::vector<std::string> paths;
    for(jt::Json& path : View::json_get_array(message, "compare"))
    {
        paths.push_back(View::json_to_string(path));
    }
    if(paths.empty())
    {
        paths.push_back(View::json_get_string(message, "path"));
    }

    jt::Json reply;
    reply["type"]  = View::MSG_TRACE_OPENED;
    reply["trace"] = static_cast<long long>(trace_id);
    if(m_traces.count(trace_id))
    {
        reply["ok"]    = false;
        reply["error"] = "Trace id " + std::to_string(trace_id) + " is already open";
    }
    else
    {
        std::unique_ptr<ServerTrace> trace = std::make_unique<ServerTrace>(
            [this, trace_id](jt::Json& trace_message) {
                return SendTraceMessage(trace_id, trace_message);
            },
            [this, trace_id](uint64_t request_id, const char* data, size_t size) {
                return SendFileChunk(trace_id, request_id, data, size);
            });
        trace->SetAllowScripts(m_options.allow_scripts);
        bool        is_compute = false;
        std::string error;
        if(trace->Open(paths, m_options.config_path, is_compute, error))
        {
            reply["ok"]      = true;
            reply["compute"] = is_compute;
            m_traces.emplace(trace_id, std::move(trace));
        }
        else
        {
            spdlog::warn("Could not open {}: {}", paths.front(), error);
            reply["ok"]    = false;
            reply["error"] = error;
        }
    }
    Send(reply);
}

void
ServerSession::HandleListDirectory(jt::Json& message)
{
    std::string path = View::json_get_string(message, "path");
    if(path.empty())
    {
        path = get_home_directory();
    }

    std::error_code       error;
    std::filesystem::path directory = std::filesystem::weakly_canonical(path, error);
    if(error)
    {
        directory = path;
        error.clear();
    }

    std::vector<DirectoryEntry>         entries;
    std::filesystem::directory_iterator it(
        directory, std::filesystem::directory_options::skip_permission_denied, error);
    std::filesystem::directory_iterator end;
    while(!error && it != end)
    {
        const std::filesystem::directory_entry& entry = *it;
        std::string name = entry.path().filename().string();
        if(!name.empty() && name[0] != '.')
        {
            std::error_code entry_error;
            DirectoryEntry  listed = { name, entry.is_directory(entry_error), 0 };
            if(!listed.is_directory)
            {
                uint64_t size = entry.file_size(entry_error);
                listed.size   = entry_error ? 0 : size;
            }
            entries.push_back(std::move(listed));
        }
        it.increment(error);
    }
    std::sort(entries.begin(), entries.end(),
              [](const DirectoryEntry& a, const DirectoryEntry& b) {
                  if(a.is_directory != b.is_directory)
                  {
                      return a.is_directory;
                  }
                  return a.name < b.name;
              });

    jt::Json reply;
    reply["type"]    = View::MSG_DIRECTORY;
    reply["listing"] = View::json_from_uint64(View::json_get_uint64(message, "listing"));
    reply["path"]    = directory.string();
    reply["parent"]  = directory.parent_path().string();
    reply["ok"]      = !error;
    if(error)
    {
        reply["error"] = error.message();
    }
    jt::Json listed;
    listed.setArray();
    for(const DirectoryEntry& entry : entries)
    {
        jt::Json entry_json;
        entry_json["name"]      = entry.name;
        entry_json["directory"] = entry.is_directory;
        entry_json["size"]      = View::json_from_uint64(entry.size);
        listed.getArray().push_back(std::move(entry_json));
    }
    reply["entries"] = std::move(listed);
    Send(reply);
}

void
ServerSession::HandleUploadBegin(jt::Json& message)
{
    const uint64_t upload_id = View::json_get_uint64(message, "upload");
    const uint64_t size      = View::json_get_uint64(message, "size");
    // Only the file name is kept, so a client cannot write outside its folder.
    std::string name =
        std::filesystem::path(View::json_get_string(message, "name")).filename().string();
    if(name.empty() || name == "." || name == "..")
    {
        name = "uploaded-trace";
    }

    jt::Json reply;
    reply["type"]   = View::MSG_UPLOAD_READY;
    reply["upload"] = View::json_from_uint64(upload_id);
    std::string error;
    if(m_options.upload_directory.empty())
    {
        error = "This server does not accept uploads.";
    }
    else if(m_options.max_upload_bytes != 0 && size > m_options.max_upload_bytes)
    {
        error = "The trace is larger than this server accepts (" +
                std::to_string(m_options.max_upload_bytes / (1024 * 1024)) + " MiB).";
    }
    else if(m_uploads.count(upload_id))
    {
        error = "Upload " + std::to_string(upload_id) + " is already in progress.";
    }
    else
    {
        std::error_code       fs_error;
        std::filesystem::path directory =
            std::filesystem::path(m_options.upload_directory) / random_directory_name();
        std::filesystem::create_directories(directory, fs_error);
        Upload& upload       = m_uploads[upload_id];
        upload.path          = (directory / name).string();
        upload.expected_size = size;
        upload.received      = 0;
        upload.stream.open(upload.path, std::ios::binary | std::ios::trunc);
        upload.ok = !fs_error && upload.stream.good();
        if(upload.ok)
        {
            reply["path"] = upload.path;
            spdlog::info("Receiving upload of {} bytes into {}", size, upload.path);
        }
        else
        {
            error = "Could not create " + upload.path + " on the server.";
            m_uploads.erase(upload_id);
        }
    }
    reply["ok"] = error.empty();
    if(!error.empty())
    {
        reply["error"] = error;
    }
    Send(reply);
}

void
ServerSession::HandleUploadChunk(const std::string& frame)
{
    uint64_t    upload_id = 0;
    const char* data      = nullptr;
    size_t      size      = 0;
    if(!View::decode_upload_chunk(frame, upload_id, data, size))
    {
        return;
    }
    auto it = m_uploads.find(upload_id);
    if(it == m_uploads.end() || !it->second.ok)
    {
        return;
    }
    Upload& upload = it->second;
    if(upload.received + size > upload.expected_size)
    {
        upload.ok = false;
        return;
    }
    upload.stream.write(data, static_cast<std::streamsize>(size));
    upload.received += size;
    upload.ok = upload.stream.good();
}

void
ServerSession::HandleUploadEnd(jt::Json& message, bool cancel)
{
    const uint64_t upload_id = View::json_get_uint64(message, "upload");
    auto           it        = m_uploads.find(upload_id);
    if(it == m_uploads.end())
    {
        return;
    }
    Upload& upload = it->second;
    upload.stream.close();
    const bool ok = !cancel && upload.ok && !upload.stream.fail() &&
                    upload.received == upload.expected_size;
    if(!ok)
    {
        std::error_code error;
        std::filesystem::remove(upload.path, error);
        std::filesystem::remove(std::filesystem::path(upload.path).parent_path(), error);
    }
    if(!cancel)
    {
        jt::Json reply;
        reply["type"]   = View::MSG_UPLOAD_DONE;
        reply["upload"] = View::json_from_uint64(upload_id);
        reply["ok"]     = ok;
        if(ok)
        {
            reply["path"] = upload.path;
            spdlog::info("Received upload {}", upload.path);
        }
        else
        {
            reply["error"] = "The upload did not arrive complete (" +
                             std::to_string(upload.received) + " of " +
                             std::to_string(upload.expected_size) + " bytes).";
        }
        Send(reply);
    }
    m_uploads.erase(it);
}

void
ServerSession::DiscardUploads()
{
    for(std::pair<const uint64_t, Upload>& upload : m_uploads)
    {
        upload.second.stream.close();
        std::error_code error;
        std::filesystem::remove(upload.second.path, error);
        std::filesystem::remove(std::filesystem::path(upload.second.path).parent_path(),
                                error);
    }
    m_uploads.clear();
}

void
ServerSession::HandleProfilerLaunch(jt::Json& message)
{
    const uint64_t run_id = View::json_get_uint64(message, "run");
    std::string    error;
#ifdef ROCPROFVIS_ENABLE_PROFILER
    if(!m_options.allow_profiler)
    {
        error = "This server does not run profilers. Start it with --allow-profiler.";
    }
    else if(m_profiler_runs.count(run_id))
    {
        error = "Profiler run " + std::to_string(run_id) + " is already in progress.";
    }
    else
    {
        std::unique_ptr<ServerProfilerRun> run = std::make_unique<ServerProfilerRun>(
            run_id, [this](jt::Json& run_message) { return Send(run_message); });
        if(run->Launch(message, error))
        {
            m_profiler_runs.emplace(run_id, std::move(run));
        }
    }
#else
    error = "This server was built without profiler support.";
#endif
    if(!error.empty())
    {
        spdlog::warn("Profiler run {} not started: {}", run_id, error);
        jt::Json reply;
        reply["type"]      = View::MSG_PROFILER_STATE;
        reply["run"]       = View::json_from_uint64(run_id);
        reply["state"]     = static_cast<long long>(kRPVProfilerStateFailed);
        reply["exit_code"] = -1;
        reply["error"]     = error;
        Send(reply);
    }
}

ServerTrace*
ServerSession::FindTrace(uint32_t trace_id)
{
    auto it = m_traces.find(trace_id);
    return it != m_traces.end() ? it->second.get() : nullptr;
}

bool
ServerSession::Send(jt::Json& message)
{
    std::lock_guard<std::mutex> lock(m_send_mutex);
    message["seq"]    = View::json_from_uint64(++m_last_seq);
    std::string frame = View::encode_frame(message);
    spdlog::trace("Sent {} ({} bytes)", View::json_get_string(message, "type"), frame.size());
    bool result = m_send ? m_send(frame) : true;
    m_replay_bytes += frame.size();
    m_replay.push_back({ m_last_seq, std::move(frame) });
    while(m_replay_bytes > REPLAY_BUFFER_BYTES && m_replay.size() > 1)
    {
        m_replay_bytes -= m_replay.front().frame.size();
        m_replay.pop_front();
    }
    return result;
}

bool
ServerSession::SendTraceMessage(uint32_t trace_id, jt::Json& message)
{
    message["trace"] = static_cast<long long>(trace_id);
    return Send(message);
}

bool
ServerSession::SendFileChunk(uint32_t trace_id, uint64_t request_id, const char* data,
                             size_t size)
{
    std::lock_guard<std::mutex> lock(m_send_mutex);
    return m_send && m_send(View::encode_file_chunk(trace_id, request_id, data, size));
}

}  // namespace Server
}  // namespace RocProfVis
