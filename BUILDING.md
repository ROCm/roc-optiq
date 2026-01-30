# Build Instructions

This document describes how to build ROCm Optiq (roc-optiq) on Windows, Linux, and macOS. Linux dependency lists are based on the GitHub Actions workflows in [.github/workflows](.github/workflows).

## Common setup

1. Clone the repository with submodules:
   - `git clone --recursive <repo-url>`
2. Ensure CMake presets are available (see [CMakePresets.json](CMakePresets.json)).
3. Use the appropriate configure and build presets for your platform.

### CMake presets (summary)

- **Windows**: `x64-release`, `x64-release-symbols`, `x64-debug`
- **Linux**: `linux-release`, `linux-release-symbols`, `linux-debug`
- **macOS**: `macos-release`, `macos-release-symbols`, `macos-debug`

> The build presets are named in [CMakePresets.json](CMakePresets.json) under `buildPresets`.

---

## Windows (Visual Studio 2022)

### Prerequisites

- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.21+
- Vulkan SDK (LunarG)

### Install Vulkan SDK

Download and install the latest Vulkan SDK from LunarG:

- https://vulkan.lunarg.com/sdk/home#windows

Ensure the `VULKAN_SDK` environment variable is set and `Bin` is on `PATH`.

### Build (Release)

```powershell
cmake --preset "x64-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/x64-release --preset "Windows Release Build" --parallel 4
```

### Build (Release with Symbols)

```powershell
cmake --preset "x64-release-symbols" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/x64-release-symbols --preset "Windows Release Build with Symbols" --parallel 4
```

---

## Linux (Ubuntu 22.04 / 24.04)

### Dependencies

```bash
sudo apt update
sudo apt install -y \
  cmake build-essential \
  libwayland-bin \
  libwayland-dev libxkbcommon-dev wayland-protocols \
  pkg-config \
  libdbus-1-dev
```

### Vulkan SDK

```bash
wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-$(. /etc/os-release; echo $VERSION_CODENAME).list \
  http://packages.lunarg.com/vulkan/lunarg-vulkan-$(. /etc/os-release; echo $VERSION_CODENAME).list
sudo apt update
sudo apt install -y vulkan-sdk xorg-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

### Build (Release)

```bash
cmake -B build/linux-release --preset "linux-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/linux-release --preset "Linux Release Build" --parallel 4 --target package
```

---

## Linux (RHEL 8 / Rocky 8)

### Dependencies

```bash
dnf groupinstall -y "Development Tools"
dnf install -y epel-release
# RHEL 8 / Rocky 8 uses powertools
if dnf repolist | grep -q powertools; then
  dnf config-manager --set-enabled powertools
fi

dnf install -y \
  cmake libxkbcommon-devel mesa-libGL-devel ncurses-devel ninja-build wayland-devel wget \
  libX11-devel libXcursor-devel libXi-devel libXinerama-devel libXrandr-devel dbus-devel
```

### Vulkan SDK

```bash
mkdir -p /opt/vulkan
VULKAN_SDK_VERSION=1.4.328.1
wget -q https://sdk.lunarg.com/sdk/download/${VULKAN_SDK_VERSION}/linux/vulkansdk-linux-x86_64-${VULKAN_SDK_VERSION}.tar.xz -O /tmp/vulkansdk.tar.xz
tar -xf /tmp/vulkansdk.tar.xz -C /opt/vulkan
source /opt/vulkan/${VULKAN_SDK_VERSION}/setup-env.sh
```

### Build (Release)

```bash
cmake -B build/linux-release --preset "linux-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/linux-release --preset "Linux Release Build" --parallel 4 --target package
```

---

## Linux (RHEL 9/10 / Rocky 9/10)

### Dependencies

```bash
dnf groupinstall -y "Development Tools"
dnf install -y epel-release && crb enable

dnf install -y \
  cmake libxkbcommon-devel mesa-libGL-devel ncurses-devel ninja-build wayland-devel wget \
  libX11-devel libXcursor-devel libXi-devel libXinerama-devel libXrandr-devel dbus-devel
```

### Vulkan SDK

```bash
mkdir -p /opt/vulkan
VULKAN_SDK_VERSION=1.4.328.1
wget -q https://sdk.lunarg.com/sdk/download/${VULKAN_SDK_VERSION}/linux/vulkansdk-linux-x86_64-${VULKAN_SDK_VERSION}.tar.xz -O /tmp/vulkansdk.tar.xz
tar -xf /tmp/vulkansdk.tar.xz -C /opt/vulkan
source /opt/vulkan/${VULKAN_SDK_VERSION}/setup-env.sh
```

### Build (Release)

```bash
cmake -B build/linux-release --preset "linux-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/linux-release --preset "Linux Release Build" --parallel 4 --target package
```

---

## macOS (Apple Silicon or Intel)

### Prerequisites

- Xcode Command Line Tools (`xcode-select --install`)
- CMake 3.21+
- Vulkan SDK (LunarG) — required for Vulkan headers/libs
- Homebrew (for Molten VK)
- Molten VK

### Install Homebrew

```
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" 
```

### Install Vulkan SDK

Download and install the latest Vulkan SDK from LunarG:

- https://vulkan.lunarg.com/sdk/home#mac

### Install Molten VK

```
 brew install molten-vk
```

### Build (Release)

```bash
cmake -B build/macos-release --preset "macos-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/macos-release --preset "macOS Release Build" --parallel 4
```

---

## Artifacts

- Linux: packages are emitted into the build directory (e.g., `.deb`, `.rpm`, `.gz`).
- Windows: the executable is in `build/<preset>/<config>/roc-optiq.exe`.
- macOS: the executable is in `build/<preset>/`.

If you need symbol builds, use the `*-release-symbols` presets.
