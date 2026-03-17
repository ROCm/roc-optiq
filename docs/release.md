:orphan:

<head>
  <meta charset="UTF-8">
  <meta name="description" content="ROCm Optiq (Beta) release history">
  <meta name="keywords" content="documentation, release history, ROCm, AMD">
</head>

# ROCm Optiq (Beta) release history

| Version | Release date |
| ------- | ------------ |
| [Beta 0.3.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.3.0/index.html) | March 26, 2026 |
| [Beta 0.2.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.2.0/index.html) | February 11, 2026 |
| [Beta 0.1.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.1.0/index.html) | December 10, 2025 |

## ROCm Optiq (Beta) 0.3.0 

### Added

ROCm Optiq for visualizing ROCm Compute Profiler's data. New features include:

- **Summary View**: Shows a high-level overview of the captured data.  

  - **Table**: lists the top 10 longest-running kernels sorted by Total Execution Time. 
  - **Charts**: Plot duration and invocation statistics across kernels. 
  - **Roofline Chart**: plots kernel performance against empirical hardware ceilings to reveal the dominant performance bottleneck for all kernels.

- **Kernel Details**: displays details of each kernel. 

  - **Kernel Selection Table**: Lists kernels with GPU metrics. Use `Add Metric` to append additional GPU metric columns. Per-column search box accepts names or metric expressions (e.g., `metric > threshold`). Click `Apply Filters` to execute; combine multiple filters to narrow the analysis. 
  - **Memory Chart**: Shows memory transactions and throughput per cache hierarchy level for the selected kernel. 
  - **System Speed-of-Light**: Displays key kernel-level performance metrics with unit, average, peak, and percentage of peak values. 
  - **Kernel Roofline Chart**: Shows a kernel-specific roofline analysis to determine whether a kernel is compute-bound or memory-bound. Click the gear icon to access customization options. 

- **Table View**: provides a complete list of available metrics for the selected kernel.
- **Workload Details**: Provides contextual information about the workload.

### Changed

Changes in ROCm Optiq for visualizing ROCm System Profiler traces: 

- System Topology tree was restructured to show hardware and software topologies. 
- Memory allocation activity tracks are now displayed in Timeline and System Topology Views. 
- RPD files populate Topology. 
- **Multinode support**: Time normalization for multi-node configurations. 

## ROCm Optiq (Beta) 0.2.0

### Added

- **Summary View**: Displays the top ten kernels by execution time using pie charts, bar charts, or tables.
- **Minimap**: Provides a compact overview of event density and counter values across the entire trace, enabling rapid navigation of large datasets.

### Changed

- **Timeline View**: Improved navigation and selection. Added context menu option to create a time range filter from a selected event or events. 
- **Advanced Details Panel**: **Aggregate by Column** drop-down groups the results by the selected column. Options to size columns to fit in **Event Table** and **Sample Table**. **Event Details** now shows the function call’s arguments, if available.  
- **Time Range Filtering**: Improved time range selection.
- **Histogram**: Shows event density in two display modes: "Normalization: All Tracks" and "Normalization: Visible Tracks".
- **Multi-node**: Multi-node data and a new multi-database yaml file format are supported.

## ROCm Optiq (Beta) 0.1.0

Initial release of ROCm Optiq (Beta).

### Added

- **System Topology View**: Displays a hierarchical representation of the hardware or system components, such as nodes, processes, as well as the GPU queues, memory operations, threads, and more that belong to them.
- **Timeline View**: Shows CPU and GPU activities, events, and performance metrics in chronological order for a detailed temporal analysis. ROCm Optiq allows you to zoom, filter, and bookmark data for fine-grained inspection. You can correlate GPU workloads with in-application CPU events and performance with hardware resource usage, enabling easy identification and remediation of performance blockers.
- **Advanced Details Panel**: Provides an in-depth view of profiling data, enabling you to analyze performance metrics and event-specific information. It offers SQL-like filters and group-by operations.
- **Histogram**: Shows the event density across all visible tracks and highlights the zoomed-in region to quickly identify hotspots.
- **Time Range Filtering**: Select a specific time interval to filter events and counter samples for focused analysis.
- **Event Search**: Quickly locate target events.