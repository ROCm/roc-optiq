// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <algorithm>
#include <cctype>

namespace RocProfVis
{
namespace Core
{
namespace String
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

    // ---- UTF-8 boundaries ----

    // Whether a cut at pos would split a UTF-8 character: the byte there
    // continues one rather than starting it.
    inline bool is_utf8_continuation(const std::string& s, size_t pos)
    {
        constexpr unsigned char UTF8_CONTINUATION_MASK = 0xC0;
        constexpr unsigned char UTF8_CONTINUATION_BITS = 0x80;
        return pos < s.size() &&
               (static_cast<unsigned char>(s[pos]) & UTF8_CONTINUATION_MASK) ==
                   UTF8_CONTINUATION_BITS;
    }

    // The nearest character boundary at or before pos, where s can be cut short.
    inline size_t utf8_floor(const std::string& s, size_t pos)
    {
        while(pos > 0 && is_utf8_continuation(s, pos))
        {
            --pos;
        }
        return pos;
    }

    // The nearest character boundary at or after pos, where a tail can start.
    inline size_t utf8_ceil(const std::string& s, size_t pos)
    {
        while(pos < s.size() && is_utf8_continuation(s, pos))
        {
            ++pos;
        }
        return pos;
    }

}  // namespace String
}  // namespace Core
}  // namespace RocProfVis