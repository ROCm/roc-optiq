// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_summary_cache.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED
#include <profiler-hub/c/profiler_hub.h>
#endif

namespace RocProfVis
{
namespace DataModel
{
namespace
{

constexpr char     kSummaryMagic[4]          = {'P', 'H', 'U', '1'};
constexpr uint32_t kSummaryVersion           = 1;
constexpr uint32_t kSummaryQueryLimit        = 16u * 1024u * 1024u;
constexpr uint32_t kSummaryNameLimit         = 1024u * 1024u;
constexpr uint32_t kSummaryRowLimit          = 1000000u;
constexpr uint32_t kSummaryHeaderBytes       = 8u;

std::mutex g_summary_cache_mutex;

bool
ReadExact(std::istream& input, void* data, uint32_t size)
{
    input.read(static_cast<char*>(data), size);
    return input.gcount() == static_cast<std::streamsize>(size);
}

bool
ReadU32(std::istream& input, uint32_t& value)
{
    return ReadExact(input, &value, sizeof(value));
}

bool
ReadU64(std::istream& input, uint64_t& value)
{
    return ReadExact(input, &value, sizeof(value));
}

bool
ReadString(std::istream& input, std::string& value, uint32_t limit)
{
    uint32_t size = 0;
    if(!ReadU32(input, size) || size > limit)
    {
        return false;
    }
    value.resize(size);
    if(size == 0)
    {
        return true;
    }
    return ReadExact(input, value.data(), size);
}

void
WriteU32(std::ostream& output, uint32_t value)
{
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void
WriteU64(std::ostream& output, uint64_t value)
{
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void
WriteString(std::ostream& output, const std::string& value)
{
    WriteU32(output, static_cast<uint32_t>(value.size()));
    if(!value.empty())
    {
        output.write(value.data(), static_cast<std::streamsize>(value.size()));
    }
}

bool
ReadHeader(std::istream& input)
{
    char     magic[4] = {};
    uint32_t version  = 0;
    if(!ReadExact(input, magic, sizeof(magic)) || !ReadU32(input, version))
    {
        return false;
    }
    return std::memcmp(magic, kSummaryMagic, sizeof(magic)) == 0 && version == kSummaryVersion;
}

void
WriteHeader(std::ostream& output)
{
    output.write(kSummaryMagic, sizeof(kSummaryMagic));
    WriteU32(output, kSummaryVersion);
}

bool
ReadRows(std::istream& input, std::vector<rocprofvis_db_name_duration_t>& rows)
{
    uint32_t count = 0;
    if(!ReadU32(input, count) || count > kSummaryRowLimit)
    {
        return false;
    }
    rows.clear();
    rows.reserve(count);
    for(uint32_t index = 0; index < count; ++index)
    {
        rocprofvis_db_name_duration_t row;
        if(!ReadString(input, row.name, kSummaryNameLimit) || !ReadU64(input, row.count) ||
           !ReadU64(input, row.min_duration) || !ReadU64(input, row.max_duration) ||
           !ReadU64(input, row.total_duration))
        {
            rows.clear();
            return false;
        }
        rows.push_back(std::move(row));
    }
    return true;
}

void
WriteRows(std::ostream& output, const std::vector<rocprofvis_db_name_duration_t>& rows)
{
    WriteU32(output, static_cast<uint32_t>(rows.size()));
    for(const rocprofvis_db_name_duration_t& row : rows)
    {
        WriteString(output, row.name);
        WriteU64(output, row.count);
        WriteU64(output, row.min_duration);
        WriteU64(output, row.max_duration);
        WriteU64(output, row.total_duration);
    }
}

enum class ScanStatus
{
    Missing,
    Found,
    Truncated
};

// Walk records. On a match, rows holds that record and good_end is unused.
// On a short tail, good_end is the offset of the last complete record.
ScanStatus
ScanRecords(std::istream& input,
            const std::string& key,
            std::vector<rocprofvis_db_name_duration_t>& rows,
            std::streamoff& good_end)
{
    good_end = static_cast<std::streamoff>(kSummaryHeaderBytes);
    while(input)
    {
        const std::streamoff record_start = input.tellg();
        if(record_start < 0)
        {
            return ScanStatus::Truncated;
        }
        uint32_t key_size = 0;
        input.read(reinterpret_cast<char*>(&key_size), sizeof(key_size));
        const std::streamsize key_size_bytes = input.gcount();
        if(key_size_bytes == 0)
        {
            return ScanStatus::Missing;
        }
        if(key_size_bytes != static_cast<std::streamsize>(sizeof(key_size)) || key_size > kSummaryQueryLimit)
        {
            return ScanStatus::Truncated;
        }
        std::string record_key(key_size, '\0');
        if(key_size > 0 && !ReadExact(input, record_key.data(), key_size))
        {
            return ScanStatus::Truncated;
        }
        if(record_key == key)
        {
            return ReadRows(input, rows) ? ScanStatus::Found : ScanStatus::Truncated;
        }
        std::vector<rocprofvis_db_name_duration_t> skipped;
        if(!ReadRows(input, skipped))
        {
            return ScanStatus::Truncated;
        }
        good_end = input.tellg();
        if(good_end < 0)
        {
            return ScanStatus::Truncated;
        }
    }
    return ScanStatus::Missing;
}

std::filesystem::path
SummaryFile(const std::string& directory)
{
    return std::filesystem::path(directory) / "summary.bin";
}

}  // namespace

std::string
SummaryCacheDirectory(const std::string& config_path, const std::vector<std::string>& sources)
{
#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED
    if(config_path.empty() || sources.empty())
    {
        return std::string();
    }
    std::vector<const char*> paths;
    paths.reserve(sources.size());
    for(const std::string& source : sources)
    {
        paths.push_back(source.c_str());
    }
    uint32_t capacity = 0;
    if(ph_trace_cache_directory(config_path.c_str(), paths.data(), static_cast<uint32_t>(paths.size()),
                                nullptr, &capacity) != PH_RESULT_SUCCESS ||
       capacity == 0)
    {
        return std::string();
    }
    std::string directory(capacity, '\0');
    if(ph_trace_cache_directory(config_path.c_str(), paths.data(), static_cast<uint32_t>(paths.size()),
                                directory.data(), &capacity) != PH_RESULT_SUCCESS)
    {
        return std::string();
    }
    if(!directory.empty() && directory.back() == '\0')
    {
        directory.pop_back();
    }
    return directory;
#else
    (void) config_path;
    (void) sources;
    return std::string();
#endif
}

std::string
SummaryCacheKey(const rocprofvis_db_duration_summary_query_t& request)
{
    std::string key;
    if(request.db_instance != nullptr)
    {
        key = std::to_string(request.db_instance->GuidIndex());
    }
    key.push_back('\n');
    key.append(request.query);
    return key;
}

bool
SummaryCacheFind(const std::string& directory,
                 const std::string& key,
                 std::vector<rocprofvis_db_name_duration_t>& rows)
{
    rows.clear();
    if(directory.empty() || key.empty())
    {
        return false;
    }
    try
    {
        std::lock_guard<std::mutex> lock(g_summary_cache_mutex);
        std::ifstream               input(SummaryFile(directory), std::ios::binary);
        if(!input || !ReadHeader(input))
        {
            return false;
        }
        std::streamoff good_end = 0;
        return ScanRecords(input, key, rows, good_end) == ScanStatus::Found;
    }
    catch(const std::exception&)
    {
        rows.clear();
        return false;
    }
}

void
SummaryCacheStore(const std::string& directory,
                  const std::string& key,
                  const std::vector<rocprofvis_db_name_duration_t>& rows)
{
    if(directory.empty() || key.empty() || key.size() > kSummaryQueryLimit || rows.size() > kSummaryRowLimit)
    {
        return;
    }
    try
    {
        std::lock_guard<std::mutex> lock(g_summary_cache_mutex);
        const std::filesystem::path file_path = SummaryFile(directory);
        std::filesystem::create_directories(directory);

        std::fstream file(file_path, std::ios::in | std::ios::out | std::ios::binary);
        if(!file)
        {
            file.open(file_path, std::ios::out | std::ios::binary | std::ios::trunc);
            if(!file)
            {
                return;
            }
            WriteHeader(file);
            WriteString(file, key);
            WriteRows(file, rows);
            return;
        }

        if(!ReadHeader(file))
        {
            file.close();
            file.open(file_path, std::ios::out | std::ios::binary | std::ios::trunc);
            if(!file)
            {
                return;
            }
            WriteHeader(file);
            WriteString(file, key);
            WriteRows(file, rows);
            return;
        }

        std::vector<rocprofvis_db_name_duration_t> existing;
        std::streamoff                            good_end = 0;
        const ScanStatus                          status   = ScanRecords(file, key, existing, good_end);
        if(status == ScanStatus::Found)
        {
            return;
        }
        if(status == ScanStatus::Truncated)
        {
            file.close();
            std::filesystem::resize_file(file_path, static_cast<std::uintmax_t>(good_end));
            file.open(file_path, std::ios::in | std::ios::out | std::ios::binary);
            if(!file)
            {
                return;
            }
        }
        file.clear();
        file.seekp(0, std::ios::end);
        WriteString(file, key);
        WriteRows(file, rows);
    }
    catch(const std::exception&)
    {
    }
}

void
NameDurationsFromGroups(const std::unordered_map<std::string, rocprofvis_db_name_duration_t>& groups,
                        std::vector<rocprofvis_db_name_duration_t>& rows)
{
    rows.clear();
    rows.reserve(groups.size());
    for(const std::pair<const std::string, rocprofvis_db_name_duration_t>& entry : groups)
    {
        rocprofvis_db_name_duration_t row = entry.second;
        row.name                          = entry.first;
        rows.push_back(std::move(row));
    }
}

void
MergeNameDurations(std::unordered_map<std::string, rocprofvis_db_name_duration_t>& groups,
                   const std::vector<rocprofvis_db_name_duration_t>& rows)
{
    for(const rocprofvis_db_name_duration_t& row : rows)
    {
        const std::unordered_map<std::string, rocprofvis_db_name_duration_t>::iterator found =
            groups.find(row.name);
        if(found == groups.end())
        {
            groups.emplace(row.name, row);
            continue;
        }
        rocprofvis_db_name_duration_t& group = found->second;
        group.count += row.count;
        group.total_duration += row.total_duration;
        if(row.min_duration < group.min_duration)
        {
            group.min_duration = row.min_duration;
        }
        if(row.max_duration > group.max_duration)
        {
            group.max_duration = row.max_duration;
        }
    }
}

}  // namespace DataModel
}  // namespace RocProfVis
