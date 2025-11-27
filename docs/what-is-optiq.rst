.. meta::
  :description: ROCm Optiq documentation
  :keywords: Optiq, ROCm, documentation, profiler

********************
What is ROCm Optiq?
********************

Use ROCm™ Optiq to inspect CPU-GPU interactions, ROCm API calls, kernel execution timelines, memory usage, and system telemetry data for applications running on the ROCm stack.

.. image:: /images/optiq.png
   :width: 800

Since ROCm Optiq doesn't have any dependencies with the ROCm stack, trace files captured by the ROCm Systems Profiler can be visualized with the ROCm Optiq GUI on any machine running a Microsoft Windows or Linux operating system.
See :ref:`requirements` for more information.

ROCm Optiq helps you identify stalls, memory bandwidth issues, and inefficient kernel launches. 
It correlates GPU workloads with in-application CPU events and performance with hardware resource usage, providing a holistic view for optimization.

ROCm Optiq is structured around several key panels that work together to deliver a comprehensive profiling experience:

- :ref:`topology`: Displays a hierarchical view of processes, queues, streams, and threads, enabling quick navigation and selective visibility of tracks.
- :ref:`timeline`: Displays CPU and GPU activities, events, and performance metrics chronologically. It has:
  
  - Event tracks that show events such as API calls, kernel dispatches, and more.
  - Sample performance counter tracks are displayed as charts showing the data points. 
  - Tools for zooming, filtering, and bookmarking for detailed analysis.

- :ref:`advanced`: Provides an in-depth view of profiling data, enabling you to analyze performance metrics and event-specific information. This section provides a unified interface for multiple data perspectives, offering granular insights.



