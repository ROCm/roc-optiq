.. meta::
  :description: ROCm Optiq documentation
  :keywords: Optiq, ROCm, documentation, profiler

********************
What is ROCm™ Optiq?
********************

ROCm™ Optiq allows you to inspect CPU-GPU interactions, ROCm API calls, kernel execution timelines, memory usage, and system telemetry data for applications running on the ROCm stack.

.. image:: /images/optiq.png
   :width: 800

Since ROCm Optiq does not have any dependencies with the ROCm stack, trace files captured by the ROCm Systems Profiler can be visualized with the ROCm Optiq GUI on any machine that has: 

-	Windows 11
-	Ubuntu 22.04 / Ubuntu 24.04
-	Oracle Linux 9 / Oracle Linux 10
-	RHEL 9 / RHEL 10

ROCm Optiq helps you identify stalls, memory bandwidth issues, and inefficient kernel launches. 
Correlating GPU workloads with in-application CPU events and performance with hardware resource usage provides a holistic view for optimization.

ROCm Optiq is structured around several key panels that work together to deliver a comprehensive profiling experience:

- **System Topology Tree**: Displays a hierarchical view of processes, queues, streams, and threads which enables quick navigation and selective visibility of tracks.
- **Timeline View**: Displays CPU and GPU activities, events, and performance metrics chronologically. It has:
  
  - Event tracks such as API calls, kernel dispatches, and more. 
  - Sample performance Counter tracks in the form of charts displaying the data points. 
  - Tools for zooming, filtering, and bookmarking for detailed analysis.

- **Advanced Details**: This section provides an in-depth view of profiling data, enabling you to analyze performance metrics and event-specific information. This section provides a unified interface for multiple data perspectives, offering granular insights. See :ref:`advanced` for more information.



