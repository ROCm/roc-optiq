# AGENTS.md - ROCm Optiq

The full AI/agent guide for this repo lives at:

> [`.agents/AGENTS.md`](./.agents/AGENTS.md)

**If you are an AI coding assistant** (Cursor, Codex, Claude Code,
Copilot agent, etc.), read `.agents/AGENTS.md` in full before making
non-trivial changes. It is the single source of truth for:

- Project identity, build, and repo layout
- Module boundaries (`app` / `core` / `model` / `controller` / `view`)
- The View layer top-down tour and full widget inventory
- Track-item, trace-view, and compute-view internals
- UI models and cross-cutting services (events, settings, hotkeys,
  notifications)
- Data flow (click -> request -> event -> pixels)
- Coding conventions, comment style, and reuse catalog
- Common pitfalls and a quick-reference index of every UI class

**Human contributors** should read these in order:

1. [`README.md`](./README.md) - what the app is and how to use it
2. [`BUILDING.md`](./BUILDING.md) - per-platform build steps
3. [`CODING.md`](./CODING.md) - hard rules on style, naming, format
   (READ THIS FIRST before changing any C++)
4. [`.agents/AGENTS.md`](./.agents/AGENTS.md) - architecture and reuse
   guide (this is also the agent guide above)
5. [`.github/CONTRIBUTING.md`](./.github/CONTRIBUTING.md) -
   contribution workflow

When `CODING.md` and `.agents/AGENTS.md` disagree, `CODING.md` wins.
