.. meta::
  :description: ROCm Optiq documentation
  :keywords: Visualizer, ROCm, documentation, profiler

************************
ROCm Optiq documentation
************************

:doc:`ROCm Optiq <what-is-optiq>` provides deep insights into system-level performance for applications running on the ROCm stack. 

It serves as the GUI for visualizing traces collected by ROCm profiling tools, specifically `ROCm Systems Profiler <https://rocm.docs.amd.com/projects/rocprofiler-systems/en/latest/index.html>`_, enabling developers to analyze traces, identify bottlenecks, optimize workloads, and efficiently scale applications across CPUs and GPUs. 

The component public repository is located at `https://github.com/ROCm/roc-optiq <https://github.com/ROCm/roc-optiq>`_.

.. note::

  ROCm Optiq is in beta. Running production workloads is not recommended.

ROCm Optiq 0.2.0 
================

Listed below are the incremental changes in this release compared to the previous release (`ROCm Optiq 0.1.0 <https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.1.0/index.html>`_).

Added
-----

- **Summary View**: Displays the top ten kernels by execution time using pie charts, bar charts, or tables.
- **Minimap**: Provides a compact overview of event density and counter values across the entire trace, enabling rapid navigation of large datasets.

Changed
-------

- **Timeline View**: Improved navigation and selection. Added context menu option to create a time range filter from a selected event or events. 
- **Advanced Details Panel**: **Aggregate by Column** drop-down groups the results by the selected column. Options to size columns to fit in **Event Table** and **Sample Table**. **Event Details** now shows the function call’s arguments, if available.  
- **Time Range Filtering**: Improved time range selection.
- **Histogram**: Shows event density in two display modes: "Normalization: All Tracks" and "Normalization: Visible Tracks".
- **Multi-node**: Multi-node data and a new multi-database yaml file format are supported.

.. tip::

  See :doc:`ROCm Optiq release history <release>` for the full release history. 

.. grid:: 2
  :gutter: 3

  .. grid-item-card:: Install

    * :doc:`ROCm Optiq installation </install/optiq-install>`

  .. grid-item-card:: How to

    * :doc:`View trace data <how-to/view-trace>`
    * :doc:`Customize your project <how-to/customize-views>`

To contribute to the documentation, refer to the
`Programming Guide in the GitHub repo <https://github.com/ROCm/roc-optiq/blob/main/CODING.md>`_.

You can find licensing information on the
:doc:`License <license>` page.
