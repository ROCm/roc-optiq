.. meta::
  :description: Learn what ROCm Optiq is: a unified GUI for visualizing ROCm Systems Profiler traces and ROCm Compute Profiler analysis data on Windows, Linux, and macOS.
  :keywords: Optiq, ROCm, profiler, visualization, trace, analysis, GPU, AMD, performance

*******************
What is ROCm Optiq?
*******************

ROCm Optiq is a unified visualization and analysis tool for performance data collected by ROCm profiling tools. It reads trace databases produced by `ROCm Systems Profiler <https://rocm.docs.amd.com/projects/rocprofiler-systems/en/latest/index.html>`_ and analysis databases produced by `ROCm Compute Profiler <https://rocm.docs.amd.com/projects/rocprofiler-compute/en/latest/>`_, and renders both in a single interactive interface.

ROCm Optiq has no dependency on the ROCm stack itself, so trace and analysis files can be visualized on any machine running a supported Windows, Linux, or macOS operating system, independent of where the data was collected.

.. _trace-file:

Visualize ROCm Systems Profiler traces
======================================

Use ROCm Optiq to inspect CPU-GPU interactions, ROCm API calls, kernel execution timelines, memory usage, and system telemetry data for applications running on the ROCm stack. 

.. image:: /images/optiq-systems.png
   :width: 800
   :align: center
   :alt: ROCm Optiq timeline view showing CPU and GPU activity tracks for a ROCm Systems Profiler trace

ROCm Optiq helps you identify stalls, memory bandwidth issues, and inefficient kernel launches. 
It correlates GPU workloads with in-application CPU events and performance with hardware resource usage, providing a holistic view for optimization. 

Key views include: 

- :ref:`topology`: Displays hardware (processors, queues, counters) and software hierarchy (processes, streams, threads, sampled threads), enabling clear navigation and correlation between application execution and hardware resources.
- :ref:`timeline`: Displays CPU and GPU activities, events, and performance metrics in chronological order. It supports tools for zooming, filtering, and bookmarking for detailed analysis.
- :ref:`advanced`: Provides an in-depth view of profiling data, enabling you to analyze performance metrics and event-specific information. This section provides a unified interface for multiple data perspectives, offering granular insights.
- :ref:`summary-view`: Displays the top 10 kernels by execution time using pie charts, bar charts, or tables.
- :ref:`minimap`: Provides a compact overview of event density and counter values across the entire trace, enabling rapid navigation of large datasets.

Analyze ROCm Compute Profiler data
==================================

Use ROCm Optiq to visualize profiling analysis data to rapidly identify performance bottlenecks and explore kernel-level metrics for a profiled workload. 

.. image:: /images/optiq-compute.png
   :width: 800
   :align: center
   :alt: ROCm Optiq analysis view showing roofline chart and kernel metrics for a ROCm Compute Profiler workload

Key features include: 

- :ref:`analysis-summary`: High-level overview of the captured compute profiling data. 
- :ref:`kernel-details`: Focuses on individual kernels.  
- :ref:`analysis-table`: Complete list of available metrics for the selected kernel. Metrics are grouped by category. 
- :ref:`analysis-workload`: Contextual information about the profiled workload, including system information and profiling configuration. 
- :ref:`baseline-comparison`: A side-by-side view that compares two workload measurements (baseline vs. target) to quickly identify and assess performance regressions or improvements. 

.. _glance-data-sources:

Supported data sources
=========================

.. list-table::
   :header-rows: 1
   :widths: 25 12 28 35

   * - Input format
     - Extension
     - Producer
     - ROCm requirement (trace collection)
   * - ROCm Systems Profiler database
     - ``.db``
     - `ROCm Systems Profiler <https://rocm.docs.amd.com/projects/rocprofiler-systems/en/latest/index.html>`_
     - ROCm 7.1.0 or later
   * - ROCm Compute Profiler analysis database
     - ``.db``
     - `ROCm Compute Profiler <https://rocm.docs.amd.com/projects/rocprofiler-compute/en/latest/>`_
     - ROCm 7.12.0 or later
   * - ROCm profiling data
     - ``.rpd``
     - `RPD tracer <https://github.com/ROCm/rocmProfileData/tree/master/rpd_tracer>`_
     - —
   * - ROCm Optiq project file
     - ``.rpv``
     - Saved from a previous ROCm Optiq session
     - —

For details on viewing each format, see :ref:`view-systems` and :ref:`view-analysis`. If a file doesn't open, see :ref:`trace troubleshooting <view-trace-troubleshooting>` and :ref:`analysis troubleshooting <view-analysis-troubleshooting>`.

Welcome page
============

When ROCm Optiq starts without an open project, the Welcome page displays: 

- **Open File:** Open a trace (``.db``, ``.rpd``) or project (``.rpv``). You can also drag and drop a supported file into the window to open it.
- **Recent files:** Quick access to recently opened files. 
- **Documentation links:** For ROCm Optiq, ROCm Systems Profiler, and ROCm Compute Profiler documentation.

.. image:: /images/welcome-page.png
   :width: 800
   :align: center
   :alt: ROCm Optiq welcome page showing Start, Recent files, and Documentation links sections

.. note::

   - Opening a file creates a new tab and activates it. 
   - ROCm Optiq prevents opening a second tab for the same underlying trace database, including the case when a ``.rpv`` project references an already-open ``.db``. 


