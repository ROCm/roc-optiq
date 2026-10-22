# Styling and theme

Where every color, size, and font comes from, and how to add one that is missing.

**Code:** `src/view/src/rocprofvis_settings_manager.{h,cpp}`, `rocprofvis_font_manager.{h,cpp}`, `widgets/rocprofvis_gui_helpers.h`, `widgets/rocprofvis_widget.h` (`PopUpStyle`), `icons/rocprovfis_icon_defines.h`

**Related:** [Widgets](UI.md#widgets) (controls that already apply the theme; prefer them to raw ImGui), [Settings and persistence](UI.md#settings-and-persistence) (the dark-mode and font-size preferences behind this), [Events](UI.md#events) (`kThemeChanged`, `kFontSizeChanged`).

## Rules

- No `IM_COL32`, `ImVec4` color, or hex value outside `rocprofvis_settings_manager.cpp`. No literal padding, spacing, rounding, border, or font size. Zero is fine.
- Read `GetDefaultStyle()` when you want the app's value. `ImGui::GetStyle()` includes whatever a parent pushed, and `GetDefaultIMGUIStyle()` is stock Dear ImGui.
- Size anything that holds text from the font: `GetTextLineHeight()`, `GetFrameHeight()`, `TableRowHeight()`, `GetEventLevelHeight()`.
- Pop what you push in the same scope. Prefer the paired helpers: `PopUpStyle`, `PushComboStyles` / `PopComboStyles`, `BeginPanelCard` / `EndPanelCard`.
- A widget that caches colors refreshes on `kThemeChanged`; one that caches measured text refreshes on `kFontSizeChanged`. Calling `GetColor()` each frame needs neither.

## Picking a default-style field

Choose by what you are spacing, not by the number you want. Derive variants from the field (`style.ItemSpacing.y * 0.5f`).

| Spacing | Field |
|---|---|
| Window or child edge to content | `WindowPadding` |
| Inside a button, input, combo, or tab | `FramePadding` |
| Between widgets | `ItemSpacing` |
| Inside one composite widget | `ItemInnerSpacing` |
| Table cells | `CellPadding` |
| Tree indentation | `IndentSpacing` |
| Control corners | `FrameRounding` |
| Child panels, popups | `ChildRounding`, `PopupRounding` |
| Windows and modals | `WindowRounding` |

## Colors, fonts, and form helpers

- Pick a `Colors` value by role, not hue: `kBg*`, `kText*`, `kAccent*`, `kButton*`, `kBorder*`, `kTable*`, timeline roles (`kSelection`, `kGridColor`, `kRuler*`, `kMeasurement*`), and feature palettes (`kComparison*`, `kMemChart*`, `kStickyNote*`, `kLog*`). Stock ImGui widgets are already themed by `ApplyColorStyling()`.
- Transparency: `ThemeColor(settings, Colors::k..., alpha)` or `ApplyAlpha(color, alpha)`. Per-series colors: `GetColorWheel()`. ImPlot colormaps: `GetFlameColormapName()`.
- Fonts: `FontType::kMainText`, `kIcon`, `kCode` (monospace). `FontSize::kXSmall` to `kLarge` are steps relative to the user's font-size setting, not point sizes. Push with `ImGui::PushFont(fonts.GetFont(FontType::kCode), 0.0f)`.
- Dialogs and settings-style forms: `BeginPanelCard(id, PanelCardTone::k...)` groups controls (`kFrame` for header and footer bands, `kPanel` for inner groups, `kMain` for the body), with `PanelFieldLabel`, `PanelIcon`, `AccentButton` for the primary action, `ColoredButton` for semantic actions, and `SectionTitle`. Wrap `BeginPopupModal` in a `PopUpStyle` and size it with `GetResponsiveWindowSize`.

## Recipes

**Add a color.**

1. Append a value to `enum class Colors` in `rocprofvis_settings_manager.h`, in its group and before `__kLastColor`.
2. Add an entry at the same position in both `DARK_THEME_COLORS` and `LIGHT_THEME_COLORS`, with a `// Colors::kName` comment like its neighbors. A missing entry fails to compile; a misplaced one silently shifts every later color.
3. Map it in `ApplyColorStyling()` only if stock ImGui widgets should use it.

**Add a size, spacing, or rounding value.** The first match wins:

1. An existing default-style field expresses it: use it or derive from it.
2. It should change app-wide: change it in `InitStyling()`.
3. Other components could want it: an `inline constexpr` in `rocprofvis_settings_manager.cpp` with a getter, like `GetEventLevelSpacing()`.
4. It is geometry private to one drawing: a named `inline constexpr` at the top of that `.cpp`.

**Add an icon.** Reuse an `ICON_*` define from `icons/rocprovfis_icon_defines.h` (the file-name typo is intentional). The icon font is the full Ionicons TTF, compressed into a generated header at build time, but only codepoints in `icon_ranges[]` are loaded: a new glyph needs its define and a range entry.
