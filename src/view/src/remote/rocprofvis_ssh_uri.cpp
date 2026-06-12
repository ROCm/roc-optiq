// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_uri.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_core_string_utils.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <filesystem>



namespace RocProfVis
{
namespace View
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

    RemoteUri::RemoteUri()
    {
        SetDefaults();
    }

    void RemoteUri::SetDefaults()
    {
        m_file_browser_source.clear();
        m_remote_port = "22";
    }

    std::string RemoteUri::GetConfigRoot()
    {
        std::filesystem::path config_root = get_application_config_path(true);
        std::filesystem::path cache_root  = config_root / "remote_cache";
        return cache_root.string();
    }

    std::string RemoteUri::GetRemoteCacheKey()
    {
        std::string canon = GetRemoteUserString() + "@" + GetRemoteHostString() + ":" + GetRemotePortString() + GetRemoteResultPathString();
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

    bool RemoteUri::SaveToJson()
    {
        jt::Json j;
        std::string path = GetConfigRoot();
        j.setObject();

        auto& obj = j.getObject();

        obj["remote_host"] = m_remote_host;
        obj["remote_port"] = m_remote_port;
        obj["remote_user"] = m_remote_user;
        obj["remote_password"] = m_remote_password;
        obj["remote_command_line"] = m_remote_command_line;
        obj["remote_result_path"] = m_remote_result_path;
        obj["remote_identity_file"] = m_remote_identity_file;
        obj["passphrase"] = m_passphrase;


        auto full = std::filesystem::weakly_canonical(path+ "/remote.json");

        if (!std::filesystem::exists(full.parent_path())) {
            return false;
        }

        std::ofstream out(full, std::ios::binary);


        if (!out)
        {
            return false;
        }

        out << j.toStringPretty();

        return true;
    }

    bool RemoteUri::LoadFromJson()
    {
        std::string path = GetConfigRoot() + "/remote.json";
        auto full = std::filesystem::weakly_canonical(path);

        if (!std::filesystem::exists(full.parent_path())) {
            return false;
        }
        
        std::ifstream in(full, std::ios::binary);

        if (!in)
        {
            return false;
        }

        std::string json_text(
            (std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>());

        auto parsed = jt::Json::parse(json_text);

        if (parsed.first != jt::Json::success)
        {
            return false;
        }

        const jt::Json& j = parsed.second;

        if (!j.isObject())
        {
            return false;
        }

        LoadField(j, "remote_host", m_remote_host);
        LoadField(j, "remote_port", m_remote_port);
        LoadField(j, "remote_user", m_remote_user);
        LoadField(j, "remote_password", m_remote_password);
        LoadField(j, "remote_command_line", m_remote_command_line);
        LoadField(j, "remote_result_path", m_remote_result_path);
        LoadField(j, "remote_identity_file", m_remote_identity_file);
        LoadField(j, "passphrase", m_passphrase);

        return true;
    }

    void RemoteUri::LoadField(
        const jt::Json& j,
        const std::string& key,
        std::string& out)
    {
        if (!j.contains(key))
        {
            return;
        }

        auto& obj = const_cast<jt::Json&>(j).getObject();

        auto it = obj.find(key);

        if (it == obj.end())
        {
            return;
        }

        if (!it->second.isString())
        {
            return;
        }

        out = it->second.getString();
    }

    std::string RemoteUri::GetRemoteHostString() const
    {
        return util::string::trim_copy(m_remote_host);
    }

    std::string RemoteUri::GetRemotePortString() const
    {
        return util::string::trim_copy(m_remote_port);
    }

    int RemoteUri::GetRemotePortInt() const
    {
        return std::atoi(m_remote_port.c_str());
    }

    std::string RemoteUri::GetRemoteUserString() const
    {
        return util::string::trim_copy(m_remote_user);
    }

    std::string RemoteUri::GetRemotePasswordString() const
    {
        return util::string::trim_copy(m_remote_password);
    }

    std::string RemoteUri::GetRemoteCommandLineString() const
    {
        return util::string::trim_copy(m_remote_command_line);
    }

    std::string RemoteUri::GetRemoteResultPathString() const
    {
        return util::string::trim_copy(m_remote_result_path);
    }

    std::string RemoteUri::GetRemoteIdentityFileString() const
    {
        return util::string::trim_copy(m_remote_identity_file);
    }

    std::string RemoteUri::GetPassphraseString() const
    {
        return util::string::trim_copy(m_passphrase);
    }

    std::string RemoteUri::GetRemoteBrowsingPathString() const
    {
        return m_file_browser_buffer;
    }

    std::string& RemoteUri::GetRemoteHost()
    {
        return m_remote_host;
    }

    std::string& RemoteUri::GetRemotePort()
    {
        return m_remote_port;
    }

    std::string& RemoteUri::GetRemoteUser()
    {
        return m_remote_user;
    }

    std::string& RemoteUri::GetRemotePassword()
    {
        return m_remote_password;
    }

    std::string& RemoteUri::GetRemoteCommandLine()
    {
        return m_remote_command_line;
    }

    std::string& RemoteUri::GetRemoteResultPath()
    {
        return m_remote_result_path;
    }

    std::string& RemoteUri::GetRemoteIdentityFile()
    {
        return m_remote_identity_file;
    }

    std::string& RemoteUri::GetPassphrase()
    {
        return m_passphrase;
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



}  // namespace DataModel
}  // namespace RocProfVis
