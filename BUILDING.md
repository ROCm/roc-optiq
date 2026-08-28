# Build Instructions

This document describes how to build ROCm Optiq (roc-optiq) on Windows, Linux, and macOS. Linux dependency lists are based on the GitHub Actions workflows in [.github/workflows](.github/workflows).

## Common setup

1. Clone the repository with submodules:
   - `git clone --recursive <repo-url>`
   - The `thirdparty/mbedtls` submodule is only needed when you enable remote/SSH or agentic profiling, and `thirdparty/cpp-httplib` only for agentic profiling (see the options below). If you already have a non-recursive clone, run `git submodule update --init --recursive`.
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

### MSI installer (WiX v4)

The `PACKAGE_WIX` CMake target builds a self-contained MSI using a hand-authored WiX v4 source file (`wix/roc-optiq.wxs`).
We are currently using WiX v4.0.6 for building. An upgrade to WiX v7 is planned for a later date.

#### One-time setup

Install the WiX v4 CLI and the UI extension (requires the [.NET SDK](https://dotnet.microsoft.com/download)):

```powershell
dotnet tool install --global wix --version 4.0.6
wix extension add --global WixToolset.UI.wixext/4.0.6
```

#### Building the installer

Building the installer is a two stage process - first we need to build the application, and then we can build the installer.

**Stage 1 — build the application:**

```powershell
cmake --preset "x64-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/x64-release --preset "Windows Release Build" --target roc-optiq --parallel 4
# Output: build\x64-release\Release\roc-optiq.exe
```

**Stage 2 — build the installer:**

```powershell
cmake --build build/x64-release --preset "Windows Release Build" --target PACKAGE_WIX
# Output: build\x64-release\roc-optiq-X.X.X.X-win64.msi
```

CMake will not recompile the application between stages because no sources have changed. `PACKAGE_WIX` picks up the executable already on disk and passes it directly to `wix.exe`.

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

## Agentic profiling (Ask Optiq)

The in-app assistant is opt-in and **disabled by default** (the feature set is
in development). Enable it at configure time with
`-DROCPROFVIS_ENABLE_AGENTIC_PROFILING=ON`. When enabled, the build pulls in
`cpp-httplib` for in-process HTTPS, mbedTLS to back it, and the OS credential
vault used to hold the API token; with the option off, none of the three is
compiled and the panel, its toolbar buttons, its View-menu entry, and its
settings page are all absent. Sources live under
`src/view/src/agenticprofiling/`.

`cpp-httplib` and mbedTLS are both submodules, so a non-recursive clone must
initialize them before configuring with this option on:
`git submodule update --init thirdparty/cpp-httplib thirdparty/mbedtls`.
Configure fails with an explicit message if `thirdparty/cpp-httplib` is still
empty.

Saved endpoint URLs and model names round-trip through `settings.json` whether
or not the option is on, so switching between builds does not discard an
assistant configuration. The API token itself lives in the OS credential store
and is only reachable from a build with the option enabled.

## Remote / SSH support

Remote/SSH connectivity and remote profiling are opt-in and **disabled by
default** (the feature set is in development). Enable it at configure time with
`-DROCPROFVIS_ENABLE_REMOTE=ON`. When enabled, the build pulls in the SSH
transport (`libssh2`), a crypto backend, and the OS credential vault used to
persist SSH secrets. Each of these has its own dependency notes below.

### Crypto backend

The SSH and remote-profiling features use `libssh2`, which needs a crypto backend selected at configure time via `CRYPTO_BACKEND`.

- **Default: `mbedTLS`** — vendored under `thirdparty/mbedtls` and linked statically. This is the default build and requires **no extra dependency to install** and **nothing extra to deploy**. While remote features are disabled by default this is what ships. mbedTLS is also built when `ROCPROFVIS_ENABLE_AGENTIC_PROFILING` is on, independently of `ROCPROFVIS_ENABLE_REMOTE`, because the Ask Optiq assistant links it through `cpp-httplib` for in-process HTTPS; with both options off it is not built at all.
- **Opt-in: `OpenSSL`** — configure with `-DCRYPTO_BACKEND=OpenSSL`. OpenSSL is **not** vendored; it is resolved as an external dependency via `find_package(OpenSSL)`, the same way the Vulkan SDK is treated. Install a system OpenSSL first:

| Platform | Install | Notes |
|----------|---------|-------|
| Linux (Ubuntu/Debian) | `sudo apt install -y libssl-dev` | |
| Linux (RHEL/Rocky/Oracle) | `sudo dnf install -y openssl-devel` | |
| macOS | `brew install openssl@3` | configure with `-DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)` |
| Windows | `choco install openssl -y` (recommended; matches CI) | installs to `C:\Program Files\OpenSSL-Win64`; set `OPENSSL_ROOT_DIR` to the install root |

On Windows, [Chocolatey](https://chocolatey.org/install) is the recommended way to install OpenSSL because it is exactly what the Windows CI workflow (`.github/workflows/ci-windows.yml`) uses, so a local build matches CI. From an elevated (Administrator) PowerShell:

```powershell
choco install openssl -y
```

This installs a VC-compatible build (the Shining Light [Win64 OpenSSL](https://slproweb.com/products/Win32OpenSSL.html) package) to `C:\Program Files\OpenSSL-Win64`. Then point CMake at it when configuring:

```powershell
cmake --preset "x64-release" -DCRYPTO_BACKEND=OpenSSL -DOPENSSL_ROOT_DIR="C:\Program Files\OpenSSL-Win64"
```

Newer Chocolatey/Shining Light builds nest the MSVC import libraries under `lib\VC\x64\MD`. If `find_package(OpenSSL)` fails to locate them from `OPENSSL_ROOT_DIR` alone, pass explicit hints (this mirrors what CI does):

```powershell
cmake --preset "x64-release" -DCRYPTO_BACKEND=OpenSSL `
  -DOPENSSL_ROOT_DIR="C:\Program Files\OpenSSL-Win64" `
  -DOPENSSL_INCLUDE_DIR="C:\Program Files\OpenSSL-Win64\include" `
  -DLIB_EAY_RELEASE="C:\Program Files\OpenSSL-Win64\lib\VC\x64\MD\libcrypto.lib" `
  -DSSL_EAY_RELEASE="C:\Program Files\OpenSSL-Win64\lib\VC\x64\MD\libssl.lib"
```

When the OpenSSL backend is built, its runtime libraries are deployed automatically: DLLs are copied next to `roc-optiq.exe` on Windows, `libssl`/`libcrypto` dylibs are staged into the `.app` bundle's `Frameworks` on macOS, and the Linux `.deb`/`.rpm` declares the system OpenSSL runtime as a dependency.

On macOS the staging step (`cmake/macos_openssl_fixup.sh.in`) resolves the **versioned** dylibs (e.g. `libssl.3.dylib`), copies them under their real names, and rewrites every recorded OpenSSL load command to `@rpath/<name>` using `otool`. This avoids the failure mode where the executable kept an absolute Homebrew reference (`Library not loaded: /opt/homebrew/.../libssl.3.dylib`) and crashed at launch on a clean machine.

Example (OpenSSL backend on Linux):

```bash
sudo apt install -y libssl-dev
cmake -B build/linux-release --preset "linux-release" -DCRYPTO_BACKEND=OpenSSL
cmake --build build/linux-release --preset "Linux Release Build" --parallel 4 --target package
```

### Credential storage (SSH secret vault)

When remote/SSH support is enabled, the app can persist SSH passwords and
key passphrases in the operating system's credential vault rather than
prompting for them on every connect. This is handled by `SecretStore`
(`src/view/src/remote/rocprofvis_secret_store.cpp`), which links a different
backend per platform:

| Platform | Backend | Extra dependency to install |
|----------|---------|------------------------------|
| Windows | Windows Credential Manager (`Advapi32` / `wincred.h`) | None — part of the Windows SDK |
| macOS | Keychain (`Security` + `CoreFoundation` frameworks) | None — part of the system SDK |
| Linux | libsecret (Secret Service / freedesktop.org, e.g. GNOME Keyring or KWallet) | `libsecret-1` development package (**optional**) |

On Windows and macOS the credential store is always available and needs nothing
extra installed. On Linux it is **optional**: CMake probes for `libsecret-1`
via `pkg-config` at configure time.

- If found, it is linked and `ROCPROFVIS_HAVE_LIBSECRET` is defined, enabling
  secure persistence of SSH secrets.
- If **not** found, CMake emits a warning and `SecretStore` compiles a stub
  that reports itself unavailable. The build still succeeds, but SSH secrets
  are never persisted — the user is prompted at connect time instead (secrets
  are never written to disk in plaintext).

To enable secure credential storage on Linux, install the development package
before configuring:

| Platform | Install |
|----------|---------|
| Linux (Ubuntu/Debian) | `sudo apt install -y libsecret-1-dev` |
| Linux (RHEL/Rocky/Oracle) | `sudo dnf install -y libsecret-devel` |

At runtime, a Secret Service provider (e.g. `gnome-keyring` or `kwallet` with
its Secret Service interface) must be running for storage/retrieval to
actually work; if none is present, `SecretStore::IsAvailable()` returns false
and the app falls back to prompting.

Example (remote support with credential storage on Ubuntu):

```bash
sudo apt install -y libsecret-1-dev
cmake -B build/linux-release --preset "linux-release" -DROCPROFVIS_ENABLE_REMOTE=ON
cmake --build build/linux-release --preset "Linux Release Build" --parallel 4 --target package
```

---

## Artifacts

- Linux: packages are emitted into the build directory (e.g., `.deb`, `.rpm`, `.gz`).
- Windows: the executable is in `build/<preset>/<config>/roc-optiq.exe`; the MSI (when built via `PACKAGE_WIX`) is in `build/<preset>/roc-optiq-<version>-win64.msi`.
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
