#pragma once

#include <string>
#include <algorithm>
#include <cctype>

namespace util::string
{
    // ---- trim (in-place) ----

    inline void ltrim(std::string& s)
    {
        s.erase(s.begin(),
            std::find_if(s.begin(), s.end(),
                [](unsigned char ch) { return !std::isspace(ch); }));
    }

    inline void rtrim(std::string& s)
    {
        s.erase(
            std::find_if(s.rbegin(), s.rend(),
                [](unsigned char ch) { return !std::isspace(ch); }).base(),
            s.end());
    }

    inline void trim(std::string& s)
    {
        ltrim(s);
        rtrim(s);
    }

    // ---- trim (copy) ----

    inline std::string ltrim_copy(std::string s)
    {
        ltrim(s);
        return s;
    }

    inline std::string rtrim_copy(std::string s)
    {
        rtrim(s);
        return s;
    }

    inline std::string trim_copy(std::string s)
    {
        trim(s);
        return s;
    }

    // ---- helpers ----

    inline bool starts_with(const std::string& s, const std::string& prefix)
    {
        return s.size() >= prefix.size() &&
            std::equal(prefix.begin(), prefix.end(), s.begin());
    }

    inline bool ends_with(const std::string& s, const std::string& suffix)
    {
        return s.size() >= suffix.size() &&
            std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
    }

    inline bool contains(const std::string& s, const std::string& sub)
    {
        return s.find(sub) != std::string::npos;
    }

    // ---- case conversion ----

    inline void to_lower(std::string& s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return std::tolower(c); });
    }

    inline void to_upper(std::string& s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return std::toupper(c); });
    }

    inline std::string to_lower_copy(std::string s)
    {
        to_lower(s);
        return s;
    }

    inline std::string to_upper_copy(std::string s)
    {
        to_upper(s);
        return s;
    }

} // namespace util::string