.. meta::
   :description: ROCm Optiq release notes
   :keywords: ROCm Optiq, release notes, ROCm

***********************************************
ROCm Optiq 1.2.0 release notes
***********************************************

These release notes describe notable changes since the previous ROCm Optiq release.

Release highlights
------------------

This release expands trace summary, compute comparison, and trace-format support. It also includes fixes for duration reporting, tables, trace data, and packaging.

Added
-----

Systems Trace Summary
~~~~~~~~~~~~~~~~~~~~~

* The **Summary** view now covers kernel executions, memory operations, and thread operations, each with a top-by-execution-time chart. Top entries also report percent of total duration and mean duration.
* **Details** on a category card opens that category's tab, which gives a fuller breakdown.
* A **System Topology** tab shows the system topology and counters.

Compute profiling
~~~~~~~~~~~~~~~~~

* Roofline comparison: compare a kernel against a baseline kernel, including a kernel from another workload, and choose whether hardware peaks come from the baseline or the compared workload.
* Roofline option menus open outside the plot by default.
* Roofline now has toggleable labels for hardware peaks.
* **Profile Details** (formerly **Workload Details**) includes an **Analysis Tool** panel with the ROCm Compute Profiler version, Git revision, and database schema version.

Memory chart
~~~~~~~~~~~~

* Architecture-specific memory-chart layouts: gfx950, gfx94x (including gfx940), and gfx115x (RDNA 3.5).
* The chart can be panned with the scroll wheel or a trackpad.

Tables
~~~~~~

* Table columns auto-fit to their content and are capped in width, so long values (such as demangled kernel names) are elided. The full value stays in a hover tooltip. Columns resized by hand are left alone, and widths are re-fit when the result set changes.

Trace formats
~~~~~~~~~~~~~

* ROCpd schema 3.0.4 adds the ``rocpd_hip_event`` table. When that table contains rows, ROCm Optiq shows those HIP API events on the timeline, in search, in the topology tree, and in data flow from region events. HIP API events are not listed in **Top Events**.

Timeline and navigation
~~~~~~~~~~~~~~~~~~~~~~~

* File Explorer-style track selection: In the timeline and the topology sidebar, click a track to select only that track, **Ctrl**+click to add or remove it, and **Shift**+click to select every track in between.

ISA View
~~~~~~~~

* ISA View is included in the release build and marked **Tech Preview**, because the collected data format is still under development. The tab is inactive when the database has no ISA lines.

Binaries / build
~~~~~~~~~~~~~~~~

* Official builds print the git commit hash with ``--version``. A build that was not stamped prints ``unknown``.

Resolved issues
---------------

* Fixed kernel dispatch durations wrapping above about 4.29 seconds in the **Top Kernels** table and its bar chart, pie chart, and tooltips. Durations are stored as 64-bit nanosecond counts.
* Fixed incorrect averages when a grouped query was merged across threads. Per-thread averages are now weighted, and MAX no longer starts at the smallest positive double.
* Fixed table CSV export reporting success when the path was unwritable, the filter was bad, the export was cancelled, or the final write failed (for example, a full disk).
* Fixed tables restoring another table's column layout from a previous session.
* Fixed **Top Events** tables displaying incorrectly when they had no results.
* Fixed **Track Details** showing PID 0 for counter tracks.
* Fixed rocpd sample tracks missing their PMC samples.
* Fixed a crash when a table filter could not be parsed.
* Fixed a hang on macOS.
* Fixed a leaked row-count query when a table had nothing to count.
* Fixed invalid table filter feedback. Improved error notifications when table filters are invalid.
* Fixed the kernel selection table disappearing when a filter matches no rows.
* Fixed Linux tarball extracts at the install prefix. It no longer wraps its contents in a ``<package>-<version>-<system>/`` directory, matching the .deb and .rpm layout.
* Fixed missing dependencies in Debian packages.
* Fixed commit hash missing from Linux builds.
* Fixed process id not read correctly in some trace files.
