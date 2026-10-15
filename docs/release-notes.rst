.. meta::
   :description: ROCm Optiq release notes
   :keywords: ROCm Optiq, release notes, ROCm

***********************************************
ROCm Optiq 1.1.0 release notes
***********************************************

These release notes describe notable changes since the previous ROCm Optiq release.

Added
-----

* Sortable timeline tracks: Right-click the track-list header (or use the down-arrow at its bottom) to sort tracks by **Topology**, **Default** (track type), or a remembered **Custom** order. **Drag-to-reorder** becomes the **Custom** order, and the choice persists per project.
* Draggable measurement duration label: The label can now be dragged (its connecting line and notches follow it), and the ruler timestamps are split to top left and bottom right so they no longer overlap.
* Time-range selection indicators across the timeline, tables, and overview: Per-track analysis statistics pills are tinted blue while a selection is active. The events and top-events tables show a **Limited to time-range selection** vs **Full trace** scope indicator, and the overview histogram greys out bars outside the selection, marks the selection's start and end, and draws a duration bracket.

Changed
-------

* Search enhancements: Memory events are now included in search for supported formats, plus a new **Advanced** search mode for finer control.
* Simplified table filter UI. Original filter options available via **Advanced mode**.
* Clipped the macOS app icon to a squircle to match other icons in the Dock, while keeping the white-on-red design fully opaque.
* CI: Linux targets now built as ManyLinux binaries.

Resolved issues
---------------

* Fixed incorrect table row count and missing rows under specific scenarios.
* Fixed memory-chart values exceeding 100% (schema 2.0.0 or higher).
* Memory manager: Fixed intermittent crashes, permanent pool retention, and unbounded memory growth from stranded pooled objects.
* Fixed the UI rendering at half size on HiDPI Linux (XWayland) displays.
* Fixed the description-column resize panning the timeline.
* Fixed timeline tracks flickering during fast vertical scroll.
* Fixed a crash opening a compute ```.db``` when a roofline ceiling category is empty (uncaught ```std::out_of_range```); the **Roofline** tab now shows "No data available."
* Fixed a crash in **Baseline Comparison** when a kernel with a non-finite (Inf) metric is compared to itself.
* Fixed the version number reported by the Windows installer.
* Fixed topology node order that is inconsistent between target environments.
* Fixed memory chart layout and metrics for the gfx950 and gfx940 series.
