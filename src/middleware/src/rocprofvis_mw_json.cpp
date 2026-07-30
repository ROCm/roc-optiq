// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_mw_json.h"

#include <cmath>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "json.h"

namespace RocProfVis
{
namespace Middleware
{
namespace Json
{

/*
 * jt::Json exposes only non-const accessors. Reads go through this so the
 * const_cast lives in exactly one place; none of the callers below mutate.
 */
static jt::Json*
FindMember(const jt::Json& object, const std::string& key)
{
    jt::Json* found = nullptr;
    if(object.isObject() && object.contains(key))
    {
        jt::Json&                            mutable_object = const_cast<jt::Json&>(object);
        std::map<std::string, jt::Json>&     members        = mutable_object.getObject();
        std::map<std::string, jt::Json>::iterator entry      = members.find(key);
        if(entry != members.end())
        {
            found = &entry->second;
        }
    }
    return found;
}

/*
 * Decimal-string to uint64. Returns false on any trailing garbage or overflow
 * so a bad id is rejected rather than silently clamped.
 */
static bool
ParseUInt(const std::string& text, uint64_t& out)
{
    bool parsed = false;
    if(!text.empty())
    {
        char*              end   = nullptr;
        unsigned long long value = std::strtoull(text.c_str(), &end, 10);
        if(end != nullptr && *end == '\0' && errno != ERANGE)
        {
            out    = static_cast<uint64_t>(value);
            parsed = true;
        }
    }
    return parsed;
}

jt::Json
MakeObject(void)
{
    jt::Json object;
    object.setObject();
    return object;
}

jt::Json
MakeArray(void)
{
    jt::Json array;
    array.setArray();
    return array;
}

jt::Json
MakeUInt(uint64_t value)
{
    jt::Json encoded;
    if(value <= MAX_EXACT_INTEGER)
    {
        encoded = jt::Json(static_cast<long long>(value));
    }
    else
    {
        encoded = jt::Json(std::to_string(value));
    }
    return encoded;
}

jt::Json
MakeDouble(double value)
{
    jt::Json encoded;
    if(std::isfinite(value))
    {
        encoded = jt::Json(value);
    }
    return encoded;
}

void
Append(jt::Json& array, jt::Json value)
{
    if(!array.isArray())
    {
        array.setArray();
    }
    array.getArray().push_back(std::move(value));
}

bool
Has(const jt::Json& object, const std::string& key)
{
    return object.isObject() && object.contains(key);
}

bool
GetBool(const jt::Json& object, const std::string& key, bool def)
{
    bool      value  = def;
    jt::Json* member = FindMember(object, key);
    if(member != nullptr)
    {
        if(member->isBool())
        {
            value = member->getBool();
        }
        else if(member->isNumber())
        {
            value = member->getNumber() != 0.0;
        }
    }
    return value;
}

uint64_t
GetUInt(const jt::Json& object, const std::string& key, uint64_t def)
{
    uint64_t  value  = def;
    jt::Json* member = FindMember(object, key);
    if(member != nullptr)
    {
        if(member->isLong())
        {
            long long stored = member->getLong();
            if(stored >= 0)
            {
                value = static_cast<uint64_t>(stored);
            }
        }
        else if(member->isNumber())
        {
            double stored = member->getNumber();
            if(stored >= 0.0)
            {
                value = static_cast<uint64_t>(stored);
            }
        }
        else if(member->isString())
        {
            uint64_t parsed = 0;
            if(ParseUInt(member->getString(), parsed))
            {
                value = parsed;
            }
        }
    }
    return value;
}

IdStatus
GetId(const jt::Json& object, const std::string& key, uint64_t& out)
{
    jt::Json* member = FindMember(object, key);
    if(member == nullptr)
    {
        return IdStatus::kMissing;
    }

    if(member->isString())
    {
        return ParseUInt(member->getString(), out) ? IdStatus::kValid
                                                   : IdStatus::kMalformed;
    }

    if(member->isLong())
    {
        long long stored = member->getLong();
        if(stored < 0)
        {
            return IdStatus::kMalformed;
        }
        out = static_cast<uint64_t>(stored);
        return (out > MAX_EXACT_INTEGER) ? IdStatus::kImprecise : IdStatus::kValid;
    }

    if(member->isNumber())
    {
        double stored = member->getNumber();
        if(stored < 0.0 || !std::isfinite(stored))
        {
            return IdStatus::kMalformed;
        }
        if(stored > static_cast<double>(MAX_EXACT_INTEGER))
        {
            return IdStatus::kImprecise;
        }
        out = static_cast<uint64_t>(stored);
        return IdStatus::kValid;
    }

    return IdStatus::kMalformed;
}

char const*
IdStatusToString(IdStatus status)
{
    switch(status)
    {
        case IdStatus::kValid: return "valid";
        case IdStatus::kMissing: return "is required";
        case IdStatus::kImprecise:
            return "is too large to be an exact JSON number; send it as a decimal "
                   "string, exactly as it was received";
        case IdStatus::kMalformed: return "is not a non-negative integer";
    }
    return "is not usable";
}

int64_t
GetInt(const jt::Json& object, const std::string& key, int64_t def)
{
    int64_t   value  = def;
    jt::Json* member = FindMember(object, key);
    if(member != nullptr)
    {
        if(member->isLong())
        {
            value = static_cast<int64_t>(member->getLong());
        }
        else if(member->isNumber())
        {
            value = static_cast<int64_t>(member->getNumber());
        }
    }
    return value;
}

double
GetDouble(const jt::Json& object, const std::string& key, double def)
{
    double    value  = def;
    jt::Json* member = FindMember(object, key);
    if(member != nullptr && member->isNumber())
    {
        value = member->getNumber();
    }
    return value;
}

std::string
GetString(const jt::Json& object, const std::string& key, const std::string& def)
{
    std::string value  = def;
    jt::Json*   member = FindMember(object, key);
    if(member != nullptr && member->isString())
    {
        value = member->getString();
    }
    return value;
}

std::vector<uint64_t>
GetUIntArray(const jt::Json& object, const std::string& key)
{
    std::vector<uint64_t> values;
    jt::Json*             member = FindMember(object, key);
    if(member != nullptr && member->isArray())
    {
        std::vector<jt::Json>& entries = member->getArray();
        values.reserve(entries.size());
        for(jt::Json& entry : entries)
        {
            if(entry.isLong())
            {
                long long stored = entry.getLong();
                if(stored >= 0)
                {
                    values.push_back(static_cast<uint64_t>(stored));
                }
            }
            else if(entry.isNumber())
            {
                double stored = entry.getNumber();
                if(stored >= 0.0)
                {
                    values.push_back(static_cast<uint64_t>(stored));
                }
            }
            else if(entry.isString())
            {
                uint64_t parsed = 0;
                if(ParseUInt(entry.getString(), parsed))
                {
                    values.push_back(parsed);
                }
            }
        }
    }
    return values;
}

std::vector<std::string>
GetStringArray(const jt::Json& object, const std::string& key)
{
    std::vector<std::string> values;
    jt::Json*                member = FindMember(object, key);
    if(member != nullptr && member->isArray())
    {
        std::vector<jt::Json>& entries = member->getArray();
        values.reserve(entries.size());
        for(jt::Json& entry : entries)
        {
            if(entry.isString())
            {
                values.push_back(entry.getString());
            }
        }
    }
    return values;
}

jt::Json
GetObject(const jt::Json& object, const std::string& key)
{
    jt::Json  nested = MakeObject();
    jt::Json* member = FindMember(object, key);
    if(member != nullptr && member->isObject())
    {
        nested = *member;
    }
    return nested;
}

jt::Json
GetValue(const jt::Json& object, const std::string& key)
{
    jt::Json  value;
    jt::Json* member = FindMember(object, key);
    if(member != nullptr)
    {
        value = *member;
    }
    return value;
}

bool
Parse(const std::string& text, jt::Json& out, std::string& error_message)
{
    bool                                parsed = false;
    std::pair<jt::Json::Status, jt::Json> result = jt::Json::parse(text);
    if(result.first != jt::Json::success)
    {
        error_message = jt::Json::StatusToString(result.first);
    }
    else if(!result.second.isObject())
    {
        error_message = "request must be a JSON object";
    }
    else
    {
        out    = std::move(result.second);
        parsed = true;
    }
    return parsed;
}

}  // namespace Json
}  // namespace Middleware
}  // namespace RocProfVis
