// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

#include "rocprofvis_ssh_connection_config.h"

namespace RocProfVis
{
namespace View
{

    // Per-operation/session state for a remote workflow. RemoteUri no longer owns
    // SSH connection configuration (host/port/user/credentials) - that lives in a
    // separately persisted, named SshConnectionConfig. RemoteUri now holds only
    // the operation fields (command line, result path) and transient remote file
    // browsing state, plus a copy of the SSH connection selected for the workflow
    // so cache-key / local-path derivation still works.
    class RemoteUri
    {
    public:
        RemoteUri();

        // The SSH connection this operation runs against.
        void SetConnection(const SshConnectionConfig& connection);
        const SshConnectionConfig& GetConnection() const;
        SshConnectionConfig& GetConnection();

        // Convenience pass-throughs to the bound connection (trimmed).
        std::string GetRemoteHostString() const;
        std::string GetRemotePortString() const;
        int GetRemotePortInt() const;
        std::string GetRemoteUserString() const;
        std::string GetRemotePasswordString() const;
        std::string GetRemoteIdentityFileString() const;
        std::string GetPassphraseString() const;

        // Operation fields (trimmed read accessors).
        std::string GetRemoteCommandLineString() const;
        std::string GetRemoteResultPathString() const;
        std::string GetRemoteBrowsingPathString() const;

        // Mutable references for ImGui text inputs (CallbackResize paradigm).
        std::string& GetRemoteCommandLine();
        std::string& GetRemoteResultPath();

        std::string GetRemoteCacheKey();

        std::string GetLocalResultPathString();

        void InitRemoteBrowsingPathString(const char * source);
        void UseRemoteBrowsingPathString();
        void MakeRemoteBrowsingPath(const char* file_name);
        void SetCurrentDirectoryPath(const char* path);

    private:
        SshConnectionConfig m_connection;

        std::string m_remote_command_line;
        std::string m_remote_result_path;
        std::string m_file_browser_buffer;
        std::string m_current_directory_buffer;
        std::string m_file_browser_source;
    };


}  // namespace View
}  // namespace RocProfVis
