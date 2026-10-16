# AGENTS.md - ROCm Optiq

The root file is the repo entry point. Layer-specific AI/agent guides
live in `.agents/`:

> - [`.agents/UI.md`](./.agents/UI.md) - View-layer architecture,
>   widgets, timelines, profiler launch, and remote UI
> - [`.agents/CONTROLLER.md`](./.agents/CONTROLLER.md) - deep dive on
>   `src/controller/` (C ABI, async fetch, memory manager, segment
>   timeline)
> - [`.agents/DATABASE.md`](./.agents/DATABASE.md) - deep dive on
>   `src/model/` (SQLite adapters, query pipeline, packed table,
>   data model, topology, metadata versioning)
> - [`.agents/SCRIPTING.md`](./.agents/SCRIPTING.md) - planned in-app
>   Python analysis (interpreter lib, controller ABI, phases)

**If you are an AI coding assistant** (Cursor, Codex, Claude Code,
Copilot agent, etc.), read `.agents/UI.md` in full before making
non-trivial changes under `src/view/` or to UI-facing app integration.
If your change touches `src/controller/`, also read
`.agents/CONTROLLER.md`. If it touches `src/model/` (the database /
data-model layer), also read `.agents/DATABASE.md`. If it touches
in-app Python scripting (`src/python/`, `rocprofvis_script_*`, or
script UI), also read `.agents/SCRIPTING.md`. Together these
guides are the single source of truth for:

- Project identity, build, and repo layout
- Module boundaries (`app` / `core` / `model` / `controller` / `view`)
- The View layer top-down tour and full widget inventory
- Track-item, trace-view, and compute-view internals
- UI models and cross-cutting services (events, settings, monitoring,
  logging, hotkeys, notifications)
- Compare, measurement, profiler-launch, and remote/SSH workflows
- Data flow (click -> request -> event -> pixels)
- Coding conventions, comment style, and reuse catalog
- Common pitfalls and a quick-reference index of every UI class

**Human contributors** should read these in order:

1. [`README.md`](./README.md) - what the app is and how to use it
2. [`BUILDING.md`](./BUILDING.md) - per-platform build steps
3. [`CODING.md`](./CODING.md) - hard rules on style, naming, format
   (READ THIS FIRST before changing any C++)
4. [`.agents/UI.md`](./.agents/UI.md) - View architecture and reuse
   guide (read for UI work)
5. [`.agents/CONTROLLER.md`](./.agents/CONTROLLER.md) - controller
   deep dive (read when working on `src/controller/`)
6. [`.agents/DATABASE.md`](./.agents/DATABASE.md) - database / model
   layer deep dive (read when working on `src/model/`)
7. [`.agents/SCRIPTING.md`](./.agents/SCRIPTING.md) - Python analysis
   scripting design (read when working on `src/python/` or script ABI)
8. [`.github/CONTRIBUTING.md`](./.github/CONTRIBUTING.md) -
   contribution workflow

When `CODING.md` and the `.agents/` guides disagree, `CODING.md` wins.
