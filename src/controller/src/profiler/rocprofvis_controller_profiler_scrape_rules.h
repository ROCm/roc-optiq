// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_profiler_scrape.h"

#include <string>
#include <vector>

namespace RocProfVis
{
namespace Controller
{
namespace ProfilerScrapeRules
{

/*
 * The regexes that read artifact paths out of profiler output, keyed by tool,
 * operation, and tool version.
 *
 * These live here rather than being passed in through the C ABI because they
 * are a property of the tool, not of the caller: the same rocprof-compute
 * analyze run prints the same "Created file:" line no matter which UI launched
 * it, and a caller that had to supply the pattern would have to be updated in
 * lockstep with a tool it does not own. A View names what it wants to run -
 * tool, operation, version - and the controller decides how to read the
 * result.
 *
 * It also closes the injection question for good. A pattern can never arrive
 * from a preset, a project file, or a remote host, so no persisted or
 * corrupted input can steer matching into a pathological pattern; the only
 * patterns that exist are the constants below.
 *
 * Adding a tool means adding rows here and nothing else.
 */

/*
 * The scrape vocabulary. Keys are part of the controller's contract, not free
 * text: a caller passes one of these to get_scraped_value rather than
 * inventing a name that no rule will ever fill.
 */
inline constexpr char const* kKeyWorkloadDir = "workload_dir";
inline constexpr char const* kKeyAnalysisDb  = "analysis_db";
inline constexpr char const* kKeyTraceDb     = "trace_db";
inline constexpr char const* kKeyRunIndex    = "run_index";
inline constexpr char const* kKeyRunTotal    = "run_total";

/*
 * Fills stage.scrape_rules for `stage` from its tool, operation, and version,
 * discarding whatever was there. Also reports the key that names the stage's
 * artifact, empty when the stage produces no file worth opening.
 *
 * A tool with no rules is not an error: probing and instrumentation stages run
 * for their exit code and print nothing we read.
 */
void Apply(ProfilerStageSpec& stage, std::string& out_artifact_key);

/*
 * Applies rules to every stage. out_artifact_key is the artifact key of the
 * last stage that declares one, which is the file a pipeline is understood to
 * have produced; a config that sets its own artifact key overrides it.
 *
 * out_stage_artifact_keys is that same key per stage, parallel to `stages`.
 * Relocation needs it: a stage moves the file *it* produced, which in a
 * multi-stage pipeline is not the one the pipeline reports.
 */
void ApplyAll(std::vector<ProfilerStageSpec>& stages,
              std::vector<std::string>&       out_stage_artifact_keys,
              std::string&                    out_artifact_key);

/*
 * "3.1.0" -> 3, 1, 0. Leading numeric components only, so "4.0.0-rc1" and
 * "3.1" parse. Returns false for a string with no leading digit, which is how
 * an unparsable --version banner falls back to the base rules.
 */
bool ParseVersion(std::string const& text, uint32_t& major, uint32_t& minor, uint32_t& patch);

} // namespace ProfilerScrapeRules
} // namespace Controller
} // namespace RocProfVis
