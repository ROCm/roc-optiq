:orphan:

<head>
  <meta charset="UTF-8">
  <meta name="description" content="ROCm Optiq (Beta) release history">
  <meta name="keywords" content="documentation, release history, ROCm, AMD">
</head>

# ROCm Optiq (Beta) release history

| Version | Release date |
| ------- | ------------ |
| [(Beta) 0.2.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.2.0/index.html) | February 11, 2026 |
| (Beta) 0.1.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.1.0/index.html) | December 10, 2025 |

## ROCm Optiq (Beta) 0.2.0

### Additions

- **Summary View**: Displays the top 10 kernels by execution time using pie charts, bar charts, or tables.
- **Minimap**: Provides a compact overview of event density and counter values across the entire trace, enabling rapid navigation of large datasets.

### Enhancements/Changes

- **Timeline View**: Improved navigation and selection. Added context menu option to create a time range filter from a selected event or events. 
- **Advanced Details Panel**: **Aggregate by Column** drop-down groups the results by the selected column. Options to size column(s) to fit in **Event Table** and **Sample Table**. **Event Details** now shows the function call’s arguments, if available.  
- **Time Range Filtering**: Improved time range selection.
- **Histogram**: Shows event density in two display modes: "all visible tracks" and "all tracks".
- **Multi-node**: Multi-node data and new multi-database yaml file format are supported.

## ROCm Optiq (Beta) 0.1.0

Initial release of the AMD ROCm Optiq (Beta).

### Additions

- **System Topology View**: Displays a hierarchical representation of the hardware or system components such as nodes, processes as well as the GPU queues, memory operations, threads, and more that belong to them.
- **Timeline View**: Shows CPU and GPU activities, events, and performance metrics in chronological order for a detailed temporal analysis. ROCm Optiq allows you to zoom, filter, and bookmark data for finegrained inspection. You can correlate GPU workloads with in-application CPU events and performance with hardware resource usage, allowing for performance blockers to be easily identified and remedied.
- **Advanced Details Panel**: Provides an in-depth view of profiling data, enabling you to analyze performance metrics and event-specific information. It offers SQL-like filters and group-by operations.
- **Histogram**: Shows the event density across all visible tracks and highlights the zoomed-in region to quickly identify hotspots.
- **Time Range Filtering**: Select a specific time interval to filter events and counter samples for focused analysis.
- **Event Search**: Quickly locate target events.