:orphan:

<head>
  <meta charset="UTF-8">
  <meta name="description" content="ROCm Optiq release history">
  <meta name="keywords" content="documentation, release history, ROCm, AMD">
</head>

# ROCm Optiq (Beta) release history

| Version | Release date |
| ------- | ------------ |
| [1.0.0](https://rocm.docs.amd.com/projects/roc-optiq/en/1.0.0/index.html) | August 26, 2026 |
| [Beta 0.5.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.5.0/index.html) | July 15, 2026 |
| [Beta 0.4.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.4.0/index.html) | May 6, 2026 |
| [Beta 0.3.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.3.0/index.html) | March 26, 2026 |
| [Beta 0.2.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.2.0/index.html) | February 11, 2026 |
| [Beta 0.1.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.1.0/index.html) | December 10, 2025 |

## ROCm Optiq 1.0.0

### Added

#### Features and improvements for viewing ROCm Systems Profiler trace data

##### Timeline and navigation
- **Zoom to Measurement** and **Zoom to Time Range Selection**: Right-click the timeline and choose **Zoom to Measurement** or **Zoom to Time Range Selection** to fit that span to the timeline width, with the markers landing on the left and right edges.
##### Tracks and topology
- **Node-based color coding** for tracks and the sidebar topology, improving navigation in multi-node traces.
- **Reveal in Topology:** Right-click action on a track to jump directly from a track to its location in the System Topology View.
- **Batch track settings**: Apply Track Options changes to multiple selected tracks at once.
- New sidebar right-click context menu options: **Go To Track**, **Hide Track**, **Show/Hide All But This Track**, **Show/Hide Selected Tracks**.
##### Annotations
- Annotation UX enhancements: **Lock Annotation**, **Go-to-Anchor**, cross-highlighting between annotations and their associated tracks/events, text wrapping, and placement fixes.
##### Track details and tables
- Refactored track statistics into reusable components. **Track Details** now displays statistics at full precision, without rounding or truncation.

#### Features and improvements for visualizing ROCm Compute Profiler analysis data

##### Roofline analysis
- **Single-click filtering** for kernel data points, memory-level lines (L1, L2, HBM, LDS), and bandwidth peaks.
- **Roofline line-thickness preference** to improve chart readability.

#### Experimental

```{note}
These features are in progress and available for preview only when ROCm Optiq is built from source. Behavior and UI are subject to change.
```
- **Profiler launch**: Launch the ROCm profiler locally or on a remote host, with remote file access/browsing and an SSH-based remote profiling workflow (redesigned remote profile panels and profiler launcher UI).
- **New trace formats**: Load Perfetto and Chrome (JSON) traces.


### Changed

- **Rendering and performance**: Added a ``RenderScheduler`` to drive lazy-render wake-ups, and adopted ImGui's built-in DPI handling, replacing the previous custom solution.
- Improved how Stream nodes are displayed in the sidebar topology tree.
- More compact flame-chart events, with fixed resize/expand behavior.
- Incremental Y-axis scale tick labels for expanded counter tracks.

### Fixes
- Fixed inconsistent kernel highlight color in the compute summary view.
- Fixed welcome-page links firing through overlaying modal dialogs (links are now real ImGui buttons that respect hover ownership).
- Fixed macOS Ctrl-Space modifier recovery.
- Fixed editable counter-track value inputs.
- Fixed data-flow arrow starting from level zero on ``.rpd`` traces.
- Fixed the Systems multi-node summary window showing data only for the first node in the ``.yaml``.
- Fixed loading workload Speed-of-Light for compute schema < 1.3.0: Query builders now report and skip unsupported queries.
- JobSystem fixes.

## ROCm Optiq (Beta) 0.5.0

### Added

#### Features and improvements for viewing ROCm Systems Profiler trace data

- **Timeline View - Measure Mode**: Measure the time delta between events or any two points on the timeline. Includes a measurement toolbar (edge toggle, freehand drag, reset), click-to-place freehand rulers, draggable ruler lines with grab cursors, viewport-clamped labels, and theme-aware highlighting. Measurement state is per-trace so it no longer leaks between loaded traces. Right-click a measurement label to **Copy Start Timestamp**, **Copy End Timestamp**, or **Copy Measurement Duration**.
- **Timeline View - Queue Utilization**: Per-queue utilization is computed and surfaced on the timeline, shown as a pill in the track meta area.
- **Sample counter track statistics** (min, max, average, standard deviation) displayed in track meta data, shown as user-selectable pills.
- **Track meta-area tooltip** enriched with track ID, type, node/process IDs, and event/sample counts.
- **Timeline View - Track summary** in the upper-left corner of the Timeline View shows total track count and a per-type breakdown with compact/elided display and full details in a tooltip.
- **Advanced Details - Top Events view**: A new view summarizing the most significant events.
- **Track display options** moved to the right-click **Track Options** submenu; inline gear button removed.
- **Track tooltip and copy actions** (Copy track name, Copy track ID).
- **Context menu icons** across tables, timeline, flow data, and call stack.
- **Right-click copy and export actions** in Event Table, Flow Data, Call Stack, and Top Events.
- **Time-range filter** now dims events outside the selected range.
- **Call Stack table** row navigation and highlighting.
- **Flow Data table** hover and origin highlights, with an owner-row tint marking the event the flow was opened from.
- **Copy menu** added to the Call Stack table, with fixed cell right-click hitboxes (correct column under cursor) for both flow and call stack tables.
- **Icons** added to context menus across the view (table rows, timeline events, call stack/flow menus, kernel bar-chart column menu) with aligned icon columns.
- **Right-click copy** for timeline event names and details (mirrors the hover tooltip fields: name, start, duration, ID), gated on the current selection.
- **Annotations**: Annotations are now track-bound: annotations stay attached to their tracks and follow their tracks when scrolling, reordering, or rearranging the timeline. Expanded notes open as movable floating windows with inline editing; empty notes are discarded automatically. A time guide line appears while a note is hovered or dragged. 
- Statistics in **Track Details**, these include queue utilization, counter Minimum, Maximum, Mean, and Standard Deviation. 
- Right-click a row or cell of Track Details to **Copy Row Data** or **Copy Cell Data**. 

#### Improvements for visualizing ROCm Compute Profiler analysis data

- Support for the LDS AI point on roofline plots.
- Metric table view scrolling and tab-persistence consistency improvements aligned with the comparison tables.
- Tooltip to the delta-threshold control in Baseline Comparison. 
- **Copy Row Data** or **Copy Cell Data** context menu in Kernel Selection table. 

#### Welcome page and UI

- Homepage/welcome page with Start, Recent, and Resources.
- Status bar area now shows active controller requests (or “Ready”), with height that scales to font size.

### Changed

- **Application look and feel redesign**: Refreshed app shell, palette, timeline surfaces, compute panels (unified card-based design), memory-chart flow view, and event details/annotations.
- Improved splitter and tab visibility.
- Newly opened files now activate their tab automatically.
- **New user-facing Log Viewer** mirroring the application’s log stream into a virtualized, filterable, color-coded table: per-level filtering with counts, substring/regex search with highlighting, pause/auto-scroll, absolute/relative timestamps, copy actions, and “open log file”.
- **Lazy render on idle:** Event-driven rendering so an idle app sleeps (near-zero CPU/GPU) and wakes instantly on input, while loads, async tasks, animations, and in-flight requests keep rendering continuous.
- **Texture management rework:** Startup logo (and other images) uploaded as GPU textures via the modern ImGui textures API instead of per-frame CPU rasterization.
- Migrated to the modern ImGui font-uploading system.
- Saving a project (``.rpv``) adds it to the **Recent Files** list. 

### Fixes

- **Memory leaks**:
  - Call destructors when a memory pool is freed.
  - Free leaked (sub) futures in model layer.
  - Future cleanup in system tests.
  - Fix Array/Data object ownership.
- Fixed timeline highlight artifacts.
- **Duplicate file open protection**: Canonicalize trace paths so a `.db` and a `.rpv` (or two `.rpv` files) pointing at the same trace are detected, with a clear popup instead of a confusing toast.
- Fixed reopening a `.rpv` whose source `.db` is missing: the missing trace is reported by name and no empty 0-byte database is created.
- Fixed multi-node topology node identification (aligned processor topology IDs across instances/queues), plus additional topology display fixes (null parent lookups, out-of-bounds stream processor lookup). 
- Corrected stream track entry counts (accumulate record counts across per-operation build queries); bumped the track-info cache version, forcing a one-time rebuild of affected caches.
- Made the per-track table count inclusive on the trace’s upper time bound so it matches the tooltip total (off-by-one fix).
- Don’t block the UI thread during backend teardown when closing a tab or the application.
- Fixed annotation scroll interactions so annotation scrollbars no longer drive timeline navigation and note dragging is limited to the header.
- Various redesign polish fixes (event details styling, annotation row alignment, metric table row hover, settings table controls, aggregate clear button).
- Fix flow rendering for `.rpd` traces.
- Fix Compute chart metric mapping.
- Fixed the Welcome page sometimes appearing only partially drawn until the user moved the mouse or clicked (idle wait is now bounded). 
- Fixed a macOS app packaging issue that could prevent the app bundle from being properly signed (static GLFW no longer staged into the bundle). 

## ROCm Optiq (Beta) 0.4.0

### Added

New visualization features for analyzing data include:

- Summary View -- Speed of Light: Provides an aggregated, system-level summary of key performance and hardware utilization metrics across all kernels, showing utilization relative to architectural peak capabilities. The Percent-of-Peak values help quickly identify whether the workload is limited.
- Kernel Details -- Kernel Selection Table: Added a bar chart visualization of metric values and a tooltip that displays kernels’ full names.
- Baseline Comparison: Enables you to compare two workload measurements (baseline vs. target) side-by-side in a unified table. It helps to quickly spot regressions, improvements, and behavior changes. It highlights per-metric deltas (including percentage change) to make the performance impact easy to quantify.
- Added support for ROCm compute profiler's database schema 1.3 and related performance improvements.
- Presets: Save and recall pinned metric configurations for Table View and Baseline Comparison.
- New context menu to add metrics to Kernel Selection Table from Table View.
- Configurable delta-threshold control for Baseline Comparison.

Other new features:

- Data clean-up: Enables the removal of metadata added by ROCm Optiq in a database file.
- Command-line interface support.
- OpenGL backend as a fallback when Vulkan is unavailable; optional software rendering path; command-line option to force a specific graphics backend.
- New settings panel allowing keyboard shortcuts to be customized.

### Changed

Changes in viewing analysis data include:

- Roofline charts for Summary View and Kernel Details: The legend can be repositioned; aspect ratio follows the window; multi-workload chart fixes and top-kernels presentation updates.
- Kernel Details updates: Added mini-graphs in cells, pinned title/header improvements, and a global toggle for inline charts. Added a tooltip to display clipped names and adjusted the name-column sizing to free up space for metrics.

Changes in viewing trace data include:

- Topology View and Timeline:

  - Enhanced System Topology tree for better representation of hardware and software topologies.
  - Toggle on device nodes to show or hide all tracks under that device.

Navigation and inspection:

- Use **Go To Event** in tables and the **Flow Data** panel to go to a specific event. Double-click the event, or click **Go To Event** from the right-click context menu, to open the event on the timeline with vertical track centering and highlighted feedback.
- Highlight-on-navigate with a dedicated event path using a pulsing indicator.
- Callstack experience improvements.

## ROCm Optiq (Beta) 0.3.0

### Added

ROCm Optiq for visualizing ROCm Compute Profiler's data. New features include:

- **Summary View**: Shows a high-level overview of the captured data.

  - **Table**: lists the top 10 longest-running kernels sorted by Total Execution Time.
  - **Charts**: Plot duration and invocation statistics across kernels.
  - **Roofline Chart**: plots kernel performance against empirical hardware ceilings to reveal the dominant performance bottleneck for all kernels.

- **Kernel Details**: displays details of each kernel.

  - **Kernel Selection Table**: Lists kernels with GPU metrics. Use **Add Metric** to append additional GPU metric columns. Per-column search box accepts names or metric expressions (for example, `metric > threshold`). Click **Apply Filters** to execute; combine multiple filters to narrow the analysis.
  - **Memory Chart**: Shows memory transactions and throughput per cache hierarchy level for the selected kernel.
  - **System Speed-of-Light**: Displays key kernel-level performance metrics with unit, average, peak, and percentage of peak values.
  - **Kernel Roofline Chart**: Shows a kernel-specific roofline analysis to determine whether a kernel is compute-bound or memory-bound. Click the gear icon to access customization options.

- **Table View**: provides a complete list of available metrics for the selected kernel.
- **Workload Details**: Provides contextual information about the workload.

### Changed

Changes in ROCm Optiq for visualizing ROCm System Profiler traces:

- System Topology tree was restructured to show hardware and software topologies.
- Memory allocation activity tracks are now displayed in Timeline and System Topology Views.
- RPD files populate Topology.
- Multinode support: Time normalization for multi-node configurations.

### Known issues

#### Metrics that reference ``None`` return N/A

If a metric expression contains ``None``, ROCm Compute Profiler might ignore the metric value even when it isn't ``None``. As a result, ROCm Optiq displays **N/A** for affected metrics.

- System Speed of Light (0200)
  - VALU Active Threads
  - LDS Bank Conflicts/Access
  - vL1D Cache Hit Rate
  - L2 Cache Hit Rate
  - L2-Fabric Read Latency
  - L2-Fabric Write Latency
  - sL1D Cache Hit Rate
  - L1I Fetch Latency
- Memory Chart (0300)
  - LDS Latency
  - VL1 Hit
  - VL1 Lat
  - VL1 Coalesce
  - VL1 Stall
  - sL1D Hit
  - sL1D Lat
  - IL1 Lat
  - L2 Rd Lat
  - L2 Wr Lat
- Command Processor CPC/CPF (0500)
  - CPF Utilization
  - CPF Stall
  - CPF-L2 Utilization
  - CPF-L2 Stall
  - CPF-UTCL1 Stall
  - CPC SYNC FIFO Full Rate
  - CPC CANE Stall Rate
  - CPC ADC Utilization
  - CPC Utilization
  - CPC Stall Rate
  - CPC Packet Decoding Utilization
  - CPC-Workgroup Manager Utilization
  - CPC-L2 Utilization
  - CPC-UTCL1 Stall
  - CPC-UTCL2 Utilization
- Workgroup Manager SPI (0600)
  - VGPR Writes
  - SGPR Writes
  - Not-scheduled Rate (Workgroup Manager)
  - Not-scheduled Rate (Scheduler-Pipe)
  - Scheduler-Pipe FIFO Full Rate
  - Scheduler-Pipe Stall Rate
  - Scratch Stall Rate
- Compute Units Compute Pipeline (1100)
  - VALU Active Threads
  - MFMA Instruction Cycles
  - VMEM Latency
  - SMEM Latency
- Local Data Share LDS (1200)
  - Bank Conflict Rate
  - LDS Latency
  - Bank Conflicts/Access
- Scalar L1 Data Cache (1400)
  - Cache Hit Rate
- Vector L1 Data Cache (1600)
  - Hit rate
  - Utilization
  - Coalescing
  - Stalled on L2 Data
  - Stalled on L2 Req
  - Stalled on Address
  - Stalled on Data
  - Stalled on Latency FIFO
  - Stalled on Request FIFO
  - Stalled on Read Return
  - Tag RAM Stall (Read)
  - Tag RAM Stall (Write)
  - Tag RAM Stall (Atomic)
  - Cache Hit Rate
  - Hit Ratio
- L2 Cache (1700)
  - HBM Read Traffic
  - Remote Read Traffic
  - Uncached Read Traffic
  - HBM Write and Atomic Traffic
  - Remote Write and Atomic Traffic
  - Atomic Traffic
  - Uncached Write and Atomic Traffic
  - Read Latency
  - Write and Atomic Latency
  - Atomic Latency
  - Read Stall
  - Write Stall
  - Cache Hit
  - Read - PCIe Stall
  - Read - Infinity Fabric Stall
  - Read - HBM Stall
  - Write - PCIe Stall
  - Write - Infinity Fabric Stall
  - Write - HBM Stall
  - Write - Credit Starvation

#### ``workload_name`` is missing in ``sysinfo.csv`` when using ``--output-directory``

When you profile with the ``--output-directory`` option, the ``workload_name`` column in ``sysinfo.csv`` might be empty. This can prevent views in the ROCm Compute Profiler analysis database from joining tables based on ``workload_name``, which makes system information unavailable.

## ROCm Optiq (Beta) 0.2.0

### Added

- **Summary View**: Displays the top ten kernels by execution time using pie charts, bar charts, or tables.
- **Minimap**: Provides a compact overview of event density and counter values across the entire trace, enabling rapid navigation of large datasets.

### Changed

- **Timeline View**: Improved navigation and selection. Added context menu option to create a time range filter from a selected event or events.
- **Advanced Details Panel**: **Aggregate by Column** drop-down groups the results by the selected column. Options to size columns to fit in **Event Table** and **Sample Table**. **Event Details** now shows the function call’s arguments, if available.
- **Time Range Filtering**: Improved time range selection.
- **Histogram**: Shows event density in two display modes: "Normalization: All Tracks" and "Normalization: Visible Tracks".
- **Multi-node**: Multi-node data and a new multi-database yaml file format are supported.

## ROCm Optiq (Beta) 0.1.0

Initial release of ROCm Optiq (Beta).

### Added

- **System Topology View**: Displays a hierarchical representation of the hardware or system components, such as nodes, processes, as well as the GPU queues, memory operations, threads, and more that belong to them.
- **Timeline View**: Shows CPU and GPU activities, events, and performance metrics in chronological order for a detailed temporal analysis. ROCm Optiq allows you to zoom, filter, and bookmark data for fine-grained inspection. You can correlate GPU workloads with in-application CPU events and performance with hardware resource usage, enabling easy identification and remediation of performance blockers.
- **Advanced Details Panel**: Provides an in-depth view of profiling data, enabling you to analyze performance metrics and event-specific information. It offers SQL-like filters and group-by operations.
- **Histogram**: Shows the event density across all visible tracks and highlights the zoomed-in region to quickly identify hotspots.
- **Time Range Filtering**: Select a specific time interval to filter events and counter samples for focused analysis.
- **Event Search**: Quickly locate target events.

