# ROCm Optiq Loop (VS Code / Cursor extension)

The editor half of the Ask Optiq closed loop: **profile, analyze, edit, profile
again**. Optiq reads the trace and works out what to change; this extension is
what actually writes the change into your workspace and rebuilds it.

Optiq never edits files itself, and this extension never talks to a profiler.

## What it does

It listens on `127.0.0.1` with an ephemeral port and writes a handshake file
into Optiq's own config directory:

| Platform | Handshake file |
|----------|----------------|
| Windows  | `%LOCALAPPDATA%\AMD\ROCm-Optiq\optiq-ide.json` |
| macOS    | `~/Library/Application Support/ROCm-Optiq/optiq-ide.json` |
| Linux    | `$XDG_CONFIG_HOME/rocm-optiq/optiq-ide.json` (or `~/.config/...`) |

That file carries the port, a per-session token, the workspace path, and
whether the window is a Remote-SSH one. Optiq is the client; the extension
never dials out. The file is removed on shutdown, which is how Optiq knows no
editor is attached.

Two requests, both requiring the token:

- `POST /read` - hands back a file so the assistant can match on the real text
  instead of guessing.
- `POST /apply` - replaces one **unique** occurrence of a string, saves, shows
  you the file, then runs your build command and returns its output.

A `find` string that matches zero times, or more than once, is refused. A change
you cannot check at a glance is not applied.

## Install

```bash
cd extensions/vscode-optiq
npm install
npm run compile
```

Then load it: **Run and Debug > Run Extension** in VS Code, or package it with
`npx @vscode/vsce package` and install the `.vsix`.

## Configure the build

Set `optiq.buildCommand` in settings to whatever rebuilds your project. It runs
from the first workspace folder.

```json
{
  "optiq.buildCommand": "cmake --build build"
}
```

With it unset, an approved change is still applied, but Optiq is told nothing
was compiled - so it will not offer to profile again until you rebuild.

## Two supported setups

**Local.** Editor and Optiq on the same machine. The handshake lands where
Optiq looks and there is nothing else to do.

**Remote-SSH.** You open the GPU box in VS Code over Remote-SSH, so this
extension runs *there* - which is right, because that is where the source and
the compiler are. Two things then bridge the gap, and both are automatic:

- The extension calls `vscode.env.asExternalUri`, so VS Code forwards its port
  back to your machine, and puts that `localhost` URL in the handshake as
  `externalUrl`. Optiq still only ever dials its own loopback.
- Optiq reads the handshake off the remote by running one `cat` over the SSH
  connection from your profiler launch profile.

For that second step Optiq needs to know which host to ask, so load the remote
launch profile you profile with (or have exactly one saved SSH connection).
Ask Optiq finds the editor when it calls `loop_status`.

If the port cannot be forwarded, the extension says so in a warning rather than
leaving Optiq to time out.
