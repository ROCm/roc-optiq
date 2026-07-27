.. meta::
  :description: Learn what ROCm Optiq is: a unified GUI for visualizing ROCm Systems Profiler traces and ROCm Compute Profiler analysis data on Windows and Linux.
  :keywords: Optiq, ROCm, profiler, visualization, trace, analysis, GPU, AMD, performance

*******************
What is ROCm Optiq?
*******************

ROCm Optiq is a unified visualization and analysis tool for performance data collected by ROCm profiling tools, specifically `ROCm Systems Profiler <https://rocm.docs.amd.com/projects/rocprofiler-systems/en/latest/index.html>`_ and `ROCm Compute Profiler <https://rocm.docs.amd.com/projects/rocprofiler-compute/en/latest/>`_. 

Because ROCm Optiq doesn't have any dependencies on the ROCm stack, trace files and profiling data files can be visualized with the ROCm Optiq GUI on any machine running a supported Microsoft Windows or Linux operating system. For more information, see :ref:`requirements`.

Welcome page 
============

When ROCm Optiq starts without an open project, the Welcome page displays: 

- Open File—open a trace (``.db``, ``.rpd``) or project (``.rpv``). You can also drag and drop a supported file into the window to open it.
- Recent files—quick access to recently opened files 
- Documentation links—ROCm Optiq, ROCm Systems Profiler, and ROCm Compute Profiler documentation

.. image:: /images/welcome-page.png
   :width: 800
   :align: center
   :alt: ROCm Optiq welcome page showing Start, Recent files, and Documentation links sections

.. note::

   - Opening a file creates a new tab and activates it. 
   - ROCm Optiq prevents opening a second tab for the same underlying trace database, including when a ``.rpv`` project references an already-open ``.db``. 

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

