// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_uri.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_core_string_utils.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace RocProfVis
{
namespace View
{
    namespace
    {
        uint64_t Fnv1a64(const std::string& s)
        {
            uint64_t h = 0xcbf29ce484222325ull;
            for(unsigned char c : s)
            {
                h ^= c;
                h *= 0x100000001b3ull;
            }
            return h;
        }

        std::string ToHex(uint64_t v)
        {
            char buf[17];
            std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
            return buf;
        }
    }  // namespace

    RemoteUri::RemoteUri()
    {
    }

    void RemoteUri::SetConnection(const SshConnectionConfig& connection)
    {
        m_connection = connection;
    }

    const SshConnectionConfig& RemoteUri::GetConnection() const
    {
        return m_connection;
    }

    SshConnectionConfig& RemoteUri::GetConnection()
    {
        return m_connection;
    }

    std::string RemoteUri::GetRemoteHostString() const
    {
        return m_connection.HostTrimmed();
    }

    std::string RemoteUri::GetRemotePortString() const
    {
        return m_connection.PortTrimmed();
    }

    int RemoteUri::GetRemotePortInt() const
    {
        return m_connection.PortInt();
    }

    std::string RemoteUri::GetRemoteUserString() const
    {
        return m_connection.UserTrimmed();
    }

    std::string RemoteUri::GetRemotePasswordString() const
    {
        return m_connection.PasswordTrimmed();
    }

    std::string RemoteUri::GetRemoteIdentityFileString() const
    {
        return m_connection.IdentityFileTrimmed();
    }

    std::string RemoteUri::GetPassphraseString() const
    {
        return m_connection.PassphraseTrimmed();
    }

    std::string RemoteUri::GetRemoteCommandLineString() const
    {
        return util::string::trim_copy(m_remote_command_line);
    }

    std::string RemoteUri::GetRemoteResultPathString() const
    {
        return util::string::trim_copy(m_remote_result_path);
    }

    std::string RemoteUri::GetRemoteBrowsingPathString() const
    {
        return m_file_browser_buffer;
    }

    std::string& RemoteUri::GetRemoteCommandLine()
    {
        return m_remote_command_line;
    }

    std::string& RemoteUri::GetRemoteResultPath()
    {
        return m_remote_result_path;
    }

    std::string RemoteUri::GetRemoteCacheKey()
    {
        std::string canon = GetRemoteUserString() + "@" + GetRemoteHostString() + ":" +
                            GetRemotePortString() + GetRemoteResultPathString();
        return ToHex(Fnv1a64(canon));
    }

    std::string RemoteUri::GetLocalResultPathString()
    {
        std::string remote = GetRemoteResultPathString();
        auto pos = remote.rfind('/');
        std::string filename = pos == std::string::npos ? remote : remote.substr(pos + 1);
        std::filesystem::path config_root = get_application_config_path(true);
        std::filesystem::path cache_root  = config_root / "remote_cache";
        std::filesystem::path key_root = cache_root / GetRemoteCacheKey();

        if (!std::filesystem::exists(key_root))
        {
            std::filesystem::create_directories(key_root);
        }

        std::filesystem::path path = key_root / filename;
        return path.string();
    }

    void RemoteUri::InitRemoteBrowsingPathString(const char* source)
    {
        std::string dir = std::filesystem::path(source ? source : "").parent_path().string();
        if (dir.empty())
        {
            dir = ".";
        }
        m_file_browser_buffer = dir;
        m_file_browser_source = source ? source : "";
    }

    void RemoteUri::UseRemoteBrowsingPathString()
    {
        // The browse flow seeds m_file_browser_source from the result path; commit
        // the chosen path back into the result-path field.
        m_remote_result_path = m_file_browser_buffer;
    }

    void RemoteUri::SetCurrentDirectoryPath(const char* path)
    {
        m_current_directory_buffer = path ? path : "";
    }

    void RemoteUri::MakeRemoteBrowsingPath(const char* file_name)
    {
        if (!file_name)
        {
            return;
        }

        std::string path = m_current_directory_buffer;

        if (std::strcmp(file_name, "..") == 0)
        {
            if (path.size() > 1 && path.back() == '/')
            {
                path.pop_back();
            }

            auto last_slash = path.rfind('/');
            if (last_slash != std::string::npos)
            {
                // Preserve the leading '/' for absolute paths.
                path.erase(last_slash == 0 ? 1 : last_slash);
            }

            m_file_browser_buffer = path;
            return;
        }

        if (!path.empty() && path.back() != '/')
        {
            path += '/';
        }
        path += file_name;

        m_file_browser_buffer = path;
    }

}  // namespace View
}  // namespace RocProfVis
