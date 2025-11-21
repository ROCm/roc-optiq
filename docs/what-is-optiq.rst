.. meta::
  :description: ROCm-Optiq documentation
  :keywords: Visualizer, ROCm, documentation, profiler

*******************
What is ROCm-Optiq?
*******************

ROCm-Optiq allows you to inspect CPU-GPU interactions, ROCm API calls, kernel execution timelines, memory usage and system telemetry data for applications running on the ROCm stack.

Since ROCm-Optiq does not have any dependencies with the ROCm stack, trace files captured by the ROCm Systems Profiler can be visualized with the ROCm-Optiq GUI on any machine that has: 

-	Windows 11
-	Ubuntu 22.04 / Ubuntu 24.04
-	Oracle Linux 9 / Oracle Linux 10
-	RHEL 9 / RHEL 10

ROCm-Optiq helps you identify stalls, memory bandwidth issues, and inefficient kernel launches. Correlating GPU workloads with in-application CPU events, and performance with hardware resource usage provides a holistic view for optimization.

ROCm-Optiq is structured around several key panels that work together to deliver a comprehensive profiling experience:

- **System Topology Tree**: Displays a hierarchical view of processes, queues, streams, and threads, enabling quick navigation and selective visibility of tracks.
- **Timeline View**: Displays CPU and GPU activities, events and performance metrics chronologically. 
  
  - It shows Event tracks such as API calls, kernel dispatches, and more. 
  - It also visualizes sample performance Counter tracks in the form of charts displaying the data points. 
  - The tool allows zooming, filtering, and bookmarking for detailed analysis. Useful for correlating GPU workloads with in-application CPU events, and performance with hardware resource usage, allowing for performance blockers to be easily identified and remedied.

- **Advanced Details**: This section provides an in-depth view of profiling data, enabling you to analyze performance metrics and event-specific information with precision. This section provides a unified interface for multiple data perspectives, offering granular insights through these components:

  - **Event Table**: Displays all events within the selected tracks. You can refine your analysis by applying a time range selection or executing customized SQL-like queries, ensuring targeted event exploration.
  - **Sample Table**: Presents all performance counter data points associated with the selected tracks. Similar to the **Event Table**, it supports time range selection and SQL-like query capabilities for detailed performance analysis.
  - **Event Details**: Offers comprehensive information about individual events, including attributes and contextual data, to help you understand event behavior and impact.
  - **Track Details**: Provides detailed information about selected tracks, enabling you to examine track-specific characteristics.
  - **Annotations**: Displays user-created annotations, allowing for easier navigating across critical points within large traces, enhancing collaboration and sharing knowledge.




