// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_json_utils.h"

#include "spdlog/spdlog.h"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace RocProfVis
{
namespace View
{
namespace JsonUtils
{

bool GetBool(const jt::Json& j, const std::string& key, bool def)
{
    jt::Json& mj = const_cast<jt::Json&>(j);
    if(!mj.contains(key))
    {
        return def;
    }
    jt::Json& v = mj[key];
    switch(v.getType())
    {
        case jt::Json::Bool:   return v.getBool();
        case jt::Json::String: return v.getString() == "true";
        case jt::Json::Long:   return v.getLong() != 0;
        default:               return def;
    }
}

int32_t GetInt(const jt::Json& j, const std::string& key, int32_t def)
{
    jt::Json& mj = const_cast<jt::Json&>(j);
    if(!mj.contains(key))
    {
        return def;
    }
    jt::Json& v = mj[key];
    switch(v.getType())
    {
        case jt::Json::Long:   return static_cast<int32_t>(v.getLong());
        case jt::Json::Double: return static_cast<int32_t>(v.getDouble());
        case jt::Json::Float:  return static_cast<int32_t>(v.getFloat());
        default:               return def;
    }
}

double GetDouble(const jt::Json& j, const std::string& key, double def)
{
    jt::Json& mj = const_cast<jt::Json&>(j);
    if(!mj.contains(key))
    {
        return def;
    }
    jt::Json& v = mj[key];
    switch(v.getType())
    {
        case jt::Json::Double: return v.getDouble();
        case jt::Json::Long:   return static_cast<double>(v.getLong());
        case jt::Json::Float:  return static_cast<double>(v.getFloat());
        default:               return def;
    }
}

std::string GetString(const jt::Json& j, const std::string& key, const std::string& def)
{
    jt::Json& mj = const_cast<jt::Json&>(j);
    if(!mj.contains(key))
    {
        return def;
    }
    jt::Json& v = mj[key];
    if(v.getType() == jt::Json::String)
    {
        return v.getString();
    }
    return def;
}

jt::Json BoolMapToJson(const std::map<std::string, bool>& m)
{
    jt::Json obj;
    obj.setObject();
    for(const auto& kv : m)
    {
        obj[kv.first] = kv.second;
    }
    return obj;
}

std::map<std::string, bool> JsonToBoolMap(const jt::Json& j, const std::string& key,
                                          const std::map<std::string, bool>& defaults)
{
    jt::Json& mj = const_cast<jt::Json&>(j);
    if(!mj.contains(key))
    {
        return defaults;
    }
    jt::Json& v = mj[key];
    if(v.getType() != jt::Json::Object)
    {
        return defaults;
    }

    std::map<std::string, bool> result;
    for(const auto& kv : v.getObject())
    {
        if(kv.second.getType() == jt::Json::Bool)
        {
            result[kv.first] = const_cast<jt::Json&>(kv.second).getBool();
        }
        else
        {
            result[kv.first] = false;
        }
    }
    return result;
}

std::vector<std::string> GetStringArray(const jt::Json& j, const std::string& key)
{
    std::vector<std::string> result;
    jt::Json& mj = const_cast<jt::Json&>(j);
    if(!mj.contains(key))
    {
        return result;
    }
    jt::Json& v = mj[key];
    if(!v.isArray())
    {
        return result;
    }
    for(jt::Json& entry : v.getArray())
    {
        if(entry.isString())
        {
            result.push_back(entry.getString());
        }
    }
    return result;
}

bool ReadFile(const std::string& path, jt::Json& out)
{
    std::filesystem::path full(path);
    if(!std::filesystem::exists(full))
    {
        return false;
    }

    std::ifstream in(full, std::ios::binary);
    if(!in)
    {
        return false;
    }

    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());

    auto parsed = jt::Json::parse(text);
    if(parsed.first != jt::Json::success)
    {
        spdlog::warn("Failed to parse JSON file '{}'", path);
        return false;
    }

    out = std::move(parsed.second);
    return true;
}

bool WriteFile(const std::string& path, const jt::Json& json, bool pretty)
{
    std::filesystem::path full(path);

    std::error_code ec;
    if(full.has_parent_path())
    {
        std::filesystem::create_directories(full.parent_path(), ec);
        if(ec)
        {
            spdlog::error("Failed to create directory '{}': {}",
                          full.parent_path().string(), ec.message());
            return false;
        }
    }

    std::ofstream out(full, std::ios::binary);
    if(!out)
    {
        spdlog::error("Failed to open JSON file for writing: {}", path);
        return false;
    }

    out << (pretty ? json.toStringPretty() : json.toString());
    return true;
}

}  // namespace JsonUtils
}  // namespace View
}  // namespace RocProfVis
