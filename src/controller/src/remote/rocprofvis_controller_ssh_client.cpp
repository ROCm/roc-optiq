// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_ssh_client.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_ssh_known_hosts.h"
#include "rocprofvis_controller_ssh_auth_bridge.h"
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <algorithm>

#ifdef _WIN32
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSE_SOCKET(s) closesocket(s)
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCKET(s) ::close(s)
#endif

namespace RocProfVis
{
namespace Controller
{

    rocprofvis_controller_object_type_t SshConnection::GetType(void) 
    {
        return kRPVControllerObjectTypeRemoteConnection;
    }

    SshClient::SshClient()
    {
#ifdef _WIN32
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
        if (libssh2_init(0) != 0) {
#ifdef _WIN32
            WSACleanup();
#endif
        }
    }

    SshClient::~SshClient()
    {
        std::lock_guard lock(m_mutex);
        for (auto & connection : m_connections)
        {
            connection->Disconnect();
        }
        m_connections.clear();
        libssh2_exit();
#ifdef _WIN32
        WSACleanup();
#endif
    }

//---------------------------------------------INTERACTIVE PARAMETERS CALLBACK----------------------------------------------//

    void SshClient::KbdIntCallback(const char* name, int name_len,
        const char* instruction, int instruction_len,
        int num_prompts,
        const LIBSSH2_USERAUTH_KBDINT_PROMPT* prompts,
        LIBSSH2_USERAUTH_KBDINT_RESPONSE* responses,
        void** abstract)
    {
        auto* ctx = static_cast<KbdintCtx*>(*abstract);
        spdlog::info("[ssh] kbdint callback fired: name_len={} instr_len={} num_prompts={} ctx={}",
            name_len, instruction_len, num_prompts,
            static_cast<void*>(ctx));
        if(!ctx || !ctx->bridge) return;

        // libssh2 fires this callback for every kbdint round, including "info"
        // rounds (banner / status messages with no input). If there are no
        // prompts, there is nothing to ask the user — just acknowledge and
        // continue without touching the UI.
        if(num_prompts == 0)
        {
            spdlog::info("[ssh] kbdint callback: zero-prompt round (info/banner), auto-acking");
            return;
        }

        PromptRequest req;
        if(name_len > 0) req.name.assign(name, name_len);
        if(instruction_len > 0) req.instruction.assign(instruction, instruction_len);
        req.prompts.reserve(num_prompts);
        for(int i = 0; i < num_prompts; i++)
        {
            PromptItem p;
            p.text.assign(reinterpret_cast<const char*>(prompts[i].text),
                prompts[i].length);
            p.echo = prompts[i].echo != 0;
            req.prompts.push_back(std::move(p));
        }

        spdlog::info("[ssh] kbdint callback: posting {} prompt(s) to UI bridge, blocking...",
            num_prompts);
        auto answer = ctx->bridge->AskPrompts(ctx->future, req);
        spdlog::info("[ssh] kbdint callback: bridge returned (cancelled={})", !answer.has_value());
        if(!answer)
        {
            ctx->was_cancelled = true;
            for(int i = 0; i < num_prompts; i++)
            {
                responses[i].text   = nullptr;
                responses[i].length = 0;
            }
            return;
        }

        const auto& vec = *answer;
        for(int i = 0; i < num_prompts; i++)
        {
            const std::string& s = (i < (int) vec.size()) ? vec[i] : std::string{};
            responses[i].text   = static_cast<char*>(malloc(s.size() + 1));
            if(responses[i].text)
            {
                std::memcpy(responses[i].text, s.data(), s.size());
                responses[i].text[s.size()] = '\0';
                responses[i].length         = static_cast<unsigned int>(s.size());
            }
            else
            {
                responses[i].length = 0;
            }
        }
    }

//---------------------------------------------STATIC HELPERS----------------------------------------------//

    bool SshClient::MethodListed(const char* methods, const char* needle)
    {
        return methods && std::strstr(methods, needle) != nullptr;
    }

    std::string SshClient::ExpandTilde(const std::string& p)
    {
        if(p.empty() || p[0] != '~') return p;
        if(p.size() == 1 || p[1] == '/' || p[1] == '\\')
        {
#ifdef _WIN32
            const char* home = std::getenv("USERPROFILE");
#else
            const char* home = std::getenv("HOME");
#endif
            if(home) return std::string(home) + p.substr(1);
        }
        // ~user/... is not expanded (would require getpwnam); pass through.
        return p;
    }

    bool SshClient::TryPublicKey(LIBSSH2_SESSION* session, const std::string& user,
        const std::string& priv_path_in, const std::string& passphrase)
    {
        std::string priv_path = ExpandTilde(priv_path_in);
        if(!std::filesystem::exists(priv_path))
        {
            spdlog::warn("[ssh] publickey: private key not found: '{}' (expanded from '{}')",
                priv_path, priv_path_in);
            return false;
        }
        std::string pub_path = priv_path + ".pub";
        bool        have_pub = std::filesystem::exists(pub_path);
        const char* pub      = have_pub ? pub_path.c_str() : nullptr;
        spdlog::info("[ssh] trying publickey: priv={} pub={} have_passphrase={}",
            priv_path, have_pub ? pub_path : std::string("(derived from priv)"),
            !passphrase.empty());
        int rc = libssh2_userauth_publickey_fromfile(
            session, user.c_str(), pub, priv_path.c_str(),
            passphrase.empty() ? nullptr : passphrase.c_str());
        if(rc == 0)
        {
            spdlog::info("[ssh] publickey auth OK ({})", priv_path);
            return true;
        }
        char* msg = nullptr;
        libssh2_session_last_error(session, &msg, nullptr, 0);
        const char* hint = "";
        switch(rc)
        {
        case LIBSSH2_ERROR_FILE:
            hint = " [file unreadable or unsupported format]"; break;
        case LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED:
            hint = " [server rejected the key — possibly encrypted (passphrase needed) or not in authorized_keys]"; break;
        case LIBSSH2_ERROR_AUTHENTICATION_FAILED:
            hint = " [auth rejected by server]"; break;
        case LIBSSH2_ERROR_METHOD_NOT_SUPPORTED:
            hint = " [server does not support this key type]"; break;
        default: break;
        }
        spdlog::warn("[ssh] publickey auth failed (rc={}): {}{}", rc,
            msg ? msg : "(no message)", hint);
        return false;
    }

    // Try every identity loaded into ssh-agent. Returns true on first success.
    bool SshClient::TryAgent(LIBSSH2_SESSION* session, const std::string& user)
    {
        LIBSSH2_AGENT* agent = libssh2_agent_init(session);
        if(!agent)
        {
            spdlog::info("[ssh] agent: init failed (libssh2 built without agent support?)");
            return false;
        }
        if(libssh2_agent_connect(agent) != 0)
        {
            spdlog::info("[ssh] agent: connect failed (no $SSH_AUTH_SOCK or agent not running)");
            libssh2_agent_free(agent);
            return false;
        }
        if(libssh2_agent_list_identities(agent) != 0)
        {
            spdlog::warn("[ssh] agent: list_identities failed");
            libssh2_agent_disconnect(agent);
            libssh2_agent_free(agent);
            return false;
        }

        bool ok = false;
        struct libssh2_agent_publickey* identity = nullptr;
        struct libssh2_agent_publickey* prev     = nullptr;
        int  identity_count                      = 0;
        while(true)
        {
            int rc = libssh2_agent_get_identity(agent, &identity, prev);
            if(rc == 1) break;       // end of list
            if(rc < 0)
            {
                spdlog::warn("[ssh] agent: get_identity rc={}", rc);
                break;
            }
            identity_count++;
            spdlog::info("[ssh] agent: trying identity '{}'",
                identity->comment ? identity->comment : "(no comment)");
            if(libssh2_agent_userauth(agent, user.c_str(), identity) == 0)
            {
                spdlog::info("[ssh] agent: auth OK with '{}'",
                    identity->comment ? identity->comment : "(no comment)");
                ok = true;
                break;
            }
            char* msg = nullptr;
            libssh2_session_last_error(session, &msg, nullptr, 0);
            spdlog::info("[ssh] agent: identity rejected: {}", msg ? msg : "(no message)");
            prev = identity;
        }
        if(!ok && identity_count == 0)
        {
            spdlog::info("[ssh] agent: connected but no identities loaded (run 'ssh-add')");
        }

        libssh2_agent_disconnect(agent);
        libssh2_agent_free(agent);
        return ok;
    }

    std::vector<std::string> SshClient::DefaultKeyPaths()
    {
        std::string home;
#ifdef _WIN32
        if(const char* h = std::getenv("USERPROFILE")) home = h;
        std::string sep = "\\";
        std::string sub = "\\.ssh\\";
#else
        if(const char* h = std::getenv("HOME")) home = h;
        std::string sep = "/";
        (void) sep;
        std::string sub = "/.ssh/";
#endif
        if(home.empty()) return {};
        return {home + sub + "id_ed25519",
            home + sub + "id_rsa",
            home + sub + "id_ecdsa",
            home + sub + "id_dsa"};
    }

//---------------------------------------------SSH CLIENT CONNECT----------------------------------------------//

    SshConnection * SshClient::Connect(
        const std::string& host, 
        int port, 
        Future* future)
    {
        std::string err;
        struct addrinfo hints{}, *res = nullptr;

        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        std::string portstr = std::to_string(port);
        int rc = getaddrinfo(host.c_str(), portstr.c_str(), &hints, &res);
        if (rc != 0)
        {
            err = std::string("DNS resolution failed: ") + gai_strerror(rc);
            spdlog::error("[ssh] {}", err);
            future->SaveError(err);
            return nullptr;
        }

        sockaddr_in* addr = (sockaddr_in*)res->ai_addr;
        addr->sin_port = htons(port);

        auto connection =
            std::make_unique<SshConnection>(host, port);

        for(addrinfo* p = res; p; p = p->ai_next)
        {
            connection->SetSocket(socket(p->ai_family, p->ai_socktype, p->ai_protocol));
            if(connection->GetSocket() == kInvalidSocket) continue;
            if(connect(connection->GetSocket(), p->ai_addr, static_cast<socklen_t>(p->ai_addrlen)) == 0)
            {
                break;
            }
            CLOSE_SOCKET(connection->GetSocket());
            connection->SetSocket(kInvalidSocket);
        }

        freeaddrinfo(res);   

        if (connection->GetSocket() == kInvalidSocket)
        {
            err = "TCP connect failed";
            spdlog::error("[ssh] {}", err);
            future->SaveError(err);
            return nullptr;
        }

        spdlog::info("[ssh] TCP connected");

        connection->SetSession(libssh2_session_init());
        if (!connection->GetSession())
        {
            err = "libssh2_session_init failed";
            spdlog::error("[ssh] {}", err);
            future->SaveError(err);
            connection->Disconnect();
            return nullptr;
        }

        libssh2_session_set_blocking(connection->GetSession(), 1);

        if (libssh2_session_handshake(connection->GetSession(), connection->GetSocket()))
        {
            char* msg = nullptr;
            libssh2_session_last_error(connection->GetSession(), &msg, nullptr, 0);
            err = std::string("SSH handshake failed: ") + (msg ? msg : "");
            spdlog::error("[ssh] {}", err);
            future->SaveError(err);
            connection->Disconnect();
            return nullptr;
        }

        spdlog::info("[ssh] handshake ok");

        SshConnection* ptr = connection.get();

        m_connections.push_back(std::move(connection));

        return ptr;
    }

    SshClient::Result SshClient::Authenticate(
        SshConnection * connection,
        const std::string& user, 
        const std::string& password, 
        const std::string& identity_file, 
        const std::string& passphrase, 
        Future* future)
    {
        std::string err;
        // ---- host key check ----
        {
            KnownHosts kh(connection->GetSession());
            bool       loaded = kh.Load();
            spdlog::info("[ssh] known_hosts loaded={} path={} fingerprint={}",
                loaded, kh.Path(), FormatHostKeyFingerprint(connection->GetSession()));
            KnownHostMatch m = kh.Check(connection->GetHost(), connection->GetPort());
            const char* mstr = m == KnownHostMatch::Match    ? "Match"
                : m == KnownHostMatch::Mismatch ? "Mismatch"
                : m == KnownHostMatch::NotFound ? "NotFound"
                : "Failure";
            spdlog::info("[ssh] host key check: {}", mstr);

            if(m == KnownHostMatch::NotFound || m == KnownHostMatch::Mismatch)
            {
                HostKeyRequest req;
                req.host                   = connection->GetHost();
                req.port                   = connection->GetPort();
                req.fingerprint_sha256_b64 = FormatHostKeyFingerprint(connection->GetSession());
                req.key_type               = HostKeyTypeName(connection->GetSession());
                req.state = (m == KnownHostMatch::Mismatch) ? HostKeyState::Mismatch
                    : HostKeyState::NotFound;

                auto decision = connection->GetAuthBridge()->AskHostKey(future, req);
                if(!decision)
                {
                    err = "Authentication bridge not initialized";
                    spdlog::error("[ssh] {}", err);
                    future->SaveError(err);
                    connection->Disconnect();
                    return Result::AuthError;
                }
                if(*decision == HostKeyDecision::Reject)
                {
                    connection->Disconnect();
                    err = (m == KnownHostMatch::Mismatch)
                        ? "Host key mismatch — connection rejected."
                        : "Host key not trusted.";
                    return Result::AuthError;
                }
                if(*decision == HostKeyDecision::TrustPermanently)
                {
                    kh.Add(connection->GetHost(), connection->GetPort());
                    if(!kh.Save())
                    {
                        spdlog::warn("Could not persist known_hosts at {}", kh.Path());
                    }
                }
                // TrustOnce: continue without saving.
            }
            else if(m == KnownHostMatch::Failure)
            {
                err = "Host key check failed";
                future->SaveError(err);
                connection->Disconnect();
                return Result::AuthError;
            }
        }

        // ---- auth ----
        char* methods = libssh2_userauth_list(connection->GetSession(), user.c_str(),
            static_cast<unsigned int>(user.size()));
        spdlog::info("[ssh] server auth methods: {}", methods ? methods : "(none)");

        bool tried_password = false;
        bool tried_kbdint   = false;
        int  auth_rc        = LIBSSH2_ERROR_AUTHENTICATION_FAILED;

        // 1) publickey
        if(MethodListed(methods, "publickey"))
        {
            // 1a) explicit identity_file from the dialog
            if(!identity_file.empty())
            {
                if(TryPublicKey(connection->GetSession(), user, identity_file, passphrase))
                    auth_rc = 0;
            }
            // 1b) ssh-agent — handles encrypted keys without us needing a passphrase.
            if(auth_rc != 0)
            {
                spdlog::info("[ssh] trying ssh-agent");
                if(TryAgent(connection->GetSession(), user)) auth_rc = 0;
            }
            // 1c) default on-disk identity files
            if(auth_rc != 0)
            {
                spdlog::info("[ssh] trying default identity files");
                for(const auto& p : DefaultKeyPaths())
                {
                    if(!std::filesystem::exists(p))
                    {
                        spdlog::info("[ssh]   default key absent: {}", p);
                        continue;
                    }
                    if(TryPublicKey(connection->GetSession(), user, p, passphrase))
                    {
                        auth_rc = 0;
                        break;
                    }
                }
            }
        }
        else
        {
            spdlog::info("[ssh] server does not advertise publickey");
        }

        // 2) password
        if(auth_rc != 0 && MethodListed(methods, "password") && !password.empty())
        {
            spdlog::info("[ssh] trying password auth");
            tried_password = true;
            auth_rc = libssh2_userauth_password(connection->GetSession(), user.c_str(),
                password.c_str());
            if(auth_rc == 0)
            {
                spdlog::info("[ssh] password auth OK");
            }
            else
            {
                char* msg = nullptr;
                libssh2_session_last_error(connection->GetSession(), &msg, nullptr, 0);
                spdlog::warn("[ssh] password auth failed (rc={}): {}", auth_rc,
                    msg ? msg : "(no message)");
            }
        }
        else if(auth_rc != 0)
        {
            spdlog::info("[ssh] skipping password auth (server_lists={} have_password={})",
                MethodListed(methods, "password"), !password.empty());
        }

        // 3) keyboard-interactive
        KbdintCtx kbd_ctx{connection->GetAuthBridge(), future, false};
        if(auth_rc != 0 && MethodListed(methods, "keyboard-interactive"))
        {
            spdlog::info("[ssh] trying keyboard-interactive auth (will route prompts to UI)");
            tried_kbdint = true;
            // libssh2's session abstract slot: stash kbd_ctx so the C callback can find it.
            void** abstract = libssh2_session_abstract(connection->GetSession());
            if(abstract) *abstract = &kbd_ctx;
            auth_rc = libssh2_userauth_keyboard_interactive(connection->GetSession(), user.c_str(),
                &KbdIntCallback);
            if(abstract) *abstract = nullptr;
            if(kbd_ctx.was_cancelled)
            {
                spdlog::info("[ssh] kbdint cancelled by user");
                connection->Disconnect();
                return Result::AuthError;
            }
            if(auth_rc == 0)
            {
                spdlog::info("[ssh] kbdint auth OK");
            }
            else
            {
                char* msg = nullptr;
                libssh2_session_last_error(connection->GetSession(), &msg, nullptr, 0);
                spdlog::warn("[ssh] kbdint auth failed (rc={}): {}", auth_rc,
                    msg ? msg : "(no message)");
            }
        }

        return Result::Success;
    }

    void SshClient::Disconnect(SshConnection* connection)
    {
        auto it = std::find_if(m_connections.begin(), m_connections.end(), [connection](std::unique_ptr<SshConnection>& c) {return connection->GetSession() == c->GetSession() && connection->GetSocket() == c->GetSocket(); });
        if (it != m_connections.end())
        {
            it->get()->Disconnect();
            m_connections.erase(it);
        }
    }

    void SshClient::SubmitPromptResponses(SshConnection* connection, std::vector<std::string> responses)
    {
        connection->GetAuthBridge()->SubmitPromptResponses(responses);
    }

    void SshClient::SubmitHostKeyDecision(SshConnection* connection, HostKeyDecision decision)
    {
        connection->GetAuthBridge()->SubmitHostKeyDecision(decision);
    }

    void SshClient::CancelPrompt(SshConnection* connection)
    {
        connection->GetAuthBridge()->Cancel();
    }


    void SshConnection::Disconnect() {
        m_auth_bridge.Cancel();
        if (m_session)
        {
            libssh2_session_disconnect(m_session, "Normal Shutdown");
            libssh2_session_free(m_session);
        }

        if (m_socket != kInvalidSocket)
        {
            CLOSE_SOCKET(m_socket);
            m_socket = kInvalidSocket;
        }
    }


    void wait_socket(int socket_fd, LIBSSH2_SESSION* session)
    {
        struct timeval timeout;
        fd_set fdread, fdwrite;
        fd_set fdex;

        int dir;

        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdex);

        FD_SET(socket_fd, &fdread);
        FD_SET(socket_fd, &fdwrite);
        FD_SET(socket_fd, &fdex);

        dir = libssh2_session_block_directions(session);

        select(socket_fd + 1,
            (dir & LIBSSH2_SESSION_BLOCK_INBOUND) ? &fdread : NULL,
            (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND) ? &fdwrite : NULL,
            &fdex,
            &timeout);
    }

    SshClient::Result SshClient::ExecuteCommand(SshConnection * connection, const std::string& command, Future* future)
    {
        std::string output;
        int exit_code = -1;

        if (!connection || !connection->GetSession())
        {
            return Result::SessionError;
        }
        libssh2_session_set_blocking(connection->GetSession(), 1);

        LIBSSH2_CHANNEL* channel = libssh2_channel_open_session(connection->GetSession());
        if (!channel)
        {
            output = "Failed to open SSH channel";
            future->AddStdOut(output.data(), output.size()+1);
            return Result::ChannelError;
        }

        if (libssh2_channel_exec(channel, command.c_str()))
        {
            output = "libssh2_channel_exec failed";
            future->AddStdOut(output.data(), output.size()+1);
            libssh2_channel_free(channel);
            return Result::ChannelError;
        }

        char buffer[4096];

        libssh2_session_set_blocking(connection->GetSession(), 0);

        while (true)
        {
            bool got_data = false;
            ssize_t n1 = libssh2_channel_read(channel, buffer, sizeof(buffer));
            ssize_t n2 = libssh2_channel_read_stderr(channel, buffer, sizeof(buffer));

            if (n1 > 0)
            {
                future->AddStdOut(buffer, n1);

                got_data = true;
            }

            if (n2 > 0)
            {
                future->AddStdOut(buffer, n2);

                got_data = true;
            }

            if (n1 == LIBSSH2_ERROR_EAGAIN && n2 == LIBSSH2_ERROR_EAGAIN)
            {
                wait_socket(connection->GetSocket(), connection->GetSession()); 
            }

            if (!got_data)
            {
                if (libssh2_channel_eof(channel))
                    break;
            }
        }

/*
        while (true)
        {
            bool got_data = false;

            ssize_t n1 =
                libssh2_channel_read(
                    channel,
                    buffer,
                    sizeof(buffer));

            if (n1 > 0)
            {
                future->AddStdOut(buffer, n1);

                got_data = true;
            }

            ssize_t n2 =
                libssh2_channel_read_stderr(
                    channel,
                    buffer,
                    sizeof(buffer));

            if (n2 > 0)
            {
                future->AddStdOut(buffer, n2);

                got_data = true;
            }


            if (!got_data)
            {
                if (libssh2_channel_eof(channel))
                {
                    break;
                }
            }
        }
*/
        libssh2_channel_close(channel);

        libssh2_channel_wait_closed(channel);

        exit_code = libssh2_channel_get_exit_status(channel);

        output = "Exit code : " + std::to_string(exit_code);
        future->AddStdOut(output.data(), output.size()+1);

        libssh2_channel_free(channel);

        return Result::Success;
    }
 

    SshClient::Result SshClient::DownloadFile(SshConnection * connection, const std::string& remote_path, const std::string& local_path, Future* future)
    {
        std::string err;
        libssh2_session_set_blocking(connection->GetSession(), 1);

        LIBSSH2_SFTP* sftp = libssh2_sftp_init(connection->GetSession());
        if (!sftp)
            return Result::SftpError;

        LIBSSH2_SFTP_ATTRIBUTES attrs{};
        int rc = libssh2_sftp_stat(sftp, remote_path.c_str(), &attrs);

        if(rc != 0)
        {
            unsigned long sftp_err = libssh2_sftp_last_error(sftp);
            char*         lib_msg  = nullptr;
            int           lib_rc   = libssh2_session_last_error(connection->GetSession(), &lib_msg, nullptr, 0);
            const char*   sftp_str = "unknown";
            switch(sftp_err)
            {
            case LIBSSH2_FX_OK:                  sftp_str = "OK"; break;
            case LIBSSH2_FX_EOF:                 sftp_str = "EOF"; break;
            case LIBSSH2_FX_NO_SUCH_FILE:        sftp_str = "NO_SUCH_FILE"; break;
            case LIBSSH2_FX_PERMISSION_DENIED:   sftp_str = "PERMISSION_DENIED"; break;
            case LIBSSH2_FX_FAILURE:             sftp_str = "FAILURE"; break;
            case LIBSSH2_FX_BAD_MESSAGE:         sftp_str = "BAD_MESSAGE"; break;
            case LIBSSH2_FX_NO_CONNECTION:       sftp_str = "NO_CONNECTION"; break;
            case LIBSSH2_FX_CONNECTION_LOST:     sftp_str = "CONNECTION_LOST"; break;
            case LIBSSH2_FX_OP_UNSUPPORTED:      sftp_str = "OP_UNSUPPORTED"; break;
            case LIBSSH2_FX_INVALID_HANDLE:      sftp_str = "INVALID_HANDLE"; break;
            case LIBSSH2_FX_NO_SUCH_PATH:        sftp_str = "NO_SUCH_PATH"; break;
            case LIBSSH2_FX_FILE_ALREADY_EXISTS: sftp_str = "FILE_ALREADY_EXISTS"; break;
            case LIBSSH2_FX_WRITE_PROTECT:       sftp_str = "WRITE_PROTECT"; break;
            case LIBSSH2_FX_NO_MEDIA:            sftp_str = "NO_MEDIA"; break;
            default: break;
            }
            std::string err = "sftp stat failed for '" + remote_path + "': " + sftp_str +
                " (sftp_err=" + std::to_string(sftp_err) +
                ", libssh2_rc=" + std::to_string(lib_rc) +
                ", msg=" + (lib_msg ? lib_msg : "") + ")";
            spdlog::error("[ssh] {}", err);
            future->SaveError(err);
            return Result::FileError;
        }
        else
        {
            future->SetFileStat(remote_path, attrs.filesize, attrs.mtime);
        }

        LIBSSH2_SFTP_HANDLE* remote = libssh2_sftp_open(
            sftp,
            remote_path.c_str(),
            LIBSSH2_FXF_READ,
            0);

        if (!remote)
        {
            err = "sftp open failed: " + remote_path;
            spdlog::error("[ssh] {}", err);
            future->SaveError(err);
            return Result::SftpError;
        }

        FILE* File = fopen(local_path.c_str(), "wb");
        if (!File)
        {
            err = "file open failed: " + local_path;
            spdlog::error("[ssh] {}", err);
            future->SaveError(err);
            return Result::FileError;
        }

        char Buffer[8192];

        while (true)
        {
            ssize_t bytes = libssh2_sftp_read(remote, Buffer, sizeof(Buffer));
            if (bytes > 0)
            {
                fwrite(Buffer, 1, bytes, File);
                future->SetDownloaded(bytes);
            }
            else if (bytes == 0)
            {
                break;
            }
            else
            {
                libssh2_sftp_close(remote);
                return Result::ReadError;
            }
        }

        libssh2_sftp_close(remote);
        fclose(File);
        libssh2_sftp_shutdown(sftp);

        return Result::Success;
    }


    bool SshClient::IsAlive(SshConnection * connection)
    {
        if (!connection->GetSession())
            return false;

        int Rc = libssh2_keepalive_send(connection->GetSession(), nullptr);
        return Rc == 0;
    }

    void SshClient::SetKeepAlive(SshConnection * connection, int intervalSeconds)
    {
        libssh2_keepalive_config(connection->GetSession(), 1, intervalSeconds);
    }

}
}
