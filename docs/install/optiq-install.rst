.. meta::
  :description: Learn how to install ROCm Optiq on Windows, Linux (Ubuntu, RHEL, Oracle Linux), and macOS, including system requirements and verification steps.
  :keywords: ROCm Optiq, ROCm, install, profiler

:selector-toc2: Installation environment
:selector-toc2-icon: fa-solid fa-computer

******************
Install ROCm Optiq
******************

Install ROCm Optiq for Linux, Windows, or macOS using the installation files on the `release page <https://github.com/ROCm/roc-optiq/releases>`_ of the GitHub repository.

.. _requirements:

Prerequisites
=============

The following are the prerequisites for using ROCm Optiq:

- **ROCm:** ROCm Optiq only visualizes profiler database files. The machine that runs ROCm Optiq doesn't need ROCm, ROCm Systems Profiler, or ROCm Compute Profiler installed. However, see :ref:`glance-data-sources` for the ROCm versions required for the profiling host that *generates* the ``.db`` file.
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

.. selector:: Ubuntu version
   :key: ubuntuver
   :show-cond: os=linux distro=ubuntu

   .. selector-option:: 26.04
      :value: 2604
      :default:
      :width: 4

   .. selector-option:: 24.04
      :value: 2404
      :width: 4

   .. selector-option:: 22.04
      :value: 2204
      :width: 4

.. selector:: RHEL version
   :key: rhelver
   :show-cond: os=linux distro=rhel

   .. selector-option:: 10
      :value: 10
      :default:
      :width: 4

   .. selector-option:: 9
      :value: 9
      :width: 4

   .. selector-option:: 8
      :value: 8
      :width: 4

.. selector:: Oracle Linux version
   :key: olver
   :show-cond: os=linux distro=ol

   .. selector-option:: 10
      :value: 10
      :default:
      :width: 4

   .. selector-option:: 9
      :value: 9
      :width: 4

   .. selector-option:: 8
      :value: 8
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

.. selected-content:: os=linux distro=ubuntu ubuntuver=2604
   :heading: Supported versions
   :heading-level: 3

   Ubuntu 26.04, Ubuntu 24.04, and Ubuntu 22.04 are supported.

.. selected-content:: os=linux distro=ubuntu ubuntuver=2404
   :heading: Supported versions
   :heading-level: 3

   Ubuntu 26.04, Ubuntu 24.04, and Ubuntu 22.04 are supported.

.. selected-content:: os=linux distro=ubuntu ubuntuver=2204
   :heading: Supported versions
   :heading-level: 3

   Ubuntu 26.04, Ubuntu 24.04, and Ubuntu 22.04 are supported.

.. selected-content:: os=linux distro=rhel rhelver=10
   :heading: Supported versions
   :heading-level: 3

   RHEL 10, RHEL 9, and RHEL 8 are supported.

.. selected-content:: os=linux distro=rhel rhelver=9
   :heading: Supported versions
   :heading-level: 3

   RHEL 10, RHEL 9, and RHEL 8 are supported.

.. selected-content:: os=linux distro=rhel rhelver=8
   :heading: Supported versions
   :heading-level: 3

   RHEL 10, RHEL 9, and RHEL 8 are supported.

.. selected-content:: os=linux distro=ol olver=10
   :heading: Supported versions
   :heading-level: 3

   Oracle Linux 10, Oracle Linux 9, and Oracle Linux 8 are supported.

.. selected-content:: os=linux distro=ol olver=9
   :heading: Supported versions
   :heading-level: 3

   Oracle Linux 10, Oracle Linux 9, and Oracle Linux 8 are supported.

.. selected-content:: os=linux distro=ol olver=8
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

.. selected-content:: os=linux distro=ubuntu ubuntuver=2604

   If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

   .. code-block:: shell

      $ cat /etc/os-release
      NAME="Ubuntu"
      VERSION_ID="24.04"
      VERSION="24.04.3 LTS (Noble Numbat)"
      VERSION_CODENAME=noble
      ID=ubuntu

   The relevant fields are ``ID`` and ``VERSION_ID``.

.. selected-content:: os=linux distro=ubuntu ubuntuver=2404

   If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

   .. code-block:: shell

      $ cat /etc/os-release
      NAME="Ubuntu"
      VERSION_ID="24.04"
      VERSION="24.04.3 LTS (Noble Numbat)"
      VERSION_CODENAME=noble
      ID=ubuntu

   The relevant fields are ``ID`` and ``VERSION_ID``.

.. selected-content:: os=linux distro=ubuntu ubuntuver=2204

   If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

   .. code-block:: shell

      $ cat /etc/os-release
      NAME="Ubuntu"
      VERSION_ID="24.04"
      VERSION="24.04.3 LTS (Noble Numbat)"
      VERSION_CODENAME=noble
      ID=ubuntu

   The relevant fields are ``ID`` and ``VERSION_ID``.

.. selected-content:: os=linux distro=rhel rhelver=10

   If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

   .. code-block:: shell

      $ cat /etc/os-release
      NAME="Red Hat Enterprise Linux"
      VERSION_ID="10.0"
      VERSION="10.0 (Coughlan)"
      ID="rhel"
      ID_LIKE="fedora"

   The relevant fields are ``ID`` and ``VERSION_ID``.

.. selected-content:: os=linux distro=rhel rhelver=9

   If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

   .. code-block:: shell

      $ cat /etc/os-release
      NAME="Red Hat Enterprise Linux"
      VERSION_ID="10.0"
      VERSION="10.0 (Coughlan)"
      ID="rhel"
      ID_LIKE="fedora"

   The relevant fields are ``ID`` and ``VERSION_ID``.

.. selected-content:: os=linux distro=rhel rhelver=8

   If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

   .. code-block:: shell

      $ cat /etc/os-release
      NAME="Red Hat Enterprise Linux"
      VERSION_ID="10.0"
      VERSION="10.0 (Coughlan)"
      ID="rhel"
      ID_LIKE="fedora"

   The relevant fields are ``ID`` and ``VERSION_ID``.

.. selected-content:: os=linux distro=ol olver=10

   If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

   .. code-block:: shell

      $ cat /etc/os-release
      NAME="Oracle Linux Server"
      VERSION_ID="9.4"
      VERSION="9.4"
      ID="ol"
      ID_LIKE="fedora"

   The relevant fields are ``ID`` and ``VERSION_ID``.

.. selected-content:: os=linux distro=ol olver=9

   If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

   .. code-block:: shell

      $ cat /etc/os-release
      NAME="Oracle Linux Server"
      VERSION_ID="9.4"
      VERSION="9.4"
      ID="ol"
      ID_LIKE="fedora"

   The relevant fields are ``ID`` and ``VERSION_ID``.

.. selected-content:: os=linux distro=ol olver=8

   If you're not sure which Linux distribution or version you're running, the ``/etc/os-release`` file contains this information:

   .. code-block:: shell

      $ cat /etc/os-release
      NAME="Oracle Linux Server"
      VERSION_ID="9.4"
      VERSION="9.4"
      ID="ol"
      ID_LIKE="fedora"

   The relevant fields are ``ID`` and ``VERSION_ID``.

Install
=======

.. selected-content:: os=windows

   1. Download the ``.msi`` installer from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases/tag/v1.0.0-optiq>`__ page and follow the instructions in the install wizard.

      .. image:: ../images/wizard.png
         :width: 500
         :alt: ROCm Optiq installation wizard welcome screen on Windows

   2. Accept the agreement to install, then follow the installation instructions.

      .. image:: ../images/agreement.png
         :width: 500
         :alt: ROCm Optiq installer license agreement screen

   3. Launch ``roc-optiq.exe`` from the installation directory or the Start menu.

.. selected-content:: os=linux distro=ubuntu ubuntuver=2604 method=apt
   :heading: Install using apt
   :heading-level: 3

   1. Register the ROCm Optiq repository:

      .. code-block:: shell

         sudo mkdir --parents --mode=0755 /etc/apt/keyrings
         wget https://stable.repo.amd.com/rocm/gpg/packages.gpg -O - | gpg --dearmor | sudo tee /etc/apt/keyrings/amdrocm.gpg > /dev/null
         sudo tee /etc/apt/sources.list.d/amdrocm-optiq.sources << 'EOF'
         X-Repo-Id: amdrocm-optiq
         Types: deb
         URIs: https://stable.repo.amd.com/rocm/extras/rocoptiq/packages/ubuntu2604/
         Suites: stable
         Components: main
         Architectures: amd64
         Signed-By: /etc/apt/keyrings/amdrocm.gpg
         Enabled: yes
         EOF

         sudo apt update

   2. Install the ROCm Optiq package:

      .. code-block:: shell

         sudo apt install amdrocm10-roc-optiq


   3. Complete the following post-installation steps:

      Configure environment variables so that ROCm Optiq is added to the ``PATH`` variable:

      **User-specific setup:**

      .. code-block:: shell

         # Optiq Environment Setup
         tee --append ~/.bashrc << EOF
         # BEGIN Optiq environment configuration
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         # END Optiq environment configuration
         EOF

         source ~/.bashrc


      **System-wide setup:**

      .. code-block:: shell

         # Optiq Environment Setup
         sudo tee /etc/profile.d/set-optiq-env.sh << EOF
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         EOF

         sudo chmod +x /etc/profile.d/set-optiq-env.sh
         source /etc/profile.d/set-optiq-env.sh

   4. Verify Your Installation:

      .. code-block:: shell

         roc-optiq -v

.. selected-content:: os=linux distro=ubuntu ubuntuver=2404 method=apt
   :heading: Install using apt
   :heading-level: 3

   1. Register the ROCm Optiq repository:

      .. code-block:: shell

         sudo mkdir --parents --mode=0755 /etc/apt/keyrings
         wget https://stable.repo.amd.com/rocm/gpg/packages.gpg -O - | gpg --dearmor | sudo tee /etc/apt/keyrings/amdrocm.gpg > /dev/null
         sudo tee /etc/apt/sources.list.d/amdrocm-optiq.sources << 'EOF'
         X-Repo-Id: amdrocm-optiq
         Types: deb
         URIs: https://stable.repo.amd.com/rocm/extras/rocoptiq/packages/ubuntu2404/
         Suites: stable
         Components: main
         Architectures: amd64
         Signed-By: /etc/apt/keyrings/amdrocm.gpg
         Enabled: yes
         EOF

         sudo apt update

   2. Install the ROCm Optiq package:

      .. code-block:: shell

         sudo apt install amdrocm10-roc-optiq


   3. Complete the following post-installation steps:

      Configure environment variables so that ROCm Optiq is added to the ``PATH`` variable:

      **User-specific setup:**

      .. code-block:: shell

         # Optiq Environment Setup
         tee --append ~/.bashrc << EOF
         # BEGIN Optiq environment configuration
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         # END Optiq environment configuration
         EOF

         source ~/.bashrc


      **System-wide setup:**

      .. code-block:: shell

         # Optiq Environment Setup
         sudo tee /etc/profile.d/set-optiq-env.sh << EOF
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         EOF

         sudo chmod +x /etc/profile.d/set-optiq-env.sh
         source /etc/profile.d/set-optiq-env.sh

   4. Verify Your Installation:

      .. code-block:: shell

         roc-optiq -v

.. selected-content:: os=linux distro=ubuntu ubuntuver=2204 method=apt
   :heading: Install using apt
   :heading-level: 3

   1. Register the ROCm Optiq repository:

      .. code-block:: shell

         sudo mkdir --parents --mode=0755 /etc/apt/keyrings
         wget https://stable.repo.amd.com/rocm/gpg/packages.gpg -O - | gpg --dearmor | sudo tee /etc/apt/keyrings/amdrocm.gpg > /dev/null
         sudo tee /etc/apt/sources.list.d/amdrocm-optiq.sources << 'EOF'
         X-Repo-Id: amdrocm-optiq
         Types: deb
         URIs: https://stable.repo.amd.com/rocm/extras/rocoptiq/packages/ubuntu2204/
         Suites: stable
         Components: main
         Architectures: amd64
         Signed-By: /etc/apt/keyrings/amdrocm.gpg
         Enabled: yes
         EOF

         sudo apt update

   2. Install the ROCm Optiq package:

      .. code-block:: shell

         sudo apt install amdrocm10-roc-optiq


   3. Complete the following post-installation steps:

      Configure environment variables so that ROCm Optiq is added to the ``PATH`` variable:

      **User-specific setup:**

      .. code-block:: shell

         # Optiq Environment Setup
         tee --append ~/.bashrc << EOF
         # BEGIN Optiq environment configuration
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         # END Optiq environment configuration
         EOF

         source ~/.bashrc


      **System-wide setup:**

      .. code-block:: shell

         # Optiq Environment Setup
         sudo tee /etc/profile.d/set-optiq-env.sh << EOF
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         EOF

         sudo chmod +x /etc/profile.d/set-optiq-env.sh
         source /etc/profile.d/set-optiq-env.sh

   4. Verify Your Installation:

      .. code-block:: shell

         roc-optiq -v

.. selected-content:: os=linux distro=rhel rhelver=10 method=dnf
   :heading: Install using dnf
   :heading-level: 3

   1. Register the ROCm Optiq repository:

      .. code-block:: bash

         sudo tee /etc/yum.repos.d/amdrocm-optiq.repo <<EOF
         [amdrocm-optiq]
         name=AMD ROCm Optiq Repository
         baseurl=https://stable.repo.amd.com/rocm/extras/rocoptiq/packages/rhel10/x86_64
         enabled=1
         gpgcheck=1
         gpgkey=https://stable.repo.amd.com/rocm/gpg/packages.gpg
         priority=50
         EOF
         sudo dnf clean all

   2. Install the ROCm Optiq package.

      .. code-block:: bash

         sudo dnf install amdrocm10-roc-optiq

   3. Complete the following post-installation steps:

      Configure environment variables so that ROCm Optiq is added to the ``PATH`` variable.

      **User-specific setup:**

      .. code-block:: bash

         # Optiq Environment Setup
         tee --append ~/.bashrc << EOF
         # BEGIN Optiq environment configuration
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         # END Optiq environment configuration
         EOF

         source ~/.bashrc

      **System-Wide Setup:**

      .. code-block:: bash

         # Optiq Environment Setup
         sudo tee /etc/profile.d/set-optiq-env.sh << EOF
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         EOF

         sudo chmod +x /etc/profile.d/set-optiq-env.sh
         source /etc/profile.d/set-optiq-env.sh

   4. Verify Your Installation:

      .. code-block:: bash

         roc-optiq -v

.. selected-content:: os=linux distro=rhel rhelver=9 method=dnf
   :heading: Install using dnf
   :heading-level: 3

   1. Register the ROCm Optiq repository:

      .. code-block:: bash

         sudo tee /etc/yum.repos.d/amdrocm-optiq.repo <<EOF
         [amdrocm-optiq]
         name=AMD ROCm Optiq Repository
         baseurl=https://stable.repo.amd.com/rocm/extras/rocoptiq/packages/rhel9/x86_64
         enabled=1
         gpgcheck=1
         gpgkey=https://stable.repo.amd.com/rocm/gpg/packages.gpg
         priority=50
         EOF
         sudo dnf clean all

   2. Install the ROCm Optiq package.

      .. code-block:: bash

         sudo dnf install amdrocm10-roc-optiq

   3. Complete the following post-installation steps:

      Configure environment variables so that ROCm Optiq is added to the ``PATH`` variable.

      **User-specific setup:**

      .. code-block:: bash

         # Optiq Environment Setup
         tee --append ~/.bashrc << EOF
         # BEGIN Optiq environment configuration
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         # END Optiq environment configuration
         EOF

         source ~/.bashrc

      **System-Wide Setup:**

      .. code-block:: bash

         # Optiq Environment Setup
         sudo tee /etc/profile.d/set-optiq-env.sh << EOF
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         EOF

         sudo chmod +x /etc/profile.d/set-optiq-env.sh
         source /etc/profile.d/set-optiq-env.sh

   4. Verify Your Installation:

      .. code-block:: bash

         roc-optiq -v

.. selected-content:: os=linux distro=rhel rhelver=8 method=dnf
   :heading: Install using dnf
   :heading-level: 3

   1. Register the ROCm Optiq repository:

      .. code-block:: bash

         sudo tee /etc/yum.repos.d/amdrocm-optiq.repo <<EOF
         [amdrocm-optiq]
         name=AMD ROCm Optiq Repository
         baseurl=https://stable.repo.amd.com/rocm/extras/rocoptiq/packages/rhel8/x86_64
         enabled=1
         gpgcheck=1
         gpgkey=https://stable.repo.amd.com/rocm/gpg/packages.gpg
         priority=50
         EOF
         sudo dnf clean all

   2. Install the ROCm Optiq package.

      .. code-block:: bash

         sudo dnf install amdrocm10-roc-optiq

   3. Complete the following post-installation steps:

      Configure environment variables so that ROCm Optiq is added to the ``PATH`` variable.

      **User-specific setup:**

      .. code-block:: bash

         # Optiq Environment Setup
         tee --append ~/.bashrc << EOF
         # BEGIN Optiq environment configuration
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         # END Optiq environment configuration
         EOF

         source ~/.bashrc

      **System-Wide Setup:**

      .. code-block:: bash

         # Optiq Environment Setup
         sudo tee /etc/profile.d/set-optiq-env.sh << EOF
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         EOF

         sudo chmod +x /etc/profile.d/set-optiq-env.sh
         source /etc/profile.d/set-optiq-env.sh

   4. Verify Your Installation:

      .. code-block:: bash

         roc-optiq -v

.. selected-content:: os=linux distro=ol olver=10 method=dnf
   :heading: Install using dnf
   :heading-level: 3

   1. Register the ROCm Optiq repository:

      .. code-block:: bash

         sudo tee /etc/yum.repos.d/amdrocm-optiq.repo <<EOF
         [amdrocm-optiq]
         name=AMD ROCm Optiq Repository
         baseurl=https://stable.repo.amd.com/rocm/extras/rocoptiq/packages/rhel10/x86_64
         enabled=1
         gpgcheck=1
         gpgkey=https://stable.repo.amd.com/rocm/gpg/packages.gpg
         priority=50
         EOF
         sudo dnf clean all

   2. Install the ROCm Optiq package.

      .. code-block:: bash

         sudo dnf install amdrocm10-roc-optiq

   3. Complete the following post-installation steps:

      Configure environment variables so that ROCm Optiq is added to the ``PATH`` variable.

      **User-specific setup:**

      .. code-block:: bash

         # Optiq Environment Setup
         tee --append ~/.bashrc << EOF
         # BEGIN Optiq environment configuration
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         # END Optiq environment configuration
         EOF

         source ~/.bashrc

      **System-Wide Setup:**

      .. code-block:: bash

         # Optiq Environment Setup
         sudo tee /etc/profile.d/set-optiq-env.sh << EOF
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         EOF

         sudo chmod +x /etc/profile.d/set-optiq-env.sh
         source /etc/profile.d/set-optiq-env.sh

   4. Verify Your Installation:

      .. code-block:: bash

         roc-optiq -v

.. selected-content:: os=linux distro=ol olver=9 method=dnf
   :heading: Install using dnf
   :heading-level: 3

   1. Register the ROCm Optiq repository:

      .. code-block:: bash

         sudo tee /etc/yum.repos.d/amdrocm-optiq.repo <<EOF
         [amdrocm-optiq]
         name=AMD ROCm Optiq Repository
         baseurl=https://stable.repo.amd.com/rocm/extras/rocoptiq/packages/rhel9/x86_64
         enabled=1
         gpgcheck=1
         gpgkey=https://stable.repo.amd.com/rocm/gpg/packages.gpg
         priority=50
         EOF
         sudo dnf clean all

   2. Install the ROCm Optiq package.

      .. code-block:: bash

         sudo dnf install amdrocm10-roc-optiq

   3. Complete the following post-installation steps:

      Configure environment variables so that ROCm Optiq is added to the ``PATH`` variable.

      **User-specific setup:**

      .. code-block:: bash

         # Optiq Environment Setup
         tee --append ~/.bashrc << EOF
         # BEGIN Optiq environment configuration
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         # END Optiq environment configuration
         EOF

         source ~/.bashrc

      **System-Wide Setup:**

      .. code-block:: bash

         # Optiq Environment Setup
         sudo tee /etc/profile.d/set-optiq-env.sh << EOF
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         EOF

         sudo chmod +x /etc/profile.d/set-optiq-env.sh
         source /etc/profile.d/set-optiq-env.sh

   4. Verify Your Installation:

      .. code-block:: bash

         roc-optiq -v

.. selected-content:: os=linux distro=ol olver=8 method=dnf
   :heading: Install using dnf
   :heading-level: 3

   1. Register the ROCm Optiq repository:

      .. code-block:: bash

         sudo tee /etc/yum.repos.d/amdrocm-optiq.repo <<EOF
         [amdrocm-optiq]
         name=AMD ROCm Optiq Repository
         baseurl=https://stable.repo.amd.com/rocm/extras/rocoptiq/packages/rhel8/x86_64
         enabled=1
         gpgcheck=1
         gpgkey=https://stable.repo.amd.com/rocm/gpg/packages.gpg
         priority=50
         EOF
         sudo dnf clean all

   2. Install the ROCm Optiq package.

      .. code-block:: bash

         sudo dnf install amdrocm10-roc-optiq

   3. Complete the following post-installation steps:

      Configure environment variables so that ROCm Optiq is added to the ``PATH`` variable.

      **User-specific setup:**

      .. code-block:: bash

         # Optiq Environment Setup
         tee --append ~/.bashrc << EOF
         # BEGIN Optiq environment configuration
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         # END Optiq environment configuration
         EOF

         source ~/.bashrc

      **System-Wide Setup:**

      .. code-block:: bash

         # Optiq Environment Setup
         sudo tee /etc/profile.d/set-optiq-env.sh << EOF
         export OPTIQ_PATH=/opt/rocm/extras-10/rocm-optiq
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         EOF

         sudo chmod +x /etc/profile.d/set-optiq-env.sh
         source /etc/profile.d/set-optiq-env.sh

   4. Verify Your Installation:

      .. code-block:: bash

         roc-optiq -v


.. selected-content:: os=linux distro=ubuntu method=tarball
   :heading: Install using tarball
   :heading-level: 3

   1. Install system dependencies:

      .. code-block:: shell

         sudo apt install libvulkan1 libgl1 libopengl0 libdbus-1-3 libc6 libstdc++6

   2. Download the ROCm Optiq tarball:

      .. code-block:: shell

         wget https://stable.repo.amd.com/rocm/extras/rocoptiq/tarball/amdrocm10-roc-optiq-1.1.0.2-Linux.tar.gz


   3. Extract the tarball:

      .. code-block:: shell

         # Set Optiq installation path
         OPTIQ_INSTALL_PATH="$HOME/roc-optiq"

         # Extract to the extras directory
         mkdir -p $OPTIQ_INSTALL_PATH
         tar -xzf amdrocm10-roc-optiq-1.1.0.2-Linux.tar.gz -C $OPTIQ_INSTALL_PATH --strip-components=5

      .. note::

         ``OPTIQ_INSTALL_PATH`` can be changed to your preferred location. For example, ``$HOME/roc-optiq``.


   4. Complete the following post-installation steps:

      Configure environment variables so that ROCm Optiq is added to the ``PATH`` variable:

      User-specific setup:

      .. code-block:: shell

         # Optiq Environment Setup
         tee --append ~/.bashrc << EOF
         # BEGIN Optiq environment configuration
         export OPTIQ_PATH="$HOME/roc-optiq"
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         # END Optiq environment configuration
         EOF

         source ~/.bashrc

      System-Wide Setup:

      .. code-block:: shell

         # Optiq Environment Setup
         sudo tee /etc/profile.d/set-optiq-env.sh << EOF
         export OPTIQ_PATH="$HOME/roc-optiq"
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         EOF

         sudo chmod +x /etc/profile.d/set-optiq-env.sh
         source /etc/profile.d/set-optiq-env.sh

   5. Verify your installation:

      .. code-block:: shell

         roc-optiq -v

.. selected-content:: os=linux distro=rhel method=tarball
   :heading: Install using tarball
   :heading-level: 3

   1. Install system dependencies:

      .. code-block:: shell

         sudo dnf install vulkan-loader mesa-libGL libglvnd-opengl dbus-libs glibc libstdc++

   2. Download the ROCm Optiq tarball:

      .. code-block:: shell

         wget https://stable.repo.amd.com/rocm/extras/rocoptiq/tarball/amdrocm10-roc-optiq-1.1.0.2-Linux.tar.gz


   3. Extract the tarball:

      .. code-block:: shell

         # Set Optiq installation path
         OPTIQ_INSTALL_PATH="$HOME/roc-optiq"

         # Extract to the extras directory
         mkdir -p $OPTIQ_INSTALL_PATH
         tar -xzf amdrocm10-roc-optiq-1.1.0.2-Linux.tar.gz -C $OPTIQ_INSTALL_PATH --strip-components=5

      .. note::

         ``OPTIQ_INSTALL_PATH`` can be changed to your preferred location. For example, ``$HOME/roc-optiq``.


   4. Complete the following post-installation steps:

      Configure environment variables so that ROCm Optiq is added to the ``PATH`` variable:

      User-specific setup:

      .. code-block:: shell

         # Optiq Environment Setup
         tee --append ~/.bashrc << EOF
         # BEGIN Optiq environment configuration
         export OPTIQ_PATH="$HOME/roc-optiq"
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         # END Optiq environment configuration
         EOF

         source ~/.bashrc

      System-Wide Setup:

      .. code-block:: shell

         # Optiq Environment Setup
         sudo tee /etc/profile.d/set-optiq-env.sh << EOF
         export OPTIQ_PATH="$HOME/roc-optiq"
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         EOF

         sudo chmod +x /etc/profile.d/set-optiq-env.sh
         source /etc/profile.d/set-optiq-env.sh

   5. Verify your installation:

      .. code-block:: shell

         roc-optiq -v

.. selected-content:: os=linux distro=ol method=tarball
   :heading: Install using tarball
   :heading-level: 3

   1. Install system dependencies:

      .. code-block:: shell

         sudo dnf install vulkan-loader mesa-libGL libglvnd-opengl dbus-libs glibc libstdc++

   2. Download the ROCm Optiq tarball:

      .. code-block:: shell

         wget https://stable.repo.amd.com/rocm/extras/rocoptiq/tarball/amdrocm10-roc-optiq-1.1.0.2-Linux.tar.gz


   3. Extract the tarball:

      .. code-block:: shell

         # Set Optiq installation path
         OPTIQ_INSTALL_PATH="$HOME/roc-optiq"

         # Extract to the extras directory
         mkdir -p $OPTIQ_INSTALL_PATH
         tar -xzf amdrocm10-roc-optiq-1.1.0.2-Linux.tar.gz -C $OPTIQ_INSTALL_PATH --strip-components=5

      .. note::

         ``OPTIQ_INSTALL_PATH`` can be changed to your preferred location. For example, ``$HOME/roc-optiq``.


   4. Complete the following post-installation steps:

      Configure environment variables so that ROCm Optiq is added to the ``PATH`` variable:

      User-specific setup:

      .. code-block:: shell

         # Optiq Environment Setup
         tee --append ~/.bashrc << EOF
         # BEGIN Optiq environment configuration
         export OPTIQ_PATH="$HOME/roc-optiq"
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         # END Optiq environment configuration
         EOF

         source ~/.bashrc

      System-Wide Setup:

      .. code-block:: shell

         # Optiq Environment Setup
         sudo tee /etc/profile.d/set-optiq-env.sh << EOF
         export OPTIQ_PATH="$HOME/roc-optiq"
         export PATH=\$OPTIQ_PATH/bin:\$PATH
         EOF

         sudo chmod +x /etc/profile.d/set-optiq-env.sh
         source /etc/profile.d/set-optiq-env.sh

   5. Verify your installation:

      .. code-block:: shell

         roc-optiq -v

.. selected-content:: os=macos

   1. Download the ``.zip`` from the `ROCm Optiq GitHub Releases <https://github.com/ROCm/roc-optiq/releases/tag/v1.0.0-optiq>`__ page.
   2. Unzip it, then drag and drop ``roc-optiq.app`` from the extracted folder to the ``Applications`` folder.
   3. Launch ROCm Optiq from **Applications**.

   Settings, logs, and presets are stored at ``~/Library/Application Support/ROCm-Optiq/``.

Uninstall
=========

.. selected-content:: os=windows

   Go to **Settings > Apps > Installed apps**, select **ROCm Optiq**, and click **Uninstall**.

.. selected-content:: os=linux distro=ubuntu method=apt
   :heading: Uninstall using apt
   :heading-level: 3

   1. Remove the installed package:

      .. code-block:: shell

         sudo apt autoremove amdrocm10-roc-optiq

   2. Remove ROCm Optiq repositories:

      .. code-block:: shell

         # Remove ROCm Optiq repositories
         sudo rm /etc/apt/sources.list.d/amdrocm-optiq.sources

         # Clear the cache and clean the system
         sudo rm -rf /var/cache/apt/*
         sudo apt clean all
         sudo apt update

   3. Remove ROCm Optiq environment configuration:

      **User-sepcfic setup:**

      If you opted for a user-specific setup during the installation process, remove the ROCm Optiq environment configuration block you added to your shell configuration file (``~/.bashrc``).

      **System-wide setup:**

      If you opted for a system-wide setup during the installation process, remove the ROCm Optiq environment variables.

      .. code-block:: shell

         sudo rm -f /etc/profile.d/set-optiq-env.sh

.. selected-content:: os=linux distro=rhel method=dnf
   :heading: Uninstall using dnf
   :heading-level: 3

   1. Remove the installed package:

      .. code-block:: bash

         sudo dnf remove amdrocm10-roc-optiq

   2. Remove ROCm Optiq repositories:

      .. code-block:: bash

         # Remove ROCm Optiq repositories
         sudo rm /etc/yum.repos.d/amdrocm-optiq.repo

         # Clear the cache and clean the system
         sudo rm -rf /var/cache/dnf
         sudo dnf clean all

   3. Remove ROCm Optiq environment configuration:

      **User-specific setup:**

      If you opted for a user-specific setup during the installation process, remove the ROCm Optiq environment configuration block you added to your shell configuration file (``~/.bashrc``).

      **System-wide setup:**

      If you opted for a system-wide setup during the installation process, remove the ROCm Optiq environment variables.

      .. code-block:: bash

         sudo rm -f /etc/profile.d/set-optiq-env.sh

.. selected-content:: os=linux distro=ol method=dnf
   :heading: Uninstall using dnf
   :heading-level: 3

   1. Remove the installed package:

      .. code-block:: bash

         sudo dnf remove amdrocm10-roc-optiq

   2. Remove ROCm Optiq repositories:

      .. code-block:: bash

         # Remove ROCm Optiq repositories
         sudo rm /etc/yum.repos.d/amdrocm-optiq.repo

         # Clear the cache and clean the system
         sudo rm -rf /var/cache/dnf
         sudo dnf clean all

   3. Remove ROCm Optiq environment configuration:

      **User-specific setup:**

      If you opted for a user-specific setup during the installation process, remove the ROCm Optiq environment configuration block you added to your shell configuration file (``~/.bashrc``).

      **System-wide setup:**

      If you opted for a system-wide setup during the installation process, remove the ROCm Optiq environment variables.

      .. code-block:: bash

         sudo rm -f /etc/profile.d/set-optiq-env.sh


.. selected-content:: os=linux method=tarball
   :heading: Uninstall tarball installation
   :heading-level: 3

   1. Remove the installation directory:
   
      .. important::
         
         The following command assumes you had set ``OPTIQ_INSTALL_PATH`` to ``$HOME/roc-optiq`` during installation. 
         If you have chosen a different installation directory, remove it instead.

      .. code-block:: shell

         rm -rf "$HOME/opt/roc-optiq"

   Then remove ``$HOME/opt/roc-optiq/bin`` from your ``PATH`` variable.

   2. Remove ROCm Optiq environment configuration:

      **User-specific setup:**

      If you opted for a user-specific setup during the installation process, remove the ROCm Optiq environment configuration block you added to your shell configuration file (``~/.bashrc``).

      **System-wide setup:**

      If you opted for a system-wide setup during the installation process, remove the ROCm Optiq environment variables:

      .. code-block:: shell
   
         sudo rm -f /etc/profile.d/set-optiq-env.sh


.. selected-content:: os=macos

   Drag ``roc-optiq.app`` from ``/Applications`` to the Trash. Optionally remove the settings directory:

   .. code-block:: shell

      rm -rf "$HOME/Library/Application Support/ROCm-Optiq"
