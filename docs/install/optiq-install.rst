.. meta::
  :description: Learn how to install ROCm Optiq on Windows, Linux (Ubuntu, RHEL, Oracle Linux), and macOS, including system requirements and verification steps.
  :keywords: ROCm Optiq, ROCm, install, profiler

:selector-toc2: Installation environment
:selector-toc2-icon: fa-solid fa-computer

******************
Install ROCm Optiq
******************

Install ROCm Optiq for Linux, Windows, or macOS using the installation files in the `https://github.com/ROCm/roc-optiq <https://github.com/ROCm/roc-optiq>`_  GitHub repository.

.. _requirements:

Prerequisites
=============

The following are the prerequisites for using ROCm Optiq: 

- **ROCm:** ROCm Optiq only visualizes profiler database files. The machine that runs ROCm Optiq doesn't need ROCm, ROCm Systems Profiler, or ROCm Compute Profiler installed. However, see :ref:`glance-data-sources` for the ROCm versions required for the profiling host that *generates* the ``.db`` file.

 .. tip::

   Use the following command on the profiling host to check which ROCm version is installed:

   .. code-block:: shell

      cat /opt/rocm/.info/version

- **Memory**: At least 16 GB of RAM is recommended for working with large traces.
- **Operating system**: See the supported operating systems and versions below.

Supported operating systems
============================

The following selector lists all the operating systems and installation methods for ROCm Optiq. Choose your operating system and installation method to see the supported versions and the installation instructions. 

.. selector:: Operating system
   :key: os

   .. selector-option:: Linux
      :value: linux
      :default:
      :width: 4

   .. selector-option:: Windows
      :value: windows
      :width: 4

   .. selector-option:: macOS
      :value: macos
      :width: 4

.. selector:: Linux distribution
   :key: distro
   :show-cond: os=linux

   .. selector-option:: Ubuntu
      :value: ubuntu
      :default:
      :width: 4

   .. selector-option:: RHEL
      :value: rhel
      :width: 4

   .. selector-option:: Oracle Linux
      :value: ol
      :width: 4

.. selector:: Install method
   :key: method
   :show-cond: os=linux distro=ubuntu

   .. selector-option:: apt
      :value: apt
      :default:
      :width: 6

   .. selector-option:: tarball
      :value: tarball
      :width: 6

.. selector:: Install method
   :key: method
   :show-cond: os=linux distro=rhel

   .. selector-option:: dnf
      :value: dnf
      :default:
      :width: 6

   .. selector-option:: tarball
      :value: tarball
      :width: 6

.. selector:: Install method
   :key: method
   :show-cond: os=linux distro=ol

   .. selector-option:: dnf
      :value: dnf
      :default:
      :width: 6

   .. selector-option:: tarball
      :value: tarball
      :width: 6

.. selected-content:: os=linux distro=ubuntu
   :heading: Supported versions
   :heading-level: 3

   Ubuntu 26.04, Ubuntu 24.04, and Ubuntu 22.04 are supported.

.. selected-content:: os=linux distro=rhel
   :heading: Supported versions
   :heading-level: 3

   RHEL 10, RHEL 9, and RHEL 8 are supported.

.. selected-content:: os=linux distro=ol
   :heading: Supported versions
   :heading-level: 3

   Oracle Linux 10, Oracle Linux 9, and Oracle Linux 8 are supported.

.. selected-content:: os=windows
   :heading: Supported Windows versions
   :heading-level: 3

   Windows 11 is supported.

.. selected-content:: os=macos
   :heading: Supported versions
   :heading-level: 3

   macOS Tahoe (Version 26), macOS Sequoia (Version 15), and macOS Sonoma (Version 14) are supported.

.. selected-content:: os=linux distro=ubuntu

   If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

   .. code-block:: shell

      $ cat /etc/os-release
      NAME="Ubuntu"
      VERSION_ID="24.04"
      VERSION="24.04.3 LTS (Noble Numbat)"
      VERSION_CODENAME=noble
      ID=ubuntu

   The relevant fields are ``ID`` and ``VERSION_ID``.

.. selected-content:: os=linux distro=rhel

   If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

   .. code-block:: shell

      $ cat /etc/os-release
      NAME="Red Hat Enterprise Linux"
      VERSION_ID="10.0"
      VERSION="10.0 (Coughlan)"
      ID="rhel"
      ID_LIKE="fedora"

   The relevant fields are ``ID`` and ``VERSION_ID``.

.. selected-content:: os=linux distro=ol

   If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

   .. code-block:: shell

      $ cat /etc/os-release
      NAME="Oracle Linux Server"
      VERSION_ID="9.4"
      VERSION="9.4"
      ID="ol"
      ID_LIKE="fedora"

   The relevant fields are ``ID`` and ``VERSION_ID``.

Installation
============

.. selected-content:: os=windows

   1. Download the ``.exe`` installer from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases>`_ page and follow the instructions in the install wizard.

      .. image:: ../images/wizard.png
         :width: 500
         :alt: ROCm Optiq installation wizard welcome screen on Windows

   2. Accept the agreement to install, then follow the installation instructions.

      .. image:: ../images/agreement.png
         :width: 500
         :alt: ROCm Optiq installer license agreement screen

   3. Launch **roc-optiq.exe** from the installation directory or the Start menu.

.. selected-content:: os=linux distro=ubuntu method=apt
   :heading: Install using apt
   :heading-level: 3

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

.. selected-content:: os=linux distro=rhel method=dnf
   :heading: Install using dnf
   :heading-level: 3

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

.. selected-content:: os=linux distro=ol method=dnf
   :heading: Install using dnf
   :heading-level: 3

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

.. selected-content:: os=linux method=tarball
   :heading: Install using tarball
   :heading-level: 3

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

.. selected-content:: os=macos

   1. Download the ``.zip`` from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases>`_ page.
   2. Unzip it, then drag and drop ``roc-optiq.app`` from the extracted folder to the ``Applications`` folder.
   3. Launch ROCm Optiq from **Applications**.

   Settings, logs, and presets are stored at ``~/Library/Application Support/ROCm-Optiq/``.

Uninstall
=========

.. selected-content:: os=windows

   Go to **Control Panel > Programs > Uninstall a program**, select **ROCm Optiq**, and follow the prompts.

.. selected-content:: os=linux distro=ubuntu method=apt
   :heading: Uninstall using apt
   :heading-level: 3

   .. code-block:: shell

      sudo apt remove roc-optiq
      sudo apt autoremove

.. selected-content:: os=linux distro=rhel method=dnf
   :heading: Uninstall using dnf
   :heading-level: 3

   .. code-block:: shell

      sudo dnf remove roc-optiq

.. selected-content:: os=linux distro=ol method=dnf
   :heading: Uninstall using dnf
   :heading-level: 3

   .. code-block:: shell

      sudo dnf remove roc-optiq

.. selected-content:: os=linux method=tarball
   :heading: Uninstall tarball installation
   :heading-level: 3

   .. code-block:: shell

      rm -rf "$HOME/opt/roc-optiq"

   Then remove the ``export PATH=...`` line you added to your shell profile.

.. selected-content:: os=macos

   Drag ``roc-optiq.app`` from ``/Applications`` to the Trash. Optionally remove the settings directory:

   .. code-block:: shell

      rm -rf "$HOME/Library/Application Support/ROCm-Optiq"

