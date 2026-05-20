// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_uri.h"
#include "rocprofvis_utils.h"

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
        CopyToArray(m_remote_port, "22");
    }

    std::string RemoteUri::GetConfigPath()
    {
        std::filesystem::path config_root = get_application_config_path(true);
        std::filesystem::path cache_root  = config_root / "remote_cache";
        std::filesystem::path path = cache_root / "remote.json";
        return path.string();
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
        std::filesystem::path key_root =cache_root / GetRemoteCacheKey();
        std::filesystem::path path = key_root / filename;
        return path.string();
    }

    bool RemoteUri::SaveToJson()
    {
        jt::Json j;
        std::string path = GetConfigPath();
        j.setObject();

        auto& obj = j.getObject();

        obj["remote_host"] = ToString(m_remote_host);
        obj["remote_port"] = ToString(m_remote_port);
        obj["remote_user"] = ToString(m_remote_user);
        obj["remote_password"] = ToString(m_remote_password);
        obj["remote_command_line"] = ToString(m_remote_command_line);
        obj["remote_result_path"] = ToString(m_remote_result_path);
        obj["remote_identity_file"] = ToString(m_remote_identity_file);
        obj["passphrase"] = ToString(m_passphrase);

        std::ofstream out(GetConfigPath().c_str(), std::ios::binary);

        if (!out)
        {
            return false;
        }

        out << j.toStringPretty();

        return true;
    }

    bool RemoteUri::LoadFromJson()
    {
        std::string path = GetConfigPath();
        std::ifstream in(path, std::ios::binary);

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

    template <size_t N>
    void RemoteUri::CopyToArray(
        std::array<char, N>& dst,
        const std::string& src)
    {
        dst.fill('\0');

        size_t len = std::min(src.size(), N - 1);

        memcpy(dst.data(), src.data(), len);

        dst[len] = '\0';
    }

    template <size_t N>
    std::string RemoteUri::ToString(
        const std::array<char, N>& value)
    {
        return std::string(value.data());
    }

    template <size_t N>
    void RemoteUri::LoadField(
        const jt::Json& j,
        const std::string& key,
        std::array<char, N>& out)
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

        CopyToArray(out, it->second.getString());
    }

    const std::array<char, 256>& RemoteUri::GetRemoteHostArray() const
    {
        return m_remote_host;
    }

    std::string RemoteUri::GetRemoteHostString() const
    {
        return std::string(m_remote_host.data());
    }

    const std::array<char, 16>& RemoteUri::GetRemotePortArray() const
    {
        return m_remote_port;
    }

    std::string RemoteUri::GetRemotePortString() const
    {
        return std::string(m_remote_port.data());
    }

    int RemoteUri::GetRemotePortInt() const
    {
        return std::atoi(m_remote_port.data());
    }

    const std::array<char, 128>& RemoteUri::GetRemoteUserArray() const
    {
        return m_remote_user;
    }

    std::string RemoteUri::GetRemoteUserString() const
    {
        return std::string(m_remote_user.data());
    }

    const std::array<char, 256>& RemoteUri::GetRemotePasswordArray() const
    {
        return m_remote_password;
    }

    std::string RemoteUri::GetRemotePasswordString() const
    {
        return std::string(m_remote_password.data());
    }

    const std::array<char, 1024>& RemoteUri::GetRemoteCommandLineArray() const
    {
        return m_remote_command_line;
    }

    std::string RemoteUri::GetRemoteCommandLineString() const
    {
        return std::string(m_remote_command_line.data());
    }

    const std::array<char, 1024>& RemoteUri::GetRemoteResultPathArray() const
    {
        return m_remote_result_path;
    }

    std::string RemoteUri::GetRemoteResultPathString() const
    {
        return std::string(m_remote_result_path.data());
    }

    const std::array<char, 1024>& RemoteUri::GetRemoteIdentityFileArray() const
    {
        return m_remote_identity_file;
    }

    std::string RemoteUri::GetRemoteIdentityFileString() const
    {
        return std::string(m_remote_identity_file.data());
    }

    const std::array<char, 256>& RemoteUri::GetPassphraseArray() const
    {
        return m_passphrase;
    }

    std::string RemoteUri::GetPassphraseString() const
    {
        return std::string(m_passphrase.data());
    }

    char* RemoteUri::GetRemoteHostBuffer()
    {
        return m_remote_host.data();
    }

    char* RemoteUri::GetRemotePortBuffer()
    {
        return m_remote_port.data();
    }

    char* RemoteUri::GetRemoteUserBuffer()
    {
        return m_remote_user.data();
    }

    char* RemoteUri::GetRemotePasswordBuffer()
    {
        return m_remote_password.data();
    }

    char* RemoteUri::GetRemoteCommandLineBuffer()
    {
        return m_remote_command_line.data();
    }

    char* RemoteUri::GetRemoteResultPathBuffer()
    {
        return m_remote_result_path.data();
    }

    char* RemoteUri::GetRemoteIdentityFileBuffer()
    {
        return m_remote_identity_file.data();
    }

    char* RemoteUri::GetPassphraseBuffer()
    {
        return m_passphrase.data();
    }

    size_t RemoteUri::GetRemoteHostBufferSize() const
    {
        return m_remote_host.size();
    }

    size_t RemoteUri::GetRemotePortBufferSize() const
    {
        return m_remote_port.size();
    }

    size_t RemoteUri::GetRemoteUserBufferSize() const
    {
        return m_remote_user.size();
    }

    size_t RemoteUri::GetRemotePasswordBufferSize() const
    {
        return m_remote_password.size();
    }

    size_t RemoteUri::GetRemoteCommandLineBufferSize() const
    {
        return m_remote_command_line.size();
    }

    size_t RemoteUri::GetRemoteResultPathBufferSize() const
    {
        return m_remote_result_path.size();
    }

    size_t RemoteUri::GetRemoteIdentityFileBufferSize() const
    {
        return m_remote_identity_file.size();
    }

    size_t RemoteUri::GetPassphraseBufferSize() const
    {
        return m_passphrase.size();
    }


}  // namespace DataModel
}  // namespace RocProfVis
