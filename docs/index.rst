.. meta::
  :description: ROCm Optiq documentation
  :keywords: Visualizer, ROCm, documentation, profiler

************************
ROCm Optiq documentation
************************

:doc:`ROCm Optiq <what-is-optiq>` is a unified visualization and analysis tool for performance data collected by ROCm profiling tools, specifically `ROCm Systems Profiler <https://rocm.docs.amd.com/projects/rocprofiler-systems/en/latest/index.html>`_ and `ROCm Compute Profiler <https://rocm.docs.amd.com/projects/rocprofiler-compute/en/latest/>`_. 
It provides deep insights into both system-level behavior and kernel-level performance for applications running on the ROCm stack. 

ROCm Optiq enables developers to visualize execution traces and profiling analysis data in a single interface, helping to identify performance bottlenecks, understand hardware utilization, optimize workloads, and efficiently scale applications across CPUs and GPUs. 

The component public repository is located at `https://github.com/ROCm/roc-optiq <https://github.com/ROCm/roc-optiq>`_.

.. note::

  ROCm Optiq is in beta. Running production workloads is not recommended.

ROCm Optiq (Beta) 0.4.0 
=======================

Added
-----

New visualization features for analysis data include: 

- Summary View – Speed of Light: provides an aggregated, system-level summary of key performance and hardware utilization metrics across all kernels, showing utilization relative to architectural peak capabilities. The Percent-of-Peak values help quickly identify whether the workload is limited. 
- Kernel Details - Kernel Selection Table: added bar chart visualization for values of metrics and a tooltip feature for displaying kernels’ full names. 
- Baseline Comparison: enables you to compare two workload measurements (baseline vs. target) side by side in a unified table. It helps to quickly spot regressions, improvements, and behavior changes. It highlights per-metric deltas (including percentage change) to make the performance impact easy to quantify. 
- Added support for ROCm compute profiler’s database schema 1.3 and related performance improvements. 

Other new features: 

- Data clean-up: remove metadata added by ROCm-Optiq in database file. 
- Command line Interface support. 
- OpenGL backend as a fallback when Vulkan is unavailable; optional software rendering path; command-line option to force a specific graphics backend. 
- Dear ImGui updated (docking-capable line, ImPlot aligned); Linux session defaults to X11 for compatibility. 
- Windows packages link GLFW statically by default (no separate glfw.dll in the installer). 
- New settings panel allowing keyboard shortcuts to be customized. 

Changed
-------

Changes in viewing analysis data include: 

- Legend can be repositioned in Roofline charts of Summary View and Kernel Details; multi-workload chart fixes and top-kernels presentation updates. 
- Kernel Details – Kernel Selection Table updates: Added mini-graphs in cells, pinned title/header improvements, and a global toggle for inline charts. Added tooltip for displaying clipped names and adjusted name-column sizing to free space for metrics. 

Changes in viewing trace data include: 

- Topology View and Timeline:  
- Enhanced System Topology tree for better representation of hardware and software topologies.  
- Toggle on device nodes to show or hide all tracks under that device. 

Navigation and inspection:  

- Go-to-event behavior across tables, flow panel, and call stack panel. Double-click or context menu to open the event on the timeline, with vertical track centering and highlight feedback.  
- Highlight-on-navigate with a dedicated event path, pulsing indicator, and timed auto-clear; selection and highlight handled independently 

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

To contribute to the documentation, refer to the
`Programming Guide in the GitHub repo <https://github.com/ROCm/roc-optiq/blob/main/CODING.md>`_.

You can find licensing information on the
:doc:`License <license>` page.
