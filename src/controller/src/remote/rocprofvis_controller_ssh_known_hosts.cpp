// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_ssh_known_hosts.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>   // SHGetFolderPath
#else
#include <pwd.h>
#include <unistd.h>
#endif


namespace RocProfVis
{
namespace Controller
{

namespace
{

constexpr int SSH_DEFAULT_PORT = 22;
// What libssh2_knownhost_checkp takes as "no port".
constexpr int KNOWNHOST_NO_PORT = -1;

std::string DefaultHomePath()
{
#if defined(_WIN32)
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PROFILE, nullptr, 0, path))) {
        return std::string(path);
    }  
#else
    // POSIX secure source
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
        return std::string(pw->pw_dir);

#endif
    return "";
}

std::string DefaultKnownHostsPath()
{
    std::string path = DefaultHomePath();
#if defined(_WIN32)
    return path.empty() ? path : path + "\\.ssh\\known_hosts";
#else
    return path.empty() ? path : path + "/.ssh/known_hosts";
#endif
}

std::string Base64Encode(const unsigned char* data, size_t n)
{
    static const char* alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((n + 2) / 3) * 4);
    for(size_t i = 0; i < n; i += 3)
    {
        unsigned v = data[i] << 16;
        if(i + 1 < n) v |= data[i + 1] << 8;
        if(i + 2 < n) v |= data[i + 2];
        out.push_back(alphabet[(v >> 18) & 0x3F]);
        out.push_back(alphabet[(v >> 12) & 0x3F]);
        out.push_back(i + 1 < n ? alphabet[(v >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < n ? alphabet[v & 0x3F] : '=');
    }
    return out;
}

// Looks `name` up exactly as written. Given no port, libssh2 does not also try
// "[name]:port" and then the bare name, which is how a key for one port ends
// up judged against the entry for another.
KnownHostMatch CheckName(LIBSSH2_KNOWNHOSTS* known_hosts, const std::string& name,
                         const char* key, size_t key_len, int type_mask)
{
    KnownHostMatch result = KnownHostMatch::Failure;
    int rc = libssh2_knownhost_checkp(known_hosts, name.c_str(), KNOWNHOST_NO_PORT, key,
                                      key_len, type_mask, nullptr);
    switch(rc)
    {
        case LIBSSH2_KNOWNHOST_CHECK_MATCH:
            result = KnownHostMatch::Match;
            break;
        case LIBSSH2_KNOWNHOST_CHECK_MISMATCH:
            result = KnownHostMatch::Mismatch;
            break;
        case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND:
            result = KnownHostMatch::NotFound;
            break;
        default:
            break;
    }
    return result;
}
}  // namespace

std::string KnownHostName(const std::string& host, int port)
{
    return port == SSH_DEFAULT_PORT ? host : "[" + host + "]:" + std::to_string(port);
}

bool AppendKnownHostLine(const std::string& path, const std::string& line)
{
    std::error_code       ec;
    std::filesystem::path folder = std::filesystem::path(path).parent_path();
    if(!folder.empty() && std::filesystem::create_directories(folder, ec))
    {
        // A new ~/.ssh is private to the user, as OpenSSH creates it.
        std::filesystem::permissions(folder, std::filesystem::perms::owner_all, ec);
    }

    // An entry appended to a file whose last line has no newline would run on
    // from that line and corrupt both.
    bool needs_newline = false;
    {
        std::ifstream existing(path, std::ios::binary | std::ios::ate);
        if(existing.is_open() && existing.tellg() > 0)
        {
            existing.seekg(-1, std::ios::end);
            needs_newline = existing.get() != '\n';
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::app);
    if(!file.is_open())
    {
        return false;
    }
    if(needs_newline)
    {
        file.put('\n');
    }
    file.write(line.data(), static_cast<std::streamsize>(line.size()));
    file.flush();
    return file.good();
}

KnownHosts::KnownHosts(LIBSSH2_SESSION* session)
    : m_session(session), m_path(DefaultKnownHostsPath())
{
    m_kh = libssh2_knownhost_init(m_session);
}

KnownHosts::~KnownHosts()
{
    if(m_kh) libssh2_knownhost_free(m_kh);
}

bool KnownHosts::Load()
{
    if(!m_kh || m_path.empty()) return false;

    std::ifstream file(m_path, std::ios::binary);
    if(!file.is_open())
    {
        return false;
    }

    // Line by line rather than libssh2_knownhost_readfile, which stops at the
    // first line it cannot parse. Every entry after such a line would then be
    // unknown: a host the user already trusts is asked about again, and a
    // changed key there shows up as a first connection instead of a mismatch.
    std::string line;
    while(std::getline(file, line))
    {
        if(!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if(libssh2_knownhost_readline(m_kh, line.c_str(), line.size(),
                                      LIBSSH2_KNOWNHOST_FILE_OPENSSH) != 0)
        {
            spdlog::debug("[ssh] known_hosts: skipped a line that could not be parsed");
        }
    }
    return true;
}

KnownHostMatch KnownHosts::Check(const std::string& host, int port) const
{
    if(!m_kh) return KnownHostMatch::Failure;

    size_t      key_len  = 0;
    int         key_type = 0;
    const char* key      = libssh2_session_hostkey(m_session, &key_len, &key_type);
    if(!key) return KnownHostMatch::Failure;

    int kh_type_mask = LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW;
    switch(key_type)
    {
        case LIBSSH2_HOSTKEY_TYPE_RSA:     kh_type_mask |= LIBSSH2_KNOWNHOST_KEY_SSHRSA; break;
        case LIBSSH2_HOSTKEY_TYPE_DSS:     kh_type_mask |= LIBSSH2_KNOWNHOST_KEY_SSHDSS; break;
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_256
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_256: kh_type_mask |= LIBSSH2_KNOWNHOST_KEY_ECDSA_256; break;
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_384: kh_type_mask |= LIBSSH2_KNOWNHOST_KEY_ECDSA_384; break;
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_521: kh_type_mask |= LIBSSH2_KNOWNHOST_KEY_ECDSA_521; break;
#endif
#ifdef LIBSSH2_HOSTKEY_TYPE_ED25519
        case LIBSSH2_HOSTKEY_TYPE_ED25519: kh_type_mask |= LIBSSH2_KNOWNHOST_KEY_ED25519; break;
#endif
        default: break;
    }

    KnownHostMatch result = CheckName(m_kh, KnownHostName(host, port), key, key_len, kh_type_mask);
    if(result == KnownHostMatch::NotFound && port != SSH_DEFAULT_PORT)
    {
        // Only a match counts here. The bare host name is the port-22 entry, so
        // a different key under it says nothing about this port, and reporting
        // it as a changed key would teach the user to accept that warning.
        if(CheckName(m_kh, host, key, key_len, kh_type_mask) == KnownHostMatch::Match)
        {
            result = KnownHostMatch::Match;
        }
    }
    return result;
}

bool KnownHosts::Add(const std::string& host, int port)
{
    m_added = nullptr;
    if(!m_kh) return false;
    size_t      key_len  = 0;
    int         key_type = 0;
    int         rc = 0;
    const char* key      = libssh2_session_hostkey(m_session, &key_len, &key_type);
    if(!key) return false;

    int type_mask = LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW;
    switch(key_type)
    {
        case LIBSSH2_HOSTKEY_TYPE_RSA:     type_mask |= LIBSSH2_KNOWNHOST_KEY_SSHRSA; break;
        case LIBSSH2_HOSTKEY_TYPE_DSS:     type_mask |= LIBSSH2_KNOWNHOST_KEY_SSHDSS; break;
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_256
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_256: type_mask |= LIBSSH2_KNOWNHOST_KEY_ECDSA_256; break;
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_384: type_mask |= LIBSSH2_KNOWNHOST_KEY_ECDSA_384; break;
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_521: type_mask |= LIBSSH2_KNOWNHOST_KEY_ECDSA_521; break;
#endif
#ifdef LIBSSH2_HOSTKEY_TYPE_ED25519
        case LIBSSH2_HOSTKEY_TYPE_ED25519: type_mask |= LIBSSH2_KNOWNHOST_KEY_ED25519; break;
#endif
        default: return false;
    }

    const std::string name = KnownHostName(host, port);
    rc = libssh2_knownhost_addc(m_kh, name.c_str(), nullptr, key, key_len,
        "added by roc-optiq", strlen("added by roc-optiq"),
        type_mask, &m_added);
    if (rc != 0) {
        spdlog::error("failed adding new host: {}", rc);
        m_added = nullptr;
        return false;
    }
    return true;
}


bool KnownHosts::Save() const
{
    if (!m_kh || !m_added || m_path.empty())
    {
        return false;
    }

    // libssh2 writes nothing into a buffer that is too small but still reports
    // the length it needs, so the first call only sizes the line.
    size_t length = 0;
    libssh2_knownhost_writeline(m_kh, m_added, nullptr, 0, &length,
                                LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    std::string line(length + 1, '\0');
    if (libssh2_knownhost_writeline(m_kh, m_added, line.data(), line.size(), &length,
                                    LIBSSH2_KNOWNHOST_FILE_OPENSSH) != 0)
    {
        return false;
    }
    line.resize(length);

    return AppendKnownHostLine(m_path, line);
}

std::string FormatHostKeyFingerprint(LIBSSH2_SESSION* session)
{
    const char* sha = libssh2_hostkey_hash(session, LIBSSH2_HOSTKEY_HASH_SHA256);
    if(!sha) return {};
    std::string b64 =
        Base64Encode(reinterpret_cast<const unsigned char*>(sha), 32);
    while(!b64.empty() && b64.back() == '=') b64.pop_back();  // OpenSSH strips '='
    return "SHA256:" + b64;
}

std::string HostKeyTypeName(LIBSSH2_SESSION* session)
{
    size_t len  = 0;
    int    type = 0;
    if(!libssh2_session_hostkey(session, &len, &type)) return {};
    switch(type)
    {
        case LIBSSH2_HOSTKEY_TYPE_RSA: return "ssh-rsa";
        case LIBSSH2_HOSTKEY_TYPE_DSS: return "ssh-dss";
#ifdef LIBSSH2_HOSTKEY_TYPE_ECDSA_256
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_256: return "ecdsa-sha2-nistp256";
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_384: return "ecdsa-sha2-nistp384";
        case LIBSSH2_HOSTKEY_TYPE_ECDSA_521: return "ecdsa-sha2-nistp521";
#endif
#ifdef LIBSSH2_HOSTKEY_TYPE_ED25519
        case LIBSSH2_HOSTKEY_TYPE_ED25519: return "ssh-ed25519";
#endif
        default: return "unknown";
    }
}

}  // namespace Controller
}  // namespace RocProfVis
