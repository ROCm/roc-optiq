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

.. grid:: 2
   :gutter: 3

   .. grid-item-card:: Install

      * :doc:`Install ROCm Optiq <install/optiq-install>`

   .. grid-item-card:: How to

      * :doc:`View trace data <how-to/view-trace>`
      * :doc:`View analysis data <how-to/view-analysis>`
      * :doc:`Customize your project <how-to/customize-views>`

   .. grid-item-card:: Reference

      * :doc:`Command-line support <reference/cli-support>`
      * :doc:`Keyboard shortcuts <reference/keyboard-shortcuts>`

ROCm Optiq 1.0.0
=================
Added
-----
Features and improvements for viewing ROCm Systems Profiler trace data
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Timeline and navigation
""""""""""""""""""""""""
- **Zoom to Measurement** and **Zoom to Time Range Selection**: right-click the timeline and choose **Zoom to Measurement** or **Zoom to Time Range Selection** to fit that span to the timeline width, with the markers landing on the left and right edges.
Tracks and topology
""""""""""""""""""""
- **Node-based color coding** for tracks and the sidebar topology, improving navigation in multi-node traces.
- **Reveal in Topology** right-click action on a track, to jump directly from a track to its location in the System Topology View.
- **Batch track settings**: apply Track Options changes to multiple selected tracks at once.
- New sidebar right-click context menu options.
- **Display Settings preferences**: show or hide the show/hide-track and go-to-track sidebar icons in the System Topology View from **Edit** > **Preferences** > **Display Settings**.
Annotations
""""""""""""
- Annotation UX enhancements: **Lock Annotation**, **Go-to-Anchor**, cross-highlighting between annotations and their associated tracks/events, text wrapping, and placement fixes.
Track details and tables
""""""""""""""""""""""""
- Refactored track statistics into reusable components; **Track Details** now displays statistics at full precision, without rounding or truncation.
Features and improvements for visualizing ROCm Compute Profiler analysis data
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Roofline analysis
""""""""""""""""""
- **Single-click filtering** for kernel data points, memory-level lines (L1, L2, HBM, LDS), and bandwidth peaks.
- **Roofline line-thickness preference** to improve chart readability.
Experimental
^^^^^^^^^^^^
.. note::
   These features are in progress and available for preview only (build from source). Behavior and UI are subject to change.
- **Profiler launch**: Launch the ROCm profiler locally or on a remote host, with remote file access/browsing and an SSH-based remote profiling workflow (redesigned remote profile panels and profiler launcher UI).
- **New trace formats**: Load Perfetto and Chrome (JSON) traces.

Changed
-------
- **Rendering and performance**: Added a ``RenderScheduler`` to drive lazy-render wake-ups, and adopted ImGui's built-in DPI handling, replacing the previous custom solution.
- Improved how Stream nodes are displayed in the sidebar topology tree.
- More compact flame-chart events, with fixed resize/expand behavior.
- Incremental Y-axis scale tick labels for expanded counter tracks.

Fixes
-----
- Fixed inconsistent kernel highlight color in the compute summary view.
- Fixed welcome-page links firing through overlaying modal dialogs (links are now real ImGui buttons that respect hover ownership).
- Fixed macOS Ctrl-Space modifier recovery.
- Fixed editable counter-track value inputs.
- Fixed data-flow arrow starting from level zero on ``.rpd`` traces.
- Fixed the Systems multi-node summary window showing data only for the first node in the ``.yaml``.
- Fixed loading workload Speed-of-Light for compute schema < 1.3.0; query builders now report and skip unsupported queries.
- JobSystem fixes.

To contribute to the documentation, refer to the
`Programming Guide in the GitHub repo <https://github.com/ROCm/roc-optiq/blob/main/CODING.md>`_.

You can find licensing information on the
:doc:`License <license>` page.
