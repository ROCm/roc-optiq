// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "imgui.h"
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace RocProfVis
{
namespace View
{

class SettingsManager;
enum class Colors;

constexpr float PI = 3.14159265358979323846f;  // Define PI constant

void
RenderLoadingIndicatorDots(float dot_radius, int num_dots, float spacing,
                           ImU32 color, float speed);
ImVec2
MeasureLoadingIndicatorDots(float dot_radius, int num_dots, float spacing);

enum LoadingIndicatorCentering
{
    kCenterNone,
    kCenterHorizontal,
    kCenterVertical,
    kCenterBoth,
};

/**
 * @brief Render a loading indicator with animated dots.
 *
 * @param color The color of the dots (including alpha for transparency).
 * @param window_id Optional ID for an overlay child window to render the indicator in.
 * If null, the indicator will be rendered in the current window. The overlay window will
 * be sized to fill the parent window.
 * @param centering How to center the indicator within the window (horizontal, vertical,
 * both, or none).
 * @param dot_radius The radius of each dot in the indicator.
 * @param num_dots The number of dots in the indicator.
 * @param dot_spacing The spacing between each dot.
 * @param anim_speed The speed of the dot animation.
 */
void
RenderLoadingIndicator(ImU32 color, const char* window_id = nullptr,
                       LoadingIndicatorCentering centering = kCenterBoth,
                       float dot_radius = 5.0f, int num_dots = 3,
                       float dot_spacing = 5.0f, float anim_speed = 5.0f);

ImU32
ApplyAlpha(ImU32 color, float alpha);

ImVec4
ThemeColor(SettingsManager& settings, Colors color, float alpha = 1.0f);

ImVec2
GetResponsiveWindowSize(ImVec2 desired_size, ImVec2 min_size = ImVec2(0.0f, 0.0f),
                        float viewport_margin = 32.0f);

// Push combo frame fill so dropdowns stand out from text inputs. Pair with
// PopComboStyles.
void
PushComboStyles();

void
PopComboStyles();

bool
IconButton(const char* icon, ImFont* icon_font, ImVec2 size = ImVec2(0, 0),
           const char* tooltip = nullptr, bool frameless = true,
           ImVec2 frame_padding = ImVec2(0, 0), ImU32 bg_color = IM_COL32(0, 0, 0, 0),
           ImU32 bg_color_hover  = IM_COL32(0, 0, 0, 0),
           ImU32 bg_color_active = IM_COL32(0, 0, 0, 0), const char* id = nullptr);

bool
IsMouseReleasedWithDragCheck(ImGuiMouseButton button, float drag_threshold = 5.0f);

std::pair<bool, bool>
InputTextWithClear(const char* id, const char* hint, char* buf, size_t buf_size,
                   ImFont* icon_font, ImU32 bg_color, const ImGuiStyle& style,
                   float width = 0);

// std::string-backed ImGui text inputs using ImGuiInputTextFlags_CallbackResize,
// so callers do not need to manage fixed-size char buffers.
bool
InputTextString(const char* id, std::string& str, ImGuiInputTextFlags flags = 0);

bool
InputTextStringWithHint(const char* id, const char* hint, std::string& str,
                        ImGuiInputTextFlags flags = 0);

void
SetTooltipStyled(const char* fmt, ...);

void
BeginTooltipStyled();

bool
BeginItemTooltipStyled();

void
EndTooltipStyled();

enum Alignment
{
    Alignment_Left,
    Alignment_Center,
    Alignment_Right,
};

void
ElidedText(const char* text, float available_width, float tooltip_width = 0.0f,
           Alignment alignment                     = Alignment_Left,
           bool      imgui_AlignTextToFramePadding = false);

// Trims text to a single line fitting max_width (and max_chars), appending "..."
// when shortened. Uses the current font for measurement.
std::string
ElideWithEllipsis(const std::string& text, float max_width, size_t max_chars);

void
CenterNextTextItem(const char* text);

void
CenterNextItem(float width);

bool
XButton(const char* id = nullptr, const char* tool_tip_label = nullptr,
        SettingsManager* settings = nullptr);

void
SectionTitle(const char* text, bool large = true, SettingsManager* settings = nullptr);

void
VerticalSeparator(SettingsManager* settings = nullptr);

// -----------------------------------------------------------------------------
// Design-language "panel card" vocabulary
//
// The remote and profiler dialogs share a stacked-card look: rounded, subtly
// bordered panels grouped over tiered backgrounds, with dim field labels,
// accent-colored primary buttons, and icon-font glyphs tinted from the theme.
// These helpers are the single source of truth for that vocabulary so every
// dialog stays consistent - prefer them over hand-rolled BeginChild /
// PushStyleColor sequences.
// -----------------------------------------------------------------------------

// Shared corner radius for every stacked design-language panel.
inline constexpr float PANEL_CARD_ROUNDING = 10.0f;

// Inner padding for the panel-card tiers, shared so every remote/profiler
// dialog uses the same spacing. A standard grouping card is PANEL_CARD_PADDING
// (also BeginPanelCard's default); header/footer bands are a touch roomier, and
// cards holding wrapped prose get extra vertical breathing room.
inline constexpr ImVec2 PANEL_CARD_PADDING     = ImVec2(14.0f, 8.0f);
inline constexpr ImVec2 PANEL_HEADER_PADDING   = ImVec2(16.0f, 10.0f);
inline constexpr ImVec2 PANEL_BODY_PADDING     = ImVec2(14.0f, 10.0f);
inline constexpr ImVec2 PANEL_BACKDROP_PADDING = ImVec2(12.0f, 4.0f);

// Vertical gap between cards in a stack. Deliberately tighter than the default
// ItemSpacing so a column of panel cards reads as one continuous surface.
inline constexpr float PANEL_CARD_STACK_SPACING_Y = 4.0f;

// Metrics repeated across the stacked dialogs: the width they open at and the
// narrowest they shrink to, the width of footer buttons and field-label
// columns, and the width of a header's close-button column.
inline constexpr float DIALOG_DEFAULT_WIDTH      = 560.0f;
inline constexpr float DIALOG_MIN_WIDTH          = 480.0f;
inline constexpr float DIALOG_BUTTON_WIDTH       = 104.0f;
inline constexpr float DIALOG_LABEL_WIDTH        = 104.0f;
inline constexpr float DIALOG_CLOSE_COLUMN_WIDTH = 24.0f;

// Tooltip wrap width for HelpMarker, as a multiple of the current font size.
inline constexpr float HELP_MARKER_WRAP_EM = 25.0f;

// Which background tier a panel card paints. Header/footer bands use kFrame,
// inner grouping cards use kPanel, and the body backdrop uses kMain.
enum class PanelCardTone
{
    kFrame,
    kPanel,
    kMain,
};

// Begins a rounded, padded, optionally-bordered child that groups related
// controls - the building block of the remote/profiler dialog design language.
// Pass a stable id. Always pair with EndPanelCard.
void
BeginPanelCard(const char* id, PanelCardTone tone = PanelCardTone::kPanel,
               ImVec2 padding = PANEL_CARD_PADDING, bool bordered = true,
               SettingsManager* settings = nullptr);

void
EndPanelCard();

// Dim form-field label (Colors::kTextDim), optionally aligned to the frame
// padding so it lines up with inputs in table rows.
void
PanelFieldLabel(const char* text, bool align_to_frame = true,
                SettingsManager* settings = nullptr);

// Renders an icon-font glyph tinted with a theme color; advances the cursor
// like TextUnformatted.
void
PanelIcon(const char* glyph, Colors color, SettingsManager* settings = nullptr);

// As above, for callers that have already resolved a packed color (e.g. table
// rows that tint per entry type). The Colors overload delegates to this.
void
PanelIcon(const char* glyph, ImU32 color, SettingsManager* settings = nullptr);

// Accent-colored primary action button (accent fill + on-accent text). Returns
// true when clicked.
bool
AccentButton(const char* label, ImVec2 size = ImVec2(0.0f, 0.0f),
             SettingsManager* settings = nullptr);

// Dim "(?)" marker on the same line as the preceding item, with a wrapped
// tooltip on hover. When env_var is non-null it is shown above the description,
// separated by a rule. Shared by the profiler and rocprof-sys option tabs.
void
HelpMarker(const char* description, const char* env_var = nullptr);

float
TableRowHeight();

// Toast text shown after copying a single cell / a whole row to the clipboard.
inline constexpr std::string_view COPY_DATA_NOTIFICATION     = "Cell data was copied";
inline constexpr std::string_view COPY_ROW_DATA_NOTIFICATION = "Row data copied to clipboard";

// Similar to ImGui::TextUnformatted(), but allows copying the text to the clipboard
// via left-click or a context menu.
// If multiple instances with the same text can appear within the same frame,
// the caller must provide a unique, stable identifier to avoid ID collisions.
// For items created in loops, the loop index or iterator is typically sufficient.
bool
CopyableTextUnformatted(
    const char* text, std::string_view unique_id = "", std::string_view notification = "",
    bool one_click_copy = false, bool context_menu = false,
    std::function<void(const char* value_to_copy)> menu_func = nullptr);

// Renders a selectable menu item with an icon (from the icon font) followed by a text label.
// Returns true when clicked. Intended for use inside BeginPopup/BeginPopupContextItem blocks.
bool
IconMenuItem(const char* icon, const char* label, bool enabled = true);

// Opens a submenu entry with a leading icon (from the icon font) before the label.
// Returns true when the submenu is open; call ImGui::EndMenu() only when it returns true.
bool
IconBeginMenu(const char* icon, const char* label);

// Identifies the row/column of the cell whose right-click opened a table's copy
// context menu. Reusable by any table that wants the shared full-row right-click
// hit-box plus copy menu (see RenderRowHitbox / AddCopyRowCellMenuItems).
struct CellMenuTarget
{
    int row    = -1;
    int column = -1;
};

// Positions the cursor for a cell: column 0 shares the row with the hit-box via
// SameLine(), later columns move to their own column.
void
PositionCell(int col);

// Renders the transparent, full-row selectable that acts as the right-click
// hit-box for the empty areas of a table row. Cells drawn on top capture
// right-clicks that land directly on their text (see CaptureCellRightClick).
// On a right-click, records the row and hovered column into target and raises
// open. Returns whether the row is hovered (for double-click / highlight use).
bool
RenderRowHitbox(const char* hitbox_id, int row, int column_count,
                CellMenuTarget& target, bool& open);

// Records a right-click that landed on the cell content just submitted (the
// copyable text/button steals hover from the row hit-box over its own area).
// Call immediately after rendering a cell's content.
void
CaptureCellRightClick(int col, int row, CellMenuTarget& target, bool& open);

// Opens the shared cell context-menu popup with consistent styling. When true is
// returned, the caller fills in menu items and must call EndCellContextMenu().
bool
BeginCellContextMenu(const char* popup_id);

void
EndCellContextMenu();

// Adds the shared "Copy Row Data" / "Copy Cell Data" items for the given row,
// copying to the clipboard and raising the copy notification.
void
AddCopyRowCellMenuItems(const std::string* cells, int column_count, int column);

#ifdef ROCPROFVIS_ENABLE_INTERNAL_BANNER
void
DrawInternalBuildBanner(const char* text = "Internal Build");
#endif

}  // namespace View
}  // namespace RocProfVis
