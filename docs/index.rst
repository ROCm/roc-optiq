.. meta::
  :description: ROCm Optiq is a unified visualization and analysis tool for ROCm Systems Profiler and ROCm Compute Profiler performance data on AMD GPUs.
  :keywords: ROCm Optiq, ROCm, profiler, visualization, trace, analysis, GPU, AMD, performance, Systems Profiler, Compute Profiler

************************
ROCm Optiq documentation
************************

:doc:`ROCm Optiq <what-is-optiq>` is a unified visualization and analysis tool for performance data collected by ROCm profiling tools, specifically `ROCm Systems Profiler <https://rocm.docs.amd.com/projects/rocprofiler-systems/en/latest/index.html>`_ and `ROCm Compute Profiler <https://rocm.docs.amd.com/projects/rocprofiler-compute/en/latest/>`_. 
It provides deep insights into both system-level behavior and kernel-level performance for applications running on the ROCm stack. 

ROCm Optiq enables developers to visualize execution traces and profiling analysis data in a single interface, helping to identify performance bottlenecks, understand hardware utilization, optimize workloads, and efficiently scale applications across CPUs and GPUs. 

The ROCm Optiq project repository is located at `https://github.com/ROCm/roc-optiq <https://github.com/ROCm/roc-optiq>`_.

.. note::

  ROCm Optiq is in beta. Running production workloads is not recommended.

User Interface Overview
======================

Systems Profiler Trace View:

.. image:: /images/ui_sections.png
   :alt: ROCm Optiq Systems Profiler UI layout

1. **System topology tree** — expand nodes to see the relationships between tracks, toggle track visibility, and jump to a track in
   the timeline.
2. **Timeline view** — the list of tracks holding event and counter data. Tracks can be resized, reordered, and configured
   individually; right-click for track options.
3. **Advanced details area** — tabbed detail for the current selection: event table, sample table, event details, track details,
   top events, and annotations.
4. **Histogram area** — an event density histogram. The area currently in view is highlighted and can be dragged to scroll the timeline.
5. **Toolbar** — flow rendering modes, annotations, event search, bookmarks, the minimap toggle, and view reset.

Compute Profiler Analysis View:

.. image:: /images/ui_computer_sections.png
   :alt: ROCm Optiq Compute Profiler UI layout

1. **Toolbar** - Choose workload and kernel to inspect.
2. **Tab view** - Switch between the Summary, Kernel details, Table view, Baseline comparison, Workload details
3. **Work area** - Displays the selected view. 

How to use ROCm Optiq
=====================

.. grid:: 2
  :gutter: 3

  .. grid-item-card:: Install

    * :doc:`Install ROCm Optiq </install/optiq-install>`

  .. grid-item-card:: How to

    * :doc:`View trace data <how-to/view-trace>`
    * :doc:`View analysis data <how-to/view-analysis>`
    * :doc:`Customize your project <how-to/customize-views>`

  .. grid-item-card:: Reference

    * :doc:`Command-line support <reference/cli-support>`
    * :doc:`Keyboard shortcuts <reference/keyboard-shortcuts>`

ROCm Optiq (Beta) 0.5.0
=======================

Added
-----

Features and improvements for viewing ROCm Systems Profiler trace data
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Timeline View - Measure Mode**: Measure the time delta between events or any two points on the timeline. Includes a measurement toolbar (edge toggle, freehand drag, reset), click-to-place freehand rulers, draggable ruler lines with grab cursors, viewport-clamped labels, and theme-aware highlighting. Measurement state is per-trace so it no longer leaks between loaded traces. Right-click a measurement label to **Copy Start Timestamp**, **Copy End Timestamp**, or **Copy Measurement Duration**.
- **Timeline View - Queue Utilization**: Per-queue utilization is computed and surfaced on the timeline, shown as a pill in the track meta area.
- **Sample counter track statistics** (min, max, average, standard deviation) displayed in track meta data, shown as user-selectable pills.
- **Track meta-area tooltip** enriched with track ID, type, node/process IDs, and event/sample counts.
- **Timeline View - Track summary** in the upper-left corner of the Timeline View shows total track count and a per-type breakdown with compact/elided display and full details in a tooltip.
- **Advanced Details - Top Events view**: A new view summarizing the most significant events.
- **Track display options** moved to the right-click **Track Options** submenu; inline gear button removed.
- **Track tooltip and copy actions** (**Copy track name**, **Copy track ID**).
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

Improvements for visualizing ROCm Compute Profiler analysis data
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- Support for the LDS AI point on roofline plots.
- Metric table view scrolling and tab-persistence consistency improvements aligned with the comparison tables.
- Tooltip to the delta-threshold control in Baseline Comparison. 
- **Copy Row Data** or **Copy Cell Data** context menu in Kernel Selection table. 

Welcome page and UI
^^^^^^^^^^^^^^^^^^^

- Homepage/welcome page with Start, Recent, and Resources.
- Status bar area now shows active controller requests (or "Ready"), with height that scales to font size.

Changed
-------

- **Application look and feel redesign**: Refreshed app shell, palette, timeline surfaces, compute panels (unified card-based design), memory-chart flow view, and event details/annotations.
- Improved splitter and tab visibility.
- Newly opened files now activate their tab automatically.
- **New user-facing Log Viewer** mirroring the application's log stream into a virtualized, filterable, color-coded table: per-level filtering with counts, substring/regex search with highlighting, pause/auto-scroll, absolute/relative timestamps, copy actions, and “open log file”.
- **Lazy render on idle:** Event-driven rendering so an idle app sleeps (near-zero CPU/GPU) and wakes instantly on input, while loads, async tasks, animations, and in-flight requests keep rendering continuous.
- **Texture management rework:** Startup logo (and other images) uploaded as GPU textures via the modern ImGui textures API instead of per-frame CPU rasterization.
- Migrated to the modern ImGui font-uploading system.
- Saving a project (``.rpv``) adds it to the **Recent Files** list. 

Fixes
-----

- **Memory leaks**:

  - Call destructors when a memory pool is freed.
  - Free leaked (sub) futures in model layer.
  - Future cleanup in system tests.
  - Fix Array/Data object ownership.

- Fixed timeline highlight artifacts.
- **Duplicate file open protection**: Canonicalize trace paths so a ``.db`` and a ``.rpv`` (or two ``.rpv`` files) pointing at the same trace are detected, with a clear popup instead of a confusing toast.
- Fixed reopening a ``.rpv`` whose source ``.db`` is missing: the missing trace is reported by name and no empty 0-byte database is created.
- Fixed multi-node topology node identification (aligned processor topology IDs across instances/queues), plus additional topology display fixes (null parent lookups, out-of-bounds stream processor lookup). 
- Corrected stream track entry counts (accumulate record counts across per-operation build queries); bumped the track-info cache version, forcing a one-time rebuild of affected caches.
- Made the per-track table count inclusive on the trace's upper time bound so it matches the tooltip total (off-by-one fix).
- Don't block the UI thread during backend teardown when closing a tab or the application.
- Fixed annotation scroll interactions so annotation scrollbars no longer drive timeline navigation and note dragging is limited to the header.
- Various redesign polish fixes (event details styling, annotation row alignment, metric table row hover, settings table controls, aggregate clear button).
- Fix flow rendering for ``.rpd`` traces.
- Fix Compute chart metric mapping.
- Fixed the Welcome page sometimes appearing only partially drawn until the user moved the mouse or clicked (idle wait is now bounded). 
- Fixed a macOS app packaging issue that could prevent the app bundle from being properly signed (static GLFW no longer staged into the bundle). 


To contribute to the documentation, refer to the
`Programming Guide in the GitHub repo <https://github.com/ROCm/roc-optiq/blob/main/CODING.md>`_.

You can find licensing information on the
:doc:`License <license>` page.
