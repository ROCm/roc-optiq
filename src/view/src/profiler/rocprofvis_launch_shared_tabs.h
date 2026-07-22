// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_launch_config.h"
#include "rocprofvis_profiler_backend.h"
#include "rocprofvis_launch_preset_manager.h"
#include <string>
#include <vector>
#include <utility>

namespace RocProfVis
{
namespace View
{

class AppWindow;

/**
 * Renders the Target section: executable, arguments, working dir, output dir.
 * The connection-mode selector lives in the launcher dialog (ProfilerLauncher
 * Dialog::RenderRemoteSection), next to the dialog-owned SSH options; the mode
 * is passed in here so labels read "Remote ..." and local file/path pickers are
 * disabled when targeting a remote host.
 * Returns true if any field was modified.
 */
bool RenderTargetSection(TargetSpec& target, ConnectionType connection, AppWindow* app_window);

/**
 * Renders the Raw Env Vars tab: editable name/value table with add/remove rows.
 * Highlights entries that shadow curated env var names.
 */
void RenderRawEnvVarsTab(std::map<std::string, std::string>& extra_env,
                         std::vector<std::pair<std::string, std::string>> const& curated_env);

/**
 * Builds the displayed command line from cached execution inputs.
 */
std::string BuildCommandPreviewString(
    LaunchConfig const& config,
    std::string const& profiler_path,
    std::vector<std::pair<std::string, std::string>> const& env_vars,
    std::vector<std::string> const& argv);

/**
 * Renders the Command Preview panel showing the composed env block + full argv.
 */
void RenderCommandPreview(std::string const& preview_text);

/**
 * Semantic severity of the console status badge. This lets
 * local (profiler state) and remote (workflow phase) report a single,
 * consistent badge.
 */
enum class ConsoleStatusLevel
{
    kIdle,     // idle / cancelled
    kRunning,  // any in-progress phase
    kSuccess,  // completed
    kError,    // failed
};

/**
 * Renders the Output Console panel with status badge, auto-scroll, copy, and clear buttons.
 * state_label is the badge text (e.g. "Running", "Downloading", "Completed");
 * state_level selects the badge color from the theme palette. detail is an
 * optional phase description shown next to the badge (e.g. the download path);
 * pass an empty string to omit it.
 * Returns true if the user clicked "Clear".
 */
bool RenderOutputConsole(
    std::string const& output_text,
    std::string const& error_message,
    std::string const& state_label,
    ConsoleStatusLevel state_level,
    std::string const& detail,
    bool&              auto_scroll);

/**
 * Renders the "Saved Profile" bar (Optiq JSON presets):
 * dropdown, save, save-as, delete buttons.
 * Returns the name of a profile to load (empty if none selected).
 */
std::string RenderSavedProfileBar(
    LaunchPresetManager& preset_mgr,
    std::string const& profiler_id,
    std::string& current_preset_name,
    LaunchConfig& config,
    IProfilerBackend const* backend,
    AppWindow* app_window);

} // namespace View
} // namespace RocProfVis
