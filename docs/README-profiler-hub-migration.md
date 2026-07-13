# Reference Migration: Optiq Loader onto the ProfilerHub Reader API

## Status and framing

This branch is a **prototype, not a proposal**. To validate that the ProfilerHub reader API
(the `get_interval_track` / `get_scalar_track` / `get_flows` surface described in
`profiler-hub-new-reader-api-overview.md`, over in ProfilerHub) actually covers everything
Optiq's loader needs — not just in theory, but against real query semantics — this branch
migrates Optiq's own loader end to end, one track type at a time, and verifies each migration
for fidelity against the existing SQL-based behavior it replaces.

This branch lives on a personal fork (`avansick-amd/roc-optiq`), not on the official
`ROCm/roc-optiq` repo, and nothing here is merged or PR'd against that repo. It's here so the
Optiq team can look at a working, fidelity-checked example before any decision is made — you're
free to adopt all of it, part of it, or none of it. Where this document says "replaces," it
means "replaces in this prototype branch," not "should replace in your codebase."

## What changed, at a glance

Baseline: `48e29fbd` ("Duplicate File Open Protection"). Nine commits on top:

| Commit | Track type / capability | What it replaced |
|---|---|---|
| `adc7a6b7` | Build wiring | Adds `find_package(profiler-hub REQUIRED)` and links the imported `profiler-hub::profiler-hub` target into the datamodel library — the prerequisite for every migration below |
| `344fef7f` | `cpu_thread` | Standalone CPU-region SQL discovery |
| `6cbdf6e5` | `gpu_queue` + `stream` | Queue-keyed kernel-dispatch SQL + the cross-cutting Stream-track SQL |
| `59a80078` | `memory` (standalone memory-allocate) | `GetRocprofMemoryAllocTrackQuery`/`LevelQuery`/`SliceQuery` |
| `722bc2d7` | `dma` (standalone memory-copy) | `GetRocprofMemoryCopyTrackQuery`/`LevelQuery`/`SliceQuery`/`TableQuery` |
| `79a22f84` + `697f532a` | `counter` (SMI PMC) | Standalone counter-track SQL; second commit made system-test assertions order/index-independent |
| `aab7609d` | dataflow (`get_flows()`) | Four `GetRocprofDataFlowQueryFor*` SQL methods, deleted entirely |
| `aa16c861` | — | Formatting only (clang-format-18, no logic change) |

Net diff vs. baseline: 10 files, +6,870/-4,985 lines. The two largest files by volume are
`rocprofvis_db_query_factory.cpp` (losing most of its hand-written SQL generation) and
`rocprofvis_db_rocprof.cpp` (gaining the adapter functions described below).

## The pattern, once, since it repeats seven times

Every track-type migration in this branch follows the same two-function shape in
`rocprofvis_db_rocprof.cpp`:

1. **`Reader<Type>TrackToTrackParams(...)`** — a small adapter that reads a ProfilerHub
   `track_info_t` (id, agent/queue/stream/thread/pmc info) and populates Optiq's internal
   `rocprofvis_dm_track_params_t` (identifiers, category, op-type). This is where reader fields
   map onto Optiq's existing topology/naming conventions — e.g. numeric `agent_info->id` feeding
   `TRACK_ID_AGENT` for GPU-topology nesting.
2. **`AddReader<Type>Tracks(Future* future)`** — the discovery/load function. Checks the
   metadata-version cache first (a cache hit skips the reader entirely and reloads from the DB's
   own saved track table); on a cache miss, calls `reader->get_all_tracks()`, filters to the
   relevant `track_type_t`, and for each track computes `record_count` via `get_track_stats()`
   and min/max timestamp/level via a single `get_interval_track()` walk. Threaded per DB
   instance, matching the existing threading model.

At the call site, the old block — typically a raw `ExecuteSQLQuery` wired to 3-6 hand-written
`QueryFactory` methods via callbacks — collapses to a single `AddReader<Type>Tracks(future)`
call.

## Track-type notes worth knowing

- **`dma`**: the reader keys these tracks by *destination agent* (not stream), a deliberate
  reader-side design choice made specifically so this migration wouldn't change Optiq's existing
  by-agent swimlane grouping. `queue_info` isn't exposed on the public `dma` track, so `queue_id`
  defaults to 0 in the adapter — harmless against the verification fixture (already 0 there), but
  worth knowing if a different fixture has non-zero queue ids on dma tracks. Verified against
  `rocpd-transpose.db`: 2 dma tracks partitioned by destination agent (24 events each), per-event
  category `rocm_memory_copy` preserved, 387/387 system + 465,050/465,050 compute assertions
  green.
- **`counter`**: track count matches exactly (18 PMC tracks, same as the old SQL SMI path), and
  compute-tests are exact (465,050/465,050). But `datamodel-system-tests-DB` settled at 388
  assertions (was 387) — a deterministic +1 traced to the reader doing correct 462-sample
  deduplication where the old SQL's AMD-SMI `event_id` path fanned out to 2,772 rows. That's a
  more-correct topology, not a data loss, and was accepted as the corrected baseline. Because the
  old test suite picked assertions by numeric index over a track ordering that shifted shape once
  this fixed, the `697f532a` companion commit makes those system-test assertions order/index-
  independent so this class of false-positive can't recur.
- **Dataflow (`get_flows()`)**: the migration with the strongest fidelity evidence. It replaces
  four per-event `GetRocprofDataFlowQueryFor*` SQL branches with two eager, per-database-instance
  in-memory indexes built from one `get_flows()` call each: a TOPOLOGY index (undirected
  adjacency keyed on typed `(event_type, opaque_id)` pairs, since raw opaque ids collide across
  the region/kernel_dispatch/memory_copy/memory_allocate tables) and a PAYLOAD index (event
  metadata from `get_all_tracks()` + `get_interval_track()`, restricted to the four native
  single-table track types to avoid double-keying). Verified byte-identical against a pristine
  pre-migration SQL oracle (a separate frozen worktree, `roc-optiq-sql-oracle` at `48e29fbd`) for
  region, kernel-dispatch, and memory-copy causal edges, plus a fabricated fixture for
  memory-allocate edges — including reproducing the memory-allocate asymmetry (no sibling leg)
  that the old SQL also had. The four old SQL methods are deleted outright rather than left
  dormant.

## Verification evidence

Each migration commit's message documents its own fidelity check (membership verification
against `rocpd-transpose.db`, or the SQL-oracle comparison for flows), plus the project-wide
regression gates: `datamodel-system-tests-DB` and `datamodel-compute-tests` both pass at every
step, with the one documented, understood exception (system-tests count moving 387→388 across
the counter migration, explained above and locked down by the order-independence fix).

## Bottom line for the Optiq team

If you want to adopt any of this, this branch is a working reference for exactly how each track
type maps onto the ProfilerHub reader API and what each migration's fidelity check looked like.
If you'd rather keep your existing SQL path for some or all track types, nothing here forces a
change — the reader API itself is the durable interface; this branch is just proof it's
sufficient to build on. Questions, or want to walk through any specific migration in more depth,
just ask.
