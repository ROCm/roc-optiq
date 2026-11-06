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

// What a Browse button picks.
enum BrowseKind
{
    File,
    Directory
};

// Browse handler for a path field: opens the local picker (titled `title`) or,
// in SSH mode, the remote file browser in `kind` mode, and writes the chosen
// path into `field` when the pick completes (possibly frames later). Empty when
// browsing is not possible, which disables the Browse button.
using TargetBrowseFn =
    std::function<void(std::string& field, BrowseKind kind, const char* title)>;

// One entry in the launcher's tool combo. The tool is identified by the
// controller's enum rather than a backend-local string id, so there is a single
// spelling of "which tool" from the combo through to argv[0] - and no per-backend
// id table to keep in step with it.
struct ToolOption
{
    rocprofvis_profiler_tool_t tool = kRPVProfilerToolNone;
    rocprofvis_profiler_operation_t operation = kRPVProfilerOperationDefault;
    std::string                display_name;
};

struct TabDescriptor
{
    std::string id;
    std::string display_name;
    // Returns true if the user changed a setting this frame
    std::function<bool()> render_fn;
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
     * The general options for the given tool, rendered in the launcher's
     * always-visible "Profiling Options" card. The tool is always one of
     * GetTools() - the launcher keeps LaunchConfig::tool in step with the
     * selected backend, so this does not need to handle a foreign or unset value.
     */
    virtual TabDescriptor GetOptions(rocprofvis_profiler_tool_t      tool,
                                     rocprofvis_profiler_operation_t operation) const = 0;

    /**
     * The power-user options for the given tool, one tab each in the Advanced
     * window, in display order. Empty when the tool has none.
     */
    virtual std::vector<TabDescriptor> GetAdvancedOptions(
        rocprofvis_profiler_tool_t      tool,
        rocprofvis_profiler_operation_t operation) const = 0;

    /**
     * Validate the config before launch. Returns empty string on success,
     * or a human-readable error message on failure.
     */
    virtual std::string Validate(LaunchConfig const& config) const = 0;

    /**
     * The target the given operation runs against. Each backend owns its own
     * target (persisted in its settings payload), so it is not shared between
     * profilers.
     */
    virtual TargetSpec const& GetTarget(rocprofvis_profiler_operation_t operation) const = 0;

    /**
     * The extra command-line arguments and environment variables the user added
     * for the given operation. Each backend owns its own (persisted in its
     * settings payload), so they are not shared between profilers; a backend
     * keeps one list for all its operations or one per operation. The launcher
     * edits them in place and merges the environment variables on top of
     * FlattenToExecution's.
     */
    virtual ExtraArguments& GetExtraArguments(rocprofvis_profiler_operation_t operation) = 0;

    /**
     * Render the launcher's Target card for the config's current tool and
     * operation, editing this backend's own target. Renders nothing by default.
     * @param config: read for the operation and connection mode.
     * @param on_browse: Browse handler for the path fields; empty disables Browse.
     * Returns true if any field was modified this frame. A Browse pick lands
     * later and is reported by the handler, not by this return value.
     */
    virtual bool RenderTargetSection(LaunchConfig const& config, TargetBrowseFn const& on_browse)
    {
        (void) config;
        (void) on_browse;
        return false;
    }

    /**
     * The recent target list for the given tool and operation, from
     * SettingsManager::GetRecentTargets. A backend chooses how finely its lists
     * are split - one for all its tools, or one per tool and operation - and the
     * Target card and the launch both read through this, so they always agree.
     */
    virtual std::vector<std::string>& GetRecentTargets(
        rocprofvis_profiler_tool_t tool, rocprofvis_profiler_operation_t operation) = 0;

    /**
     * Convert the curated settings into the env vars and the complete argument
     * list for the profiler process.
     *
     * argv_out receives every argument after argv[0] (the profiler binary), in
     * the exact order and form the process will see it: the backend's own flags,
     * its extra arguments, any output-path flag this profiler uses on the command
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
     * The caller merges this backend's extra environment variables on top of
     * env_out afterwards.
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
     * tool routing suggestions, deprecated aliases in the extra environment
     * variables, etc.).
     */
    virtual std::vector<WarningMessage> GetWarnings(LaunchConfig const& config) const
    {
        (void)config;
        return {};
    }

    /**
     * Whether this run is expected to have produced an output file, judged from
     * the profiler's captured stdout. False means there is nothing to load - not
     * a failure - so callers skip trace resolution, the remote download and the
     * cache directory rather than reporting the file missing. Defaults to true.
     */
    virtual bool HasOutputFile(rocprofvis_profiler_operation_t operation,
                               std::string const&              profiler_stdout) const
    {
        (void) operation;
        (void) profiler_stdout;
        return true;
    }

    /**
     * Deduce the produced trace file path from the profiler's captured stdout.
     * Each profiler reports its output location differently, so parsing rules
     * are backend-specific. Returns an empty string if the path cannot be
     * determined (callers should fall back to other discovery, e.g. scanning
     * the output directory). The returned path is whatever the profiler
     * reported, so it may be a remote path for SSH launches. Only meaningful
     * when HasOutputFile is true.
     */
    virtual std::string ParseTraceOutputPath(rocprofvis_profiler_operation_t operation,
                                             std::string const& profiler_stdout) const
    {
        (void) operation;
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
