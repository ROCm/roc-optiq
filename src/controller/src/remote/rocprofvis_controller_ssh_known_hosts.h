// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <libssh2.h>
#include <string>

namespace RocProfVis
{
namespace Controller
{

enum class KnownHostMatch
{
    Match,
    Mismatch,
    NotFound,
    Failure
};

// Thin RAII wrapper around libssh2_knownhost_*. Reads the per-user OpenSSH
// known_hosts file and appends newly trusted keys to it.
class KnownHosts
{
public:
    explicit KnownHosts(LIBSSH2_SESSION* session);
    ~KnownHosts();

    KnownHosts(const KnownHosts&) = delete;
    KnownHosts& operator=(const KnownHosts&) = delete;

    // Loads the user's known_hosts file. Returns true if the file was
    // present and parseable, false otherwise (still safe to use Check()).
    bool Load();

    // Compares the live server key for `host`/`port` against the loaded
    // known_hosts entries.
    KnownHostMatch Check(const std::string& host, int port) const;

    // Appends the live server key for `host`/`port` to the known_hosts file,
    // recorded as "[host]:port" for non-default ports like OpenSSH does. The
    // file is appended to rather than rewritten so comments and entries libssh2
    // cannot parse survive.
    bool Remember(const std::string& host, int port);

    // Returns the known_hosts path (resolved at construction time).
    const std::string& Path() const { return m_path; }

private:
    LIBSSH2_SESSION*    m_session = nullptr;
    LIBSSH2_KNOWNHOSTS* m_kh      = nullptr;
    std::string         m_path;
};

// Returns base64(SHA256(host_key_bytes)) — matches `ssh-keygen -lf` output.
std::string FormatHostKeyFingerprint(LIBSSH2_SESSION* session);

// Returns the OpenSSH key-type label (e.g. "ssh-ed25519") for the live
// server key on `session`, or "" if unavailable.
std::string HostKeyTypeName(LIBSSH2_SESSION* session);

}  // namespace Controller
}  // namespace RocProfVis
