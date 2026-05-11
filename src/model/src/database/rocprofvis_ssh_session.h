// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <atomic>
#include <cstdint>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
using socket_t                          = SOCKET;
constexpr socket_t kInvalidSocket       = INVALID_SOCKET;
#else
using socket_t                          = int;
constexpr socket_t kInvalidSocket       = -1;
#endif

namespace RocProfVis
{
namespace DataModel
{

class AuthBridge;

struct AuthOptions
{
    std::string password;        // empty => not provided
    std::string identity_file;   // empty => try defaults
    std::string passphrase;      // empty => key assumed unencrypted
};

enum class ConnectResult
{
    Ok,
    NeedsPassword,
    AuthFailed,
    HostKeyMismatch,
    HostKeyRejected,
    ConnectionError,
    Cancelled
};

struct ProgressSink
{
    std::atomic<uint64_t> bytes_read{0};
    uint64_t              total = 0;
};

// RAII libssh2 session + SFTP wrapper. Single-threaded; meant to live on a
// worker thread for the duration of one Connect()+DownloadTo()+Disconnect().
class SshSession
{
public:
    SshSession();
    ~SshSession();

    SshSession(const SshSession&)            = delete;
    SshSession& operator=(const SshSession&) = delete;

    ConnectResult Connect(const std::string& host,
                          int                port,
                          const std::string& user,
                          const AuthOptions& auth,
                          AuthBridge&        bridge,
                          std::string&       err);

    // SFTP-stat the file. Returns 0 on failure.
    uint64_t StatFileSize(const std::string& remote_path,
                          uint64_t&          mtime_out,
                          std::string&       err);

    bool DownloadTo(const std::string& remote_path,
                    const std::string& local_path,
                    ProgressSink&      sink,
                    std::string&       err);

    void Disconnect();

    bool IsConnected() const { return m_session != nullptr && m_sftp != nullptr; }

private:
    socket_t         m_sock    = kInvalidSocket;
    LIBSSH2_SESSION* m_session = nullptr;
    LIBSSH2_SFTP*    m_sftp    = nullptr;
};

// Process-wide one-shot init/exit for libssh2.
void EnsureLibssh2Inited();
void Libssh2GlobalShutdown();

}  // namespace DataModel
}  // namespace RocProfVis
