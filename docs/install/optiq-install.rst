.. meta::
  :description: Learn how to install ROCm Optiq on Ubuntu, Debian, RHEL, Oracle Linux, Windows, and macOS, including supported versions, install methods, and verification steps.
  :keywords: ROCm Optiq, ROCm, install, profiler, Ubuntu, Debian, RHEL, Oracle Linux, Windows, macOS

******************
Install ROCm Optiq
******************

Install ROCm Optiq using the installation files on the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases>`_ page. Choose your operating system, then follow the matching install method below.

.. _requirements:

Supported operating systems
===========================

ROCm Optiq is available for the following operating systems and versions:

================  ====================
Operating system  Supported versions
================  ====================
Ubuntu            24, 22
Debian            13, 12
RHEL              10, 9, 8
Oracle Linux      10, 9, 8
Windows           11
macOS             26, 15, 14
================  ====================

Each operating system supports one or more install methods:

================  =====================
Operating system  Available methods
================  =====================
Ubuntu            apt, Tarball
Debian            apt, Tarball
RHEL              dnf, Tarball
Oracle Linux      dnf, Tarball
Windows           Windows installer
macOS             .zip
================  =====================

Compare installation methods
============================

Use the following descriptions to choose an installation method for your operating system.

- **apt (Ubuntu / Debian)**: Uses the native ``apt`` package manager. Suits standard installations where the ROCm Optiq package is tracked, updated, and removed through normal system package workflows.
- **dnf (RHEL / Oracle Linux)**: Uses the native ``dnf`` package manager. Suits standard installations where the ROCm Optiq package is tracked, updated, and removed through normal system package workflows.
- **Tarball (any supported Linux distribution)**: Provides ROCm Optiq as a self-contained pre-built archive. Suits controlled or restricted environments requiring manual placement, updates, and removal outside the system package manager.
- **Windows installer**: A guided, wizard-based setup using the ROCm Optiq installer for Windows.
- **.zip (macOS)**: A compressed folder containing the ROCm Optiq application. No installer or package manager is required.

Prerequisites
=============

Before installing ROCm Optiq, confirm the following:

- **Memory**: At least 16 GB of RAM is recommended for working with large traces.

.. note::

   ROCm Optiq only visualizes profiler database files. The machine that runs ROCm Optiq does not need ROCm, ROCm Systems Profiler, or ROCm Compute Profiler installed. The following ROCm versions apply to the profiling host that *generates* the ``.db`` file:

   - ROCm 7.1.0 or later for ROCm Systems Profiler database file support.
   - ROCm 7.12.0 or later for ROCm Compute Profiler analysis database file support.
   - ROCm 7.14.0 or later (ROCm Compute Profiler analysis database schema 1.4.0) for LDS roofline chart support.

   .. tip::

      Use the following command to check which ROCm version is installed on the profiling host:

      .. code-block:: shell

         cat /opt/rocm/.info/version

Linux: identify your distribution and version
=============================================

If you're not sure which distribution or version you're running, the ``/etc/os-release`` and ``/usr/lib/os-release`` files contain this information:

.. code-block:: shell

   $ cat /etc/os-release
   NAME="Ubuntu"
   VERSION_ID="24.04"
   VERSION="24.04.3 LTS (Noble Numbat)"
   VERSION_CODENAME=noble
   ID=ubuntu

The relevant fields are ``ID`` and ``VERSION_ID``.

Installation
============

ROCm Optiq is distributed as direct package downloads from GitHub Releases. Package and file names below are examples that follow the naming convention; confirm the exact names for your release on the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases>`_ page.

Ubuntu / Debian -- apt
----------------------

1. Download the ``.zip`` package for your operating system and version from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases>`_ page:

   ============  ========================================
   OS / version  Example package name
   ============  ========================================
   Ubuntu 24     ``roc-optiq_1.0.0.0_amd64_ubuntu24.zip``
   Ubuntu 22     ``roc-optiq_1.0.0.0_amd64_ubuntu22.zip``
   Debian 13     ``roc-optiq_1.0.0.0_amd64_debian13.zip``
   Debian 12     ``roc-optiq_1.0.0.0_amd64_debian12.zip``
   ============  ========================================

2. Unzip the package to extract the ``.deb`` file:

   .. code-block:: shell

      unzip <file>.zip

3. Install the ``.deb`` package:

   .. code-block:: shell

      sudo apt install ./<file>.deb

4. Verify the installation:

   .. code-block:: shell

      dpkg -l | grep roc-optiq

.. tip::

   The ``roc-optiq`` binary is installed to ``/opt/roc-optiq/bin``. To launch ROCm Optiq by name, add that directory to your ``PATH``:

   .. code-block:: shell

      export PATH=/opt/roc-optiq/bin:$PATH

RHEL / Oracle Linux -- dnf
--------------------------

1. Download the ``.zip`` package for your operating system and version from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases>`_ page:

   ===============  ==============================================
   OS / version     Example package name
   ===============  ==============================================
   RHEL 10          ``roc-optiq-1.0.0.0.el10.x86_64.zip``
   RHEL 9           ``roc-optiq-1.0.0.0.el9.x86_64.zip``
   RHEL 8           ``roc-optiq-1.0.0.0.el8.x86_64.zip``
   Oracle Linux 10  ``roc-optiq-1.0.0.0.ol10.x86_64.zip``
   Oracle Linux 9   ``roc-optiq-1.0.0.0.ol9.x86_64.zip``
   Oracle Linux 8   ``roc-optiq-1.0.0.0.ol8.x86_64.zip``
   ===============  ==============================================

2. Unzip the package to extract the ``.rpm`` file:

   .. code-block:: shell

      unzip <file>.zip

3. Install the ``.rpm`` package:

   .. code-block:: shell

      sudo dnf install ./<file>.rpm

4. Verify the installation:

   .. code-block:: shell

      rpm -q roc-optiq

.. tip::

   The ``roc-optiq`` binary is installed to ``/opt/roc-optiq/bin``. To launch ROCm Optiq by name, add that directory to your ``PATH``:

   .. code-block:: shell

      export PATH=/opt/roc-optiq/bin:$PATH

Any Linux distribution -- Tarball
---------------------------------

1. Download the tarball for your architecture from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases>`_ page:

   .. code-block:: shell

      wget https://github.com/ROCm/roc-optiq/releases/download/v1.0.0-optiq/roc-optiq-1.0.0.0-linux-x86_64.tar.gz

2. Extract it to your preferred install location (no root required):

   .. code-block:: shell

      mkdir -p "$HOME/opt/roc-optiq"
      tar -xzf roc-optiq-1.0.0.0-linux-x86_64.tar.gz -C "$HOME/opt/roc-optiq" --strip-components=1

3. Add ROCm Optiq to your ``PATH``:

   .. code-block:: shell

      export PATH="$HOME/opt/roc-optiq/bin:$PATH"

4. Verify the installation:

   .. code-block:: shell

      roc-optiq -v

Windows -- Installer
--------------------

1. Download the ``.exe`` installer from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases>`_ page and follow the instructions in the install wizard.

   .. image:: ../images/wizard.png
      :width: 500
      :alt: ROCm Optiq installation wizard welcome screen on Windows

2. Accept the agreement to install, then follow the installation instructions.

   .. image:: ../images/agreement.png
      :width: 500
      :alt: ROCm Optiq installer license agreement screen

3. Launch **roc-optiq.exe** from the installation directory or the Start menu.

macOS -- .zip
-------------

1. Download the archive for your macOS version (for example, ``roc-optiq_1.0.0.0_macos.zip``) from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases>`_ page.
2. Unzip it, then drag ``roc-optiq.app`` from the ``roc-optiq_1.0.0.0_macos`` folder to the ``Applications`` folder.
3. Launch ROCm Optiq from **Applications**.

Settings, logs, and presets are stored at ``~/Library/Application Support/ROCm-Optiq/``.

Verify your installation
========================

=========================  ===========================================================
Method                     Command
=========================  ===========================================================
apt (Ubuntu / Debian)      ``dpkg -l | grep roc-optiq``
dnf (RHEL / Oracle Linux)  ``rpm -q roc-optiq``
Tarball                    ``roc-optiq -v``
Windows                    ``roc-optiq.exe -v`` from an install-directory terminal
macOS                      Open the app; check the version under the About/Help menu
=========================  ===========================================================

Uninstall
=========

apt (Ubuntu / Debian)
---------------------

.. code-block:: shell

   sudo apt remove roc-optiq
   sudo apt autoremove

dnf (RHEL / Oracle Linux)
-------------------------

.. code-block:: shell

   sudo dnf remove roc-optiq

Tarball
-------

.. code-block:: shell

   rm -rf "$HOME/opt/roc-optiq"

Then remove the ``export PATH=...`` line you added to your shell profile.

Windows
-------

Go to **Control Panel > Programs > Uninstall a program**, select **ROCm Optiq**, and follow the prompts.

macOS
-----

Drag ``roc-optiq.app`` from ``/Applications`` to the Trash. Optionally remove the settings directory:

.. code-block:: shell

   rm -rf "$HOME/Library/Application Support/ROCm-Optiq"
