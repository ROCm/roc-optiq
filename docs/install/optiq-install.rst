.. meta::
  :description: Learn how to install ROCm Optiq on Windows, Linux (Ubuntu, RHEL, CentOS), and macOS, including system requirements and verification steps.
  :keywords: ROCm Optiq, ROCm, install, profiler

******************
Install ROCm Optiq
******************

Install ROCm Optiq for Linux, Windows, or macOS using the installation files in the `https://github.com/ROCm/roc-optiq <https://github.com/ROCm/roc-optiq>`_  GitHub repository.

.. _requirements:

Prerequisites
=============

.. note::

   ROCm Optiq only visualizes profiler database files. The machine that runs ROCm Optiq doesn't need ROCm, ROCm Systems Profiler, or ROCm Compute Profiler installed. However, the following ROCm versions apply to the profiling host that *generates* the ``.db`` file:

   - ROCm 7.1.0 or later for ROCm Systems Profiler database file support.
   - ROCm 7.12.0 or later for ROCm Compute Profiler analysis database file support.
   - ROCm 7.14.0 or later (ROCm Compute Profiler analysis database schema 1.4.0) for LDS roofline chart support.

   .. tip::

      Use the following command on the profiling host to check which ROCm version is installed:

      .. code-block:: shell

         cat /opt/rocm/.info/version

- **Memory**: At least 16 GB of RAM is recommended for working with large traces.
- **Operating system**: See the supported operating systems and versions below.

Supported operating systems
============================

.. list-table::
   :header-rows: 1

   * - Operating system
     - Supported versions
     - Install methods
   * - Ubuntu
     - Ubuntu 26.04, Ubuntu 24.04, Ubuntu 22.04
     - apt, Tarball
   * - RHEL
     - RHEL 10, RHEL 9, RHEL 8
     - dnf, Tarball
   * - Oracle Linux
     - Oracle Linux 10, Oracle Linux 9, Oracle Linux 8
     - dnf, Tarball
   * - Windows
     - Windows 11
     - Windows installer
   * - macOS
     - macOS Tahoe (Version 26), macOS Sequoia (Version 15), macOS Sonoma (Version 14)
     - .zip

If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

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

.. tab-set::

   .. tab-item:: Windows

      1. Download the ``.exe`` installer from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases>`_ page and follow the instructions in the install wizard.

         .. image:: ../images/wizard.png
            :width: 500
            :alt: ROCm Optiq installation wizard welcome screen on Windows

      2. Accept the agreement to install, then follow the installation instructions.

         .. image:: ../images/agreement.png
            :width: 500
            :alt: ROCm Optiq installer license agreement screen

      3. Launch **roc-optiq.exe** from the installation directory or the Start menu.

   .. tab-item:: Linux

      Choose the install method that matches your distribution.

      .. tab-set::

         .. tab-item:: apt (Ubuntu)

            1. Download the ``.deb`` package for your operating system and version from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases/tag/v1.0.0-optiq>`_ page.
            2. Install the ``.deb`` package:

               .. code-block:: shell

                  sudo apt install ./<package>.deb

            3. Verify the installation:

               .. code-block:: shell

                  dpkg -l | grep roc-optiq

            .. tip::

               The ``roc-optiq`` binary is installed to ``/opt/roc-optiq/bin``. Add it to your ``PATH`` to launch by name:

               .. code-block:: shell

                  export PATH=/opt/roc-optiq/bin:$PATH

         .. tab-item:: dnf (RHEL / Oracle Linux)

            1. Download the ``.rpm`` package for your operating system and version from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases/tag/v1.0.0-optiq>`_ page.
            2. Install the ``.rpm`` package:

               .. code-block:: shell

                  sudo dnf install ./<package>.rpm

            3. Verify the installation:

               .. code-block:: shell

                  rpm -q roc-optiq

            .. tip::

               The ``roc-optiq`` binary is installed to ``/opt/roc-optiq/bin``. Add it to your ``PATH`` to launch by name:

               .. code-block:: shell

                  export PATH=/opt/roc-optiq/bin:$PATH

         .. tab-item:: Tarball (any Linux distribution)

            1. Download the tarball for your architecture from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases/tag/v1.0.0-optiq>`_ page.
            2. Extract it to your preferred install location (no root required):

               .. code-block:: shell

                  mkdir -p "$HOME/opt/roc-optiq"
                  tar -xzf roc-optiq-linux-x86_64.tar.gz -C "$HOME/opt/roc-optiq" --strip-components=1

            3. Add ROCm Optiq to your ``PATH``:

               .. code-block:: shell

                  export PATH="$HOME/opt/roc-optiq/bin:$PATH"

            4. Verify the installation:

               .. code-block:: shell

                  roc-optiq -v

   .. tab-item:: macOS

      1. Download the ``.zip`` from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases>`_ page.
      2. Unzip it, then drag and drop ``roc-optiq.app`` from the extracted folder to the ``Applications`` folder.
      3. Launch ROCm Optiq from **Applications**.

      Settings, logs, and presets are stored at ``~/Library/Application Support/ROCm-Optiq/``.

Uninstall
=========

.. tab-set::

   .. tab-item:: Windows

      Go to **Control Panel > Programs > Uninstall a program**, select **ROCm Optiq**, and follow the prompts.

   .. tab-item:: Linux

      Choose the uninstall method that matches how you installed ROCm Optiq.

      .. tab-set::

         .. tab-item:: apt (Ubuntu / Debian)

            .. code-block:: shell

               sudo apt remove roc-optiq
               sudo apt autoremove

         .. tab-item:: dnf (RHEL / Oracle Linux)

            .. code-block:: shell

               sudo dnf remove roc-optiq

         .. tab-item:: Tarball (any Linux distribution)

            .. code-block:: shell

               rm -rf "$HOME/opt/roc-optiq"

            Then remove the ``export PATH=...`` line you added to your shell profile.

   .. tab-item:: macOS

      Drag ``roc-optiq.app`` from ``/Applications`` to the Trash. Optionally remove the settings directory:

      .. code-block:: shell

         rm -rf "$HOME/Library/Application Support/ROCm-Optiq"