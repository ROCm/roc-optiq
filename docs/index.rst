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

ROCm Optiq 0.3.0 
================

Listed below are the incremental changes in this release compared to the previous release (`ROCm Optiq 0.2.0 <https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.2.0/index.html>`_).

Added
-----

ROCm Optiq for visualizing ROCm Compute Profiler's data. New features include:

- **Summary View**: Shows a high-level overview of the captured data.  

  - **Table**: lists the ten longest-running kernels sorted by Total Execution Time. 
  - **Charts**: Plot duration and invocation statistics across kernels. 
  - **Roofline Chart**: plots kernel performance against empirical hardware ceilings to reveal the dominant performance bottleneck for all kernels.

- **Kernel Details**: displays details of each kernel. 

  - **Kernel Selection Table**: Lists kernels with GPU metrics. Use **Add Metric** to append additional GPU metric columns. Per-column search box accepts names or metric expressions (for example, ``metric > threshold``). Click **Apply Filters** to execute; combine multiple filters to narrow the analysis. 
  - **Memory Chart**: Shows memory transactions and throughput per cache hierarchy level for the selected kernel. 
  - **System Speed-of-Light**: Displays key kernel-level performance metrics with unit, average, peak, and percentage of peak values. 
  - **Kernel Roofline Chart**: Shows a kernel-specific roofline analysis to determine whether a kernel is compute-bound or memory-bound. Click the gear icon to access customization options. 

- **Table View**: provides a complete list of available metrics for the selected kernel.
- **Workload Details**: Provides contextual information about the workload.

Changed
-------

Changes in ROCm Optiq for visualizing ROCm System Profiler traces: 

- System Topology tree was restructured to show both hardware and software topologies. 
- Memory allocation activity tracks are now displayed in Timeline and System Topology Views. 
- RPD files populate Topology. 
- Multi-node support: Time normalization for multi-node configurations. 

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
