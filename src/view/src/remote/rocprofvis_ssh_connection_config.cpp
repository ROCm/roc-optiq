// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_connection_config.h"
#include "rocprofvis_core_string_utils.h"
#include "rocprofvis_json_utils.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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

std::string SshConnectionConfig::DisplayLabel() const
{
    // A user-chosen name wins over the raw endpoint so the UI shows what the
    // user typed (e.g. "Lab box") rather than user@host:port.
    std::string name = Core::String::trim_copy(display_name);
    if(!name.empty())
    {
        return name;
    }

    std::string h = HostTrimmed();
    if(h.empty())
    {
        return std::string();
    }

    std::string u = UserTrimmed();
    return (u.empty() ? std::string("?") : u) + "@" + h + ":" + PortTrimmed();
}

std::string SshConnectionConfig::HostTrimmed() const
{
    return Core::String::trim_copy(host);
}

std::string SshConnectionConfig::PortTrimmed() const
{
    return Core::String::trim_copy(port);
}

int SshConnectionConfig::PortInt() const
{
    return std::atoi(port.c_str());
}

std::string SshConnectionConfig::UserTrimmed() const
{
    return Core::String::trim_copy(user);
}

std::string SshConnectionConfig::PasswordTrimmed() const
{
    return Core::String::trim_copy(password);
}

std::string SshConnectionConfig::IdentityFileTrimmed() const
{
    return Core::String::trim_copy(identity_file);
}

std::string SshConnectionConfig::PassphraseTrimmed() const
{
    return Core::String::trim_copy(passphrase);
}

jt::Json SshConnectionConfig::ToJson() const
{
    jt::Json json;
    json.setObject();

    auto& obj = json.getObject();
    obj["id"]            = id;
    obj["display_name"]  = display_name;
    obj["host"]          = host;
    obj["port"]          = port;
    obj["user"]          = user;
    obj["password"]      = password;
    obj["identity_file"] = identity_file;
    obj["passphrase"]    = passphrase;

    return json;
}

SshConnectionConfig SshConnectionConfig::FromJson(jt::Json const& j)
{
    SshConnectionConfig cfg;
    cfg.id            = JsonUtils::GetString(j, "id", "");
    cfg.display_name  = JsonUtils::GetString(j, "display_name", "");
    cfg.host          = JsonUtils::GetString(j, "host", "");
    cfg.port          = JsonUtils::GetString(j, "port", "22");
    cfg.user          = JsonUtils::GetString(j, "user", "");
    cfg.password      = JsonUtils::GetString(j, "password", "");
    cfg.identity_file = JsonUtils::GetString(j, "identity_file", "");
    cfg.passphrase    = JsonUtils::GetString(j, "passphrase", "");

    if(cfg.id.empty())
    {
        cfg.id = GenerateId();
    }

    return cfg;
}

std::string SshConnectionConfig::GenerateId()
{
    static std::atomic<uint64_t> counter{0};
    uint64_t now = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    uint64_t mixed = Fnv1a64(std::to_string(now) + ":" +
                             std::to_string(counter.fetch_add(1)));
    return ToHex(mixed);
}

}  // namespace View
}  // namespace RocProfVis
