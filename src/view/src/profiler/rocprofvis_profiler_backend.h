// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_launch_config.h"
#include "rocprofvis_controller_enums.h"
#include "json.h"
#include <string>
#include <vector>
#include <utility>
#include <functional>

namespace RocProfVis
{
namespace View
{

// One entry in the launcher's tool combo. The tool is identified by the
// controller's enum rather than a backend-local string id, so there is a single
// spelling of "which tool" from the combo through to argv[0] - and no per-backend
// id table to keep in step with it.
struct ToolOption
{
    rocprofvis_profiler_tool_t tool = kRPVProfilerToolNone;
    std::string                display_name;
};

struct TabDescriptor
{
    std::string id;
    std::string display_name;
    // Returns true if the user changed a setting this frame
    std::function<bool()> render_fn;
    // false => shown in the always-visible "General Options" area of the
    // launcher; true => tucked under the collapsible "Advanced Options" section.
    bool advanced = false;
};

struct WarningMessage
{
    enum Level { kInfo, kWarning, kError };
    Level       level;
    std::string text;
};

/**
 * Interface for profiler-specific backends.
 * Each profiler type (rocprof-sys, rocprof-compute, rocprofv3) implements this.
 */
class IProfilerBackend
{
public:
    virtual ~IProfilerBackend() = default;

    virtual const char* Id() const = 0;
    virtual const char* DisplayName() const = 0;

    /**
     * The tools this backend offers, in combo order. The first is the default
     * for a fresh config and the fallback when a loaded profile names a tool this
     * backend does not offer.
     */
    virtual std::vector<ToolOption> GetTools() const = 0;

    /**
     * Tabs for the given tool, which is always one of GetTools() - the launcher
     * keeps LaunchConfig::tool in step with the selected backend, so this does
     * not need to handle a foreign or unset value.
     */
    virtual std::vector<TabDescriptor> GetTabs(rocprofvis_profiler_tool_t tool) const = 0;

    /**
     * Validate the config before launch. Returns empty string on success,
     * or a human-readable error message on failure.
     */
    virtual std::string Validate(LaunchConfig const& config) const = 0;

    /**
     * Convert the curated settings into the env vars and the complete argument
     * list for the profiler process.
     *
     * argv_out receives every argument after argv[0] (the profiler binary), in
     * the exact order and form the process will see it: the backend's own flags,
     * config.extra_argv, any output-path flag this profiler uses on the command
     * line (some tools take the path only as an environment variable, or not at
     * all), and the target executable plus its arguments (word-split with
     * SplitArguments) wherever this profiler expects them. Command-line shape
     * varies enough between profilers - and between tools of the same profiler -
     * that no part of it is synthesized for the backend downstream. What is
     * emitted here is what runs, and it is also what the command preview
     * renders, so the two cannot drift apart.
     *
     * Each entry becomes one argv entry verbatim; nothing is re-split on
     * whitespace or interpreted by a shell, so paths and arguments containing
     * spaces must be emitted as single entries rather than pre-quoted.
     *
     * The caller merges config.extra_env on top of env_out afterwards.
     */
    virtual void FlattenToExecution(
        LaunchConfig const& config,
        std::vector<std::pair<std::string, std::string>>& env_out,
        std::vector<std::string>& argv_out) const = 0;

    /**
     * Load backend-specific settings from a JSON payload (e.g. from a preset).
     * Called when the dialog opens, a preset is loaded, or the backend is switched.
     */
    virtual void LoadSettings(jt::Json const& payload) = 0;

    /**
     * Save current backend settings to JSON for persistence (presets, LaunchConfig).
     */
    virtual jt::Json SaveSettings() const = 0;

    /**
     * Export the current config to a profiler-native format string (e.g. .cfg).
     * Returns empty string if export is not supported by this backend.
     */
    virtual std::string ExportCfg() const = 0;

    /**
     * Return warnings and hints for the current config (soft conflicts,
     * tool routing suggestions, deprecated aliases in extra_env, etc.).
     */
    virtual std::vector<WarningMessage> GetWarnings(LaunchConfig const& config) const
    {
        (void)config;
        return {};
    }

    /**
     * Short, human-readable tags describing what this run will collect (e.g.
     * "Perfetto trace", "Sampling 300Hz", "AMD SMI"). Rendered as a live chip
     * summary in the launcher. Empty by default.
     */
    virtual std::vector<std::string> GetSummaryTags(LaunchConfig const& config) const
    {
        (void)config;
        return {};
    }

    /**
     * Deduce the produced trace file path from the profiler's captured stdout.
     * Each profiler reports its output location differently, so parsing rules
     * are backend-specific. Returns an empty string if the path cannot be
     * determined (callers should fall back to other discovery, e.g. scanning
     * the output directory). The returned path is whatever the profiler
     * reported, so it may be a remote path for SSH launches.
     */
    virtual std::string ParseTraceOutputPath(std::string const& profiler_stdout) const
    {
        (void)profiler_stdout;
        return {};
    }

    // TODO(launcher-phase4): Full CLI import/export API
    //
    // Future work beyond the current --preset integration:
    //
    // 1. ImportFromCommandLine(std::string const& cmdline)
    //    Parse a pasted "rocprof-sys-run --preset=balanced ..." command string
    //    into a LaunchConfig + backend settings (preset name, overrides, target).
    //
    // 2. ExportToCommandLine(LaunchConfig const&) -> std::string
    //    Produce a copy-pasteable shell command including all CLI flags
    //    (beyond just --preset and --output).
    //
    // 3. ExpandPresetIntoUI(std::string const& preset_name)
    //    Run "rocprof-sys-{tool} --explain=<name>" or parse preset JSON to
    //    populate RocprofSysSettings from the preset for power-user editing.
    //
    // 4. Full CLI flag mapping in command preview (e.g. --sampling-freq,
    //    --rocm-domains, --perfetto-backend) for parity with env-var emission.
};

} // namespace View
} // namespace RocProfVis
