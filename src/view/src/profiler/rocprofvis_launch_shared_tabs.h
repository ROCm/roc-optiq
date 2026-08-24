// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_launch_config.h"
#include "rocprofvis_profiler_backend.h"
#include "rocprofvis_launch_preset_manager.h"
#include "imgui.h"
#include <functional>
#include <string>
#include <vector>
#include <utility>

namespace RocProfVis
{
namespace View
{

class AppWindow;

// =============================================================================
// Modern launcher UI primitives
//
// Small, self-contained widgets that give the launcher a contemporary,
// card-based look instead of the default flat ImGui form. They read all colors
// and fonts from SettingsManager so they track the active theme.
// =============================================================================

// A rounded, padded, subtly-bordered panel used to group related controls.
// Auto-sizes to its content height. Always pair Begin/End.
void BeginLaunchCard(const char* id);
void EndLaunchCard();

// Card title with a leading icon (from the icon font; pass nullptr for none),
// an accent bar, and an optional trailing "(?)" help tooltip. Call immediately
// after BeginLaunchCard.
void LaunchCardHeader(const char* icon, const char* title, const char* help = nullptr);

// Lightweight accent-colored group label (with an optional trailing "(?)" help
// tooltip) used to separate blocks inside a card.
void LaunchSubHeader(const char* text, const char* help = nullptr);

// iOS-style animated on/off switch. Returns true on the frame it is toggled.
// Draws the label (and keeps it clickable) to the right of the switch.
bool ToggleSwitch(const char* label, bool* value);

// A small rounded "tag": tinted background + accent border + accent text.
// Advances the cursor by the chip size (use SameLine to place several).
void Chip(const char* label, ImU32 accent_color);

// Result of interacting with an EditablePill.
enum class PillAction
{
    kNone,
    kEdit,    // the pill body was clicked (pick it up to edit)
    kRemove,  // the trailing "x" was clicked
};

// A removable/editable pill: a rounded tag with the label and a trailing "x".
// Clicking the x returns kRemove; clicking the body returns kEdit. Used for the
// command-line argument and environment-variable lists.
PillAction EditablePill(const char* label, ImU32 accent_color);

// Lays out a set of tags as wrapping accent chips, prefixed by a dim label.
// Used for the live "this run will..." configuration summary.
void RenderConfigChips(const char* lead_label, std::vector<std::string> const& tags);

// A filled, rounded status badge with contrasting text - used for the run
// status (Running / Completed / Failed).
void StatusPill(const char* label, ImU32 bg_color);

/**
 * Renders the Target section: executable, arguments, output dir. TargetSpec::
 * working_directory is deliberately not exposed here - it is honored at launch
 * but currently only settable through a saved profile.
 * The connection-mode selector lives in the launcher dialog (ProfilerLauncher
 * Dialog::RenderRemoteSection), next to the dialog-owned SSH options; the mode
 * is passed in here so labels read "Remote ...".
 *
 * In local mode the Browse buttons open the OS file/path pickers. In remote
 * (SSH) mode they instead invoke the supplied callbacks (which open the shared
 * remote file browser); when a remote callback is not supplied the corresponding
 * Browse button is disabled, preserving the prior behavior.
 * Returns true if any field was modified.
 */
bool RenderTargetSection(TargetSpec& target, ConnectionType connection, AppWindow* app_window,
                         const std::function<void()>& on_remote_browse_program = {},
                         const std::function<void()>& on_remote_browse_output  = {});

/**
 * Renders the "where are the profiler tools" row: an optional directory that
 * replaces the default $ROCM_PATH/bin then $PATH search, for a ROCm install in a
 * non-standard location.
 *
 * Note this is a *directory*, never a path to an executable - the filename comes
 * from the controller's tool table, so nothing the user types here can name a
 * different program to run. In remote mode it is a directory on the remote host,
 * so the local path picker does not apply and Browse is driven by the supplied
 * callback (disabled when none is given), matching RenderTargetSection.
 * `resolved_hint`, when non-empty, is shown beneath the field as the absolute
 * path this selection currently resolves to.
 * Returns true if the field was modified.
 */
bool RenderToolLocationSection(std::string& tool_directory, ConnectionType connection,
                               AppWindow* app_window, std::string const& resolved_hint,
                               const std::function<void()>& on_remote_browse_directory = {});

/**
 * Renders the command that will actually run, from the same inputs handed to
 * the launch: the resolved binary, the merged env block, and the complete argv
 * produced by IProfilerBackend::FlattenToExecution. For display only - tokens
 * are not shell-quoted.
 */
std::string BuildCommandPreviewString(
    std::string const& tool_path,
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
