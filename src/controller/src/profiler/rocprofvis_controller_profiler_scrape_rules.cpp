// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_profiler_scrape_rules.h"

#include "spdlog/spdlog.h"

#include <cctype>
#include <iterator>
#include <utility>

namespace RocProfVis
{
namespace Controller
{
namespace ProfilerScrapeRules
{

namespace
{

struct RuleRow
{
    char const*  key;
    char const*  pattern;
    uint32_t     group;
    ScrapePolicy policy;
};

/*
 * Pattern style, enforced by review: anchors where possible, lazy and bounded
 * quantifiers, no nested quantifiers, and no alternation inside an unbounded
 * repeat. These run against untrusted tool output on three different
 * std::regex implementations, and a pattern that backtracks catastrophically is
 * a hang, not an exception.
 */

// rocprof-compute profile. The name is encoded in the output directory, so
// the directory is the whole artifact.
constexpr RuleRow kComputeCapture[] = {
    {kKeyWorkloadDir, R"(Output directory:\s*(\S.*?)\s*$)", 1, ScrapePolicy::kFirstMatch},
    // Progress is two values off one line, and rules are flat, so the pattern
    // appears twice with different groups. Compiling it twice is negligible.
    {kKeyRunIndex, R"(\[Run\s+(\d+)/(\d+)\])", 1, ScrapePolicy::kLastMatch},
    {kKeyRunTotal, R"(\[Run\s+(\d+)/(\d+)\])", 2, ScrapePolicy::kLastMatch},
};

// rocprof-compute analyze. "Created file:" carries a bare filename written
// relative to the stage's working directory; the engine resolves it.
constexpr RuleRow kComputeAnalyze[] = {
    {kKeyAnalysisDb, R"(Created file:\s*(\S.*?)\s*$)", 1, ScrapePolicy::kFirstMatch},
};

// rocprof-sys run and sample. Last match wins: the tool names the database
// again when it finalizes, and the earlier mentions can be other files.
// The second row is a bare-token sweep, reached only when the labelled form
// never matched, so a wording change degrades instead of failing.
constexpr RuleRow kRocprofSysTrace[] = {
    {kKeyTraceDb, R"((?:Database:|File:)\s*'?([^\s'"]+\.db)\b)", 1, ScrapePolicy::kLastMatch},
    {kKeyTraceDb, R"(([^\s'"]+\.db)\b)", 1, ScrapePolicy::kLastMatch},
};

struct RuleSet
{
    rocprofvis_profiler_tool_t      tool;
    rocprofvis_profiler_operation_t operation;
    // Lowest tool version these rules describe. The base row for a tool is
    // 0.0.0 and is what an unknown version falls back to.
    uint32_t                        min_major;
    uint32_t                        min_minor;
    uint32_t                        min_patch;
    char const*                     artifact_key;
    RuleRow const*                  rows;
    size_t                          row_count;
};

/*
 * When a tool changes its output, add a row with the version that changed it
 * rather than editing the row below: an older install on the same machine
 * still needs the old pattern.
 */
constexpr RuleSet kRuleSets[] = {
    {kRPVProfilerToolRocprofCompute, kRPVProfilerOperationCapture, 0, 0, 0, kKeyWorkloadDir,
     kComputeCapture, std::size(kComputeCapture)},
    {kRPVProfilerToolRocprofCompute, kRPVProfilerOperationAnalyze, 0, 0, 0, kKeyAnalysisDb,
     kComputeAnalyze, std::size(kComputeAnalyze)},
    {kRPVProfilerToolRocprofSysRun, kRPVProfilerOperationDefault, 0, 0, 0, kKeyTraceDb,
     kRocprofSysTrace, std::size(kRocprofSysTrace)},
    {kRPVProfilerToolRocprofSysSample, kRPVProfilerOperationDefault, 0, 0, 0, kKeyTraceDb,
     kRocprofSysTrace, std::size(kRocprofSysTrace)},
    // rocprof-sys-instrument writes an instrumented binary whose path is not
    // yet scraped, rocprof-sys-avail is a probe, and rocprofv3 is not wired up.
    // All three run for their exit code and are absent on purpose.
};

bool version_at_least(RuleSet const& set, uint32_t major, uint32_t minor, uint32_t patch)
{
    if (set.min_major != major)
    {
        return major > set.min_major;
    }
    if (set.min_minor != minor)
    {
        return minor > set.min_minor;
    }
    return patch >= set.min_patch;
}

bool newer_than(RuleSet const& candidate, RuleSet const& best)
{
    if (candidate.min_major != best.min_major)
    {
        return candidate.min_major > best.min_major;
    }
    if (candidate.min_minor != best.min_minor)
    {
        return candidate.min_minor > best.min_minor;
    }
    return candidate.min_patch > best.min_patch;
}

RuleSet const* find_rule_set(rocprofvis_profiler_tool_t      tool,
                             rocprofvis_profiler_operation_t operation,
                             std::string const&              version)
{
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t patch = 0;
    bool const versioned = ParseVersion(version, major, minor, patch);

    RuleSet const* best = nullptr;
    for (RuleSet const& set : kRuleSets)
    {
        if (set.tool != tool || set.operation != operation)
        {
            continue;
        }
        // Without a usable version only the base rules are safe to assume.
        if (versioned && !version_at_least(set, major, minor, patch))
        {
            continue;
        }
        if (!versioned && (set.min_major != 0 || set.min_minor != 0 || set.min_patch != 0))
        {
            continue;
        }
        if (best == nullptr || newer_than(set, *best))
        {
            best = &set;
        }
    }

    return best;
}

} // namespace

bool ParseVersion(std::string const& text, uint32_t& major, uint32_t& minor, uint32_t& patch)
{
    major = 0;
    minor = 0;
    patch = 0;

    size_t i = 0;
    if (text.empty() || !std::isdigit(static_cast<unsigned char>(text[0])))
    {
        return false;
    }

    uint32_t* const fields[] = {&major, &minor, &patch};
    for (size_t field = 0; field < 3; ++field)
    {
        if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i])))
        {
            break;
        }

        uint64_t value = 0;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])))
        {
            value = value * 10u + static_cast<uint64_t>(text[i] - '0');
            if (value > 0xffffffffull)
            {
                return false;
            }
            ++i;
        }
        *fields[field] = static_cast<uint32_t>(value);

        if (i < text.size() && text[i] == '.')
        {
            ++i;
        }
        else
        {
            break;
        }
    }

    return true;
}

void Apply(ProfilerStageSpec& stage, std::string& out_artifact_key)
{
    stage.scrape_rules.clear();
    out_artifact_key.clear();

    RuleSet const* set = find_rule_set(stage.tool, stage.operation, stage.tool_version);
    if (set == nullptr)
    {
        return;
    }

    stage.scrape_rules.reserve(set->row_count);
    for (size_t i = 0; i < set->row_count; ++i)
    {
        RuleRow const& row = set->rows[i];

        ProfilerScrapeRuleSpec spec;
        spec.key     = row.key;
        spec.pattern = row.pattern;
        spec.group   = row.group;
        spec.policy  = row.policy;
        stage.scrape_rules.push_back(std::move(spec));
    }

    if (set->artifact_key != nullptr)
    {
        out_artifact_key = set->artifact_key;
    }
}

void ApplyAll(std::vector<ProfilerStageSpec>& stages,
              std::vector<std::string>&       out_stage_artifact_keys,
              std::string&                    out_artifact_key)
{
    out_artifact_key.clear();
    out_stage_artifact_keys.assign(stages.size(), std::string());

    for (size_t i = 0; i < stages.size(); ++i)
    {
        std::string stage_artifact_key;
        Apply(stages[i], stage_artifact_key);
        out_stage_artifact_keys[i] = stage_artifact_key;
        if (!stage_artifact_key.empty())
        {
            out_artifact_key = stage_artifact_key;
        }
    }

    if (out_artifact_key.empty())
    {
        spdlog::debug("Profiler pipeline declares no artifact; nothing will be opened on success");
    }
}

} // namespace ProfilerScrapeRules
} // namespace Controller
} // namespace RocProfVis
