// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

#include "json.h"

namespace RocProfVis
{
namespace View
{

    class RemoteUri
    {
    public:
        RemoteUri();

        bool SaveToJson();
        bool LoadFromJson();

        // Trimmed read accessors used by the SSH/session layers.
        std::string GetRemoteHostString() const;
        std::string GetRemotePortString() const;
        int GetRemotePortInt() const;
        std::string GetRemoteUserString() const;
        std::string GetRemotePasswordString() const;
        std::string GetRemoteCommandLineString() const;
        std::string GetRemoteResultPathString() const;
        std::string GetRemoteBrowsingPathString() const;
        std::string GetRemoteIdentityFileString() const;
        std::string GetPassphraseString() const;

        // Mutable references for ImGui text inputs (CallbackResize paradigm).
        std::string& GetRemoteHost();
        std::string& GetRemotePort();
        std::string& GetRemoteUser();
        std::string& GetRemotePassword();
        std::string& GetRemoteCommandLine();
        std::string& GetRemoteResultPath();
        std::string& GetRemoteIdentityFile();
        std::string& GetPassphrase();

        std::string GetRemoteCacheKey();

        std::string GetLocalResultPathString();

        void InitRemoteBrowsingPathString(const char * source);
        void UseRemoteBrowsingPathString();
        void MakeRemoteBrowsingPath(const char* file_name);
        void SetCurrentDirectoryPath(const char* path);

    private:
        void SetDefaults();
        std::string GetConfigRoot();

        static void LoadField(
            const jt::Json& j,
            const std::string& key,
            std::string& out);

    private:
        std::string m_remote_host;
        std::string m_remote_port;
        std::string m_remote_user;
        std::string m_remote_password;
        std::string m_remote_command_line;
        std::string m_remote_result_path;
        std::string m_remote_identity_file;
        std::string m_passphrase;
        std::string m_file_browser_buffer;
        std::string m_current_directory_buffer;
        std::string m_file_browser_source;
    };


}  // namespace DataModel
}  // namespace RocProfVis
