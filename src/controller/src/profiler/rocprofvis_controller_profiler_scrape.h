// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"

#include <cstdint>
#include <map>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

// Skip matching on lines longer than this after ANSI strip. Compute prints
// multi-kilobyte demangled kernel names; libstdc++ std::regex can recurse
// deeply on those.
constexpr size_t kProfilerScrapeLineMaxBytes = 4096;

/*
 * How a rule chooses among matching lines. Not an ABI type: rules are authored
 * in the controller's rule table, never by a caller.
 */
enum class ScrapePolicy : uint32_t
{
    // Keep the first line that matches (e.g. compute's "Output directory:").
    kFirstMatch = 0,
    // Keep the last line that matches (e.g. rocprof-sys' database path).
    kLastMatch = 1,
};

struct ProfilerScrapeRuleSpec
{
    std::string  key;
    std::string  pattern;
    uint32_t     group  = 1;
    ScrapePolicy policy = ScrapePolicy::kFirstMatch;
};

struct ProfilerStageSpec
{
    std::string                                      label;
    rocprofvis_profiler_tool_t                       tool      = kRPVProfilerToolNone;
    rocprofvis_profiler_operation_t                  operation = kRPVProfilerOperationDefault;
    // Tool version as reported by the tool, e.g. "3.1.0". Empty selects the
    // base rules for the tool. Only ever read by the rule table.
    std::string                                      tool_version;
    std::string                                      tool_directory;
    std::vector<std::string>                         argv;
    std::string                                      working_directory;
    std::vector<std::pair<std::string, std::string>> env;
    // Filled by ProfilerScrapeRules::Apply at launch, not by the caller.
    std::vector<ProfilerScrapeRuleSpec>              scrape_rules;
    std::vector<std::pair<std::string, std::string>> expected;
    std::string                                      relocate_to;
};

struct ProfilerScrapeSlot
{
    rocprofvis_profiler_scrape_status_t status = kRPVProfilerScrapePending;
    std::string                         value;
    std::string                         expected;
    bool                                has_expected = false;
    bool                                has_match    = false;
    size_t                              winning_rule = static_cast<size_t>(-1);
};

/*
 * Slots are (stage, key), not key. Rules come from the tool table, so a
 * pipeline that runs the same tool twice - two capture runs feeding one
 * analyze - legitimately produces the same key in two stages, and each
 * occurrence is a separate value that {stageN.key} can name.
 */
using ProfilerScrapeSlotKey = std::pair<uint32_t, std::string>;

// CSI / color sequences so a trailing reset is not scraped into a path.
void strip_ansi(std::string& text);

bool is_absolute_path(std::string const& path);

// Resolve a scraped relative path against its stage working directory.
std::string resolve_scraped_path(std::string const& value, std::string const& working_directory);

/*
 * Per-line regex scrape for a pipeline. Patterns are compiled once at
 * Compile(). Matching is applied to newly completed lines only.
 *
 * Rules sharing a key within a stage are tried in list order: the earliest
 * rule that matches anything wins, and policy only breaks ties within that
 * rule. Lookups that name a key without a stage answer from the last stage
 * that declares it, which is what "the pipeline's artifact" means.
 */
class ProfilerScrapeEngine
{
public:
    rocprofvis_result_t Compile(std::vector<ProfilerStageSpec> const& stages);

    void BeginStage(uint32_t stage_index);
    void Feed(std::string const& chunk);
    void EndStage(std::string const& working_directory);
    void SkipRemainingFrom(uint32_t first_unstarted_stage);

    rocprofvis_result_t GetValue(std::string const& key, std::string& out) const;
    // Callers must treat an unknown key as an error before reading a status;
    // HasKey is the only way to tell "no such key" from "never matched".
    rocprofvis_profiler_scrape_status_t GetStatus(std::string const& key) const;
    bool                                HasKey(std::string const& key) const;

    bool                                GetStageValue(uint32_t           stage_index,
                                                      std::string const& key,
                                                      std::string&       out) const;
    rocprofvis_profiler_scrape_status_t GetStageStatus(uint32_t           stage_index,
                                                       std::string const& key) const;

    void SetValue(uint32_t stage_index, std::string const& key, std::string const& value);

    void SetDiagnosticSink(std::string* output_text);

    std::map<ProfilerScrapeSlotKey, ProfilerScrapeSlot> const& Slots() const { return m_slots; }

private:
    struct CompiledRule
    {
        std::string  key;
        uint32_t     group  = 1;
        ScrapePolicy policy = ScrapePolicy::kFirstMatch;
        std::regex   regex;
        bool         disabled     = false;
        bool         logged_error = false;
        size_t       index_in_key = 0;
    };

    void match_line(std::string const& raw_line);
    void inject_diagnostic(std::string const& text);
    // The last stage declaring `key`, or npos-equivalent via a null return.
    ProfilerScrapeSlot const* find_latest(std::string const& key) const;

    std::vector<std::vector<CompiledRule>>              m_rules;
    std::map<ProfilerScrapeSlotKey, ProfilerScrapeSlot> m_slots;
    std::string                                         m_remainder;
    uint32_t                                            m_current_stage   = 0;
    std::string*                                        m_diagnostic_sink = nullptr;
};

/*
 * Substitute {stageN.key} in each argv token. Returns InvalidArgument if a
 * well-formed placeholder cannot be resolved; error_message is then a
 * user-facing explanation derived from the key's scrape status.
 */
rocprofvis_result_t resolve_stage_placeholders(std::vector<std::string>&   argv,
                                               ProfilerScrapeEngine const& scrape,
                                               std::string&                error_message);

} // namespace Controller
} // namespace RocProfVis
