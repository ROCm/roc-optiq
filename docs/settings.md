:orphan:

<head>
  <meta charset="UTF-8">
  <meta name="description" content="ROCm Optiq settings and customization guide">
  <meta name="keywords" content="settings, customization, preferences, ROCm, AMD, configuration">
</head>

# Settings and customization

ROCm Optiq provides extensive customization options through the Settings panel, allowing you to tailor the application to your workflow, accessibility needs, and visual preferences.

## Accessing settings

### Opening the settings panel

1. Click **Settings** in the menu bar
2. Select **Preferences** from the dropdown
3. The Settings dialog opens with multiple configuration tabs

**Alternative method:**
- Some platforms support keyboard shortcut `Ctrl+,` (Command+, on macOS)

### Settings organization

Settings are organized into logical categories:

- **Display**: Visual appearance, themes, fonts, colors
- **Units**: Time format, data size units
- **Keyboard Shortcuts**: Customize all navigation shortcuts
- **Other**: Profiling options, API keys, application behavior

## Display settings

### Color scheme selection

ROCm Optiq includes 6 professional color schemes optimized for different environments and accessibility needs.

#### Available schemes

| Scheme | Best For | Description |
| ------ | -------- | ----------- |
| **Default** | General use | Standard light or dark theme based on system settings |
| **High Contrast** | Accessibility | Maximum contrast for users with visual impairments |
| **Solarized** | Reduced eye strain | Popular low-contrast theme with warm/cool tones |
| **Nord** | Cool aesthetics | Arctic-inspired color palette with blues and grays |
| **Monokai** | Vibrant display | Warm, vibrant colors inspired by syntax highlighting |
| **Dracula** | Dark mode lovers | Modern, eye-friendly dark theme with subtle accents |

#### Changing color scheme

1. Go to **Settings** → **Display**
2. Find **Color Scheme** dropdown
3. Select your preferred scheme
4. Changes apply immediately (no restart required)
5. Click **OK** to save

**Example use cases:**
- **Presentations**: Use High Contrast for maximum visibility on projectors
- **Long sessions**: Use Solarized or Nord to reduce eye fatigue
- **Dark environments**: Use Dracula or Monokai for comfortable viewing
- **Bright environments**: Use Default light theme for outdoor/bright office settings

### Font settings

Control text rendering for optimal readability across different display types.

#### DPI-based font scaling

**Automatic scaling based on display DPI:**

- **Enabled** (default): Font size adjusts automatically to match display DPI
- **Disabled**: Manual font size control

**When to use automatic scaling:**
- Multiple monitors with different DPI
- High-DPI displays (4K, 5K, Retina)
- Mixed DPI environments (laptop + external monitor)

**When to disable:**
- Consistent font size needed across all displays
- Manual fine-tuning preferred
- Custom accessibility requirements

#### Manual font size adjustment

**Available when DPI-based scaling is disabled:**

1. Go to **Settings** → **Display**
2. Uncheck **DPI-based Font Scaling** (if enabled)
3. Use **Font Size** slider or input field
4. Preview shows real-time changes
5. Range: 8pt - 24pt (default: 13pt)

**Recommended sizes:**
- **Small** (10-11pt): High-DPI displays, maximize screen space
- **Medium** (13-14pt): Standard displays, default setting
- **Large** (16-18pt): Accessibility, presentations, low-DPI displays
- **Extra Large** (20-24pt): Severe visual impairment, distant viewing

### Theme selection

**Light vs. Dark mode:**

ROCm Optiq respects your system theme preference by default. Override in Display settings:

- **Light**: Light backgrounds, dark text (better for bright environments)
- **Dark**: Dark backgrounds, light text (reduces eye strain in low light)

**Note:** Color schemes work with both light and dark base themes.

## Unit settings

### Time format

Control how timestamps and durations are displayed throughout the application.

#### Available formats

| Format | Example | Best For |
| ------ | ------- | -------- |
| **Timecode** | `00:02:15.347` | Video editing, familiar HH:MM:SS format |
| **Condensed Timecode** | `02:15.347` | Shorter traces, saves horizontal space |
| **Seconds** | `135.347 s` | Scientific notation, decimal representation |
| **Milliseconds** | `135347 ms` | Most traces, good balance of precision |
| **Microseconds** | `135347000 µs` | High-precision profiling, kernel analysis |
| **Nanoseconds** | `135347000000 ns` | Maximum precision, hardware-level tracing |

#### Changing time format

1. Go to **Settings** → **Units**
2. Select **Time Format** dropdown
3. Choose format
4. All timeline displays update immediately

**Usage notes:**
- Format affects timeline axis, event tables, duration columns
- Does not affect underlying data precision (always nanosecond)
- Can change format during analysis without reloading trace

**Recommendations by trace duration:**
- **< 1 second**: Microseconds or Nanoseconds
- **1-60 seconds**: Milliseconds or Seconds
- **> 1 minute**: Timecode or Condensed Timecode

### Data size units

Control display of memory sizes, transfer sizes, and throughput metrics.

#### Available units

- **Automatic** (default): Dynamically choose appropriate unit (B, KB, MB, GB)
- **Bytes (B)**: Show all sizes in raw bytes
- **Kilobytes (KB)**: Force KB display
- **Megabytes (MB)**: Force MB display
- **Gigabytes (GB)**: Force GB display

**Binary vs. Decimal:**
- ROCm Optiq uses binary units (1 KB = 1024 bytes)
- Display reflects true memory addressing

## Keyboard shortcuts

### Shortcut customization

**All keyboard shortcuts are fully customizable** to accommodate different workflows, accessibility needs, and platform conventions.

#### Accessing shortcut settings

1. Go to **Settings** → **Keyboard Shortcuts**
2. Browse shortcuts organized by category:
   - **Zoom**: Timeline zoom controls
   - **Pan**: Horizontal navigation
   - **Vertical Scroll**: Track scrolling
   - **Selection**: Time range selection

#### Modifying a shortcut

1. Click on the shortcut you want to change
2. Press the new key combination
3. Modifiers (Ctrl, Shift, Alt) are automatically detected
4. Click **OK** to save
5. Click **Reset to Defaults** to restore original bindings

#### Shortcut conflicts

- System warns if shortcut conflicts with existing binding
- Choose to override or cancel
- Some system shortcuts cannot be overridden (e.g., Alt+F4 on Windows)

### Default shortcuts reference

#### Zoom controls

| Action | Default Shortcut | Customizable |
| ------ | ---------------- | ------------ |
| Zoom in | `+` or `=` | Yes |
| Zoom out | `-` | Yes |
| Reset zoom | `Ctrl+0` | Yes |
| Zoom in (numpad) | `Numpad +` | Yes |
| Zoom out (numpad) | `Numpad -` | Yes |

#### Pan controls

| Action | Default Shortcut | Customizable |
| ------ | ---------------- | ------------ |
| Pan left | `←` Left Arrow | Yes |
| Pan right | `→` Right Arrow | Yes |
| Pan left (fast) | `Shift+←` | Yes |
| Pan right (fast) | `Shift+→` | Yes |
| Jump to start | `Home` | Yes |
| Jump to end | `End` | Yes |

#### Vertical scroll

| Action | Default Shortcut | Customizable |
| ------ | ---------------- | ------------ |
| Scroll up | `↑` Up Arrow | Yes |
| Scroll down | `↓` Down Arrow | Yes |
| Scroll up (fast) | `Ctrl+↑` | Yes |
| Scroll down (fast) | `Ctrl+↓` | Yes |
| Page up | `Page Up` | Yes |
| Page down | `Page Down` | Yes |

#### Selection

| Action | Default Shortcut | Customizable |
| ------ | ---------------- | ------------ |
| Select all | `Ctrl+A` | Yes |
| Clear selection | `Escape` | Yes |
| Frame selection | `Ctrl+F` | Yes |
| Zoom to selection | `Ctrl+Shift+Z` | Yes |

**For complete keyboard shortcuts documentation, see [Keyboard Shortcuts](keyboard-shortcuts.md).**

### Creating custom workflows

**Example: Vim-style navigation**

Rebind shortcuts to match Vim keybindings:
- Pan left: `h`
- Pan right: `l`
- Scroll up: `k`
- Scroll down: `j`

**Example: Left-handed optimization**

Use numpad for primary navigation:
- Zoom in: `Numpad +`
- Zoom out: `Numpad -`
- Pan left: `Numpad 4`
- Pan right: `Numpad 6`

## Profiling options

### Auto-run AI analysis

**Global setting for automated AI analysis after profiling.**

#### Configuration

1. Go to **Settings** → **Other** → **Profiling options**
2. Toggle **Auto-run AI analysis after profiling**
3. When enabled:
   - All profiling sessions automatically run `rocpd analyze`
   - AI analysis JSON is generated and loaded
   - AI Analysis tab opens automatically with results
4. When disabled:
   - Only trace file is loaded
   - Manual AI analysis required

**Default:** Enabled

**Override per session:**
- Profiling dialog allows per-session override
- Checkbox in profiling workflow dialog
- Useful for quick profiling runs without analysis overhead

### LLM API keys

**Store API keys for cloud-based AI analysis providers.**

#### Anthropic API key

**For Claude-powered AI analysis:**

1. Go to **Settings** → **Other** → **AI Analysis API Keys**
2. Enter **Anthropic API Key**
3. Key is stored locally (encrypted on disk)
4. Auto-populated in profiling dialogs

**Getting an API key:**
1. Visit [console.anthropic.com](https://console.anthropic.com/)
2. Create account or sign in
3. Navigate to API Keys section
4. Generate new key
5. Copy and paste into ROCm Optiq settings

**Key format:**
```
sk-ant-api03-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

#### OpenAI API key

**For GPT-powered AI analysis:**

1. Go to **Settings** → **Other** → **AI Analysis API Keys**
2. Enter **OpenAI API Key**
3. Key is stored locally (encrypted on disk)
4. Auto-populated in profiling dialogs

**Getting an API key:**
1. Visit [platform.openai.com/api-keys](https://platform.openai.com/api-keys)
2. Create account or sign in
3. Click **Create new secret key**
4. Copy key immediately (only shown once)
5. Paste into ROCm Optiq settings

**Key format:**
```
sk-proj-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

#### Security and privacy

- **Local storage**: API keys stored in encrypted settings file
- **No transmission**: Keys only used for direct API calls to providers
- **User control**: Delete keys anytime in settings
- **Local alternative**: Use "Local (No LLM)" provider for offline analysis

**Settings file locations:**
- **Linux/macOS**: `~/.config/roc-optiq/settings.json`
- **Windows**: `%APPDATA%/roc-optiq/settings.json`

### Profiling tool defaults

**Set default profiling tool and arguments.**

1. Go to **Settings** → **Other** → **Profiling options**
2. Select **Default Profiling Tool**:
   - rocprofv3 (default)
   - rocprofiler-compute
   - rocprofiler-sys
3. Set **Default Tool Arguments**:
   - Example: `--sys-trace --kernel-trace --memory-copy-trace`
4. These defaults auto-populate in profiling dialogs

## Application behavior

### Exit and close confirmations

**Control confirmation dialogs for destructive actions.**

#### Don't ask before exiting application

- **Enabled**: Application closes immediately without confirmation
- **Disabled** (default): Confirmation dialog appears on exit
- Useful for: Power users, scripted workflows

#### Don't ask before closing tabs

- **Enabled**: Tabs close immediately without confirmation
- **Disabled** (default): Confirmation dialog appears when closing tabs with unsaved changes
- Useful for: Frequent tab switching, no project saving needed

**Note:** Unsaved project changes are still warned separately.

### Recent files

**Manage recent file history.**

#### Clear recent files list

1. Go to **Settings** → **Other**
2. Click **Clear Recent Files**
3. Confirmation dialog appears
4. Recent files list in Welcome tab is emptied

**Automatic management:**
- Recent files list limited to 10 most recent
- Oldest entries automatically removed
- Non-existent files pruned on application start

## Advanced settings

### GPU selection (multi-GPU systems)

**Choose which GPU to use for profiling visualization.**

1. Go to **Settings** → **Display** → **Advanced**
2. Select **GPU Device** from dropdown
3. List shows all Vulkan-compatible GPUs
4. Application restart required to apply changes

**When to change:**
- Multi-GPU systems with different capabilities
- Discrete GPU for better rendering performance
- Integrated GPU for power savings

### Logging and diagnostics

**Enable detailed logging for troubleshooting.**

1. Go to **Settings** → **Other** → **Advanced**
2. Set **Log Level**:
   - **Error**: Only critical errors (default)
   - **Warning**: Errors and warnings
   - **Info**: Informational messages
   - **Debug**: Detailed debugging information
   - **Trace**: Maximum verbosity (very detailed)

**Log file location:**
- **Linux/macOS**: `~/.local/share/roc-optiq/logs/`
- **Windows**: `%LOCALAPPDATA%/roc-optiq/logs/`

**Note:** Debug and Trace levels significantly increase log file size and may impact performance.

## Settings persistence

### Automatic saving

**Settings are automatically saved:**
- On clicking **OK** in Settings dialog
- On application exit
- After changing settings that apply immediately

**No manual save required.**

### Settings file format

Settings stored in JSON format for portability and manual editing if needed.

**Example settings file structure:**

```json
{
  "display": {
    "color_scheme": "Nord",
    "dpi_based_font_scaling": true,
    "font_size": 13,
    "theme": "dark"
  },
  "units": {
    "time_format": "milliseconds",
    "data_size_units": "auto"
  },
  "keyboard_shortcuts": {
    "zoom_in_key": "Equal",
    "zoom_in_ctrl": false,
    "zoom_out_key": "Minus",
    "reset_zoom_key": "0",
    "reset_zoom_ctrl": true
  },
  "profiling": {
    "auto_run_ai_analysis": true,
    "default_tool": "rocprofv3",
    "default_tool_args": "--sys-trace --kernel-trace"
  },
  "api_keys": {
    "anthropic": "encrypted_key_data",
    "openai": "encrypted_key_data"
  }
}
```

### Resetting to defaults

**Reset all settings to factory defaults:**

1. Close ROCm Optiq
2. Delete settings file:
   - Linux/macOS: `rm ~/.config/roc-optiq/settings.json`
   - Windows: Delete `%APPDATA%\roc-optiq\settings.json`
3. Restart ROCm Optiq
4. Default settings are recreated

**Warning:** This deletes all customizations including API keys.

### Exporting/importing settings

**Manual backup and transfer:**

1. Copy settings file to backup location
2. Transfer to another machine
3. Place in appropriate settings directory
4. Restart ROCm Optiq

**Use cases:**
- Sync settings across multiple workstations
- Backup before major updates
- Share team configurations

## Accessibility features

### High contrast mode

**For users with visual impairments:**

1. Set **Color Scheme** to **High Contrast**
2. Increase **Font Size** to 16pt or larger
3. Disable **DPI-based Font Scaling** for consistent sizing

**High Contrast scheme features:**
- Maximum luminance difference between foreground/background
- No subtle color distinctions
- Bold, clear borders
- WCAG 2.1 AAA compliant contrast ratios

### Large text mode

**Recommended settings for low vision users:**

- **Font Size**: 18-24pt
- **Color Scheme**: High Contrast
- **DPI-based Font Scaling**: Disabled

### Keyboard-only navigation

**ROCm Optiq fully supports keyboard navigation:**

- All UI elements accessible via Tab/Shift+Tab
- Dropdown menus: Arrow keys for navigation
- Buttons: Enter to activate
- Timeline: Full keyboard shortcut support (see [Keyboard Shortcuts](keyboard-shortcuts.md))

### Screen reader compatibility

**Currently limited support:**
- Menu items announced
- Button labels read
- Text input fields accessible
- Timeline visualization: Limited annotation support

**Future enhancements planned for improved screen reader support.**

## Performance settings

### LOD (Level of Detail) thresholds

**Control when data is downsampled for visualization.**

1. Go to **Settings** → **Display** → **Advanced**
2. Adjust **LOD Threshold**:
   - **Low** (100k events): More aggressive downsampling, faster rendering
   - **Medium** (500k events, default): Balanced performance
   - **High** (2M events): Less downsampling, slower but more detailed

**When to adjust:**
- **Reduce for**: Large traces (>1M events), slower systems
- **Increase for**: Small traces, powerful GPUs, maximum detail needed

### Render settings

**Fine-tune rendering performance:**

- **VSync**: Enable for smooth rendering, disable for maximum FPS
- **Multi-sampling**: Anti-aliasing for smoother lines (2x, 4x, 8x)
- **Texture filtering**: Bilinear (fast) vs. Trilinear (quality)

**Note:** These settings require application restart.

## Troubleshooting settings

### Settings not persisting

**Symptoms**: Changes revert after restart

**Solutions:**
1. Check settings file permissions:
   - Linux/macOS: `chmod 644 ~/.config/roc-optiq/settings.json`
   - Windows: Ensure user has write access to `%APPDATA%\roc-optiq\`
2. Verify disk space available
3. Check for file system errors
4. Close multiple instances of ROCm Optiq

### Corrupted settings

**Symptoms**: Application crashes on startup, settings dialog doesn't open

**Solutions:**
1. Delete settings file (see [Resetting to defaults](#resetting-to-defaults))
2. Check log files for JSON parsing errors
3. Manually edit settings.json to fix syntax errors
4. Restore from backup if available

### API keys not working

**Symptoms**: AI analysis fails with authentication errors

**Solutions:**
1. Verify API key is correct (no extra spaces, complete key)
2. Check API provider account status and credits
3. Test key independently with curl or API playground
4. Re-enter key in settings (copy from source, paste directly)
5. Check network connectivity and firewall settings

### Shortcut conflicts

**Symptoms**: Keyboard shortcuts don't work, trigger system actions

**Solutions:**
1. Verify application window has focus
2. Check for conflicts with system shortcuts:
   - Windows: Check Windows Hotkeys settings
   - macOS: System Preferences → Keyboard → Shortcuts
   - Linux: Desktop environment shortcut settings
3. Rebind conflicting shortcuts to alternative keys
4. Disable system shortcuts if safe to do so

## Best practices

### Recommended settings for different scenarios

#### GPU Profiling on HPC Clusters

- **Time Format**: Microseconds or Nanoseconds
- **Auto-run AI Analysis**: Enabled
- **Default Tool**: rocprofv3
- **Default Tool Args**: `--sys-trace --kernel-trace --memory-copy-trace`
- **Color Scheme**: Nord or Monokai (SSH terminal compatibility)

#### Presentations and Demos

- **Color Scheme**: High Contrast
- **Font Size**: 16-18pt
- **Time Format**: Condensed Timecode (easier to read)
- **Theme**: Light (better projector visibility)

#### Long Analysis Sessions

- **Color Scheme**: Solarized or Dracula (eye strain reduction)
- **DPI-based Font Scaling**: Enabled
- **VSync**: Enabled (reduce screen flicker)

#### Collaborative Environments

- **Export settings**: Share settings.json with team
- **Standard tool args**: Agree on default profiling arguments
- **Consistent time format**: Use same format for easy comparison
- **Color scheme**: Choose team-wide standard

### Security best practices

1. **API Keys**:
   - Never commit settings.json to version control
   - Add `settings.json` to `.gitignore`
   - Use environment variables for CI/CD pipelines
   - Rotate keys periodically

2. **Settings backup**:
   - Keep encrypted backup of settings file
   - Document custom configurations
   - Test restore procedure

3. **Multi-user systems**:
   - Ensure settings file has user-only read/write permissions
   - Don't share API keys between users
   - Use separate accounts for shared workstations

## Related topics

- [Keyboard shortcuts](keyboard-shortcuts.md)
- [Profiling workflow](profiling-workflow.md)
- [Job monitoring](profiling-workflow.md#job-monitoring)

---

*For issues or feature requests related to settings, visit [GitHub Issues](https://github.com/ROCm/roc-optiq/issues)*
