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

// Shared launcher layout metrics (kept here so the Browse / Add action buttons
// and form label columns line up across the Target and Arguments sections).
inline constexpr float kLaunchActionButtonWidth   = 84.0f;
inline constexpr float kLaunchLabelColumnMinWidth = 105.0f;
inline constexpr float kLaunchTooltipWrapEm       = 25.0f;

// Width of a "Browse"/"Add" action button: fits the label, floored at the shared
// minimum so buttons (and the inputs beside them) line up at any UI scale.
float LaunchActionButtonWidth(const char* label);

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

// Advance width of a ToggleSwitch, for laying out controls beside it.
float ToggleSwitchWidth();

void HelpTip(const char* tooltip);

void HelpMarker(char const* env_var, char const* desc);

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

// A filled, rounded status badge with contrasting text - used for the run
// status (Running / Completed / Failed).
void StatusPill(const char* label, ImU32 bg_color);

// =============================================================================
// Target card rows
//
// Building blocks for IProfilerBackend::RenderTargetSection. Each backend lays
// out the rows its tool needs; the rows share one label column so the inputs
// and Browse buttons line up.
// =============================================================================

/**
 * Width of the Target card's label column: fits `widest_label`, floored at the
 * shared minimum.
 */
float TargetLabelColumnWidth(const char* widest_label);

// Everything one path row needs. Grouped rather than positional because most of
// these are strings, and a transposed pair would compile cleanly.
struct TargetPathRow
{
    // Row label, e.g. "Program".
    const char* label = "";
    // ImGui id suffix, unique within the card.
    const char* id = "";
    // Placeholder shown while the field is empty.
    const char* hint = "";
    // Title of the local file/path picker, passed to the browse handler.
    const char* browse_title = "";
    BrowseKind  kind = File;
    // Recently used values offered in a dropdown (local mode only); nullptr or
    // empty hides the dropdown.
    std::vector<std::string> const* recents = nullptr;
    // Tooltip of the dropdown button, e.g. "Recent programs".
    const char* recents_tooltip = "";
};

/**
 * Renders a labelled path input with a Browse button and an optional recents
 * dropdown (local mode only). Browse calls `on_browse`, and is disabled when
 * that is empty. Returns true if the path was edited or picked from the recents
 * this frame; a Browse pick lands later and is reported by the handler.
 */
bool RenderTargetPathRow(TargetPathRow const& row, std::string& path, ConnectionType connection,
                         TargetBrowseFn const& on_browse, float label_w);

/**
 * Renders the free-form target arguments input, passed to the program after
 * word-splitting (see SplitArguments). Returns true if it was modified.
 */
bool RenderTargetArgumentsRow(std::string& arguments, float label_w);

/**
 * Renders the "open the trace when profiling finishes" checkbox. Returns true
 * if it was toggled. When not enabled it is drawn disabled and unchecked,
 * auto_load_trace is left as it was, and disabled_reason (if any) is shown as
 * its tooltip.
 */
bool RenderTargetAutoLoadRow(bool& auto_load_trace, bool enabled = true,
                             char const* disabled_reason = nullptr);

/**
 * Renders the "where are the profiler tools" row: an optional directory that
 * replaces the default $ROCM_PATH/bin then $PATH search, for a ROCm install in a
 * non-standard location.
 *
 * Note this is a *directory*, never a path to an executable - the filename comes
 * from the controller's tool table, so nothing the user types here can name a
 * different program to run. In remote mode it is a directory on the remote host.
 * Browse calls `on_browse` (disabled when empty), matching RenderTargetPathRow.
 * `resolved_hint`, when non-empty, is shown beneath the field as the absolute
 * path this selection currently resolves to.
 * Returns true if the field was edited this frame.
 */
bool RenderToolLocationSection(std::string& tool_directory, ConnectionType connection,
                               std::string const& resolved_hint, TargetBrowseFn const& on_browse);

/**
 * Renders the "Python venv" row: an optional virtual environment the profiler
 * runs in, for tools that are Python scripts (rocprof-compute). Like the tools
 * folder this is a *directory*, never a command - the controller composes the
 * activation itself. In remote mode it is a directory on the remote host.
 * Browse calls `on_browse` (disabled when empty).
 * Returns true if the field was edited this frame.
 */
bool RenderPythonEnvironmentSection(std::string& python_environment, ConnectionType connection,
                                    TargetBrowseFn const& on_browse);

/**
 * A code panel - the Command Preview, the run output: a card holding a header
 * row (title or status at the left, actions at the right) above a monospaced
 * box that fills the rest of it. `size` follows BeginChild: (0, 0) fills the
 * remaining space, a negative height leaves that much room below. Always pair
 * Begin/End.
 */
void BeginCodePanel(const char* id, ImVec2 size);
void EndCodePanel();

/**
 * The monospaced, bordered box that fills the rest of a code panel. With
 * `follow_tail` it stays on the newest line while the user is scrolled to the
 * bottom.
 */
void RenderCodeBox(const char* id, std::string const& text, bool follow_tail);

/**
 * Continues the current line so that items totalling `width` end at its right
 * edge - a header's trailing actions.
 */
void SameLineRightAligned(float width);

/**
 * Widths of a button and of a labelled checkbox at the current style, for
 * laying out a row of them before drawing it.
 */
float ButtonWidth(const char* label);
float CheckboxWidth(const char* label);

/**
 * Renders the Command Preview panel's contents - its title with Copy at the
 * right, above the composed env block + full argv. Call inside a code panel.
 */
void RenderCommandPreview(std::string const& preview_text);

/**
 * Slim, trackless scrollbars for the launcher's scrolling panels. Push before
 * the BeginChild that scrolls and pop right after it: the scrollbar style is
 * read when the child begins.
 */
void PushSlimScrollbarStyle();
void PopSlimScrollbarStyle();

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
 * @param config_reset: set to true when "Reset Options to Defaults" reset the
 * backend's settings and `config` this frame, false otherwise.
 * Returns the name of a profile to load (empty if none selected).
 */
std::string RenderSavedProfileBar(
    LaunchPresetManager& preset_mgr,
    std::string const& profiler_id,
    std::string& current_preset_name,
    LaunchConfig& config,
    IProfilerBackend const* backend,
    AppWindow* app_window,
    bool& config_reset);

} // namespace View
} // namespace RocProfVis
