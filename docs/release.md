:orphan:

<head>
  <meta charset="UTF-8">
  <meta name="description" content="ROCm Optiq (Beta) release history">
  <meta name="keywords" content="documentation, release history, ROCm, AMD">
</head>

# ROCm Optiq (Beta) release history

| Version | Release date |
| ------- | ------------ |
| [Beta 0.4.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.4.0/index.html) | May 6, 2026 |
| [Beta 0.3.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.3.0/index.html) | March 26, 2026 |
| [Beta 0.2.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.2.0/index.html) | February 11, 2026 |
| [Beta 0.1.0](https://rocm.docs.amd.com/projects/roc-optiq/en/beta-0.1.0/index.html) | December 10, 2025 |

## ROCm Optiq (Beta) 0.4.0 

### Added

New visualization features for analysis data include: 

- Summary View – Speed of Light: Provides an aggregated, system-level summary of key performance and hardware utilization metrics across all kernels, showing utilization relative to architectural peak capabilities. The Percent-of-Peak values help quickly identify whether the workload is limited. 
- Kernel Details - Kernel Selection Table: Added a bar chart visualization for values of metrics and a tooltip feature for displaying kernels' full names. 
- Baseline Comparison: Enables you to compare two workload measurements (baseline vs. target) side-by-side in a unified table. It helps to quickly spot regressions, improvements, and behavior changes. It highlights per-metric deltas (including percentage change) to make the performance impact easy to quantify. 
- Added support for ROCm compute profiler's database schema 1.3 and related performance improvements. 
- Presets: Save and recall pinned metric configurations for Table View and Baseline Comparison.
- New context menu to add metrics to Kernel Selection Table from Table View.
- Configurable delta-threshold control for Baseline Comparison.

Other new features: 

- Data clean-up: Enables removal of metadata added by ROCm Optiq in a database file. 
- Command-line interface support. 
- OpenGL backend as a fallback when Vulkan is unavailable; optional software rendering path; command-line option to force a specific graphics backend. 
- Dear ImGui updated (docking-capable line, ImPlot aligned); Linux session defaults to X11 for compatibility. 
- Windows packages link GLFW statically by default (no separate ``glfw.dll`` in the installer). 
- New settings panel allowing keyboard shortcuts to be customized. 

### Changed

Changes in viewing analysis data include: 

- The legend can be repositioned in the Roofline charts section of the Summary View and Kernel Details; it includes multi-workload chart fixes and top-kernels presentation updates. 
- Kernel Details updates: Added mini-graphs in cells, pinned title/header improvements, and a global toggle for inline charts. Added a tooltip for displaying clipped names and adjusted the name-column sizing to free space for metrics. 

Changes in viewing trace data include: 

- Topology View and Timeline:  
  
  - Enhanced System Topology tree for better representation of hardware and software topologies.  
  - Toggle on device nodes to show or hide all tracks under that device. 

Navigation and inspection:  

- Use **Go To Event** in tables and the **Flow Data** panel to go to a specific event. Double-click the event, or click **Go To Event** from the right-click context menu, to open the event on the timeline with vertical track centering and highlighted feedback.
- Highlight-on-navigate with a dedicated event path, pulsing indicator, and timed auto-clear; selection and highlight handled independently.
- Callstack experience improvements. 

### Resolved issues

- Fixed issue where metrics that reference ``None`` return N/A.
- Fixed issue where ``workload_name`` is missing in ``sysinfo.csv`` when using ``--output-directory``. 

### Known issues

- On **Call Stack Data**, for instrumented threads, some call stack information such as address, file, and PC isn't available. 

## ROCm Optiq (Beta) 0.3.0 

### Added

ROCm Optiq for visualizing ROCm Compute Profiler's data. New features include:

- **Summary View**: Shows a high-level overview of the captured data.  

  - **Table**: lists the top 10 longest-running kernels sorted by Total Execution Time. 
  - **Charts**: Plot duration and invocation statistics across kernels. 
  - **Roofline Chart**: plots kernel performance against empirical hardware ceilings to reveal the dominant performance bottleneck for all kernels.

- **Kernel Details**: displays details of each kernel. 

  - **Kernel Selection Table**: Lists kernels with GPU metrics. Use **Add Metric** to append additional GPU metric columns. Per-column search box accepts names or metric expressions (for example, `metric > threshold`). Click **Apply Filters** to execute; combine multiple filters to narrow the analysis. 
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
- Multinode support: Time normalization for multi-node configurations.

### Known issues

#### Metrics that reference ``None`` return N/A

If a metric expression contains ``None``, ROCm Compute Profiler may ignore the metric value even when it isn't ``None``. As a result, ROCm-Optiq displays **N/A** for affected metrics. 

- System Speed of Light (0200) 
  - VALU Active Threads 
  - LDS Bank Conflicts/Access 
  - vL1D Cache Hit Rate 
  - L2 Cache Hit Rate 
  - L2-Fabric Read Latency 
  - L2-Fabric Write Latency 
  - sL1D Cache Hit Rate 
  - L1I Fetch Latency 
- Memory Chart (0300) 
  - LDS Latency 
  - VL1 Hit 
  - VL1 Lat 
  - VL1 Coalesce 
  - VL1 Stall 
  - sL1D Hit 
  - sL1D Lat 
  - IL1 Lat 
  - L2 Rd Lat 
  - L2 Wr Lat 
- Command Processor CPC/CPF (0500) 
  - CPF Utilization 
  - CPF Stall 
  - CPF-L2 Utilization 
  - CPF-L2 Stall 
  - CPF-UTCL1 Stall 
  - CPC SYNC FIFO Full Rate 
  - CPC CANE Stall Rate 
  - CPC ADC Utilization 
  - CPC Utilization 
  - CPC Stall Rate 
  - CPC Packet Decoding Utilization 
  - CPC-Workgroup Manager Utilization 
  - CPC-L2 Utilization 
  - CPC-UTCL1 Stall 
  - CPC-UTCL2 Utilization 
- Workgroup Manager SPI (0600) 
  - VGPR Writes 
  - SGPR Writes 
  - Not-scheduled Rate (Workgroup Manager) 
  - Not-scheduled Rate (Scheduler-Pipe) 
  - Scheduler-Pipe FIFO Full Rate 
  - Scheduler-Pipe Stall Rate 
  - Scratch Stall Rate 
- Compute Units Compute Pipeline (1100) 
  - VALU Active Threads 
  - MFMA Instruction Cycles 
  - VMEM Latency 
  - SMEM Latency 
- Local Data Share LDS (1200) 
  - Bank Conflict Rate 
  - LDS Latency 
  - Bank Conflicts/Access 
- Scalar L1 Data Cache (1400) 
  - Cache Hit Rate
- Vector L1 Data Cache (1600) 
  - Hit rate 
  - Utilization 
  - Coalescing 
  - Stalled on L2 Data 
  - Stalled on L2 Req 
  - Stalled on Address 
  - Stalled on Data 
  - Stalled on Latency FIFO 
  - Stalled on Request FIFO 
  - Stalled on Read Return 
  - Tag RAM Stall (Read) 
  - Tag RAM Stall (Write) 
  - Tag RAM Stall (Atomic) 
  - Cache Hit Rate 
  - Hit Ratio 
- L2 Cache (1700) 
  - HBM Read Traffic 
  - Remote Read Traffic 
  - Uncached Read Traffic 
  - HBM Write and Atomic Traffic 
  - Remote Write and Atomic Traffic 
  - Atomic Traffic 
  - Uncached Write and Atomic Traffic 
  - Read Latency 
  - Write and Atomic Latency 
  - Atomic Latency 
  - Read Stall 
  - Write Stall 
  - Cache Hit 
  - Read - PCIe Stall 
  - Read - Infinity Fabric Stall 
  - Read - HBM Stall 
  - Write - PCIe Stall 
  - Write - Infinity Fabric Stall 
  - Write - HBM Stall 
  - Write - Credit Starvation 

#### ``workload_name`` is missing in ``sysinfo.csv`` when using ``--output-directory`` 

When you profile with the ``--output-directory`` option, the ``workload_name`` column in ``sysinfo.csv`` might be empty. This can prevent views in the ROCm Compute Profiler analysis database from joining tables based on ``workload_name``, which makes system information unavailable. 

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

