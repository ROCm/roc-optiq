## Optiq Beta 0.5.0

### Features and Improvements

**Timeline and measurement**
- Measure Mode: measure the time delta between events or any two points on the timeline. Includes a measurement toolbar (edge toggle, freehand drag, reset), click-to-place freehand rulers, draggable ruler lines with grab cursors, viewport-clamped labels, and theme-aware highlighting. Measurement state is per-trace so it no longer leaks between loaded traces.
- Queue Utilization: per-queue utilization is computed and surfaced on the timeline, shown as a pill in the track meta area.
- Sample counter track statistics (min, max, average, standard deviation) displayed in track meta data, shown as user selectable pills.
- Top Events view: a new view summarizing the most significant events.
- Recolor events that overlap a time-range selection using the main flame color ramp, and draw the translucent selection fill per-track behind events so highlighted colors stay legible.
- Right-click copy for timeline event names and details (mirrors the hover tooltip fields: name, start, duration, ID), gated on the current selection.

**Tracks and meta area**
- Track meta-area cleanup: queue utilization rendered as a pill beside the QUEUE pill; removed redundant scale columns and separators.
- Track meta-area tooltip enriched with track ID, type, node/process IDs, and event/sample counts.
- Right-click the track meta area to copy the track name or track ID.
- Track options moved from the gear button into the right-click context menu.
- Histogram header now shows total track count and a per-type breakdown with compact/elided display and full details in a tooltip.
- Fixed track reorder preview clamping.
- Add support for NIC agent type. (Schema 3.0.1 and higher).

**Navigation and inspection**
- Call Stack table row navigation and highlighting.
- Flow Data table hover and origin highlights, with an owner-row tint marking the event the flow was opened from.
- Copy menu added to the Call Stack table, with fixed cell right-click hitboxes (correct column under cursor) for both flow and call stack tables.
- Icons added to context menus across the view (table rows, timeline events, call stack/flow menus, kernel bar-chart column menu) with aligned icon columns.

**Compute profiling**
- Added support for the LDS AI point on roofline plots.
- Metric table view scrolling and tab-persistence consistency improvements aligned with the comparison tables.

**Welcome page and UI**
- Homepage/welcome page redesign, now hasStart, Recent, and Resources tiles and logo 
- Application look and feel redesign: refreshed app shell, palette, timeline surfaces, compute panels (unified card-based design), memory-chart flow view, and event details/annotations.
- Status bar area now shows active controller requests (or "Ready"), with height that scales to font size.
- Improved splitter and tab visibility
- Newly opened files now activate their tab automatically.

**Logging and diagnostics**
- New user-facing Log Viewer mirroring the application's log stream into a virtualized, filterable, color-coded table: per-level filtering with counts, substring/regex search with highlighting, pause/auto-scroll, absolute/relative timestamps, copy actions, and "open log file".

**Rendering and performance**
- Lazy render on idle: event-driven rendering so an idle app sleeps (near-zero CPU/GPU) and wakes instantly on input, while loads, async tasks, animations, and in-flight requests keep rendering continuous.
- Texture management rework: startup logo (and other images) uploaded as GPU textures via the modern ImGui textures API instead of per-frame CPU rasterization.
- Migrated to the modern ImGui font-uploading system.

**Platform: macOS**
- Bundle the MoltenVK driver and ICD manifest into the macOS `.app` so Vulkan works without a system SDK install.
- Enable macOS notarization: staged Vulkan runtime, entitlements (`roc-optiq.entitlements.plist`), codesign identity/entitlements CMake options, and license notices.
- Store macOS settings under `~/Library/Application Support/ROCm-Optiq` and logs under `~/Library/Logs/ROCm-Optiq`.
- Fixed a Mission Control issue that left the Control key stuck (causing Ctrl+Click to act as right-click); consolidated platform helpers under `RocProfVis::Platform`.
- Disabled the in-app fullscreen toggle (F11 / View > Fullscreen) on macOS, which doesn't match native window behavior.

### Fixes

- Memory leaks: 
  -call destructors when a memory pool is freed 
  -free leaked (sub) futures in model layer
  -future cleanup in system tests
  -Fix `Array`/`Data` object ownership.
- Fixed timeline highlight artifacts.
- Duplicate file open protection: canonicalize trace paths so a `.db` and a `.rpv` (or two `.rpv` files) pointing at the same trace are detected, with a clear popup instead of a confusing toast.
- Fixed reopening a `.rpv` whose source `.db` is missing: the missing trace is reported by name and no empty 0-byte database is created.
- Fixed multi-node topology node identification (aligned processor topology IDs across instances/queues).
- Corrected stream track entry counts (accumulate record counts across per-operation build queries); bumped the track-info cache version, forcing a one-time rebuild of affected caches.
- Made the per-track table count inclusive on the trace's upper time bound so it matches the tooltip total (off-by-one fix).
- Don't block the UI thread during backend teardown when closing a tab or the application.
- Fixed annotation scroll interactions so annotation scrollbars no longer drive timeline navigation and note dragging is limited to the header.
- Various redesign polish fixes (event details styling, annotation row alignment, metric table row hover, settings table controls, aggregate clear button).
- Fix flow rendering for `.rpd` traces.
- Fix Compute chart metric mapping.

## Optiq Beta 0.4.0

### Features and Improvements

**Startup and progress**
- Branded loading and landing screen.
- Generalized trace-loading progress updates.

**Compute profiling**
- Side-by-side baseline comparison of compute metrics (baseline kernel vs target kernel).
- Pinned metric table with configurable rows and columns (including custom metrics).
- Compute view presets. Save and recall pinned metric configurations.
- New context menu shortcut to add metrics to kernel selection table from table view.
- Interface support for metric values grouped by workload; compute database schema 1.3 and related performance improvements.
- Roofline chart: legend can be repositioned; aspect ratio follows the window; multi-workload chart fixes and top-kernels presentation updates.
- Added workload metric support in the compute data model, including dedicated workload metric getters.
- Added Speed-of-Light (SOL) workload metrics to the compute summary view.
- Compute kernel metric table updates: mini-graphs in cells, pinned title/header behavior improvements, and a global toggle for inline charts.
- Improved metric table usability with better kernel-name handling (tooltip for clipped names) and adjusted name-column sizing to free space for metrics.

**Topology and timeline**
- Stream topology improvements (naming, processor/instance IDs for UI stability, track mapping fixes).
- Toggle on device nodes to show or hide all tracks under that device.

**Navigation and inspection**
- Unified go-to-event behavior across tables and the flow panel: double-click or context menu to open the event on the timeline, with vertical track centering and highlight feedback.
- Highlight-on-navigate with a dedicated event path, pulsing indicator, and timed auto-clear; selection and highlight handled independently.
- Callstack experience reworked (including schema version 4 updates).

**Rendering and platform**
- OpenGL backend as a fallback when Vulkan is unavailable; optional software rendering path; command-line option to force a specific graphics backend.
- Dear ImGui updated (docking-capable line, ImPlot aligned); Linux session defaults to X11 for compatibility.
- Windows packages link GLFW statically by default (no separate `glfw.dll` in the installer).

**Database and trace handling**
- Added a database cleanup feature across controller/model/view paths, including UI integration and loading feedback while cleanup runs.
- Improved cleanup flow reliability and responsiveness (including hang/file-close related handling).

**Misc UI**
- New settings panel allowing keyboard shortcuts to be customized.
- Recent file list can now be cleared.
- Changed log file names to match application name.

### Fixes

- Metric table cache crash when columns are empty.
- Counter samples missing or jumping on the timeline; level-of-detail sampling uses a weighted average where appropriate.
- Minimum width rules for rendered samples (including the single-sample case).
- RPD trace query validation and related issues.
- Track detection and mapping issues.
- Vulkan fallback and related application-window logging cleanup.
- SQLite updated to 3.51.3.
- Fixed incorrect axis labeling in compute summary bar charts.
- Kernel selection table fixes:
  - Do not allow duplicate metrics to be added.
  - Filter out non-finite metric values when rendering bar charts.
  - Fixed metric sorting when missing metrics are present with kernel UUID tiebreaker.
  - Fixed compute metric table bar-cell clipping so bars render correctly with pinned columns.
  - Fixed tooltip being shown over overlapping window.
- Prevent accidental Roofline zoom while scrolling.
- Fix race condition when compute analysis file loads before view is created.
- Fix issue where file dialog does not work if D-Bus is mis-configured.
- Fix for issues with Vulkan driver crashing when display is forwarded over ssh.
- Fix mismatched sidebar header sizes.
- Fix edge case where events overlap instead of stack in the flamechart.
- Fix memory chart values. (Certain metrics were placed in the wrong places).

## Optiq Beta 0.3.0

### Features and Improvements

**Compute Profiling Trace Support**
- Analysis results generated by ROCm Compute Profiler can be viewed.
- View top kernels, memory chart, roofline plots and metrics.

**Topology / Timeline**
 - Topology tree restructured to show Hardware and Software topologies.
 - Memory allocation activity tracks displayed on timeline and topology.
 - .rpd traces now populate topology.

**Multinode Support**
- Time normalization for multi-node configurations.

**Misc**
- UI Styling enhancements.

### Fixes
- Fix: Flow arrow rendering.
- Fix: Crash on trace load.
- Fix: Search bar and summary top kernels statistics for .rpd traces. 


## Optiq Beta 0.2.0

### Features and Improvements

**Summary View**
  - New summary view that displays information about top ten kernels.
  - Duration breakdown plotted on charts.
  - Details presented as a table.

**Mini-map**
  - New mini-map feature shows wholistic view of tracks and events
  - Timeline can be navigated using mini-map.
  - Shows location of current view.

**Timeline**
  - Improved navigation and selection controls.
  - *New* Hold Ctrl+left mouse and drag to perform a time range filter selection.
  - *New* Press left mouse to select / deselect an event.  Press Ctrl+left mouse to add to selection.
  - Context menu option to create a time range filter from a selected event or events.
  - Improved annotation interface. (Annotations start as minimized marker icons).
  - Event name aligns to always be visible on screen if possible.

**Event Details View**
  - Event arguments now displayed.
  - Empty sections not displayed instead of wasting space with headers and no data message.
  - Improved how Callstack section is displayed.

**UI Misc**
  - Improved track names and descriptions.
  - Event Histogram can normalize to all tracks or only visible tracks.
  - When displaying Event Id show database Id instead of internal App Id.
  - Command line parameter support added.  
    - "-f filename" to launch and open file.
    - "-v" to show version number.
  - Fullscreen mode (toggle with F11 key).
  - Use standard OS locations for settings and log files.

**Multi-node**
  - Multinode data now supported.
  - New multi-database yaml file format supported.
  - Improved data processing logic.

**Misc**
  - Project now builds on Mac OS. 

### Fixes

Fix: RPM package generation.
Fix: Default font path detection, and fallback font size generation.
Fix: Log spam in release builds.
Fix: Crash when same event was repeatedly selected/deselected.
Fix: Misc application crashes.
Fix: Build on linux systems that have libtbb-dev installed.


## Optiq Beta 0.1.0

### Features and Improvements

**Timeline**
  - Improved tooltip for event tracks.
    - Shows more informative stats when hovered over a combined event.
    - Shows table of event names, counts, and durations.
    - Shows total event count.
  - Combined events now colored by the underlying event that has longest combined duration.
  - Sample track graph rendering improved.
    - Area under line now filled by default.
    - Sample under cursor is now highlighted.
    - Tool tip now shows duration.
  - Scale for sample tracks can be adjusted by clicking and editing the min/max values.
  - New compact mode option that allows tracks with tall flame graphs to be "compressed".

**Histogram**
  - Now aligned with timeline.
  - Updated dynamically to only include tracks that have visibility turned on.

**Tables**
  - Added feature to export table results to a csv file.
  
**UI Misc**
  - Use native OS file dialogs when opening or saving files.
  - Improved dark theme colors.
  - About Dialog now has link to documentation.

### Fixes

  - Fix: Delete temporary files when application exits.
  - Fix: Sample tracks graph disappears when zoomed in too deeply.
  - Fix: Large values not plotted correctly on sample tracks.
  - Fix: Sample track tooltip value displayed wrong value.
  - Fix: Don't allow grouping table data by Id.
  - Fix: Ignore event with invalid time values.
  - Fix: Extension not appended when saving on Linux.
  - Fix: Counter tracks not trimmed when saving trimmed trace.
  - Fix: No data when scrolling at highest zoom.
  - Fix: Samples overlap on track.
  - Fix: Crash when re-ordering tracks and view track details.

## Alpha 0.6.3

- Initial Alpha build.
