// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_table_processor.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace DataModel
{

// <config>/<date>_<format>_<version>_<name>_<size>/summary.bin. The folder name
// matches the timeline and catalog caches, so deleting that folder removes the
// summary too. An empty directory
// means the cache is off.
std::string SummaryCacheDirectory(const std::string& config_path, const std::vector<std::string>& sources);

// Stable key for one sub-query. The GUID separates nodes that share SQL text.
std::string SummaryCacheKey(const rocprofvis_db_duration_summary_query_t& request);

// True when this query was stored, including a stored empty result.
bool SummaryCacheFind(const std::string& directory,
                      const std::string& key,
                      std::vector<rocprofvis_db_name_duration_t>& rows);

void SummaryCacheStore(const std::string& directory,
                       const std::string& key,
                       const std::vector<rocprofvis_db_name_duration_t>& rows);

void NameDurationsFromGroups(
    const std::unordered_map<std::string, rocprofvis_db_name_duration_t>& groups,
    std::vector<rocprofvis_db_name_duration_t>& rows);

void MergeNameDurations(std::unordered_map<std::string, rocprofvis_db_name_duration_t>& groups,
                        const std::vector<rocprofvis_db_name_duration_t>& rows);

}  // namespace DataModel
}  // namespace RocProfVis
