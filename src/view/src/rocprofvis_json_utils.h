// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"

#include <map>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{
namespace JsonUtils
{

// Typed, defaulted readers. Each returns the default when the key is missing or
// the stored value cannot be coerced to the requested type. The non-const
// overloads exist because jt::Json's accessors are non-const; the const
// overloads operate on a copy-free const_cast internally.
bool        GetBool(const jt::Json& j, const std::string& key, bool def);
int32_t     GetInt(const jt::Json& j, const std::string& key, int32_t def);
double      GetDouble(const jt::Json& j, const std::string& key, double def);
std::string GetString(const jt::Json& j, const std::string& key, const std::string& def);

// map<string,bool> <-> JSON object helpers (used by checkbox-group settings).
jt::Json                    BoolMapToJson(const std::map<std::string, bool>& m);
std::map<std::string, bool> JsonToBoolMap(const jt::Json& j, const std::string& key,
                                          const std::map<std::string, bool>& defaults);

// Reads an array of strings at key (skips non-string entries). Returns empty if
// the key is missing or not an array.
std::vector<std::string> GetStringArray(const jt::Json& j, const std::string& key);

// File helpers: read+parse a JSON document, and write a JSON document. WriteFile
// creates parent directories as needed. ReadFile returns false (and leaves out
// untouched) on missing file or parse error.
bool ReadFile(const std::string& path, jt::Json& out);
bool WriteFile(const std::string& path, const jt::Json& json, bool pretty = true);

}  // namespace JsonUtils
}  // namespace View
}  // namespace RocProfVis
