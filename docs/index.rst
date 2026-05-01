.. meta::
  :description: ROCm Optiq documentation
  :keywords: Visualizer, ROCm, documentation, profiler

************************
ROCm Optiq documentation
************************

:doc:`ROCm Optiq <what-is-optiq>` is a unified visualization and analysis tool for performance data collected by ROCm profiling tools, specifically `ROCm Systems Profiler <https://rocm.docs.amd.com/projects/rocprofiler-systems/en/latest/index.html>`_ and `ROCm Compute Profiler <https://rocm.docs.amd.com/projects/rocprofiler-compute/en/latest/>`_. 
It provides deep insights into both system-level behavior and kernel-level performance for applications running on the ROCm stack. 

ROCm Optiq enables developers to visualize execution traces and profiling analysis data in a single interface, helping to identify performance bottlenecks, understand hardware utilization, optimize workloads, and efficiently scale applications across CPUs and GPUs. 

The ROCm Optiq project repository is located at `https://github.com/ROCm/roc-optiq <https://github.com/ROCm/roc-optiq>`_.

.. note::

  ROCm Optiq is in beta. Running production workloads is not recommended.

ROCm Optiq (Beta) 0.4.0 
=======================

Added
-----

New visualization features for analyzing data include: 

- Summary View – Speed of Light: Provides an aggregated, system-level summary of key performance and hardware utilization metrics across all kernels, showing utilization relative to architectural peak capabilities. The Percent-of-Peak values help quickly identify whether the workload is limited. 
- Kernel Details - Kernel Selection Table: Added a bar chart visualization of metric values and a tooltip that displays kernels’ full names. 
- Baseline Comparison: Enables you to compare two workload measurements (baseline vs. target) side-by-side in a unified table. It helps to quickly spot regressions, improvements, and behavior changes. It highlights per-metric deltas (including percentage change) to make the performance impact easy to quantify. 
- Added support for ROCm compute profiler's database schema 1.3 and related performance improvements. 
- Presets: Save and recall pinned metric configurations for Table View and Baseline Comparison.
- New context menu to add metrics to Kernel Selection Table from Table View.
- Configurable delta-threshold control for Baseline Comparison.

Other new features: 

- Data clean-up: Enables the removal of metadata added by ROCm Optiq in a database file. 
- Command-line interface support. 
- OpenGL backend as a fallback when Vulkan is unavailable; optional software rendering path; command-line option to force a specific graphics backend. 
- Dear ImGui updated (docking-capable line, ImPlot aligned); Linux session defaults to X11 for compatibility. 
- Microsoft Windows packages link GLFW statically by default (no separate ``glfw.dll`` in the installer). 
- New settings panel allowing keyboard shortcuts to be customized. 

Changed
-------

Changes in viewing analysis data include: 

- Roofline charts for Summary View and Kernel Details: legend can be repositioned; aspect ratio follows the window; multi-workload chart fixes and top-kernels presentation updates.
- Kernel Details updates: Added mini-graphs in cells, pinned title/header improvements, and a global toggle for inline charts. Added a tooltip to display clipped names and adjusted the name-column sizing to free up space for metrics. 

Changes in viewing trace data include: 

- Topology View and Timeline:  
  
  - Enhanced System Topology tree for better representation of hardware and software topologies.  
  - Toggle on device nodes to show or hide all tracks under that device. 

Navigation and inspection:  

- Use **Go To Event** in tables and the **Flow Data** panel to go to a specific event. Double-click the event, or click **Go To Event** from the right-click context menu, to open the event on the timeline with vertical track centering and highlighted feedback.
- Highlight-on-navigate with a dedicated event path, pulsing indicator, and timed auto-clear; selection and highlight handled independently.
- Callstack experience improvements. 

Resolved issues
---------------

- Fixed an issue where metrics that reference ``None`` return N/A.
- Fixed an issue where ``workload_name`` is missing in ``sysinfo.csv`` when using ``--output-directory``. 

Known issues
------------

- On **Call Stack Data**, for instrumented threads, some call stack information such as address, file, and PC isn't available. 

.. tip::

  See :doc:`ROCm Optiq release history <release>` for the full release history. 

.. grid:: 2
  :gutter: 3

  .. grid-item-card:: Install

    * :doc:`ROCm Optiq installation </install/optiq-install>`

  .. grid-item-card:: How to

    * :doc:`View trace data <how-to/view-trace>`
    * :doc:`View analysis data <how-to/view-analysis>`
    * :doc:`Customize your project <how-to/customize-views>`

  .. grid-item-card:: Reference

    * :doc:`Command-line support <reference/cli-support>`

To contribute to the documentation, refer to the
`Programming Guide in the GitHub repo <https://github.com/ROCm/roc-optiq/blob/main/CODING.md>`_.

You can find licensing information on the
:doc:`License <license>` page.
