// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_ssh_bridge.h"
#include "rocprofvis_controller_future.h"
#include <string>
#include <vector>
#include <mutex>
#include "libssh2.h"
#ifdef _WIN32
#include <winsock2.h>
using socket_t                          = SOCKET;
constexpr socket_t kInvalidSocket       = INVALID_SOCKET;
#ifdef GetObject
#undef GetObject
#endif
#else
using socket_t                          = int;
constexpr socket_t kInvalidSocket       = -1;
#endif

namespace RocProfVis
{
namespace Controller
{

    class SshConnection : public Handle
    {
        public:
            SshConnection(std::string host, int port) : Handle(__kRPVControllerSshConnectionPropertiesFirst, __kRPVControllerSshConnectionPropertiesLast), 
                m_session(nullptr), m_socket(-1), m_host(host), m_port(port), m_connected(false) {};
            virtual ~SshConnection() {};
            rocprofvis_controller_object_type_t GetType(void) final;

            // Property handlers
            rocprofvis_result_t GetUInt64(rocprofvis_property_t property,
                uint64_t index,
                uint64_t* value) final;

            rocprofvis_result_t GetString(rocprofvis_property_t property,
                uint64_t index,
                char* value,
                uint32_t* length) final;

            SshBridge*           GetSshBridge() { return &m_net_bridge; }
            LIBSSH2_SESSION*     GetSession() { return m_session; }
            int                  GetSocket() { return m_socket; }
            std::string          GetHost() { return m_host; }
            int                  GetPort() { return m_port; }
            void                 SetSession(LIBSSH2_SESSION* session) { m_session = session; };
            void                 SetSocket(int socket) { m_socket = socket; };
            void                 SetConnected() { m_connected = true; }
            void                 Disconnect();
            bool                 IsConnected() { return m_connected; };
            bool                 IsValid() { return m_session != nullptr && m_socket != kInvalidSocket; };


        private:
            // Actions (called by SSH layer to send a message to UI)
            void                 AddStdOut(char* stdout_buffer, uint64_t stdout_count, bool last = false);
            void                 SaveError(std::string& err);
            void                 SetFileStat(std::string name, uint64_t size, uint64_t time, uint64_t dowloaded);
            void                 SetDownloaded(uint64_t size);

            SshBridge            m_net_bridge;
            LIBSSH2_SESSION*     m_session;
            int                  m_socket;
            std::string          m_host;
            int                  m_port;
            bool                 m_connected;
    };

    class SshClient
    {
    public:
        enum class Result
        {
            Success,
            SocketError,
            SessionError,
            HandshakeError,
            AuthError,
            ChannelError,
            SftpError,
            FileError,
            ReadError,
            WriteError,
            Cancelled
        };

        enum class KeyType {
            KEY_UNKNOWN,
            KEY_RSA,
            KEY_ECDSA,
            KEY_ED25519,
            KEY_DSA
        };

        // kbdint trampoline. The libssh2 callback signature gives us only an
        // `abstract` void**, which we set to the SshBridge before kicking auth.
        struct KbdintCtx
        {
            SshBridge*               bridge        = nullptr;
            bool                     was_cancelled = false;
        };


    public:
        SshClient();
        ~SshClient();
        SshConnection* AllocateConnection(
            const std::string& host,
            int port);
        void DeleteConnection(
            SshConnection* conn);

        static Result Connect(
            SshConnection* connection,
            Future* future);

        static Result Authenticate(
            SshConnection * connection,
            const std::string& user, 
            const std::string& password, 
            const std::string& identity_file, 
            const std::string& passphrase, 
            Future* future);


        // Executes `command` on the remote host, streaming stdout/stderr into
        // the connection's bridge. If `exit_code` is non-null, the remote
        // process's exit status (from libssh2_channel_get_exit_status) is written
        // to it. The returned Result reflects the SSH transport outcome
        // (Success/Cancelled/transport errors), NOT the remote process exit code.
        // wait_for_status_consumed: when true (interactive SshSession path), the
        // call blocks at the end until the UI consumes the final bridge status
        // (back-pressure handshake). The profiler executor path has no status
        // consumer and must pass false to avoid deadlocking the worker thread.
        static Result ExecuteCommand(SshConnection* connection, const std::string& command, Future* future, int* exit_code = nullptr, bool wait_for_status_consumed = true);
        static Result DownloadFile(SshConnection * connection, const std::string& remote_path, const std::string& local_path, Future* future);
        static Result BrowseRemoteDirectory(SshConnection * connection, const std::string& path, Future* future);

        static bool IsAlive(SshConnection * connection);
        static bool IsFutureCanceled(SshConnection * connection, Future* future);
        static void SetKeepAlive(SshConnection * connection, int interval_seconds);

    private:

        static bool MethodListed(const char* methods, const char* needle);
        static std::string ExpandTilde(const std::string& p);
        static bool TryPublicKey(SshConnection * connection, const std::string& user,
            const std::string& priv_path_in, const std::string& passphrase, Future* future);
        static bool TryAgent(SshConnection * connection, const std::string& user, Future* future);
        static std::vector<std::string> DefaultKeyPaths();
        static void KbdIntCallback(
            const char* name, 
            int name_len,
            const char* instruction, int instruction_len,
            int num_prompts,
            const LIBSSH2_USERAUTH_KBDINT_PROMPT* prompts,
            LIBSSH2_USERAUTH_KBDINT_RESPONSE* responses,
            void** abstract);
        static KeyType DetectPubkeyType(const char* pubkey_path);

        static bool Reconnect(SshConnection* connection, Future* future);
        static int  CreateTcpConnection(const std::string& host, int port);
        static bool WaitSocket(SshConnection* connection);

        std::vector<std::unique_ptr<SshConnection>> m_connections;

    };
}
}
