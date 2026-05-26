// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_ssh_auth_bridge.h"
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
                m_session(nullptr), m_socket(-1), m_host(host), m_port(port) {};
            virtual ~SshConnection() {};
            rocprofvis_controller_object_type_t GetType(void) final;
            AuthBridge* GetAuthBridge() { return &m_auth_bridge; }
            LIBSSH2_SESSION* GetSession() { return m_session; }
            int GetSocket() { return m_socket; }
            void SetSession(LIBSSH2_SESSION* session) { m_session = session; };
            void SetSocket(int socket) { m_socket = socket; };
            void Disconnect();
            std::string GetHost() { return m_host; }
            int GetPort() { return m_port; }
        private:
            AuthBridge m_auth_bridge;
            LIBSSH2_SESSION* m_session;
            int m_socket;
            std::string m_host;
            int m_port;
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
            WriteError
        };

        // kbdint trampoline. The libssh2 callback signature gives us only an
        // `abstract` void**, which we set to the AuthBridge before kicking auth.
        struct KbdintCtx
        {
            AuthBridge*           bridge        = nullptr;
            Future*               future        = nullptr;
            bool                  was_cancelled = false;
        };


    public:
        SshClient();
        ~SshClient();

        SshConnection * Connect(
            const std::string& host, 
            int port, 
            Future* future);

        Result Authenticate(
            SshConnection * connection,
            const std::string& user, 
            const std::string& password, 
            const std::string& identity_file, 
            const std::string& passphrase, 
            Future* future);

        void Disconnect( SshConnection * connection);

        Result ExecuteCommand(SshConnection* connection, const std::string& command, Future* future);

        Result DownloadFile(SshConnection * connection, const std::string& remote_path, const std::string& local_path, Future* future);
        void SubmitPromptResponses(SshConnection* connection, std::vector<std::string> responses);
        void SubmitHostKeyDecision(SshConnection* connection, HostKeyDecision decision);
        void CancelPrompt(SshConnection* connection);

        bool IsAlive(SshConnection * connection);
        void SetKeepAlive(SshConnection * connection, int interval_seconds);

    private:

        static bool MethodListed(const char* methods, const char* needle);
        static std::string ExpandTilde(const std::string& p);
        static bool TryPublicKey(LIBSSH2_SESSION* session, const std::string& user,
            const std::string& priv_path_in, const std::string& passphrase);
        static bool TryAgent(LIBSSH2_SESSION* session, const std::string& user);
        static std::vector<std::string> DefaultKeyPaths();
        static void KbdIntCallback(
            const char* name, 
            int name_len,
            const char* instruction, int instruction_len,
            int num_prompts,
            const LIBSSH2_USERAUTH_KBDINT_PROMPT* prompts,
            LIBSSH2_USERAUTH_KBDINT_RESPONSE* responses,
            void** abstract);

        std::vector<std::unique_ptr<SshConnection>> m_connections;
        std::mutex m_mutex;

    };
}
}
