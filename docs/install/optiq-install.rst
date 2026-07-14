.. meta::
  :description: Learn how to install ROCm Optiq on Windows, Linux (Ubuntu, RHEL, CentOS), and macOS, including system requirements and verification steps.
  :keywords: ROCm Optiq, ROCm, install, profiler

******************
Install ROCm Optiq
******************

Install ROCm Optiq for Windows or Linux using the installation files in the `https://github.com/ROCm/roc-optiq <https://github.com/ROCm/roc-optiq>`_  GitHub repository.

.. _requirements:

System requirements
===================

- You must be running ROCm 7.1.0 or later for ROCm Systems Profiler database file support, and ROCm 7.12.0 or later for ROCm Compute Profiler analysis database file support.

  .. tip::

   Use this command to check which ROCm version is installed on your machine: ``cat /opt/rocm/.info/version``.

- For LDS roofline chart support, ROCm Compute Profiler analysis database schema 1.4.0 is required (ROCm 7.14.0 or later). 
- Your system must be running one of these operating systems:

  - Microsoft Windows 11
  - Ubuntu 22.04 / 24.04 (Debian-based) 
  - CentOS Stream 9 
  - macOS 14 or 15 

.. note::

   Use at least 16 GB of RAM to run large traces in ROCm Optiq.


Install on Windows
==================

1. Download the **ROCm-Optiq-Beta.exe** installer from the `ROCm Optiq GitHub repo <https://github.com/ROCm/roc-optiq/releases/tag/v0.5.0-optiq>`_ and follow the instructions in the install wizard.
   
   .. image:: ../images/wizard.png
      :width: 500
      :alt: ROCm Optiq installation wizard welcome screen on Windows

2. Accept the agreement to install, then follow the installation instructions.

   .. image:: ../images/agreement.png
      :width: 500
      :alt: ROCm Optiq installer license agreement screen

3. Launch **roc-optiq.exe** from the installation directory.


Install on Linux
================

Identify the operating system
------------------------------

If you're unsure of the Linux distribution and version, the ``/etc/os-release`` and ``/usr/lib/os-release`` files contain this information.

.. code-block:: shell

   $ cat /etc/os-release
   NAME="Ubuntu"
   VERSION_ID="24.04"
   VERSION="24.04.3 LTS (Noble Numbat)"
   VERSION_CODENAME=noble
   ID=ubuntu

The relevant fields are ``ID`` and the ``VERSION_ID``.

Ubuntu (Debian-based)
---------------------

1. Download the ``.deb`` package from the `ROCm Optiq GitHub repo <https://github.com/ROCm/roc-optiq/releases/tag/v0.5.0-optiq>`_.
2. Install the ``.deb`` package:

   .. code-block:: shell

      sudo apt install ./<file>.deb

3. Verify the installation:

   .. code-block:: shell

      dpkg -l | grep roc-optiq

.. tip::

   Download the latest ``.deb`` from the `ROCm Optiq GitHub repo <https://github.com/ROCm/roc-optiq/releases/tag/v0.5.0-optiq>`_ to ensure ROCm Optiq is up-to-date.

Check the ROCm Optiq version
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use these commands to verify the ROCm Optiq version (when installed on Ubuntu):

.. code-block:: shell

   apt show roc-optiq

Add ROCm binaries to your ``PATH``
----------------------------------

Once you've installed ROCm Optiq for your operating system, add the ROCm binaries to your ``PATH`` if it isn't automatically configured:

.. code-block:: shell

   export PATH=opt/roc-optiq/bin:$PATH

Install on CentOS Stream 9
==========================

1. Download the appropriate ``.rpm`` package from GitHub Releases. 
2. Install the package: ``sudo dnf install ./roc-optiq-*.rpm``. 
3. Verify the installation: ``rpm -q roc-optiq``. 

Install on macOS
================

1. Download ``roc-optiq_0.5.0_macos.zip`` from `GitHub Releases <https://github.com/ROCm/roc-optiq/releases>`_ or your organization's artifact registry. 
2. Drag and drop ``roc-optiq.app`` from the ``roc-optiq_0.5.0_macos`` folder to the ``Applications`` folder. 
3. Launch ROCm Optiq from **Applications**. 

Settings, logs, and presets are stored at: ``~/Library/Application Support/ROCm-Optiq/``.
