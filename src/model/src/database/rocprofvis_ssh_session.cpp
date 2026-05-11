// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_session.h"

#include "rocprofvis_ssh_auth_bridge.h"
#include "rocprofvis_ssh_known_hosts.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

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
namespace DataModel
{

namespace
{
std::once_flag g_libssh2_init_once;
std::atomic<bool> g_libssh2_inited{false};
}  // namespace

void EnsureLibssh2Inited()
{
    std::call_once(g_libssh2_init_once, [] {
#ifdef _WIN32
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
        if(libssh2_init(0) == 0)
        {
            g_libssh2_inited = true;
        }
        else
        {
            spdlog::error("libssh2_init failed");
        }
    });
}

void Libssh2GlobalShutdown()
{
    if(g_libssh2_inited.exchange(false))
    {
        libssh2_exit();
#ifdef _WIN32
        WSACleanup();
#endif
    }
}

SshSession::SshSession() = default;

SshSession::~SshSession() { Disconnect(); }

namespace
{
// kbdint trampoline. The libssh2 callback signature gives us only an
// `abstract` void**, which we set to the AuthBridge before kicking auth.
struct KbdintCtx
{
    AuthBridge*           bridge        = nullptr;
    bool                  was_cancelled = false;
};

void KbdIntCallback(const char* name, int name_len,
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
    auto answer = ctx->bridge->AskPrompts(req);
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

std::string ExpandTilde(const std::string& p)
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

bool TryPublicKey(LIBSSH2_SESSION* session, const std::string& user,
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
bool TryAgent(LIBSSH2_SESSION* session, const std::string& user)
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

std::vector<std::string> DefaultKeyPaths()
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

bool MethodListed(const char* methods, const char* needle)
{
    return methods && std::strstr(methods, needle) != nullptr;
}
}  // namespace

ConnectResult SshSession::Connect(const std::string& host, int port,
                                  const std::string& user,
                                  const AuthOptions& auth, AuthBridge& bridge,
                                  std::string& err)
{
    spdlog::info("[ssh] Connect host={} port={} user={} have_password={} identity={}",
                 host, port, user, !auth.password.empty(),
                 auth.identity_file.empty() ? std::string("(defaults)")
                                            : auth.identity_file);

    EnsureLibssh2Inited();
    if(!g_libssh2_inited)
    {
        err = "libssh2 init failed";
        spdlog::error("[ssh] {}", err);
        return ConnectResult::ConnectionError;
    }

    Disconnect();

    // Resolve hostname (use getaddrinfo for IPv6 readiness; for now we keep
    // it simple with hostent which is what the reference uses).
    addrinfo hints{};
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    std::string portstr = std::to_string(port);
    int gai = getaddrinfo(host.c_str(), portstr.c_str(), &hints, &res);
    if(gai != 0 || !res)
    {
        err = std::string("DNS resolution failed: ") + gai_strerror(gai);
        spdlog::error("[ssh] {}", err);
        return ConnectResult::ConnectionError;
    }

    bool connected = false;
    for(addrinfo* p = res; p; p = p->ai_next)
    {
        m_sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(m_sock == kInvalidSocket) continue;
        if(::connect(m_sock, p->ai_addr, static_cast<socklen_t>(p->ai_addrlen)) == 0)
        {
            connected = true;
            break;
        }
        CLOSE_SOCKET(m_sock);
        m_sock = kInvalidSocket;
    }
    freeaddrinfo(res);
    if(!connected)
    {
        err = "TCP connect failed";
        spdlog::error("[ssh] {}", err);
        return ConnectResult::ConnectionError;
    }
    spdlog::info("[ssh] TCP connected");

    m_session = libssh2_session_init();
    if(!m_session)
    {
        err = "libssh2_session_init failed";
        spdlog::error("[ssh] {}", err);
        Disconnect();
        return ConnectResult::ConnectionError;
    }
    libssh2_session_set_blocking(m_session, 1);

    if(libssh2_session_handshake(m_session, m_sock) != 0)
    {
        char* msg = nullptr;
        libssh2_session_last_error(m_session, &msg, nullptr, 0);
        err = std::string("SSH handshake failed: ") + (msg ? msg : "");
        spdlog::error("[ssh] {}", err);
        Disconnect();
        return ConnectResult::ConnectionError;
    }
    spdlog::info("[ssh] handshake ok");

    // ---- host key check ----
    {
        KnownHosts kh(m_session);
        bool       loaded = kh.Load();
        spdlog::info("[ssh] known_hosts loaded={} path={} fingerprint={}",
                     loaded, kh.Path(), FormatHostKeyFingerprint(m_session));
        KnownHostMatch m = kh.Check(host, port);
        const char* mstr = m == KnownHostMatch::Match    ? "Match"
                         : m == KnownHostMatch::Mismatch ? "Mismatch"
                         : m == KnownHostMatch::NotFound ? "NotFound"
                                                         : "Failure";
        spdlog::info("[ssh] host key check: {}", mstr);
        if(m == KnownHostMatch::NotFound || m == KnownHostMatch::Mismatch)
        {
            HostKeyRequest req;
            req.host                   = host;
            req.port                   = port;
            req.fingerprint_sha256_b64 = FormatHostKeyFingerprint(m_session);
            req.key_type               = HostKeyTypeName(m_session);
            req.state = (m == KnownHostMatch::Mismatch) ? HostKeyState::Mismatch
                                                        : HostKeyState::NotFound;
            auto decision = bridge.AskHostKey(req);
            if(!decision)
            {
                Disconnect();
                return ConnectResult::Cancelled;
            }
            if(*decision == HostKeyDecision::Reject)
            {
                Disconnect();
                err = (m == KnownHostMatch::Mismatch)
                          ? "Host key mismatch — connection rejected."
                          : "Host key not trusted.";
                return (m == KnownHostMatch::Mismatch)
                           ? ConnectResult::HostKeyMismatch
                           : ConnectResult::HostKeyRejected;
            }
            if(*decision == HostKeyDecision::TrustPermanently)
            {
                kh.Add(host, port);
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
            Disconnect();
            return ConnectResult::ConnectionError;
        }
    }

    // ---- auth ----
    char* methods = libssh2_userauth_list(m_session, user.c_str(),
                                          static_cast<unsigned int>(user.size()));
    spdlog::info("[ssh] server auth methods: {}", methods ? methods : "(none)");

    bool tried_password = false;
    bool tried_kbdint   = false;
    int  auth_rc        = LIBSSH2_ERROR_AUTHENTICATION_FAILED;

    // 1) publickey
    if(MethodListed(methods, "publickey"))
    {
        // 1a) explicit identity_file from the dialog
        if(!auth.identity_file.empty())
        {
            if(TryPublicKey(m_session, user, auth.identity_file, auth.passphrase))
                auth_rc = 0;
        }
        // 1b) ssh-agent — handles encrypted keys without us needing a passphrase.
        if(auth_rc != 0)
        {
            spdlog::info("[ssh] trying ssh-agent");
            if(TryAgent(m_session, user)) auth_rc = 0;
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
                if(TryPublicKey(m_session, user, p, auth.passphrase))
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
    if(auth_rc != 0 && MethodListed(methods, "password") && !auth.password.empty())
    {
        spdlog::info("[ssh] trying password auth");
        tried_password = true;
        auth_rc = libssh2_userauth_password(m_session, user.c_str(),
                                            auth.password.c_str());
        if(auth_rc == 0)
        {
            spdlog::info("[ssh] password auth OK");
        }
        else
        {
            char* msg = nullptr;
            libssh2_session_last_error(m_session, &msg, nullptr, 0);
            spdlog::warn("[ssh] password auth failed (rc={}): {}", auth_rc,
                         msg ? msg : "(no message)");
        }
    }
    else if(auth_rc != 0)
    {
        spdlog::info("[ssh] skipping password auth (server_lists={} have_password={})",
                     MethodListed(methods, "password"), !auth.password.empty());
    }

    // 3) keyboard-interactive
    KbdintCtx kbd_ctx{&bridge, false};
    if(auth_rc != 0 && MethodListed(methods, "keyboard-interactive"))
    {
        spdlog::info("[ssh] trying keyboard-interactive auth (will route prompts to UI)");
        tried_kbdint = true;
        // libssh2's session abstract slot: stash kbd_ctx so the C callback can find it.
        void** abstract = libssh2_session_abstract(m_session);
        if(abstract) *abstract = &kbd_ctx;
        auth_rc = libssh2_userauth_keyboard_interactive(m_session, user.c_str(),
                                                        &KbdIntCallback);
        if(abstract) *abstract = nullptr;
        if(kbd_ctx.was_cancelled)
        {
            spdlog::info("[ssh] kbdint cancelled by user");
            Disconnect();
            return ConnectResult::Cancelled;
        }
        if(auth_rc == 0)
        {
            spdlog::info("[ssh] kbdint auth OK");
        }
        else
        {
            char* msg = nullptr;
            libssh2_session_last_error(m_session, &msg, nullptr, 0);
            spdlog::warn("[ssh] kbdint auth failed (rc={}): {}", auth_rc,
                         msg ? msg : "(no message)");
        }
    }
    else if(auth_rc != 0)
    {
        spdlog::info("[ssh] skipping kbdint (server_lists={})",
                     MethodListed(methods, "keyboard-interactive"));
    }

    if(auth_rc != 0)
    {
        char* msg = nullptr;
        libssh2_session_last_error(m_session, &msg, nullptr, 0);
        err = std::string("Authentication failed: ") + (msg ? msg : "");
        spdlog::error("[ssh] auth failed (final rc={}, last_err={}): tried_password={} tried_kbdint={}",
                      auth_rc, msg ? msg : "", tried_password, tried_kbdint);
        Disconnect();
        // Distinguish "no password tried" from "password tried and wrong"
        // so the dialog can prompt.
        if(auth.password.empty() && MethodListed(methods, "password"))
        {
            spdlog::info("[ssh] mapping result -> NeedsPassword (server lists password, none provided)");
            return ConnectResult::NeedsPassword;
        }
        // If the only remaining option is keyboard-interactive and we
        // never had any input to give it, treat as NeedsPassword too.
        if(auth.password.empty() && MethodListed(methods, "keyboard-interactive") &&
           !MethodListed(methods, "publickey"))
        {
            spdlog::info("[ssh] mapping result -> NeedsPassword (kbdint only, none provided)");
            return ConnectResult::NeedsPassword;
        }
        spdlog::info("[ssh] mapping result -> AuthFailed");
        return ConnectResult::AuthFailed;
    }

    // ---- SFTP ----
    m_sftp = libssh2_sftp_init(m_session);
    if(!m_sftp)
    {
        char* msg = nullptr;
        libssh2_session_last_error(m_session, &msg, nullptr, 0);
        err = std::string("SFTP init failed: ") + (msg ? msg : "");
        spdlog::error("[ssh] {}", err);
        Disconnect();
        return ConnectResult::ConnectionError;
    }

    spdlog::info("[ssh] SFTP session initialized; ready");
    return ConnectResult::Ok;
}

uint64_t SshSession::StatFileSize(const std::string& remote_path,
                                  uint64_t& mtime_out, std::string& err)
{
    if(!IsConnected()) { err = "not connected"; return 0; }
    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    spdlog::info("[ssh] sftp_stat path='{}'", remote_path);
    int rc = libssh2_sftp_stat(m_sftp, remote_path.c_str(), &attrs);
    if(rc != 0)
    {
        unsigned long sftp_err = libssh2_sftp_last_error(m_sftp);
        char*         lib_msg  = nullptr;
        int           lib_rc   = libssh2_session_last_error(m_session, &lib_msg, nullptr, 0);
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
        err = "sftp stat failed for '" + remote_path + "': " + sftp_str +
              " (sftp_err=" + std::to_string(sftp_err) +
              ", libssh2_rc=" + std::to_string(lib_rc) +
              ", msg=" + (lib_msg ? lib_msg : "") + ")";
        spdlog::error("[ssh] {}", err);
        return 0;
    }
    mtime_out = attrs.mtime;
    spdlog::info("[ssh] sftp_stat ok: size={} mtime={}", attrs.filesize, attrs.mtime);
    return attrs.filesize;
}

bool SshSession::DownloadTo(const std::string& remote_path,
                            const std::string& local_path, ProgressSink& sink,
                            std::string& err)
{
    if(!IsConnected()) { err = "not connected"; return false; }

    LIBSSH2_SFTP_HANDLE* handle = libssh2_sftp_open(m_sftp, remote_path.c_str(),
                                                    LIBSSH2_FXF_READ, 0);
    if(!handle)
    {
        err = "sftp open failed: " + remote_path;
        return false;
    }

    LIBSSH2_SFTP_ATTRIBUTES attrs{};
    if(libssh2_sftp_fstat(handle, &attrs) == 0)
    {
        sink.total = attrs.filesize;
    }

    std::string part_path = local_path + ".part";
    {
        std::ofstream out(part_path, std::ios::binary | std::ios::trunc);
        if(!out)
        {
            err = "cannot open " + part_path + " for writing";
            libssh2_sftp_close(handle);
            return false;
        }

        constexpr size_t kBuf = 32 * 1024;
        std::vector<char> buf(kBuf);
        while(true)
        {
            ssize_t n = libssh2_sftp_read(handle, buf.data(), buf.size());
            if(n == 0) break;
            if(n < 0)
            {
                err = "sftp read failed";
                out.close();
                std::error_code ec;
                std::filesystem::remove(part_path, ec);
                libssh2_sftp_close(handle);
                return false;
            }
            out.write(buf.data(), n);
            if(!out)
            {
                err = "local write failed";
                out.close();
                std::error_code ec;
                std::filesystem::remove(part_path, ec);
                libssh2_sftp_close(handle);
                return false;
            }
            sink.bytes_read.fetch_add(static_cast<uint64_t>(n),
                                      std::memory_order_relaxed);
        }
    }
    libssh2_sftp_close(handle);

    std::error_code ec;
    std::filesystem::rename(part_path, local_path, ec);
    if(ec)
    {
        // Fallback: copy + remove (e.g. across mount points).
        std::filesystem::copy_file(part_path, local_path,
                                   std::filesystem::copy_options::overwrite_existing,
                                   ec);
        if(ec)
        {
            err = "rename to " + local_path + " failed: " + ec.message();
            return false;
        }
        std::filesystem::remove(part_path, ec);
    }
    return true;
}

void SshSession::Disconnect()
{
    if(m_sftp)
    {
        libssh2_sftp_shutdown(m_sftp);
        m_sftp = nullptr;
    }
    if(m_session)
    {
        libssh2_session_disconnect(m_session, "normal shutdown");
        libssh2_session_free(m_session);
        m_session = nullptr;
    }
    if(m_sock != kInvalidSocket)
    {
        CLOSE_SOCKET(m_sock);
        m_sock = kInvalidSocket;
    }
}

}  // namespace DataModel
}  // namespace RocProfVis
