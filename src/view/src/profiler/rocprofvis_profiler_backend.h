// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_launch_config.h"
#include "json.h"
#include <string>
#include <vector>
#include <utility>
#include <functional>

namespace RocProfVis
{
namespace View
{

struct ToolOption
{
    std::string id;
    std::string display_name;
};

struct TabDescriptor
{
    std::string id;
    std::string display_name;
    std::function<void()> render_fn;
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

    virtual std::vector<ToolOption> GetTools() const = 0;
    virtual std::string GetDefaultBinary(std::string const& tool_id) const = 0;

    virtual std::vector<TabDescriptor> GetTabs(std::string const& tool_id) const = 0;

    /**
     * Validate the config before launch. Returns empty string on success,
     * or a human-readable error message on failure.
     */
    virtual std::string Validate(LaunchConfig const& config) const = 0;

    /**
     * Convert the curated settings into env vars and argv for the
     * profiler process. The caller merges extra_env on top afterwards.
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
