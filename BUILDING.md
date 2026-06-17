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
  libX11-devel libXcursor-devel libXi-devel libXinerama-devel libXrandr-devel dbus-devel tbb-devel 
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
- Vulkan SDK (LunarG) — required for Vulkan headers/libs at build time
- Homebrew

### Runtime prerequisites

The Vulkan loader must be installed on the system for the app to launch. Install it via Homebrew:

```bash
brew install vulkan-loader
```

For Vulkan rendering (optional — the app falls back to OpenGL if unavailable):

```bash
brew install molten-vk
```

### Install Homebrew

```
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" 
```

### Install Vulkan SDK (build time)

Download and install the latest Vulkan SDK from LunarG:

- https://vulkan.lunarg.com/sdk/home#mac

### Build (Release)

```bash
cmake -B build/macos-release --preset "macos-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/macos-release --preset "macOS Release Build" --parallel 4
```

---

## Crypto backend (SSH / remote features)

The SSH and remote-profiling features use `libssh2`, which needs a crypto backend selected at configure time via `CRYPTO_BACKEND`.

- **Default: `mbedTLS`** — vendored under `thirdparty/mbedtls` and linked statically. This is the default build and requires **no extra dependency to install** and **nothing extra to deploy**. While remote features are disabled by default this is what ships.
- **Opt-in: `OpenSSL`** — configure with `-DCRYPTO_BACKEND=OpenSSL`. OpenSSL is **not** vendored; it is resolved as an external dependency via `find_package(OpenSSL)`, the same way the Vulkan SDK is treated. Install a system OpenSSL first:

| Platform | Install | Notes |
|----------|---------|-------|
| Linux (Ubuntu/Debian) | `sudo apt install -y libssl-dev` | |
| Linux (RHEL/Rocky/Oracle) | `sudo dnf install -y openssl-devel` | |
| macOS | `brew install openssl@3` | configure with `-DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)` |
| Windows | Install a VC-compatible OpenSSL (e.g. [Win64 OpenSSL](https://slproweb.com/products/Win32OpenSSL.html)) | set `OPENSSL_ROOT_DIR` to the install root |

When the OpenSSL backend is built, its runtime libraries are deployed automatically: DLLs are copied next to `roc-optiq.exe` on Windows, `libssl`/`libcrypto` dylibs are staged into the `.app` bundle's `Frameworks` on macOS, and the Linux `.deb`/`.rpm` declares the system OpenSSL runtime as a dependency.

Example (OpenSSL backend on Linux):

```bash
sudo apt install -y libssl-dev
cmake -B build/linux-release --preset "linux-release" -DCRYPTO_BACKEND=OpenSSL
cmake --build build/linux-release --preset "Linux Release Build" --parallel 4 --target package
```

---

## Artifacts

- Linux: packages are emitted into the build directory (e.g., `.deb`, `.rpm`, `.gz`).
- Windows: the executable is in `build/<preset>/<config>/roc-optiq.exe`.
- macOS: the executable is in `build/<preset>/`.

If you need symbol builds, use the `*-release-symbols` presets.

---

## File dialog behavior on Linux

On Linux, `roc-optiq` uses the native [xdg-desktop-portal](https://flatpak.github.io/xdg-desktop-portal/) file chooser by default, but also ships the in-process ImGui file dialog as a fallback.

The portal dialog is launched by an external D-Bus service on the host machine and is **not** compatible with remote display forwarding (e.g. `ssh -X` / `ssh -Y`): the portal parents its window on the host's compositor, so over an SSH session the dialog would appear on the host rather than on your client — or simply never appear at all if the host has no local display.

To work around this, the application automatically picks the in-window ImGui dialog when it detects a remote session. Detection looks at:

- `SSH_CONNECTION`, `SSH_CLIENT`, `SSH_TTY` environment variables (set by `sshd`), and
- `DISPLAY` matching `localhost:N` with `N >= 10` (the range SSH uses for X11 forwarding).

The chosen backend is logged once at startup. If the auto-detection gets it wrong (for example the SSH environment was stripped by `sudo` or `systemd-run`, or you are `ssh`'ing into the same host that also runs a local desktop), you can override it with the `--file-dialog` command-line flag:

```bash
# Force the in-process ImGui dialog regardless of detection
roc-optiq --file-dialog=imgui

# Force the native (xdg-desktop-portal) dialog
roc-optiq --file-dialog=native

# Let the app auto-detect (the default)
roc-optiq --file-dialog=auto
```

If `xdg-desktop-portal` or D-Bus is not available on the host, the native dialog probe will fail gracefully at startup (or at the first dialog open) and the app will automatically fall back to the ImGui dialog for the remainder of the session.

If you want to disable the native dialog entirely at build time (so ImGui is always used), configure with `-DUSE_NATIVE_FILE_DIALOG=OFF`.
