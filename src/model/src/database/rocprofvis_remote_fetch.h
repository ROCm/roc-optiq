// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_remote_uri.h"
#include "rocprofvis_ssh_session.h"

#include <filesystem>
#include <string>

namespace RocProfVis
{
namespace DataModel
{

class AuthBridge;

enum class SshAuthResult
{
    Ok,
    NeedsPassword,
    AuthFailed,
    HostKeyMismatch,
    HostKeyRejected,
    ConnectionError,
    Cancelled
};

// Stable, filesystem-safe key derived from host+port+user+remote_path.
// Same URI -> same key across runs. Not collision-resistant by design;
// suitable for a per-user local cache only.
std::string RemoteCacheKey(const RemoteUri& u);

// Last path component of u.path; sanitized to be filesystem-safe and
// always end in `.db`.
std::string RemoteBasenameForCache(const RemoteUri& u);

// Probe-only Connect()+Disconnect().
SshAuthResult TestSshConnection(const RemoteUri& u, AuthBridge& bridge,
                                std::string& err);

// Fetch the remote file to `dest_path`. Honors a per-file sidecar
// `<dest_path>.meta` to skip the download if the remote size+mtime
// haven't changed. Returns Ok on success (cache hit or fresh download).
SshAuthResult FetchRemoteFile(const RemoteUri&             u,
                              const std::filesystem::path& dest_path,
                              AuthBridge&                  bridge,
                              ProgressSink&                sink,
                              std::string&                 err);

}  // namespace DataModel
}  // namespace RocProfVis
