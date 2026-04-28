.. meta::
  :description: Learn about the command-line support options in ROCm Optiq 
  :keywords: ROCm Optiq, ROCm, command-line, cli

*******************************
ROCm Optiq command-line support
*******************************

This table lists the ROCm Optiq command-line options:

======================  ============================================================================================================================================  =============================================
Command                 Description                                                                                                                                   Example
======================  ============================================================================================================================================  =============================================
``-h, --help``          Display all supported command-line arguments.                                                                                                 ``roc-optiq.exe -h`` 
``-v, --version``       Print the installed ROCm Optiq version.                                                                                                       ``roc-optiq.exe -v`` 
``-f, --file``          Open a Trace or Project File.                                                                                                                 ``roc-optiq.exe -f C:\traces\my_trace.db`` 
``-b, --backend``       Set or force ROCm Optiq to use a specific rendering backend: ``auto`` (default), ``Vulkan``, or ``OpenGL``.
``-d, --file-dialog``   Set file dialog backend: ``auto`` (default), ``native`` (system file dialog), or ``imgui`` (built-in). Use ``imgui`` when running over SSH.
======================  ============================================================================================================================================  ============================================
