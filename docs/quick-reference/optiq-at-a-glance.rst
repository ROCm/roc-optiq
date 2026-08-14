.. meta::
  :description: A quick reference for ROCm Optiq's supported data sources and session management
  :keywords: ROCm Optiq at a glance, quick reference, supported data sources, rocpd, trace, analysis, session management, bookmarks

.. _optiq-at-a-glance:

***********************
ROCm Optiq at a glance
***********************

ROCm Optiq is a unified visualization and analysis tool for performance data collected by ROCm profiling tools. It reads trace databases produced by `ROCm Systems Profiler <https://rocm.docs.amd.com/projects/rocprofiler-systems/en/latest/index.html>`_ and analysis databases produced by `ROCm Compute Profiler <https://rocm.docs.amd.com/projects/rocprofiler-compute/en/latest/>`_, and renders both in a single interactive interface.

ROCm Optiq has no dependency on the ROCm stack itself, so trace and analysis files can be visualized on any machine running a supported Windows or Linux operating system, independent of where the data was collected.

This topic is a quick reference to ROCm Optiq's supported inputs and session features. For a narrative introduction and an overview of the available views, see :doc:`../what-is-optiq` For system requirements and installation steps, see :ref:`requirements` and :doc:`../install/optiq-install`.

.. _glance-data-sources:

Supported data sources
=========================

.. list-table::
   :header-rows: 1
   :widths: 25 12 28 35

   * - Input format
     - Extension
     - Producer
     - ROCm requirement (trace collection)
   * - ROCm Systems Profiler database
     - ``.db``
     - `ROCm Systems Profiler <https://rocm.docs.amd.com/projects/rocprofiler-systems/en/latest/index.html>`_
     - ROCm 7.1.0 or later
   * - ROCm Compute Profiler analysis database
     - ``.db``
     - `ROCm Compute Profiler <https://rocm.docs.amd.com/projects/rocprofiler-compute/en/latest/>`_
     - ROCm 7.12.0 or later
   * - rocprofv3 profiling database
     - ``.db``
     - `ROCprofiler-SDK <https://rocm.docs.amd.com/projects/rocprofiler-sdk/en/latest/index.html>`_ (``rocprofv3``)
     - —
   * - ROCm profiling data
     - ``.rpd``
     - `RPD tracer <https://github.com/ROCm/rocmProfileData/tree/master/rpd_tracer>`_
     - —
   * - ROCm Optiq project file
     - ``.rpv``
     - Saved from a previous ROCm Optiq session
     - —

See :ref:`view-systems` and :ref:`view-analysis` for details on viewing each format, or :ref:`view-trace-troubleshooting` and :ref:`view-analysis-troubleshooting` if a file doesn't open.

.. _glance-session-management:

Session management
======================

A ROCm Optiq project file (``.rpv``) saves the full analysis session state — the referenced trace file, track layout, zoom position, bookmarks, and annotations. Opening a ``.rpv`` file restores the session exactly as it was left. For the full set of customization options, including time-range filters, annotations, and presets, see :ref:`customize`.

Bookmark navigation:

.. list-table::
   :header-rows: 1

   * - Action
     - Control
   * - Save a view bookmark
     - **Ctrl** + **0** - **9**
   * - Restore a view bookmark
     - **0** - **9**
