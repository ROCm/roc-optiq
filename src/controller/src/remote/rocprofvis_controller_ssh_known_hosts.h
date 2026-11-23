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

// Thin RAII wrapper around libssh2_knownhost_*. Loads (and appends to) the
// per-user OpenSSH known_hosts file.
class KnownHosts
{
public:
    explicit KnownHosts(LIBSSH2_SESSION* session);
    ~KnownHosts();

    KnownHosts(const KnownHosts&) = delete;
    KnownHosts& operator=(const KnownHosts&) = delete;

    // Loads the user's known_hosts file, skipping lines that cannot be parsed.
    // Returns false if the file could not be opened (still safe to use
    // Check()).
    bool Load();

    // Compares the live server key for `host`/`port` against the loaded
    // known_hosts entries, the way OpenSSH does: the entry for this host and
    // port decides. A port other than 22 with no entry of its own also accepts
    // the key the host is known by on port 22, but a different key there is
    // not a mismatch for this port.
    KnownHostMatch Check(const std::string& host, int port) const;

    // Adds the live server key to the in-memory store under the name
    // KnownHostName gives `host`/`port`.
    bool Add(const std::string& host, int port);

    // Appends the entry Add() created to the file. Nothing already in the file
    // is rewritten, so entries and comments this class cannot parse survive.
    bool Save() const;

    // Returns the path that Load/Save use (resolved at construction time).
    const std::string& Path() const { return m_path; }

private:
    LIBSSH2_SESSION*          m_session = nullptr;
    LIBSSH2_KNOWNHOSTS*       m_kh      = nullptr;
    std::string               m_path;
    // The entry Add() created, owned by m_kh.
    struct libssh2_knownhost* m_added   = nullptr;
};

// The name a host key is filed under in known_hosts: the bare host for port
// 22 and "[host]:port" for any other port, as OpenSSH writes it. A key accepted
// on one port is therefore never an entry for another.
std::string KnownHostName(const std::string& host, int port);

// Appends `line` (one complete entry, newline included) to the known_hosts
// file at `path`, creating the file and its folder when missing. False if the
// line could not be written.
bool AppendKnownHostLine(const std::string& path, const std::string& line);

// Returns base64(SHA256(host_key_bytes)) — matches `ssh-keygen -lf` output.
std::string FormatHostKeyFingerprint(LIBSSH2_SESSION* session);

// Returns the OpenSSH key-type label (e.g. "ssh-ed25519") for the live
// server key on `session`, or "" if unavailable.
std::string HostKeyTypeName(LIBSSH2_SESSION* session);

}  // namespace Controller
}  // namespace RocProfVis
