:orphan:

<head>
  <meta charset="UTF-8">
  <meta name="description" content="ROCm Optiq keyboard shortcuts reference">
  <meta name="keywords" content="keyboard shortcuts, hotkeys, navigation, ROCm, AMD">
</head>

# Keyboard shortcuts

ROCm Optiq provides comprehensive keyboard shortcuts for efficient timeline navigation and interaction. All shortcuts are customizable through the settings panel.

## Timeline navigation shortcuts

### Zoom controls

| Shortcut | Action | Description |
| -------- | ------ | ----------- |
| `+` or `=` | Zoom in | Increase zoom level by 25% |
| `-` | Zoom out | Decrease zoom level by 20% |
| `Ctrl+0` | Reset zoom | Fit entire trace in view |
| `Numpad +` | Zoom in | Same as `+` key |
| `Numpad -` | Zoom out | Same as `-` key |

**Usage**: Focus the timeline window, then press the zoom shortcut. The zoom centers on the current view.

### Pan controls

| Shortcut | Action | Description |
| -------- | ------ | ----------- |
| `←` Left Arrow | Pan left | Move view left by 10% |
| `→` Right Arrow | Pan right | Move view right by 10% |
| `Shift+←` | Pan left (fast) | Move view left by 25% |
| `Shift+→` | Pan right (fast) | Move view right by 25% |
| `Home` | Jump to start | Move to beginning of trace |
| `End` | Jump to end | Move to end of trace |

**Usage**: Focus the timeline window, then use arrow keys to pan. Hold `Shift` for faster panning.

### Vertical scrolling

| Shortcut | Action | Description |
| -------- | ------ | ----------- |
| `↑` Up Arrow | Scroll up | Scroll up 20 pixels |
| `↓` Down Arrow | Scroll down | Scroll down 20 pixels |
| `Ctrl+↑` | Scroll up (fast) | Scroll up 100 pixels |
| `Ctrl+↓` | Scroll down (fast) | Scroll down 100 pixels |
| `Page Up` | Page up | Scroll up one viewport height |
| `Page Down` | Page down | Scroll down one viewport height |

**Usage**: Focus the timeline window, then use arrow keys to scroll vertically through tracks.

### Selection controls

| Shortcut | Action | Description |
| -------- | ------ | ----------- |
| `Ctrl+A` | Select all | Select entire time range |
| `Escape` | Clear selection | Deselect current time range |
| `Ctrl+F` | Frame selection | Center selection in view without changing zoom |
| `Ctrl+Shift+Z` | Zoom to selection | Zoom to fit selected time range |

**Usage**:
- **Select all**: Selects from start to end of trace
- **Clear selection**: Removes any active time range selection
- **Frame selection**: Centers the selected time range in the viewport
- **Zoom to selection**: Adjusts zoom level to fit the selection exactly

## Context menu shortcuts

Right-click on the timeline to access the context menu, which includes:

- **Make Time Range Selection**: Create time range from selected events
- **Remove Time Range Selection**: Clear current time range
- **Add Annotation**: Add a sticky note at cursor position
- **Keyboard Shortcuts**: View complete shortcuts reference

## Viewing shortcut help

### In-application help

1. Right-click anywhere on the timeline
2. Navigate to **Keyboard Shortcuts** in the context menu
3. View organized list of all shortcuts by category

### Categories displayed

- **Zoom**: All zoom-related shortcuts
- **Pan**: Horizontal navigation shortcuts
- **Vertical Scroll**: Track scrolling shortcuts
- **Selection**: Time range selection shortcuts

## Customizing keyboard shortcuts

### Accessing shortcut settings

1. Go to **Settings** → **Keyboard Shortcuts**
2. Browse shortcuts organized by category
3. Click on a shortcut to rebind
4. Press new key combination
5. Click **OK** to save

### Modifiable settings

Each shortcut can be customized with:

- **Primary key**: Main key for the shortcut
- **Modifier keys**: Ctrl, Shift, Alt requirements
- **Enabled/Disabled**: Toggle individual shortcuts

### Available keys

Supported keys for rebinding:

- **Letter keys**: A-Z
- **Number keys**: 0-9, Numpad 0-9
- **Function keys**: F1-F12
- **Navigation keys**: Arrows, Home, End, Page Up/Down
- **Special keys**: Space, Enter, Escape, Tab
- **Symbol keys**: +, -, =, [, ], etc.

### Modifier combinations

Modifiers can be combined:

- **Ctrl**: Control key
- **Shift**: Shift key
- **Alt**: Alt key
- **Ctrl+Shift**: Both modifiers required
- **Ctrl+Alt**: Both modifiers required
- **Shift+Alt**: Both modifiers required
- **Ctrl+Shift+Alt**: All three modifiers required

## Best practices

### Efficient navigation workflow

1. **Quick zoom**: Use `+`/`-` for rapid zoom adjustments
2. **Precise positioning**: Use `Home`/`End` to jump to trace boundaries
3. **Fast panning**: Hold `Shift` with arrows for quick horizontal movement
4. **Page navigation**: Use `Page Up`/`Page Down` for vertical track browsing

### Selection workflow

1. **Select region of interest**: Click and drag on timeline
2. **Zoom to selection**: Press `Ctrl+Shift+Z` to focus on selection
3. **Fine-tune view**: Use `+`/`-` to adjust zoom
4. **Clear when done**: Press `Escape` to clear selection

### Combining shortcuts

Shortcuts can be used in sequence for complex operations:

**Example 1: Focus on trace hotspot**
```
1. Press Ctrl+A (select all)
2. Navigate to hotspot with arrows
3. Click and drag to select hotspot region
4. Press Ctrl+Shift+Z (zoom to selection)
```

**Example 2: Navigate large traces**
```
1. Press Home (jump to start)
2. Use Page Down to scroll through tracks
3. Press → repeatedly or Shift+→ to advance
4. Press End when done to see trace end
```

## Accessibility features

### High contrast mode

For improved visibility, enable High Contrast color scheme:

1. Go to **Settings** → **Display** → **Color Scheme**
2. Select **High Contrast**
3. Keyboard shortcuts remain unchanged

### Large fonts

Increase font size for better readability:

1. Go to **Settings** → **Display**
2. Disable **DPI-based Font Scaling** (if enabled)
3. Increase **Font Size** using `+` button
4. Preview shows real-time changes

### Sticky keys compatibility

ROCm Optiq supports Windows Sticky Keys for accessibility:

- Single modifier keys work when Sticky Keys is enabled
- Sequential key presses supported
- No conflicts with existing shortcuts

## Troubleshooting

### Shortcuts not working

**Symptoms**: Keyboard shortcuts don't respond

**Solutions**:

1. **Check window focus**: Click on timeline to focus window
2. **Verify no modal dialogs**: Close any open dialogs or popups
3. **Check for conflicts**: Ensure no other applications are capturing hotkeys
4. **Reset to defaults**: Restore default shortcuts in Settings

### Conflicts with system shortcuts

**Symptoms**: Shortcut triggers system action instead of ROCm Optiq

**Solutions**:

1. **Rebind conflicting shortcuts**: Change ROCm Optiq shortcuts to avoid conflicts
2. **Disable system hotkeys**: Modify system keyboard settings (OS-specific)
3. **Use alternative combinations**: Choose different modifier combinations

### Modifier keys stuck

**Symptoms**: Ctrl/Shift/Alt appears stuck after using shortcuts

**Solutions**:

1. **Press and release modifiers**: Tap Ctrl, Shift, and Alt individually
2. **Click in window**: Click anywhere in the timeline to reset state
3. **Restart application**: Close and reopen ROCm Optiq

## Platform-specific notes

### Windows

- Uses `Ctrl` for shortcuts (standard Windows convention)
- `Alt` key may trigger menu bar focus
- Function keys (F1-F12) available for custom shortcuts

### Linux

- Uses `Ctrl` for shortcuts (standard Linux convention)
- Window manager may intercept some shortcuts (e.g., `Ctrl+Alt+Arrow`)
- Check desktop environment shortcuts for conflicts

### macOS

- Uses `Cmd` (Command) instead of `Ctrl` for shortcuts
- `Ctrl` key still works as modifier for compatibility
- `Option` (Alt) key supported for combinations

## Quick reference card

### Essential shortcuts

| Action | Shortcut |
| ------ | -------- |
| Zoom in | `+` |
| Zoom out | `-` |
| Pan left | `←` |
| Pan right | `→` |
| Jump to start | `Home` |
| Jump to end | `End` |
| Select all | `Ctrl+A` |
| Clear selection | `Escape` |
| Zoom to selection | `Ctrl+Shift+Z` |

### Print-friendly reference

To print this reference:

1. Right-click on timeline
2. Select **Keyboard Shortcuts** from context menu
3. Take screenshot or note shortcuts
4. Or print this documentation page

## Advanced usage

### Macro-like sequences

Create efficient workflows by chaining shortcuts:

**Inspect kernel execution:**
```
1. Ctrl+A (select all)
2. Find kernel in event table
3. Click kernel event
4. Ctrl+Shift+Z (zoom to kernel duration)
5. ↑/↓ to examine surrounding events
```

**Compare two regions:**
```
1. Select first region with mouse
2. Note timestamps from details panel
3. Escape (clear selection)
4. Select second region
5. Compare timestamps manually
```

### Speed techniques

**Quick zoom to region:**
- Click and drag to select region
- Press `Ctrl+Shift+Z` immediately
- Result: Instant zoom to selection

**Rapid trace scanning:**
- Press `Home`
- Hold `Shift+→` to scan quickly
- Release when hotspot appears
- Press `-` to zoom out for context

**Vertical track navigation:**
- Focus on track of interest
- Use `Page Down` to scroll quickly
- Press `Ctrl+↓` for fine adjustments

## Related topics

- [Profiling workflow](profiling-workflow.md)
- [Timeline view](timeline.md) (if exists)
- [Settings and customization](settings.md) (if exists)

---

*To report issues with keyboard shortcuts, visit [GitHub Issues](https://github.com/ROCm/roc-optiq/issues)*
