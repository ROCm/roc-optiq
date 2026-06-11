// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_ssh_known_hosts.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>

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
    return path.empty() ? path : path + "\\.ssh\\known_hosts";
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
}  // namespace

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
    int rc = libssh2_knownhost_readfile(m_kh, m_path.c_str(),
        LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    return rc >= 0;
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

    struct libssh2_knownhost* match = nullptr;
    int rc = libssh2_knownhost_checkp(m_kh, host.c_str(), port, key, key_len,
                                      kh_type_mask, &match);
    switch(rc)
    {
        case LIBSSH2_KNOWNHOST_CHECK_MATCH:    return KnownHostMatch::Match;
        case LIBSSH2_KNOWNHOST_CHECK_MISMATCH: return KnownHostMatch::Mismatch;
        case LIBSSH2_KNOWNHOST_CHECK_NOTFOUND: return KnownHostMatch::NotFound;
        default:                               return KnownHostMatch::Failure;
    }
}

bool KnownHosts::Add(const std::string& host, int port)
{
    if(!m_kh) return false;
    size_t      key_len  = 0;
    int         key_type = 0;
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

    return libssh2_knownhost_addc(m_kh, host.c_str(), nullptr, key, key_len,
                                  "added by roc-optiq", strlen("added by roc-optiq"),
                                  type_mask, nullptr) == 0;
}


bool KnownHosts::Save() const
{
    if (!m_kh) return false;

    return libssh2_knownhost_writefile(
        m_kh,
        m_path.c_str(),
        LIBSSH2_KNOWNHOST_FILE_OPENSSH
    ) == 0;
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
