.. meta::
  :description: Install ROCm Optiq 
  :keywords: ROCm Optiq, ROCm, install, profiler

***********************
ROCm Optiq installation
***********************

Install ROCm Optiq for Windows or Linux using the installation files in the `https://github.com/ROCm/roc-optiq <https://github.com/ROCm/roc-optiq>`_  GitHub repository.

.. _requirements:

System requirements
===================

- You must be running ROCm version 7.1.0 or later for ROCm database file support. See :ref:`trace-file` for more information on these files.
- Your system must be running one of these operating systems:

  - Microsoft Windows 11
  - Ubuntu 22.04 / Ubuntu 24.04

.. note::

   It's recommended to have at least 16GB of RAM to run large traces in ROCm Optiq.


Install on Windows
==================

1. Download the **ROCm-Optiq-Beta.exe** installer from the `ROCm Optiq GitHub repo <https://github.com/ROCm/roc-optiq/releases/tag/v0.2.0-optiq-beta-rc4>`_ and follow the instructions in the install wizard.
   
   .. image:: ../images/wizard.png
      :width: 500

2. Accept the agreement to install, then follow the installation instructions.

   .. image:: ../images/agreement.png
      :width: 500
  
3. Launch **roc-optiq.exe** from the installation directory.

Install on Linux
================

Indentify the operating system
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

Ubuntu 22.04 / 24.04 (Debian-based)
-----------------------------------

1. Download the ``.deb`` package from the `ROCm Optiq GitHub repo <https://github.com/ROCm/roc-optiq/releases/tag/v0.2.0-optiq-beta-rc4>`_.
2. Install the ``.deb`` package:

   .. code-block:: shell

      sudo apt install ./<file>.deb

3. Verify the installation:

   .. code-block:: shell

      dpkg -l | grep roc-optiq

.. tip::

   Download the latest ``.deb`` from the `ROCm Optiq GitHub repo <https://github.com/ROCm/roc-optiq/releases/tag/v0.2.0-optiq-beta-rc4>`_ to ensure ROCm Optiq is up-to-date.

Check the ROCm Optiq version
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Use these commands to verify the ROCm Optiq version (when installed on Ubuntu):

.. code-block:: shell

   apt show roc-optiq

Add ROCm binaries to your ``PATH``
==================================

Once you've installed ROCm Optiq for your operating system, add the ROCm binaries to your ``PATH`` if it isn't automatically configured:

.. code-block:: shell

   export PATH=opt/roc-optiq/bin:$PATH




