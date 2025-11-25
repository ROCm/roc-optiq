.. meta::
  :description: ROCm Optiq install 
  :keywords: ROCm Optiq, ROCm, install, profiler

************************
ROCm™ Optiq installation
************************


System requirements
===================

Your system must be running one of these operating systems:

-	Windows 11
-	Ubuntu 22.04 / Ubuntu 24.04
-	Oracle Linux 9 / Oracle Linux 10
-	RHEL 9 / RHEL 10

.. note::

   It's recommended to have at least 16GB of RAM to run large traces.


Install on Windows
==================

1. Download the **ROCm-Optiq-Beta.exe** installer from `https://github.com/ROCm/roc-optiq <https://github.com/ROCm/roc-optiq>`_ and follow the instructions in the install wizard.
   
   .. image:: ../images/wizard.png
      :width: 800
      :align: center

2. Accept the agreement to install, then follow the installation instructions.

   .. image:: ../images/agreement.png
      :width: 800
      :align: center
  
3. Launch **roc-optiq.exe** from the installation directory.


Install on Linux
================

1. Install the package for the target distribution in `https://github.com/ROCm/roc-optiq <https://github.com/ROCm/roc-optiq>`_. Use the corresponding Debian, rpm, taz.gz, or binary packages available in the GitHub repo.
2. Run the **roc-optiq** binary.
