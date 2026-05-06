# ROCm Optiq Launcher (VS Code extension)

A minimal VS Code extension that launches the existing `roc-optiq`
(rocprofiler-visualizer) desktop application from inside VS Code, plus an
experimental WebAssembly panel that renders the ImGui UI inside the IDE.

The native launcher remains a thin wrapper: it spawns the already-built
`roc-optiq` binary as a detached child process and forwards a file path / CLI
flags using the same options that `main.cpp` already accepts (`--file`,
`--backend`, `--file-dialog`).

The WebAssembly panel is a separate experimental path. It serves
`build-wasm/index.{html,js,wasm}` through `src/app/wasm/serve.py` with the
headers required by Emscripten pthreads, then embeds that page in a Cursor /
VS Code webview.

## Features

- Command **ROCm Optiq: Open Visualizer** — launch the app with no file
- Command **ROCm Optiq: Open File in Visualizer...** — pick a trace and open it
- Command **ROCm Optiq: Open Current File in Visualizer** — open the editor's current file
- Command **ROCm Optiq: Open in Panel (WebAssembly preview)** — open the WASM UI inside the IDE
- Command **ROCm Optiq: Open File in Panel...** — copy a disk file into the WASM virtual filesystem and open it
- Command **ROCm Optiq: Locate Executable...** — set the path to the binary
- Right-click any `.rpd`, `.db`, `.yaml`, or `.rpv` file in the Explorer →
  **ROCm Optiq: Open File in Visualizer...**

## Settings

| Setting | Description |
| --- | --- |
| `rocOptiq.executablePath` | Absolute path to the `roc-optiq` binary. If empty, the extension auto-detects common CMake build output folders inside the workspace. |
| `rocOptiq.backend` | `auto` / `vulkan` / `opengl` — passed to `roc-optiq --backend`. |
| `rocOptiq.fileDialog` | `auto` / `native` / `imgui` — passed to `roc-optiq --file-dialog`. Use `imgui` when running over SSH. |
| `rocOptiq.extraArgs` | Additional CLI args forwarded verbatim to `roc-optiq`. |

## Auto-detection

If `rocOptiq.executablePath` is empty, the extension searches each workspace
folder for the binary in these locations (in order):

```
build/x64-debug/Debug/roc-optiq[.exe]
build/x64-debug/Release/roc-optiq[.exe]
build/x64-release/Release/roc-optiq[.exe]
build/Debug/roc-optiq[.exe]
build/Release/roc-optiq[.exe]
build/roc-optiq[.exe]
out/build/x64-Debug/roc-optiq[.exe]
out/build/x64-Release/roc-optiq[.exe]
```

If none of those exist, run **ROCm Optiq: Locate Executable...** and pick
the binary manually, or set `rocOptiq.executablePath` in your settings.

## Development

This extension has zero npm dependencies. To try it locally:

1. Open the repository root in VS Code.
2. Open the `vscode-extension/` folder in a separate VS Code window:
   `code vscode-extension`.
3. Press `F5` to launch an Extension Development Host.
4. In the new window, run any of the **ROCm Optiq:** commands from the
   command palette (`Ctrl+Shift+P`).

To produce a `.vsix` for distribution:

```bash
npm install -g @vscode/vsce
cd vscode-extension
vsce package
```

That produces `roc-optiq-launcher-<version>.vsix`, which can be installed
via **Extensions: Install from VSIX...** in VS Code.

## Why a launcher and not a webview?

The desktop app uses ImGui on top of GLFW with a Vulkan or OpenGL backend
(see `src/app/src/main.cpp` and `src/app/src/rocprofvis_imgui_backend.cpp`).
That stack draws into its own native window and cannot be embedded directly
inside a VS Code webview, which is a sandboxed Chromium frame. Embedding the
UI inside VS Code would require either:

- compiling the ImGui view layer to WebAssembly with Emscripten and a
  WebGL/WebGPU backend, or
- rewriting the UI in TypeScript/HTML and reusing the C++ controller and
  data model behind it.

Both are real ports rather than wrappers. This extension is the
zero-divergence option that gives VS Code users one-click access to the
existing app today.
