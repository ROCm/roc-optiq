// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_assistant_url.h"

#include <cstddef>
#include <string>

namespace RocProfVis
{
namespace View
{

// The longest value an API version takes, e.g. "2025-01-01-preview". Shorter
// than any key, so one cannot be kept by naming it api-version.
constexpr size_t ASSISTANT_API_VERSION_MAX_CHARS = 24;

// Whether one "name=value" query parameter is an API version, the only kind
// that is saved: Azure-style endpoints need it, and it is not a secret.
static bool
IsApiVersionParameter(const std::string& parameter)
{
    const size_t equals = parameter.find('=');
    if(equals == std::string::npos)
    {
        return false;
    }
    std::string name = parameter.substr(0, equals);
    for(char& c : name)
    {
        if(c >= 'A' && c <= 'Z')
        {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    const size_t value_size = parameter.size() - equals - 1;
    if((name != "api-version" && name != "apiversion") || value_size == 0 ||
       value_size > ASSISTANT_API_VERSION_MAX_CHARS)
    {
        return false;
    }
    for(size_t i = equals + 1; i < parameter.size(); ++i)
    {
        const char c = parameter[i];
        if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
             c == '-' || c == '.' || c == '_'))
        {
            return false;
        }
    }
    return true;
}

std::string
AssistantStorableUrl(const std::string& url)
{
    // A fragment is never sent, so nothing from it on is worth keeping.
    std::string  kept        = url.substr(0, url.find('#'));
    std::string  query;
    const size_t query_start = kept.find('?');
    if(query_start != std::string::npos)
    {
        query = kept.substr(query_start + 1);
        kept.resize(query_start);
    }

    // Userinfo is everything in the authority up to its last '@'.
    const size_t scheme_end    = kept.find("://");
    const size_t authority     = (scheme_end == std::string::npos) ? 0 : scheme_end + 3;
    const size_t path_start    = kept.find('/', authority);
    const size_t authority_end = (path_start == std::string::npos) ? kept.size()
                                                                   : path_start;
    const size_t at = kept.rfind('@', authority_end);
    if(at != std::string::npos && at >= authority)
    {
        kept.erase(authority, at + 1 - authority);
    }

    // No list of key-like parameter names would be complete, so the query is
    // filtered the other way round: only what is known to be harmless stays.
    std::string kept_query;
    size_t      begin = 0;
    while(begin < query.size())
    {
        size_t end = query.find('&', begin);
        if(end == std::string::npos)
        {
            end = query.size();
        }
        const std::string parameter = query.substr(begin, end - begin);
        if(IsApiVersionParameter(parameter))
        {
            kept_query += kept_query.empty() ? '?' : '&';
            kept_query += parameter;
        }
        begin = end + 1;
    }
    return kept + kept_query;
}

}  // namespace View
}  // namespace RocProfVis
