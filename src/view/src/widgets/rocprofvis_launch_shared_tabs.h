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
 * Returns true if any field was modified.
 */
bool RenderTargetSection(TargetSpec& target, AppWindow* app_window);

/**
 * Renders the Connection section (local-only this phase; SSH fields disabled).
 */
void RenderConnectionSection(ConnectionSpec& connection);

/**
 * Renders the Raw Env Vars tab: editable name/value table with add/remove rows.
 * Highlights entries that shadow curated env var names.
 */
void RenderRawEnvVarsTab(std::map<std::string, std::string>& extra_env,
                         std::vector<std::pair<std::string, std::string>> const& curated_env);

/**
 * Renders the Command Preview panel showing the composed env block + full argv.
 */
void RenderCommandPreview(
    IProfilerBackend const* backend,
    LaunchConfig const& config,
    std::string const& profiler_path);

/**
 * Renders the Output Console panel with status badge, auto-scroll, and copy button.
 */
void RenderOutputConsole(
    std::string const& output_text,
    std::string const& error_message,
    int profiler_state,
    bool& auto_scroll);

/**
 * Renders the Preset bar: dropdown, save, save-as, delete, import, export buttons.
 * Returns the name of a preset to load (empty if none selected).
 */
std::string RenderPresetBar(
    LaunchPresetManager& preset_mgr,
    std::string const& profiler_id,
    std::string& current_preset_name,
    LaunchConfig& config,
    IProfilerBackend const* backend,
    AppWindow* app_window);

} // namespace View
} // namespace RocProfVis
