// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// This file intentionally uses the portable C runtime functions (getenv,
// fopen, fscanf, open/_open) so a single code path works across Windows and
// POSIX. Suppress MSVC's "unsafe" deprecation for those calls rather than
// forking into the *_s variants (matches the thirdparty imgui convention).
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "rocprofvis_controller_ssh_client.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_ssh_known_hosts.h"
#include "rocprofvis_controller_ssh_bridge.h"
#include "rocprofvis_core_string_utils.h"
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <string>

#ifdef _WIN32
#include <shlobj.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSE_SOCKET(s) closesocket(s)

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#define OPEN    _open
#define FDOPEN  _fdopen
#define CLOSEFD _close
#define FLAGS   (_O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY)
#define MODE    (_S_IREAD | _S_IWRITE)

#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#define CLOSE_SOCKET(s) ::close(s)

#define OPEN    open
#define FDOPEN  fdopen
#define CLOSEFD close
#define FLAGS   (O_WRONLY | O_CREAT | O_TRUNC)
#define MODE    (S_IRUSR | S_IWUSR)

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
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        {
            spdlog::error("[ssh] WSAStartup failed; SSH client is unusable");
            return;
        }
#endif
        if (libssh2_init(0) != 0)
        {
            spdlog::error("[ssh] libssh2_init failed; SSH client is unusable");
#ifdef _WIN32
            WSACleanup();
#endif
            return;
        }
        m_initialized = true;
    }

    SshClient::~SshClient()
    {
        if (m_initialized)
        {
            libssh2_exit();
#ifdef _WIN32
            WSACleanup();
#endif
        }
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
        spdlog::debug("[ssh] kbdint callback fired: name_len={} instr_len={} num_prompts={} ctx={}",
            name_len, instruction_len, num_prompts,
            static_cast<void*>(ctx));
        if(!ctx || !ctx->bridge) return;

        // libssh2 fires this callback for every kbdint round, including "info"
        // rounds (banner / status messages with no input). If there are no
        // prompts, there is nothing to ask the user - just acknowledge and
        // continue without touching the UI.
        if(num_prompts == 0)
        {
            spdlog::debug("[ssh] kbdint callback: zero-prompt round (info/banner), auto-acking");
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

        spdlog::debug("[ssh] kbdint callback: posting {} prompt(s) to UI bridge, blocking...",
            num_prompts);
        auto answer = ctx->bridge->AskPrompts(req);
        spdlog::debug("[ssh] kbdint callback: bridge returned (cancelled={})", !answer.has_value());
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

    std::string SshClient::ExpandTilde(const std::string& path)
    {
        if (path.empty() || path[0] != '~')
            return path;

#ifdef _WIN32
        // Windows
        char home[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, home)))
        {
            return std::string(home) + path.substr(1);
        }
        return path;

#else
        // POSIX (Linux/macOS)
        struct passwd* pw = getpwuid(getuid());
        if (!pw) return path;

        return std::string(pw->pw_dir) + path.substr(1);
#endif
    }


    bool SshClient::TryPublicKey(SshConnection * connection, const std::string& user,
        const std::string& priv_path_in, const std::string& passphrase, Future* future)
    {
        std::string priv_path = ExpandTilde(priv_path_in);
        if(!std::filesystem::exists(priv_path))
        {
            spdlog::warn("[ssh] publickey: private key not found: '{}' (expanded from '{}')",
                priv_path, priv_path_in);
            return false;
        }

        std::filesystem::path pub_path = std::filesystem::weakly_canonical(priv_path+".pub");
        auto p = std::filesystem::path(priv_path+".pub").lexically_normal();
        
        if ( pub_path!=p) {
            return false;
        }

        bool        have_pub = std::filesystem::exists(pub_path);
        std::string pub      = pub_path.string();
        spdlog::debug("[ssh] trying publickey: priv={} pub={} have_passphrase={}",
            priv_path, have_pub ? pub_path.string().c_str() : std::string("(derived from priv)"),
            !passphrase.empty());
        int rc = libssh2_userauth_publickey_fromfile(
            connection->GetSession(), user.c_str(), have_pub ? pub.c_str() : nullptr, priv_path.c_str(),
            passphrase.empty() ? nullptr : passphrase.c_str());
        if (rc == LIBSSH2_ERROR_AUTHENTICATION_FAILED)
        {
            std::string u = Core::String::to_lower_copy(user);
            if (u != user && Reconnect(connection, future))
            {
                rc = libssh2_userauth_publickey_fromfile(
                    connection->GetSession(), u.c_str(), have_pub ? pub.c_str() : nullptr, priv_path.c_str(),
                    passphrase.empty() ? nullptr : passphrase.c_str());
            }
        }
        if(rc == 0)
        {
            spdlog::info("[ssh] publickey auth OK ({})", priv_path);
            return true;
        }
        char* msg = nullptr;
        libssh2_session_last_error(connection->GetSession(), &msg, nullptr, 0);
        const char* hint = "";
        switch(rc)
        {
        case LIBSSH2_ERROR_FILE:
            hint = " [file unreadable or unsupported format]"; break;
        case LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED:
            hint = " [server rejected the key - possibly encrypted (passphrase needed) or not in authorized_keys]"; break;
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


    bool SshClient::TryAgent(SshConnection * connection, const std::string& user, Future* future)
    {
        LIBSSH2_AGENT* agent = libssh2_agent_init(connection->GetSession());
        if(!agent)
        {
            spdlog::debug("[ssh] agent: init failed (libssh2 built without agent support?)");
            return false;
        }
        if(libssh2_agent_connect(agent) != 0)
        {
            spdlog::debug("[ssh] agent: connect failed (no $SSH_AUTH_SOCK or agent not running)");
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
            spdlog::debug("[ssh] agent: trying identity '{}'",
                identity->comment ? identity->comment : "(no comment)");

            rc = libssh2_agent_userauth(agent, user.c_str(), identity);
            if (rc == LIBSSH2_ERROR_AUTHENTICATION_FAILED)
            {
                std::string u = Core::String::to_lower_copy(user);
                if (u!=user && Reconnect(connection, future))
                {
                    rc = libssh2_agent_userauth(agent, u.c_str(), identity);
                }
            }

            if(rc == 0)
            {
                spdlog::info("[ssh] agent: auth OK with '{}'",
                    identity->comment ? identity->comment : "(no comment)");
                ok = true;
                break;
            }
            char* msg = nullptr;
            libssh2_session_last_error(connection->GetSession(), &msg, nullptr, 0);
            spdlog::debug("[ssh] agent: identity rejected: {}", msg ? msg : "(no message)");
            prev = identity;
        }
        if(!ok && identity_count == 0)
        {
            spdlog::debug("[ssh] agent: connected but no identities loaded (run 'ssh-add')");
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
        return {
            home + sub + "id_ed25519",
            home + sub + "id_dsa",
            home + sub + "id_rsa",
            home + sub + "id_ecdsa"};
    }


    SshClient::KeyType SshClient::DetectPubkeyType(const char *pubkey_path)
    {
        FILE *f = fopen(pubkey_path, "r");
        if (!f) return SshClient::KeyType::KEY_UNKNOWN;

        char type[64] = {0};
        if (fscanf(f, "%63s", type) != 1) {
            fclose(f);
            return SshClient::KeyType::KEY_UNKNOWN;
        }

        fclose(f);

        if (strcmp(type, "ssh-rsa") == 0)
            return SshClient::KeyType::KEY_RSA;

        if (strcmp(type, "ssh-ed25519") == 0)
            return SshClient::KeyType::KEY_ED25519;

        if (strcmp(type, "ssh-dss") == 0)
            return SshClient::KeyType::KEY_DSA;

        if (strncmp(type, "ecdsa-sha2-", 11) == 0)
            return SshClient::KeyType::KEY_ECDSA;

        return SshClient::KeyType::KEY_UNKNOWN;
    }

    socket_t SshClient::CreateTcpConnection(const std::string& host, int port)
    {
        std::string err;
        struct addrinfo hints{}, *res = nullptr;

        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        std::string portstr = std::to_string(port);

        int rc = getaddrinfo(host.c_str(), portstr.c_str(), &hints, &res);
        if (rc != 0)
        {
            err = std::string("DNS resolution failed: ") + gai_strerror(rc);
            spdlog::error("[ssh] {}", err);
            return kInvalidSocket;
        }

        socket_t sock = kInvalidSocket;

        for (addrinfo* p = res; p; p = p->ai_next)
        {
            sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sock == kInvalidSocket)
                continue;

            if (connect(sock, p->ai_addr, static_cast<socklen_t>(p->ai_addrlen)) == 0)
            {
                break;
            }

            CLOSE_SOCKET(sock);
            sock = kInvalidSocket;
        }

        freeaddrinfo(res);

        return sock;
    }

    bool SshClient::WaitSocket(SshConnection* connection)
    {
        struct timeval timeout;
        fd_set fdread, fdwrite, fdex;

        socket_t sock = connection->GetSocket();
        int dir = libssh2_session_block_directions(connection->GetSession());

        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdex);

        if (dir & LIBSSH2_SESSION_BLOCK_INBOUND)
            FD_SET(sock, &fdread);

        if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
            FD_SET(sock, &fdwrite);

        FD_SET(sock, &fdex);

        int rc = select(static_cast<int>(sock + 1), &fdread, &fdwrite, &fdex, &timeout);

        if (rc > 0)
            return true;   // ready

        if (rc == 0)
            return false;  // timeout

        return false;      // error
    }


//---------------------------------------------SSH CLIENT CONNECT----------------------------------------------//

    SshConnection* SshClient::AllocateConnection(
        const std::string& host,
        int port)
    {
        if (!m_initialized)
        {
            spdlog::error("[ssh] AllocateConnection called on an uninitialized SSH client");
            return nullptr;
        }

        auto connection =
            std::make_unique<SshConnection>(host, port);
        SshConnection* raw = connection.get();
        m_connections.push_back(std::move(connection));
        return raw;
    }

    void SshClient::DeleteConnection(
        SshConnection* connection) {
        auto it = std::find_if(m_connections.begin(), m_connections.end(), [connection](std::unique_ptr<SshConnection>& c) {return connection->GetSession() == c->GetSession() && connection->GetSocket() == c->GetSocket(); });
        if (it != m_connections.end())
        {
            if (it->get()->IsConnected())
            {
                it->get()->Disconnect();
            }
            m_connections.erase(it);
        }
    }

    SshClient::Result SshClient::Connect(
        SshConnection* connection,
        Future* future)
    {
        std::string err;
        connection->GetSshBridge()->SetStatus(kRPVControllerSshConnecting);

        connection->SetSocket(CreateTcpConnection(connection->GetHost(), connection->GetPort()));

        if (IsCancelRequested(connection, future))
        {
            return SshClient::Result::Cancelled;
        }

        if (connection->GetSocket() == kInvalidSocket)
        {
            err = "TCP connect failed";
            spdlog::error("[ssh] {}", err);
            connection->GetSshBridge()->SaveError(err);
            return SshClient::Result::SocketError;
        }

        spdlog::debug("[ssh] TCP connected");

        connection->SetSession(libssh2_session_init());
        if (!connection->GetSession())
        {
            err = "libssh2_session_init failed";
            spdlog::error("[ssh] {}", err);
            connection->GetSshBridge()->SaveError(err);
            connection->Disconnect();
            return SshClient::Result::SessionError;
        }

        if (IsCancelRequested(connection, future))
        {
            return SshClient::Result::Cancelled;
        }

        libssh2_session_set_blocking(connection->GetSession(), 1);

        if (libssh2_session_handshake(connection->GetSession(), connection->GetSocket()))
        {
            char* msg = nullptr;
            libssh2_session_last_error(connection->GetSession(), &msg, nullptr, 0);
            err = std::string("SSH handshake failed: ") + (msg ? msg : "");
            spdlog::error("[ssh] {}", err);
            connection->GetSshBridge()->SaveError(err);
            connection->Disconnect();
            return SshClient::Result::HandshakeError;
        }

        if (IsCancelRequested(connection, future))
        {
            return SshClient::Result::Cancelled;
        }

        connection->SetConnected();

        return SshClient::Result::Success;
    }

    bool SshClient::Reconnect(SshConnection * connection, Future* future)
    {
        connection->Disconnect();
        return Connect(connection, future) == SshClient::Result::Success;
    }

    //---------------------------------------------SSH CLIENT AUTHENTICATE-------------------------------------------//

    SshClient::Result SshClient::Authenticate(
        SshConnection * connection,
        const std::string& user, 
        const std::string& password, 
        const std::string& identity_file, 
        const std::string& passphrase, 
        Future* future)
    {
        std::string err;
        connection->GetSshBridge()->SetStatus(kRPVControllerSshAuthenticating);
        // ---- host key check ----
        {
            KnownHosts kh(connection->GetSession());
            bool       loaded = kh.Load();
            spdlog::debug("[ssh] known_hosts loaded={} path={} fingerprint={}",
                loaded, kh.Path(), FormatHostKeyFingerprint(connection->GetSession()));
            KnownHostMatch m = kh.Check(connection->GetHost(), connection->GetPort());
            const char* mstr = m == KnownHostMatch::Match    ? "Match"
                : m == KnownHostMatch::Mismatch ? "Mismatch"
                : m == KnownHostMatch::NotFound ? "NotFound"
                : "Failure";
            spdlog::debug("[ssh] host key check: {}", mstr);

            if(m == KnownHostMatch::NotFound || m == KnownHostMatch::Mismatch)
            {
                HostKeyRequest req;
                req.host                   = connection->GetHost();
                req.port                   = connection->GetPort();
                req.fingerprint_sha256_b64 = FormatHostKeyFingerprint(connection->GetSession());
                req.key_type               = HostKeyTypeName(connection->GetSession());
                req.state = (m == KnownHostMatch::Mismatch) ? HostKeyState::Mismatch
                    : HostKeyState::NotFound;

                auto decision = connection->GetSshBridge()->AskHostKey(req);
                if(!decision)
                {
                    err = "Authentication bridge not initialized";
                    spdlog::error("[ssh] {}", err);
                    connection->GetSshBridge()->SaveError(err);
                    connection->Disconnect();
                    return Result::AuthError;
                }
                if(*decision == HostKeyDecision::Reject)
                {
                    connection->Disconnect();
                    err = (m == KnownHostMatch::Mismatch)
                        ? "Host key mismatch - connection rejected."
                        : "Host key not trusted.";
                    connection->GetSshBridge()->SaveError(err);
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
                connection->GetSshBridge()->SaveError(err);
                connection->Disconnect();
                return Result::AuthError;
            }
        }

        if (IsCancelRequested(connection, future))
        {
            return SshClient::Result::Cancelled;
        }

        // ---- auth ----
        char* methods = libssh2_userauth_list(connection->GetSession(), user.c_str(),
            static_cast<unsigned int>(user.size()));
        spdlog::debug("[ssh] server auth methods: {}", methods ? methods : "(none)");

        bool tried_password = false;
        bool tried_kbdint   = false;
        int  auth_rc        = LIBSSH2_ERROR_AUTHENTICATION_FAILED;

        // 1) publickey
        if(MethodListed(methods, "publickey"))
        {
            // 1a) explicit identity_file from the dialog
            if(!identity_file.empty())
            {

                if (TryPublicKey(connection, user, identity_file, passphrase, future))
                {
                    return Result::Success;
                }
            }
            // 1b) ssh-agent - handles encrypted keys without us needing a passphrase.
            spdlog::debug("[ssh] trying ssh-agent");
            if (TryAgent(connection, user, future))
            {
                return Result::Success;
            }
            // 1c) default on-disk identity files
            spdlog::debug("[ssh] trying default identity files");
            for(const auto& p : DefaultKeyPaths())
            {
                if(!std::filesystem::exists(p))
                {
                    spdlog::debug("[ssh]   default key absent: {}", p);
                    continue;
                }
                if (IsCancelRequested(connection, future))
                {
                    return SshClient::Result::Cancelled;
                }
                if(TryPublicKey(connection, user, p, passphrase, future))
                {
                    return Result::Success;
                }
            }
        }
        else
        {
            spdlog::debug("[ssh] server does not advertise publickey");
        }
        if (IsCancelRequested(connection, future))
        {
            return SshClient::Result::Cancelled;
        }
        // 2) password
        if(MethodListed(methods, "password"))
        {
            // No password was supplied up front (e.g. empty string in the
            // connection config). Rather than skipping password auth and
            // failing silently, prompt the UI for one via the same bridge
            // mechanism keyboard-interactive uses. A null/empty return means
            // the user cancelled the prompt.
            std::string effective_password = password;
            if(effective_password.empty())
            {
                spdlog::debug("[ssh] no password supplied; prompting UI for password");
                PromptRequest req;
                std::string target = user.empty() ? connection->GetHost()
                                                   : user + "@" + connection->GetHost();
                req.instruction = target.empty() ? std::string("Enter your password")
                                                 : "Enter the password for " + target;
                PromptItem item;
                item.text = "Password: ";
                item.echo = false;  // mask input
                req.prompts.push_back(std::move(item));

                auto answer = connection->GetSshBridge()->AskPrompts(req);
                if(!answer)
                {
                    err = "password prompt cancelled by user";
                    spdlog::debug("[ssh] {}", err);
                    connection->GetSshBridge()->SaveError(err);
                    connection->Disconnect();
                    return Result::AuthError;
                }
                if(!answer->empty())
                {
                    effective_password = (*answer)[0];
                }
            }

            if(!effective_password.empty())
            {
                spdlog::debug("[ssh] trying password auth");
                tried_password = true;
                auth_rc = libssh2_userauth_password(connection->GetSession(), user.c_str(),
                    effective_password.c_str());
                if (auth_rc == LIBSSH2_ERROR_AUTHENTICATION_FAILED)
                {

                    std::string u = Core::String::to_lower_copy(user);
                    if (u != user && Reconnect(connection, future))
                    {
                        auth_rc = libssh2_userauth_password(connection->GetSession(), u.c_str(),
                            effective_password.c_str());
                    }
                }
                if(auth_rc == 0)
                {
                    spdlog::info("[ssh] password auth OK");
                    return Result::Success;
                }
                else
                {
                    char* msg = nullptr;
                    libssh2_session_last_error(connection->GetSession(), &msg, nullptr, 0);
                    spdlog::warn("[ssh] password auth failed (rc={}): {}", auth_rc,
                        msg ? msg : "(no message)");
                }
            }
        }
        else
        {
            spdlog::debug("[ssh] skipping password auth (server does not advertise password)");
        }

        if (IsCancelRequested(connection, future))
        {
            return SshClient::Result::Cancelled;
        }

        // 3) keyboard-interactive
        auto kbd_ctx = std::make_shared<KbdintCtx>(KbdintCtx{connection->GetSshBridge(), false});
        if(MethodListed(methods, "keyboard-interactive"))
        {
            spdlog::debug("[ssh] trying keyboard-interactive auth (will route prompts to UI)");
            tried_kbdint = true;
            // libssh2's session abstract slot: stash kbd_ctx so the C callback can find it.
            void** abstract = libssh2_session_abstract(connection->GetSession());
            if(abstract) *abstract = kbd_ctx.get();
            auth_rc = libssh2_userauth_keyboard_interactive(connection->GetSession(), user.c_str(),
                &KbdIntCallback);
            if (auth_rc == LIBSSH2_ERROR_AUTHENTICATION_FAILED)
            {
                std::string u = Core::String::to_lower_copy(user);
                if (u != user && Reconnect(connection, future))
                {
                    auth_rc = libssh2_userauth_keyboard_interactive(connection->GetSession(), u.c_str(),
                        &KbdIntCallback);
                }
            }
            if(abstract) *abstract = nullptr;
            if(kbd_ctx->was_cancelled)
            {
                err = "kbdint cancelled by user";
                spdlog::debug("[ssh] {}", err);
                connection->GetSshBridge()->SaveError(err);
                connection->Disconnect();
                return Result::AuthError;
            }
            if(auth_rc == 0)
            {
                spdlog::info("[ssh] kbdint auth OK");
                return Result::Success;
            }
            else
            {
                char* msg = nullptr;
                libssh2_session_last_error(connection->GetSession(), &msg, nullptr, 0);
                if (msg)
                {
                    err = msg;
                }
                else
                {
                    err = "kbdint auth failed (rc=";
                    err += std::to_string(auth_rc);
                    err += ")";
                }
                spdlog::warn("[ssh] {}", err);
            }
        }

        connection->GetSshBridge()->SaveError(err);
        return Result::AuthError;
    }



    SshClient::Result SshClient::ExecuteCommand(SshConnection * connection, const std::string& command, Future* future, int* exit_code_out)
    {
        std::string output;
        int exit_code = -1;
        connection->GetSshBridge()->SetStatus(kRPVControllerSshExecuting);

        if (!connection->IsValid())
        {
            output = "Invalid connection";
            connection->GetSshBridge()->SaveError(output);
            return Result::SessionError;
        }
        if (!connection->IsConnected())
        {
            output = "Lost connection";
            connection->GetSshBridge()->SaveError(output);
            return Result::SessionError;
        }

        // Non-blocking setup so the channel open/exec phase observes
        // cancellation and can never hang the worker thread if the server
        // stalls. Each libssh2 EAGAIN is followed by a bounded WaitSocket, and
        // the cancel flag is checked every iteration (matches the SFTP flow in
        // BrowseRemoteDirectory and the read loop below).
        libssh2_session_set_blocking(connection->GetSession(), 0);

        LIBSSH2_CHANNEL* channel = nullptr;
        while ((channel = libssh2_channel_open_session(connection->GetSession())) == nullptr)
        {
            if (IsCancelRequested(connection, future))
            {
                return Result::Cancelled;
            }
            if (libssh2_session_last_errno(connection->GetSession()) == LIBSSH2_ERROR_EAGAIN)
            {
                if (!WaitSocket(connection))
                {
                    output = "Network failure while opening SSH channel";
                    connection->GetSshBridge()->SaveError(output);
                    return Result::ChannelError;
                }
            }
            else
            {
                output = "Failed to open SSH channel";
                connection->GetSshBridge()->SaveError(output);
                return Result::ChannelError;
            }
        }

        int exec_rc = 0;
        while ((exec_rc = libssh2_channel_exec(channel, command.c_str())) == LIBSSH2_ERROR_EAGAIN)
        {
            if (IsCancelRequested(connection, future))
            {
                libssh2_channel_free(channel);
                return Result::Cancelled;
            }
            if (!WaitSocket(connection))
            {
                output = "Network failure while starting remote command";
                connection->GetSshBridge()->SaveError(output);
                libssh2_channel_free(channel);
                return Result::ChannelError;
            }
        }
        if (exec_rc != 0)
        {
            output = "libssh2_channel_exec failed";
            connection->GetSshBridge()->SaveError(output);
            libssh2_channel_free(channel);
            return Result::ChannelError;
        }

        char buffer[4096];

        while (true)
        {
            if (IsCancelRequested(connection, future))
            {
                break;
            }

            bool got_data = false;

            // Read and consume stdout BEFORE reading stderr: both reads share
            // `buffer`, so the stderr read would otherwise overwrite the stdout
            // bytes before AddStdOut has copied them out.
            ssize_t n1 = libssh2_channel_read(channel, buffer, sizeof(buffer));
            if (n1 > 0)
            {
                connection->GetSshBridge()->AddStdOut(buffer, n1);

                got_data = true;
            }

            ssize_t n2 = libssh2_channel_read_stderr(channel, buffer, sizeof(buffer));
            if (n2 > 0)
            {
                connection->GetSshBridge()->AddStdOut(buffer, n2);

                got_data = true;
            }

            if (n1 == LIBSSH2_ERROR_EAGAIN && n2 == LIBSSH2_ERROR_EAGAIN)
            {
                if (!WaitSocket(connection))
                {
                    output = "Network failure while execuing";
                    connection->GetSshBridge()->SaveError(output);
                    break;
                }
            }

            if (!got_data)
            {
                if (libssh2_channel_eof(channel))
                    break;
            }
        }

        libssh2_channel_close(channel);

        libssh2_channel_wait_closed(channel);

        exit_code = libssh2_channel_get_exit_status(channel);

        if (exit_code_out != nullptr)
        {
            *exit_code_out = exit_code;
        }

        output = "Exit code : " + std::to_string(exit_code);
        connection->GetSshBridge()->AddStdOut(output.data(), output.size());

        libssh2_channel_free(channel);

        if (IsCancelRequested(connection, future))
        {
            return SshClient::Result::Cancelled;
        }

        return Result::Success;
    }
 
    // Download a remote file via SCP/SFTP path handling and mirror it locally.
    // This routine intentionally performs end-to-end transfer orchestration in one place:
    // it validates local/remote state, executes the transfer, updates sidecar metadata,
    // and reports status/progress through `future`.
    //
    // Workflow:
    //  1) Read local file metadata (and sidecar .meta information when present).
    //  2) Query remote file metadata to determine transfer requirements.
    //  3) Stream remote contents into the local destination file.
    //  4) Persist updated metadata for future incremental checks.
    //  5) Report progress/errors through `future`.
    SshClient::Result SshClient::DownloadFile(
        SshConnection* connection,
        const std::string& remote_path,
        const std::string& local_path,
        Future* future)
    {
        std::string err;
        connection->GetSshBridge()->Clear();
        connection->GetSshBridge()->SetStatus(kRPVControllerSshDownloading);

        if (!connection->IsValid())
        {
            err = "Invalid connection";
            connection->GetSshBridge()->SaveError(err);
            return Result::SessionError;
        }
        if (!connection->IsConnected())
        {
            err = "Lost connection";
            connection->GetSshBridge()->SaveError(err);
            return Result::SessionError;
        }

        // Phase 1: prepare local metadata bookkeeping used for cache freshness checks.
        // The `.meta` sidecar stores prior remote attributes so unchanged files can be skipped.
        auto meta_path = std::filesystem::path(local_path).concat(".meta");
        LIBSSH2_SESSION* session = connection->GetSession();
        libssh2_session_set_blocking(session, 1);

        if (IsCancelRequested(connection, future))
        {
            return SshClient::Result::Cancelled;
        }

        libssh2_struct_stat fileinfo{};
        LIBSSH2_CHANNEL* channel =
            libssh2_scp_recv2(session, remote_path.c_str(), &fileinfo);

        if (!channel)
        {
            char* lib_msg = nullptr;
            libssh2_session_last_error(session, &lib_msg, nullptr, 0);

            err = "scp recv failed: " + remote_path +
                " msg=" + (lib_msg ? lib_msg : "");
            spdlog::error("[ssh] {}", err);
            connection->GetSshBridge()->SaveError(err);
            connection->GetSshBridge()->SetFileStat(remote_path, 0, 0, 0);
            return Result::SftpError;
        }

        // libssh2's stat fields are signed; the transfer bookkeeping below is
        // unsigned. Convert once so every comparison uses a matching type.
        const uint64_t remote_size  = static_cast<uint64_t>(fileinfo.st_size);
        const uint64_t remote_mtime = static_cast<uint64_t>(fileinfo.st_mtime);

        std::error_code ec;
        if(std::filesystem::exists(local_path, ec) &&
            std::filesystem::exists(meta_path, ec))
        {
            std::ifstream m(meta_path);
            uint64_t      cached_size = 0, cached_mtime = 0;
            if(m >> cached_size >> cached_mtime &&
                cached_size == remote_size && cached_mtime == remote_mtime)
            {
                spdlog::debug("[ssh] already up-to-date: {}", local_path);

                connection->GetSshBridge()->SetFileStat(remote_path, fileinfo.st_size, fileinfo.st_mtime, fileinfo.st_size);
                libssh2_channel_free(channel);
                return Result::Success;
            }
        }

        connection->GetSshBridge()->SetFileStat(remote_path, fileinfo.st_size, fileinfo.st_mtime, 0);


        int fd = OPEN(local_path.c_str(), FLAGS, MODE);
        if (fd < 0)
        {
            err = "file open failed: " + local_path;
            spdlog::error("[ssh] {}", err);
            connection->GetSshBridge()->SaveError(err);

            libssh2_channel_free(channel);
            return Result::FileError;
        }

        FILE* file = FDOPEN(fd, "wb");
        if (!file)
        {
            CLOSEFD(fd); 
            err = "fdopen failed: " + local_path;
            spdlog::error("[ssh] {}", err);
            connection->GetSshBridge()->SaveError(err);

            libssh2_channel_free(channel);
            return Result::FileError;
        }


        const size_t BUF_SIZE = 64 * 1024;
        std::vector<char> buffer(BUF_SIZE);

        ssize_t got = 0;
        uint64_t total_downloaded = 0;
        bool     cancelled        = false;

        while (!libssh2_channel_eof(channel) && total_downloaded < remote_size)
        {
            if (IsCancelRequested(connection, future))
            {
                cancelled = true;
                break;
            }
            got = libssh2_channel_read(channel, buffer.data(), buffer.size());

            if (got > 0)
            {
                if (static_cast<uint64_t>(got) > remote_size - total_downloaded)
                {
                    got = static_cast<ssize_t>(remote_size - total_downloaded);
                }

                // A short write (disk full / quota) must not be counted as
                // progress - otherwise total_downloaded reaches st_size and the
                // file is cached as complete while truncated on disk.
                size_t written = fwrite(buffer.data(), 1, static_cast<size_t>(got), file);
                if (written != static_cast<size_t>(got))
                {
                    err = "local write failed (disk full?): " + local_path;
                    spdlog::error("[ssh] {}", err);
                    connection->GetSshBridge()->SaveError(err);

                    fclose(file);
                    libssh2_channel_free(channel);
                    return Result::WriteError;
                }

                total_downloaded += got;
                connection->GetSshBridge()->SetDownloaded(got);
            }
            else if (got == LIBSSH2_ERROR_EAGAIN)
            {
                continue;
            }
            else if (got < 0)
            {

                char* lib_msg = nullptr;
                libssh2_session_last_error(session, &lib_msg, NULL, 0);

                err = "scp read error: " + std::string(lib_msg ? lib_msg : "");
                spdlog::error("[ssh] {}", err);
                connection->GetSshBridge()->SaveError(err);

                fclose(file);
                libssh2_channel_free(channel);
                return Result::ReadError;

            }

        }

        fclose(file);

        const bool complete = (total_downloaded == remote_size);

        // Persist the freshness sidecar ONLY for a fully-received file. Writing it
        // after a cancel/partial transfer would cache a truncated file as
        // up-to-date, so the next download would skip and open a corrupt trace.
        // With no .meta, the next attempt re-downloads (the freshness check needs
        // both the file and its .meta).
        if (!cancelled && complete)
        {
            std::ofstream m(meta_path, std::ios::trunc);
            if (m) m << fileinfo.st_size << ' ' << fileinfo.st_mtime << '\n';
        }

        libssh2_channel_send_eof(channel);
        libssh2_channel_wait_eof(channel);
        libssh2_channel_wait_closed(channel);
        libssh2_channel_free(channel);

        if (cancelled)
        {
            return SshClient::Result::Cancelled;
        }

        if (!complete)
        {
            err = "incomplete download: " + remote_path;
            spdlog::error("[ssh] {}", err);
            connection->GetSshBridge()->SaveError(err);
            return Result::ReadError;
        }

        return Result::Success;
    }

    // SFTP teardown can report EAGAIN on a non-blocking session, so closing a
    // handle needs the same bounded socket wait as the rest of the transport.
    class SftpSessionGuard
    {
    public:
        SftpSessionGuard(SshConnection* connection, LIBSSH2_SFTP* sftp)
            : m_connection(connection)
            , m_sftp(sftp)
        {
        }

        ~SftpSessionGuard()
        {
            while (m_sftp != nullptr &&
                   libssh2_sftp_shutdown(m_sftp) == LIBSSH2_ERROR_EAGAIN)
            {
                if (!SshClient::WaitSocket(m_connection))
                {
                    spdlog::warn("[ssh] network failure while shutting down SFTP");
                    break;
                }
            }
        }

        SftpSessionGuard(const SftpSessionGuard&) = delete;
        SftpSessionGuard& operator=(const SftpSessionGuard&) = delete;

    private:
        SshConnection* m_connection;
        LIBSSH2_SFTP*  m_sftp;
    };

    class SftpDirGuard
    {
    public:
        SftpDirGuard(SshConnection* connection, LIBSSH2_SFTP_HANDLE* dir)
            : m_connection(connection)
            , m_dir(dir)
        {
        }

        ~SftpDirGuard()
        {
            while (m_dir != nullptr &&
                   libssh2_sftp_closedir(m_dir) == LIBSSH2_ERROR_EAGAIN)
            {
                if (!SshClient::WaitSocket(m_connection))
                {
                    spdlog::warn("[ssh] network failure while closing remote directory");
                    break;
                }
            }
        }

        SftpDirGuard(const SftpDirGuard&) = delete;
        SftpDirGuard& operator=(const SftpDirGuard&) = delete;

    private:
        SshConnection*       m_connection;
        LIBSSH2_SFTP_HANDLE* m_dir;
    };

    SshClient::Result SshClient::BrowseRemoteDirectory(SshConnection * connection, const std::string& path, Future* future)
    {
        std::string output;
        connection->GetSshBridge()->Clear();
        connection->GetSshBridge()->SetStatus(kRPVControllerSshBrowsing);

        if (!connection->IsValid())
        {
            output = "Invalid connection";
            connection->GetSshBridge()->SaveError(output);
            return Result::SessionError;
        }
        if (!connection->IsConnected())
        {
            output = "Lost connection";
            connection->GetSshBridge()->SaveError(output);
            return Result::SessionError;
        }

        // Non-blocking from the start so the init loop below can honour a cancel
        // instead of stalling inside libssh2.
        libssh2_session_set_blocking(connection->GetSession(), 0);

        // --- Init SFTP (non-blocking) ---
        LIBSSH2_SFTP* sftp = nullptr;
        while ((sftp = libssh2_sftp_init(connection->GetSession())) == nullptr)
        {
            int err = libssh2_session_last_errno(connection->GetSession());
            if (err != LIBSSH2_ERROR_EAGAIN)
            {
                output = "SFTP init failed: " + std::to_string(err);
                connection->GetSshBridge()->SaveError(output);
                return Result::SessionError;
            }
            if (!WaitSocket(connection))
            {
                output = "Network failure while initializing SFTP";
                connection->GetSshBridge()->SaveError(output);
                return Result::SessionError;
            }
            if (IsCancelRequested(connection, future))
            {
                return Result::Cancelled;
            }
        }

        SftpSessionGuard sftp_guard(connection, sftp);

        LIBSSH2_SFTP_HANDLE* dir = nullptr;
        while ((dir = libssh2_sftp_opendir(sftp, path.c_str())) == nullptr)
        {
            if (libssh2_session_last_errno(connection->GetSession()) != LIBSSH2_ERROR_EAGAIN)
            {
                output = "Unable to open directory " + path;
                connection->GetSshBridge()->SaveError(output);
                return Result::SessionError;
            }
            if (!WaitSocket(connection))
            {
                output = "Network failure while opening remote directory";
                connection->GetSshBridge()->SaveError(output);
                return Result::SessionError;
            }
            if (IsCancelRequested(connection, future))
            {
                return Result::Cancelled;
            }
        }

        SftpDirGuard dir_guard(connection, dir);

        constexpr size_t SFTP_NAME_BUFFER_SIZE = 512;
        char mem[SFTP_NAME_BUFFER_SIZE];
        LIBSSH2_SFTP_ATTRIBUTES attrs;

        // Resolve the (possibly relative, e.g. ".") requested path to an absolute
        // path so the UI can show a meaningful location. Best-effort: failure here
        // leaves the resolved path empty and does not abort the listing.
        {
            char real_path[SFTP_NAME_BUFFER_SIZE];
            int  real_rc;
            while ((real_rc = libssh2_sftp_realpath(sftp, path.c_str(), real_path,
                                                    sizeof(real_path) - 1)) == LIBSSH2_ERROR_EAGAIN)
            {
                if (IsCancelRequested(connection, future) || !WaitSocket(connection))
                {
                    break;
                }
            }
            if (real_rc > 0)
            {
                connection->GetSshBridge()->SetResolvedPath(
                    std::string(real_path, static_cast<size_t>(real_rc)));
            }
        }

        while (true)
        {
            if (IsCancelRequested(connection, future))
            {
                return Result::Cancelled;
            }

            int rc;
            do
            {
                // Reserve the final byte for the NUL terminator written below:
                // libssh2_sftp_readdir returns the filename length capped at the
                // buffer size we pass, so bounding it to size-1 keeps mem[rc]
                // in range even when the name would otherwise fill the buffer.
                rc = libssh2_sftp_readdir(dir, mem, sizeof(mem) - 1, &attrs);

                if (rc == LIBSSH2_ERROR_EAGAIN && !WaitSocket(connection))
                {
                    output = "Network failure while reading remote directory content";
                    connection->GetSshBridge()->SaveError(output);
                    return Result::ReadError;
                }

            } while (rc == LIBSSH2_ERROR_EAGAIN);

            if (rc == 0)
            {
                break;  // done
            }
            if (rc < 0)
            {
                output = "Directory read error";
                connection->GetSshBridge()->SaveError(output);
                return Result::ReadError;
            }

            mem[rc] = '\0';

            if (strcmp(mem, ".") == 0 || strcmp(mem, "..") == 0)
            {
                continue;
            }

            int is_dir = 0;
            if (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS)
            {
                is_dir = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
            }

            connection->GetSshBridge()->SetFileInfo(mem, attrs.filesize, attrs.mtime, is_dir);
        }

        if (IsCancelRequested(connection, future))
        {
            return Result::Cancelled;
        }

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

    bool SshClient::IsCancelRequested(SshConnection * connection, Future* future)
    {
        // Cancellation can arrive on two independent channels: the bound future
        // (rocprofvis_*_cancel) or the bridge (SshBridge::Cancel, used by the
        // profiler executor and the auth-prompt path). Honor both so a cancel
        // request always breaks the transport loop regardless of entry point.
        if (future->IsCancelled() || connection->GetSshBridge()->IsCancelled())
        {
            std::string err = "Cancelled by user";
            spdlog::error("[ssh] {}", err);
            connection->GetSshBridge()->SaveError(err);
            return true;
        }
        return false;
    }

    void SshConnection::Disconnect() {
        m_net_bridge.Cancel();
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
        m_connected = false;
    }

    rocprofvis_result_t SshConnection::GetUInt64(
        rocprofvis_property_t property,
        uint64_t index,
        uint64_t* value)
    {
        return m_net_bridge.GetUInt64(property, index, value);
    }

    rocprofvis_result_t SshConnection::GetString(rocprofvis_property_t property,
        uint64_t index,
        char* value,
        uint32_t* length)
    {
        return m_net_bridge.GetString(property, index, value, length);
    }
}
}
