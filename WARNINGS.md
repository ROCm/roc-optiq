# Warnings & Code-Quality Audit

Branch: `dhingora/warning-fixes`  •  Generated: 2026-05-13  •  Last refreshed: 2026-05-15 (clean build, post C4100 sweep)
Build configuration: `x64-debug` (MSVC 19.50, `/W4 /EHsc`, Visual Studio 18 2026)
Source log: [`build/x64-debug-c4100-build.log`](build/x64-debug-c4100-build.log) (**full clean rebuild** after C4100 cleanup, all uncommitted on top of `5ab57678`)

This document combines:
1. **Compile-time warnings** emitted by MSVC `/W4` during a clean Debug build.
2. A **manual static review** of the first-party source tree (`src/`) for issues that the compiler did not (or could not) flag — bad patterns, smells, latent bugs.

`thirdparty/` is intentionally excluded from both sections (jsoncpp, yaml-cpp, glfw, catch2, nativefiledialog-extended, etc. are not our code).

> **Tracking convention.** Warnings are not deleted from this report after they're fixed; instead each entry gets a leading **`✓`** (fixed) or **`·`** (open) marker and the per-code/per-file counts gain a "Fixed" column. The original counts (the build snapshot) are preserved as the baseline.

---

## 0a. Pre-commit checklist (read before any commit)

> Things deliberately left in a temporary state during the warning sweep that **must** be cleaned up before any commit. Add a new line every time you take a "I'll come back to this" shortcut.

*(All items previously listed here have been actioned and the corresponding lines deleted from source. The `.clangd` IntelliSense workaround is intentionally not part of the warning-cleanup commits and is left in the working tree only.)*

---

## 0. Progress log

> Reverse-chronological. Each entry: date • category • count fixed • short note.

### 2026-05-15 — C4100 (165/165 unique sites; 481/481 raw emissions) ✓ category complete

Cleared the entire C4100 category — **all 481 raw warnings collapsed to 165 unique source sites** (the headers are included by many translation units, so MSVC re-emits per-TU). Every site followed one of three near-identical patterns:

- **Pattern A — virtual base no-op default** (~280 raw / ~95 unique): e.g. `virtual void InterruptQuery(void* connection) {};` and `virtual rocprofvis_dm_result_t Cleanup(Future* future, bool rebuild) { return kRocProfVisDmResultSuccess; };`. Lives in `rocprofvis_db.h`, `rocprofvis_db_compute.h`, `rocprofvis_db_profile.h`, `rocprofvis_dm_topology.h`. Base-class methods that intentionally do nothing; derived classes override and use the params.
- **Pattern B — derived "not supported here" assert** (~150 raw / ~55 unique): e.g. `ReadFlowTraceInfo(rocprofvis_dm_event_id_t event_id, Future* object) override { ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Compute database does not support flow trace", …); }`. The override exists for interface conformance only; the params are documented on the base.
- **Pattern C — out-of-line `.cpp` not-supported** (~50 raw / ~15 unique): same as B but the body lives in `rocprofvis_dm_track_slice.cpp` (21 sites) and `rocprofvis_dm_base.cpp` (15 sites).

**Mechanism — comment out the parameter name** (`Type name` → `Type /*name*/`). Universal C++98+, position-independent, guaranteed to suppress C4100 in MSVC, and preserves the original name for readers and for derived-class implementers copying the signature. **Why not `[[maybe_unused]]`?** First-attempt found that MSVC treats `Type [[maybe_unused]] name` as the attribute appertaining to the type, not the parameter declaration — C4100 is still emitted. Placing the attribute at the start of the parameter declaration (`[[maybe_unused]] Type name`) requires robust detection of the type-token start across multi-line and template-laden signatures, which the `/*name*/` trick avoids entirely. The first attempt was fully reverted (`git checkout -- src/`) before the `/*name*/` approach was applied.

**Mechanics — auto-fixer script** at [`build/fix_c4100.ps1`](build/fix_c4100.ps1):
1. Parses MSVC warning lines from the build log into `(file, line, col, paramname)` tuples.
2. Dedupes by tuple (cuts 481 → 165) and groups by file, processing bottom-up so column offsets stay valid.
3. For each site, locates `\b<param>\b` on the reported line; if not found exactly once (lambdas / multi-line signatures), probes 1–4 lines above for a unique standalone occurrence.
4. Wraps it as `/*<param>*/`. Skips idempotently if already commented.
5. Writes the modified file via `[System.IO.File]::WriteAllLines`.

**Result:** 164 of 165 sites auto-edited cleanly across **30 files** (model + controller + view + test). The 1 remaining site (`rocprofvis_timeline_view.cpp:130:5 param=e` — a lambda capturing `std::shared_ptr<RocEvent> e` whose declaration line had multiple `e` matches) was fixed manually with a single `StrReplace`. Full clean rebuild produces **0 errors, 0 C4100 warnings**, no other categories touched.

**Distribution of edits** (30 files):
- Headers (4 files, ~95 sites): `rocprofvis_db.h` (47 unique), `rocprofvis_db_compute.h` (43 unique — combines Pattern A bases + Pattern B overrides), `rocprofvis_db_profile.h` (15 unique), `rocprofvis_dm_topology.h` (10 unique).
- Datamodel `.cpp` (3 files, ~38 sites): `rocprofvis_dm_track_slice.cpp` (21), `rocprofvis_dm_base.cpp` (15), `rocprofvis_dm_table_row.cpp` (2).
- Database `.cpp` (8 files, ~22 sites): `rocprofvis_db_compute.cpp` (8), `rocprofvis_db_profile.cpp` (3), `rocprofvis_db_rocprof.cpp` (2), `rocprofvis_db_rocpd.cpp` (2), `rocprofvis_db_packed_storage.cpp` (2), `rocprofvis_db_sqlite.cpp` (1), `rocprofvis_db_cache.cpp` (1), `rocprofvis_db_table_processor.cpp` (1).
- Controller (4 files, ~9 sites): `rocprofvis_controller_track.cpp` (6), `rocprofvis_controller_sample.cpp` (3, including the `(void)property;` from the C4065 cleanup that was technically also a C4100 emission), `rocprofvis_controller_trace_compute.cpp` (1), `rocprofvis_controller_mem_mgmt.cpp` (1).
- View (5 files, ~5 sites): `rocprofvis_track_topology.cpp` (2), `rocprofvis_timeline_view.cpp` (1, manual), `rocprofvis_track_item.cpp` (1), `rocprofvis_compute_widget.cpp` (1), `rocprofvis_infinite_scroll_table.cpp` (1), `rocprofvis_compute_summary.cpp` (1).
- Tests + entry (3 files, ~4 sites): `rocprofvis_dm_compute_tests.cpp` (1), `rocprofvis_dm_system_tests.cpp` (1), `model/src/test/main.cpp` (1), `rocprofvis_dm_trace.cpp` (1), `rocprofvis_dm_table.cpp` (1).

**Behavioral guarantee:** zero possible runtime impact. Commenting out a parameter name does not change the compiled function — the parameter still appears in the signature, callers and the ABI are unaffected, and any code that *did* reference the parameter would have failed to compile (none did, since these are exactly the names the compiler told us are unreferenced).

Build: full clean rebuild via `cmake --build --preset "Windows Debug Build" --clean-first`, **0 errors, 0 C4100 emissions** (down from 481), total first-party warnings **677 → 196** (−481, **71 % drop in one pass**). Only **C4244 (187)** and the deferred **C4996 (9)** categories remain. Verified via `build/x64-debug-c4100-build.log`.

### 2026-05-15 — C4267 (35/35) + trivial-C4996 (4/13) sweep ✓ C4267 category complete

Cleared the **35 newly-surfaced C4267 sites** (all in view/widgets/compute + a controller cluster the original C4267 sweep didn't reach) and the **4 trivial C4996 sites** (`getch`→`_getch`, 3× `vsprintf`→`vsnprintf`) in **39 surgical edits across 13 files**. The remaining 9 C4996 sites — 4× `strncpy` and 5× `getenv` — are deliberately deferred (see §4.3); they need either an API-contract decision (the `rocprofvis_controller_data.cpp:282` `strncpy` hides a real null-termination bug) or a small portable wrapper (the `getenv` cluster). Sites:

- **C4267 (35) — same template as the prior model-side sweep: explicit `static_cast<DestType>(...)`:**
  - `controller_table_compute_pivot.cpp:167, :363, :379` — `params.size()` → `rocprofvis_db_num_of_params_t`, `m_columns[i].m_name.size()` and `title.size()` → `uint32_t` for `*length` writes.
  - `controller_trace_compute.cpp:667` — `query_args.size()` → `rocprofvis_db_num_of_params_t`.
  - `controller_future.cpp:187` — `m_progress_map.size()` → `uint16_t` for the `/=` against `m_progress_percentage`.
  - `controller_mem_mgmt.cpp:132, :473, :698, :701, :715` — five sites: `exponent << 11` → `uint32_t` for `m_mem_block_size` write; `size` (size_t param) → `uint32_t` for `MemoryPool` ctor; three `BitSet::FindFirstZero/Count` returns where `bit_pos`/`Size()`/`total` (size_t) feed `uint32_t` returns.
  - `controller_table_system.cpp:64-65, :268-269` — two pairs of `m_tracks.size()` and `m_string_table_filters_ptr.size()` → `rocprofvis_db_num_of_tracks_t` / `rocprofvis_dm_num_string_table_filters_t` for `rocprofvis_db_build_table_query` arguments.
  - `controller_track.cpp:691, :698, :705` — three `strlen(str)` (size_t) → `uint32_t` for `GetStringImpl` calls in three adjacent `case kRPVControllerTrackExtData*` blocks.
  - `compute_summary.cpp:562, :664, :673, :755, :780` — five sites: two ImPlot `.size()` arguments → `int`, two `ImGui::PushID(i)` (size_t loop counter) → `int`, one `TableSetColumnIndex(2 + j)` → `int`.
  - `compute_workload_view.cpp:129, :173` — two `ImGui::PushID(i)` → `int`.
  - `summary_view.cpp:684, :772, :895, :909, :918, :956, :971` — seven sites: `PlotPieChart` size argument → `int`, two `GetColormapColor(i)` → `int`, one `PushID(i)` → `int`, three `int = size_t` assignments (`hovered_idx = i`, two `idx = i`).
  - `compute_widget.cpp:175, :180, :444, :448` — two `m_rows.size()` → `uint32_t` for `max_rows`, two `row.values[value_index + 3]` index conversions to `uint32_t` (the map's key type).
- **C4996 (4 trivial sites):**
  - `model/src/test/main.cpp:33` — `vsprintf(buffer, fmt, argptr)` → `vsnprintf(buffer, sizeof(buffer), fmt, argptr)`. Same fix in `tests/rocprofvis_dm_compute_tests.cpp:77` and `tests/rocprofvis_dm_system_tests.cpp:89`. All three are demo/test `PrintHeader` helpers writing into a fixed `char buffer[256]`; adding the size argument is a strict safety improvement at zero behavioral cost.
  - `model/src/test/main.cpp:141` — `getch()` → `_getch()` (MSVC-conformant name; same conio function).

Build: full clean rebuild, **0 errors, 0 C4267 emissions**, **C4996 down to 9** (the 4 deferred `strncpy` + 5 deferred `getenv` — exactly the count predicted by the deferral plan). Total first-party warnings: **716 → 677** (−39, 5.5 % drop). No new warning categories surfaced; only **C4100 (481), C4244 (187), C4996 (9)** remain. Verified via `build/x64-debug-clean-build.log`.

### 2026-05-15 — C4018 + C4456 + C4189 + C4245 newly-surfaced sites (12/12 fixed) ✓ all four categories now zero

The 5+4+2+1 = 12 newly-surfaced sites from the prior clean-build calibration are all cleared in **12 surgical edits across 7 files**. No new warnings introduced (verified by clean rebuild — the live category set is now exactly **C4100, C4244, C4267, C4996** with the same per-file distribution as before). Per-site list:

- **C4018 (1):**
  - `src/view/src/widgets/rocprofvis_compute_widget.cpp:130` — `for(auto column_index = 0; column_index < m_last_column_index; …)` where `m_last_column_index` is `uint32_t` (declared in `rocprofvis_compute_widget.h:60`). Promoted to `for(uint32_t column_index = 0; …)`.
- **C4456 (2):**
  - `src/view/src/rocprofvis_project.cpp:186` — inner `rocprofvis_result_t result` shadowed outer `OpenResult result` at line 166 (different types, so a rename — not a drop-the-type collapse — is correct). Renamed inner → `controller_result`; the only reference (line 188's `if(result == kRocProfVisResultSuccess)`) updated. Verified line 218's `result = Success;` and line 225's `return result;` are *outside* the inner block and continue to refer to the outer `OpenResult`.
  - `src/controller/tests/rocprofvis_controller_system_tests.cpp:1162` — inner `rocprofvis_result_t result` shadowed the outer `result` at line 1153 (same `rocprofvis_result_t` type, same purpose — capturing the next API call's status). Cleanest fix: dropped the inner type so the assignment goes to the outer. All ~15 subsequent `result = …` / `result == …` references in the for-body continue to work unchanged.
- **C4189 (4) — all dead locals; all RHS expressions verified pure (no side-effects):**
  - `src/controller/tests/rocprofvis_controller_system_tests.cpp:1206` — `uint64_t total = 0;` (only occurrence in the entire file). Deleted.
  - `src/view/src/rocprofvis_minimap.cpp:124` — `const size_t height = data.size();` (the function has a separate, alive `height` at line 85 in a *different* function). Deleted.
  - `src/view/src/rocprofvis_summary_view.cpp:895` — `float text_width = ImGui::CalcTextSize(...).x;` (only occurrence; `CalcTextSize` is a pure measurement function). Deleted.
  - `src/view/src/rocprofvis_time_to_pixel.cpp:71` — `float zoom_before_clamp = m_zoom;` (only occurrence anywhere in `src/`; pure read of `m_zoom`). Deleted. **Parked observation added in §8** — the variable name strongly suggests intent for diagnostics ("save pre-clamp value to compare"), but the comparison was never written; worth a future eyeball.
- **C4245 (5):**
  - `src/controller/src/system/rocprofvis_controller_table_system.cpp:498` — `uint64_t table_type = kRPVControllerTableTypeEvents;` (enum → uint64). Cast: `static_cast<uint64_t>(...)`.
  - `src/controller/tests/rocprofvis_controller_system_tests.cpp:564, :868, :3004` — three `rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsType, 0, kRPVControllerTableType*)` call-site arguments (Events / Samples / SearchResults). Same cast applied to each enum.
  - `src/view/src/rocprofvis_timeline_arrow.cpp:258` — `, m_selection_changed_token(-1)` (initializing `EventManager::SubscriptionToken` = `size_t`). **Bonus quality fix:** replaced the magic `-1` with the existing named constant the header already defines for exactly this case → `, m_selection_changed_token(EventManager::InvalidSubscriptionToken)`. The constant is at `rocprofvis_event_manager.h:24` and is already in scope (the same TU calls `EventManager::GetInstance()` at lines 266, 273).

Build: full clean rebuild, **0 errors, 0 C4018, 0 C4456, 0 C4189, 0 C4245**, total first-party warnings = 716 (down from 725; the 12 fixes net to −9 because 3 of the C4456 / C4189 fixes were inside `rocprofvis_controller_system_tests.cpp` which is a single TU emitting once per warning, while the C4245 cluster in the same file was 3 sites also emitting once each — there were no header-amplified emissions in this batch). Verified via `build/x64-debug-clean-build.log`.

### 2026-05-15 — Clean-build calibration ⚠ baseline corrected upward

A full clean rebuild (`build/x64-debug/` deleted, full reconfigure + build) produced **725 first-party warnings**, materially higher than the 696 reported by the previous *incremental* logs. Root cause: every build log we'd been working from since the C4189/C4456 sweep was an **incremental** build whose dirty set was determined by which headers I'd just touched. Many `.cpp` files were never recompiled, so their `.obj`-cached warnings never appeared in the log — including in test/view/widgets/compute TUs that no header sweep had reached. **No previously-fixed warning regressed**; the apparent increase is purely "warnings that were always there but had been hidden by stale `.obj` files." Concretely, the clean build added: **C4244 +63** (was 121, actual 184), **C4189 +4**, **C4456 +2**, **C4018 +1**, **C4068 +1** (thirdparty pragma), **C4245 +5** (in *different* TUs from the 15 just-cleaned), and **C4996 +11**, while subtracting **C4267 −13** and **C4100 +59** vs the prior estimates. From now on, **all numbers in §1–§3 reflect the clean build** and any future sweep should re-baseline against a clean build, not an incremental one.

### 2026-05-15 — C4456 + C4018 in test files (3/3 + 3/3 fixed) ✓ exposed-by-clean-build cleanup

The clean rebuild surfaced 6 pre-existing warnings in two test files (`src/model/src/test/main.cpp`, `src/model/src/tests/rocprofvis_dm_system_tests.cpp`) that no incremental sweep had reached because those `.cpp` files were always cache-hit. Fixed in the same pass to keep the post-C4245 log free of any apparent regressions.

- `src/model/src/test/main.cpp:190` — `for (int i = 0; i < total_num_tracks; i++)` where `total_num_tracks` is `uint32_t` → `for (uint32_t i …)`. **C4018**.
- `src/model/src/test/main.cpp:291` — inner `uint64_t id` (per-record event id) shadowed outer `uint64_t id` at line 258 (track id). Renamed inner → **`record_id`** (3 references updated: declaration, printf, and the `event_id = { record_id, op }` brace-init). **C4456**.
- `src/model/src/test/main.cpp:317` — inner `rocprofvis_dm_event_id_t event_id` (per-endpoint) shadowed outer `event_id` at line 298 (the parent record's event id). Renamed inner → **`endpoint_event_id`** (3 references updated: declaration, `.value =` write, printf `.bitfield.event_id`). **Carefully verified** that the surrounding outer scope's `event_id` uses at lines 322/329/348/350/359 are *outside* the inner `for` loop body (lines 315–321) and continue to refer to the outer correctly. **C4456**.
- `src/model/src/test/main.cpp:351` — inner `uint64_t num_records` (extdata records) shadowed outer `num_records` at line 274 (slice records). Renamed inner → **`num_extdata_records`** (3 references updated: declaration, printf, loop bound). **C4456**.
- `src/model/src/tests/rocprofvis_dm_system_tests.cpp:172` — `for(int i = 0; i < accessInMillisec; i++)` where `accessInMillisec` is unsigned → `for(uint32_t i …)`. **C4018**.
- `src/model/src/tests/rocprofvis_dm_system_tests.cpp:195` — `for(int i = 0; i < num_tracks; i++)` where `num_tracks` is the function parameter declared `uint32_t` → `for(uint32_t i …)`. **C4018**.

Build: clean full rebuild, 0 errors, both files free of C4456/C4018 (residual warnings in those files are different categories: C4100/C4244/C4996, all pre-existing). Verified via `build/x64-debug-clean-build.log`.

### 2026-05-15 — C4245 (172/172 fixed) ✓ category complete (in covered TUs; 5 newly-discovered sites in view/test TUs deferred)

All 172 build-time emissions surfaced in the previous (incremental) C4267 log cleared via **8 source edits** across 8 files. Pattern is uniform: every site was the **`-1`-as-sentinel-into-unsigned** idiom (assigning/initializing the `int` literal `-1` into a `uint32_t` or `uint64_t` field). Standard fix: `static_cast<DestType>(-1)` (which by C++ standard rules equals `*_MAX` and preserves the "no value" sentinel intent better than `UINT32_MAX` would). Edits:

- `src/model/src/common/rocprofvis_common_types.h:310` — `DbInstance::NoGuidId` retyped from `static constexpr const int NoGuidId = -1;` to `static constexpr uint32_t NoGuidId = static_cast<uint32_t>(-1);`. The comparison at line 315 (`if (m_guid_index == NoGuidId)`) continues to work because both sides are now `uint32_t`. Eats ~23 emissions across all TUs that include this header.
- `src/model/src/database/rocprofvis_db_compute.h:80` — `m_last_matrix_workload_id(-1)` → `static_cast<uint32_t>(-1)`.
- `src/model/src/database/rocprofvis_db_packed_storage.h:43–47` — 5 default-initialized `uint32_t … = -1;` struct fields (`nid_index`, `pid_index`, `process_index`, `sub_process_index`, `stream_index`) → `static_cast<uint32_t>(-1)` each. Together with line 208's reset chain, eats the bulk of the 132 packed-storage emissions.
- `src/model/src/database/rocprofvis_db_packed_storage.h:208` — chained reset `track_ids_indices.nid_index = … = pid_index = -1;` → `… = static_cast<uint32_t>(-1);`.
- `src/model/src/database/rocprofvis_db_profile.cpp:543, :579` — both `track_id = -1;` (where `track_id` is `uint32_t`) → cast.
- `src/model/src/database/rocprofvis_db_rocprof.cpp:566` — `uint32_t file_node_id = -1;` → cast.
- `src/model/src/database/rocprofvis_db_table_processor.cpp:930, :945` — both `track_id = -1;` → cast.
- `src/model/src/database/rocprofvis_db_track.cpp:138` — `MakeKey(id_stream, -1, db_instance)` 2nd arg (decl says `uint64_t id_subprocess`) → `static_cast<uint64_t>(-1)`.
- `src/model/src/datamodel/rocprofvis_dm_topology.h:18` — macro `#define INVALID_BRANCH_LEVEL -1;` rewritten to `#define INVALID_BRANCH_LEVEL (static_cast<uint32_t>(-1))`. **Bonus latent-bug fix:** the old macro had a stray trailing `;` that injected an extra empty statement at every use site (e.g. `return INVALID_BRANCH_LEVEL;` expanded to `return -1;;`). Currently absorbed silently as a null statement, but a footgun the next time someone uses the macro in an expression context (`int x = INVALID_BRANCH_LEVEL + 1;` would become a syntax error).

**Behavioral note:** `static_cast<uint32_t>(-1) == UINT32_MAX` is well-defined by the C++ standard (modular conversion to unsigned). All sentinels and comparisons preserve their original semantics — we just made the narrowing explicit.

**Newly discovered (clean-build only, deferred):** 5 additional C4245 sites in TUs that the previous incremental sweep didn't cover:
- `src/controller/src/system/rocprofvis_controller_table_system.cpp:498` — `rocprofvis_controller_table_type_t` enum → `uint64_t`.
- `src/controller/tests/rocprofvis_controller_system_tests.cpp:564, :868, :3004` — same enum → `uint64_t`, three call-site arguments.
- `src/view/src/rocprofvis_timeline_arrow.cpp:258` — `int` → `RocProfVis::View::EventManager::SubscriptionToken` (which is unsigned).

These weren't part of the targeted 172 (which all came from the model side) and need a follow-up mini-sweep — flagged for the next pass.

Build: clean full rebuild, 0 errors, **0 C4245 emissions in the model TUs**, only the 5 newly-surfaced sites above remain. Verified via `build/x64-debug-clean-build.log`.

### 2026-05-15 — C4267 (79/79 fixed) ✓ category complete

All 79 build-time emissions cleared via **30 source edits** across 10 files. Pattern is uniform: every fix is `static_cast<DestType>(<size_t expression>)`. Zero behavioral change — the truncation was already happening implicitly; we just made it explicit and visible to the reader. Distribution: header returns (`NumDbInstances`, `NumColumns`) — 2 edits → ate ~25 emissions; `.cpp` size-of-container assignments — 16 edits; size-to-int-param call sites — 12 edits. Biggest cluster is 10 edits in `rocprofvis_db_table_processor.cpp` covering the parallel filter / aggregation dispatch + `AddNumRecordsColumn` calls. **Latent bug parked in §8** — `Track::GetHistogramBucketValueAt(size_t)` and `GetHistogramBucketNumEventsAt(size_t)` accept `size_t` but the underlying `histogram` map is keyed by `uint32_t`; the cast silences the compiler but the type contract is internally inconsistent. Build: clean, 0 errors, 0 C4267, no other categories affected. Verified via `build/x64-debug-c4267.log`.

### 2026-05-15 — C4018 (28/28 fixed) ✓ category complete

All 28 build-time emissions cleared via **6 source edits**. The "28" was multi-TU re-emission of a small set of physical sites; one header line (`rocprofvis_db.h:68`) accounted for 23 of the 28 because that header is included by 23 `.cpp` files in `datamodel.vcxproj`. Per-site list:

- `src/model/src/database/rocprofvis_db.h:68` — `OrderedMutex::init`: `for (int i = 0; i < num_instances; i++)` → `for (uint32_t i = ...)`. `num_instances` is `uint32_t`. Eliminates 23 emissions.
- `src/model/src/database/rocprofvis_db_cache.cpp:159` — `PopulateTrackExtendedDataTemplate`: `for (int i = 0; i < num_columns; i++)` → `for (uint32_t i = ...)`. `num_columns` is `uint32_t`; body calls `table.GetColumnName(uint32_t)` / `GetColumnType(uint32_t)` so the promotion also kills a hidden silent narrowing at the call site.
- `src/model/src/database/rocprofvis_db_cache.cpp:176` — `PopulateTrackTopologyData`: same loop pattern, same fix.
- `src/model/src/database/rocprofvis_db_compute.cpp:489` — `GetComputeMetricValues`: `for (int i = 0; i < num; i++)` → `for (uint32_t i = ...)`. `num` has type `rocprofvis_db_num_of_params_t` which is `typedef uint32_t`.
- `src/model/src/database/rocprofvis_db_compute.cpp:550` — `GetComputeMetricValuesByWorkload`: same pattern, same fix.
- `src/model/src/database/rocprofvis_db_profile.cpp:852` — outer `BuildAsyncQueries`-style loop: `for (int j = 0; j < split_count; j++)` → `for (uint32_t j = ...)`. `split_count` is `uint32_t`; body passes `j` to `BuildTrackQuery(..., uint32_t split_index)` so this also closes a hidden int→uint32_t narrowing at the call site.

**Pattern:** every site was the canonical `for (int X = 0; X < <unsigned-bound>; X++)`. Promoting the counter to `uint32_t` is the standard fix; semantics are unchanged because `int < uint32_t` was already implicitly promoting `i` to `uint32_t` for the comparison anyway. **Bonus reductions** (other categories also dropped from the same edits): C4100 −5, C4245 −1, C4244 −14, C4267 −4 → total first-party warnings 827 → **775**.

Build: clean, 0 errors, 0 C4018, no new categories. Verified via `build/x64-debug-c4018.log`.

### 2026-05-15 — pull live numbers, plan next pass

Re-grouped post-commit numbers from yesterday's full build (`build/x64-debug-precommit.log`, last commit `9c7a4ca6` on `dhingora/warning-fixes`). Errors: 0. First-party warnings: **827** (down from 1083 baseline). Live categories collapsed from 16 → **5**: C4100 (422), C4245 (173), C4244 (121), C4267 (83), C4018 (28). 71 % of remaining noise lives in 5 headers (`rocprofvis_db.h`, `rocprofvis_db_packed_storage.h`, `rocprofvis_db_compute.h`, `rocprofvis_dm_topology.h`, `rocprofvis_db_profile.h`). See §1, §2, §3 for tables. Next category to pick is the user's call.

### 2026-05-14 — C4189 (18/18 fixed) ✓ category complete

Completed the entire C4189 category in one batch (15 new fixes after the prior 4). All 14 outstanding C4189 sites verified individually (full-file grep + surrounding-context read) before edit. One **bonus** C4189 deleted at `db_table_processor.cpp:843` (`std::string column = azColName[column_index];`) — same loop body, same `azColName[column_index]` is used directly at lines 845 & 851; the `column` local was unread. Would have fired in the next build.

**Pure deletes** (variable provably never read, no side effects in the RHS):
- `src/controller/src/system/rocprofvis_controller_graph.cpp:389` — `double sample_last_value = 0.0;` — only declaration in entire file. Deleted.
- `src/controller/src/system/rocprofvis_controller_table_system.cpp:125` — `auto& column = m_columns[j];` — reference taken but body uses `m_columns[j].m_type` directly on the next line. Deleted.
- `src/model/src/database/rocprofvis_db_profile.cpp:259` — `uint32_t loaded_track_id = db->Sqlite3ColumnInt(func, stmt, azColName, kRpvDbTrackLoadTrackId);` in `CallBackLoadTrack`. Only one occurrence in the file. Deleted. *Parked observation:* the column `kRpvDbTrackLoadTrackId` is read from SQLite then **silently dropped**; the in-memory `track_params.track_indentifiers.track_id` is set to `db->NumTracks()` (line 258) instead. If the persisted track id matters for cross-session consistency, this is a latent bug. → §8.
- `src/model/src/database/rocprofvis_db_profile.cpp:641` — `void* func = (void*)&GetTrackIdentifierIndices;` in `GetTrackIdentifierIndices`. The `void* func` idiom is used in this file (lines 81, 168, 204) only as the first arg to `Sqlite3ColumnInt(func, …)` — but here `func` is never passed because this function does pure name-classification, not column reads. Deleted.
- `src/model/src/database/rocprofvis_db_rocprof.cpp:751` (was 751 pre-renumber) — `uint32_t num_columns = table->NumColumns();` in `PopulateStreamToHardwareFlowProperties`. Never read (loop indexes by row, not column). Deleted.
- `src/model/src/database/rocprofvis_db_rocprof.cpp:782` — `rocprofvis_dm_track_params_t* track_properties = TrackPropertiesAt(track);` — fetched but never read; the very next line uses `TrackPropertiesAt(stream_track_index)` (different track). Looks like a stale fragment from a prior implementation. Deleted.
- `src/model/src/database/rocprofvis_db_rocprof.cpp:960` — `size_t track_queries_hash_value = std::hash<std::string>{}(track_queries);` — `std::hash` is pure (no side effects); discarding the result is harmless. Deleted.
- `src/model/src/database/rocprofvis_db_table_processor.cpp:844` — `uint8_t size = 0;` in `CallbackRunCompoundQuery`. Never read. Deleted.
- `src/model/src/database/rocprofvis_db_table_processor.cpp:843` (**bonus**) — `std::string column = azColName[column_index];` in same loop. Never read. Deleted.
- `src/model/src/database/rocprofvis_db_table_processor.cpp:964` — `bool delim = false;` in `ExportToCSV`. Never read; CSV separator handling actually uses raw `","` writes elsewhere. Deleted.

**Delete + park observation** (deletion of the local exposes a likely latent bug — recorded in §8):
- `src/model/src/database/rocprofvis_db_profile.cpp:1622` — `Future* internal_future = new Future(nullptr);` in `ExportTableCSV`. Never inserted into any container, never freed → **straight memory leak**. Removing the line eliminates both the warning and the leak. *Parked observation:* the original author probably intended to use a sub-future but never wired it through; the export loop drives `m_table_processor[i].ExportToCSV(file_path)` synchronously. → §8.
- `src/model/src/database/rocprofvis_db_sqlite.cpp:671` — `char *zErrMsg = 0;` in `ExecuteSQLQuery`. Compare to line 216 of the same file where `zErrMsg` is correctly passed as the 4th arg to `Sqlite3Exec(...)` and freed with `sqlite3_free`. Here it's declared but the call at line 678 doesn't pass it. → SQLite error messages are silently dropped on this code path. Deleted local. → §8.
- `src/model/src/database/rocprofvis_db_table_processor.cpp:672` — `size_t leftover_rows_count = m_merged_table.RowCount() - (rows_per_task * thread_count);` in the second parallel-aggregation function. Compare to line 621 (first variant) where the same value is correctly used to dispatch the residual `n % thread_count` rows to a final thread. Here the leftover is **never dispatched** → up to `(thread_count - 1)` rows can be silently skipped during aggregation when `RowCount() % thread_count != 0`. → §8.
- `src/model/src/database/rocprofvis_db_version.cpp:227` — `roc_optiq_table_type type = (roc_optiq_table_type)std::atol(table->GetCellByIndex(i, "type"));` in `MetadataVersionControl::Load`. Read from the metadata cache but never written back into `data` (line 230's `data` ends up with `data.type` from a previous load, not this one). → on cache reload, the table-type field can drift from the persisted value. → §8.

**Special — keep the call, drop the variable** (RHS has side effects we must preserve):
- `src/model/src/database/rocprofvis_db_table_processor.cpp:424` — `double d = std::stod(r.first, &pos);` inside a `try { … } catch (…) { numeric = false; }`. The whole point is throw-on-non-numeric; the value `d` is never used, only the throw. Replaced with `(void)std::stod(r.first, &pos);` to keep the throw semantics while killing C4189. (Cast-to-void is the canonical "intentionally discarded" idiom; MSVC accepts it.)

**Special — redundant re-fetch** (variable shadows itself with the same expression — these are also C4456 sites; counted once in C4456 below):
- `src/model/src/database/rocprofvis_db_profile.cpp:1454` and `:1457` — both inner `rocprofvis_dm_track_params_t* props = TrackPropertiesAt(*tracks);` re-fetch the *same value* the outer `props` (line 1438) already has. Both inner declarations deleted entirely — outer `props` is in scope and identical. *Side observation:* after this fix, the doubly-nested `if (props->track_indentifiers.category == kRocProfVisDmPmcTrack)` (lines 1455 & 1458) tests the same condition twice in a row → dead `if` nesting. Left as-is to minimize disturbance; flag in §8.

### 2026-05-14 — C4456 (26/26 fixed) ✓ category complete

Completed the entire C4456 category in one batch (22 new sites after the prior 4). All renames follow the project convention: `<descriptor>_<role>` (e.g. `record_index`, `ext_data_index`, `param_index`, `cmd_it`, `level_it`, `track_db_instance`, `available_conn`, `agg_size`, `agg_value`, `agg_str`, `agg_numeric_string`, `processor_index`, `queue_index`, `cv_lock`, `kernel_id`, `ext_data_category`, `params_db_instance`).

For every site I read both the inner *and* the surrounding outer scope to confirm: (a) the inner block does not reference the outer name after the shadow, (b) lambda captures (if any) are renamed to match, (c) any `it = …;` rebinds inside the same block are also renamed.

**Standard inner-loop / inner-block renames** (no surprises):
- `src/controller/src/compute/rocprofvis_controller_trace_compute.cpp:515` — range-for `for(const uint32_t& id : kernel_ids)` shadowed outer `uint64_t id = 0;` (line 398). Renamed inner to **`kernel_id`**. **Caught a footgun:** the lambda at line 530 captures `&id` and the lambda body at line 551 uses `id` — so renaming required updating both the capture list (`&kernel_id`) and the body (`uint_data, kernel_id`). If we'd only renamed the loop variable, the capture would silently bind to the *outer* `id` (the workload id) and the roofline kernel-id column would get the wrong value. *This was the only non-trivial C4456 fix and the strongest argument for cleaning these up.*
- `src/controller/src/system/rocprofvis_controller_mem_mgmt.cpp:207` — `std::unique_lock lock(m_lru_cond_mutex);` shadowed outer `lock` (line 201) on `m_lru_inuse_mutex[type]`. Renamed inner to **`cv_lock`** (it guards the cond-var protocol).
- `src/controller/src/system/rocprofvis_controller_trace_system.cpp:285` — inner `std::string category;` (the ext-data category in the per-track loop) shadowed the outer `std::string category = …;` at line 192 (the track's own category). Renamed inner to **`ext_data_category`** (7 references updated: `.resize`, `GetString` out-arg, four `==` comparisons, debug-log `.c_str()`).
- `src/model/src/database/rocprofvis_db_packed_storage.cpp:445`, `:450`, `:451` — `size`, `numeric_string`, `str` all shadowed function-scope outers (lines 409, 421, 422). Same code is for the per-aggregation-param row, not the group-by column. Renamed inner trio to **`agg_size`**, **`agg_numeric_string`**, **`agg_str`**. Line 448 (`value = …;`) intentionally **kept** because it correctly assigns the outer `value` (the group-by row's value).
- `src/model/src/database/rocprofvis_db_packed_storage.cpp:515`, `:516` — same shape, different function. Inner `size` and `value` shadowed function-scope outers. Renamed to **`agg_size`** and **`agg_value`** (4 switch-case body references updated for `agg_value`).
- `src/model/src/database/rocprofvis_db_profile.cpp:751` — inner `for(int i …)` shadowed outer `for(int i …)` (line 740). Outer iterates UNION clauses, inner iterates track-id parameters. Renamed inner to **`param_index`** (5 references updated inside inner block).
- `src/model/src/database/rocprofvis_db_profile.cpp:1454` and `:1457` — see C4189 entry above. **Deletes**, not renames.
- `src/model/src/database/rocprofvis_db_profile.cpp:1696` — inner `auto it = …->find(id);` (per-event level lookup) shadowed outer `auto it = params->m_active_events.begin();` (line 1667). Renamed inner to **`level_it`** (2 references updated).
- `src/model/src/database/rocprofvis_db_profile.cpp:2032` — inner `DbInstance* db_instance = (DbInstance*)TrackPropertiesAt(i)->track_indentifiers.db_instance;` shadowed outer `TemporaryDbInstance db_instance(file_node->node_id);` (line 2021). Renamed inner to **`track_db_instance`** (2 references updated).
- `src/model/src/database/rocprofvis_db_profile.cpp:2043` (lambda body) — inner `DbInstance* db_instance = (DbInstance*)params->track_indentifiers.db_instance;` shadowed the same outer `db_instance`. Renamed inner to **`params_db_instance`** (1 reference updated in `db_inst_start_time[…]` lookup).
- `src/model/src/database/rocprofvis_db_profile.cpp:2107` — same shape as 2032 in the histogram-collect loop. Renamed inner to **`track_db_instance`** (1 reference updated).
- `src/model/src/database/rocprofvis_db_sqlite.cpp:335` — inner `sqlite3* conn = *it;` (the available connection picked off the LRU end) shadowed outer `sqlite3* conn;` declared at function scope (line 326) used by the open-a-new-conn branch. Renamed inner to **`available_conn`** (2 references: assignment-from-iter and return statement).
- `src/model/src/database/rocprofvis_db_table_processor.cpp:699` and the rebind at `:712` — inner `auto it = std::find_if(commands.begin(), …);` (find SORT command) shadowed an outer iterator. Renamed both the declaration *and* the rebind to **`cmd_it`** (4 references updated total: `it->parameter`, two `if (it != commands.end())` checks).
- `src/model/src/database/rocprofvis_db_table_processor.cpp:748` and the rebind at `:761` — same shape, second branch. Renamed to **`cmd_it`** (4 references updated).
- `src/view/src/rocprofvis_track_topology.cpp:387` — inner `for(int j = 0; j < stream_processors.size(); j++)` shadowed outer `for(int j …)` iterating over processes (around line 290+). Renamed inner to **`processor_index`** (≈12 references updated inside inner block; outer `j` use at line 461 left untouched).
- `src/view/src/rocprofvis_track_topology.cpp:409` — inner `for(int k = 0; k < queue_ids.size(); k++)` shadowed outer `for(int k …)` iterating streams. Renamed inner to **`queue_index`** (≈6 references updated).

### 2026-05-14 — C4189 (4/18 fixed)
- `src/controller/src/system/rocprofvis_controller_event.cpp:232` — `char* args = nullptr;` was declared but never assigned/read anywhere in the file (verified by grep). Pure dead local. **Deleted outright** (no comment-out — no behavioural risk).
- `src/model/src/database/rocprofvis_db.cpp:189` — `rocprofvis_dm_result_t result = kRocProfVisDmResultUnknownError;` declared but never returned: function unconditionally returns `kRocProfVisDmResultSuccess` on success path and `kRocProfVisDmResultUnknownError` from the catch's `ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN`. **Deleted outright**. *Parked observation:* the success path returns `kRocProfVisDmResultSuccess` immediately after launching the worker thread without confirming the thread actually started successfully — see §8.
- `src/model/src/database/rocprofvis_db_rocprof.cpp:699` — `rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;` in `LoadInformationTables`. Function returns `RunCacheQueriesAsync(...)` directly; `result` never read. Looks like a copy-paste from the next function `LoadMemoryActivityData` (line 717) where the same initializer is correctly used. **Deleted outright**.
- `src/model/src/datamodel/rocprofvis_dm_topology.cpp:78` — `rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;` in `TopologyNodeRoot::AddProperty`. Function unconditionally returns `kRocProfVisDmResultSuccess`; `result` never reassigned, never returned. **Deleted outright**.

### 2026-05-14 — C4456 (4/26 fixed)
All four sites are the canonical inner-`for(int i …)` shadowing an outer `for(int i …)`. In every case the inner loop body does not reference the outer `i`, so the rename is safe. Verified by reading the whole inner-loop block plus 5+ lines after each block to confirm no stale outer-`i` access leaked into shadow zone.
- `src/controller/src/system/rocprofvis_controller_track.cpp:322` — inner indexes records within a slice (`kRPVDMTimestampUInt64Indexed`, `kRPVDMEventIdUInt64Indexed`, etc.). Outer `i` (line 295) indexes futures. Renamed inner to **`record_index`** (5 references updated).
- `src/controller/src/system/rocprofvis_controller_track.cpp:399` — same pattern, inner indexes PMC samples. Renamed inner to **`record_index`** (5 references updated).
- `src/model/src/test/main.cpp:265` — inner indexes `track_ext_data` (`kRPVDMTrackExtDataCategoryCharPtrIndexed` etc.). Outer `i` (line 249) indexes `tracks_selection`. **Carefully verified** that line 275 (`tracks_selection[i]`) sits *after* the inner block closes — uses outer `i` correctly. Renamed inner to **`ext_data_index`** (3 references updated).
- `src/model/src/tests/rocprofvis_dm_system_tests.cpp:636` — same as `main.cpp:265`. Outer `i` at line 599. Confirmed `m_tracks_selection[i]` at line 657 lives outside the inner block. Renamed inner to **`ext_data_index`** (3 references updated).

### 2026-05-14 — C4065 (5/5 fixed) ✓ category complete
- `src/controller/src/system/rocprofvis_controller_event.cpp:466` (`Event::Fetch`) — flattened `switch(property) { default: { result = kRocProfVisResultInvalidEnum; break; } }` into a plain assignment. Added `(void) property;` to silence the resulting C4100 (parameter no longer referenced) and re-indented.
- `src/controller/src/system/rocprofvis_controller_sample.cpp:204` (`Sample::GetString`) — flattened to `return UnhandledProperty(property);`. Now-dead `result = kRocProfVisResultInvalidArgument;` line commented out (pending delete — see §0a).
- `src/controller/src/system/rocprofvis_controller_sample.cpp:221` (`Sample::SetUInt64`) — same treatment; dead `result =` line commented out (pending delete).
- `src/controller/src/system/rocprofvis_controller_sample.cpp:268` (`Sample::SetObject`) — flattened inside the existing `if(value)` guard; `result =` kept because it's still returned when `value == nullptr`.
- `src/controller/src/system/rocprofvis_controller_sample.cpp:288` (`Sample::SetString`) — same as `SetObject`.

### 2026-05-14 — C4457 (3/3 fixed) ✓ category complete
- `src/model/src/datamodel/rocprofvis_dm_table.cpp:155` — inner loop local `handle` (type `rocprofvis_dm_table_row_t`) shadowed the constructor parameter `handle` (type `rocprofvis_dm_table_t`). Renamed inner to `row_handle`. Set the project rename convention: `<descriptor>_<role>`.
- `src/model/src/datamodel/rocprofvis_dm_trace.cpp:662` — inner local `object` (`rocprofvis_dm_slice_t`) shadowed function parameter `object` (`rocprofvis_dm_trace_t`) — same name, completely different domain types. Renamed inner to `slice_object` (4 references swapped: declaration, `GetSliceAtTime` out-arg, ASSERT, cast). Outer parameter and `(Trace*) object` cast at line 652 untouched.
- `src/controller/src/system/rocprofvis_controller_track.cpp:824` — inner local `value` (`double`, sample measurement) shadowed function parameter `value` (`rocprofvis_handle_t*`, the object handle). Renamed inner to `sample_value`. Note: `sample_value` is fetched via `object->GetDouble(kRPVControllerSampleValue, …)` but never used afterwards — see §8 (this is now a latent C4189 to be picked up in the dead-locals sweep).

### 2026-05-14 — C4505 (2/2 fixed) ✓ category complete
- `src/controller/src/system/rocprofvis_controller_segment.cpp:69` (`AddSamples`) and `:92` (`AddEvents`) — both static, both with zero callers anywhere in `src/`. Commented out line-by-line. Likely leftover LOD-aware child-expansion helpers from an unfinished/abandoned feature. Style note: prefer deleting dead code over commenting it out — see §0a pre-commit checklist.

### 2026-05-14 — C4389 (2/2 fixed) ✓ category complete
- `src/model/src/test/main.cpp:202` and `src/model/src/tests/rocprofvis_dm_system_tests.cpp:509` — same shape: `tracks_selection[j] == i` where `i` is `int` and `tracks_selection` is `uint32_t*` (typedef `rocprofvis_db_track_selection_t`). Wrapped `i` in `static_cast<uint32_t>(i)` at both sites. Minimum-disturbance fix; the surrounding `int i < uint32_t m_num_tracks` loop comparison stays as a C4018 for the next sweep.

### 2026-05-14 — C4458 (1/1 fixed) ✓ category complete
- `src/model/src/database/rocprofvis_db_cache.h:55` — dead `std::string name;` member of `TableCache` (never read or written anywhere) was shadowing the `AddColumn(const std::string& name, …)` parameter. Member commented out, parameter no longer shadows anything. Style note: prefer deleting dead code over commenting it out (git history preserves it).

### 2026-05-14 — C4101 (1/1 fixed) ✓ category complete
- `src/model/src/database/rocprofvis_db.cpp:28` — caught `std::exception& ex` was never referenced. Dropped the variable name (`catch (const std::exception&)`). Note: chose suppression over `spdlog::error("…{}", ex.what())`; `ex.what()` is still being silently discarded — see §8.

### 2026-05-14 — C4996 (2/15 fixed)
- `src/model/src/common/rocprofvis_c_interface.cpp:325` (`rocprofvis_db_build_table_query`) — replaced `strncpy(ptr, query.c_str(), query.length())` with `std::memcpy(ptr, query.c_str(), query.length()); ptr[query.length()] = '\0';`. Removes the deprecation and the implicit dependency on `calloc`'s zero-init for the null terminator.
- `src/model/src/common/rocprofvis_c_interface.cpp:350` (`rocprofvis_db_build_compute_query`) — same fix; twin function.

### 2026-05-13 — C4702 (4/4 fixed) ✓ category complete; +2 bonus C4189
- `src/model/src/database/rocprofvis_db_profile.cpp:1001` (`BuildCounterSliceLeftNeighbourQuery`) — replaced `for (auto it_query = …; …; ++it_query) { …; break; }` with `if (!slice_query_map.empty()) { auto it_query = slice_query_map.begin(); … }`. The `for` was a fake loop (unconditional `break` made the `++it_query` dead). Reads honestly now.
- `src/model/src/database/rocprofvis_db_profile.cpp:1029` (`BuildCounterSliceRightNeighbourQuery`) — same pattern, same fix; left/right are near-duplicate functions.
- **Bonus C4189 fixes (2):** `bool timed_query = false;` was unused in both `BuildCounterSliceLeftNeighbourQuery` and `BuildCounterSliceRightNeighbourQuery`; deleted while in the same edit.
- `src/model/src/datamodel/rocprofvis_dm_topology.cpp:192` — dead `ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN` after an unconditional `return GetPropertyType(...);`. Deleted; `GetPropertyType` already returns `kRocProfVisDmResultInvalidParameter` for unknown variant indices, so the defensive assert was redundant.
- `src/model/src/database/rocprofvis_db_sqlite.cpp:55` — `return it->second;` was placed *before* the `spdlog::debug(...)` log line, making the log unreachable. Swapped the two lines so the log fires before the return. Restores symmetry with the sibling `GetNullExceptionString()` function below.

### 2026-05-13 — C4701 (1/1 fixed) ✓ category complete
- `src/model/src/database/rocprofvis_db_rocprof.cpp:195` — value-initialized `rocprofvis_db_memalloc_activity_t memact = {};` to remove UB. Added `type_found`/`level_found` flags around the two enum-lookup loops; on lookup miss, log via `spdlog::error` and `return 0` to skip the row instead of pushing a struct with uninitialized `.type`/`.level` into `m_memalloc_activity`.

---

## 1. Build summary

| Metric | Baseline (pre-cleanup) | Fixed since baseline | Remaining (live, **clean build**) |
|---|---:|---:|---:|
| Errors | 0 | — | **0** |
| First-party warnings | **1083** | 887 | **196** |
| Thirdparty warnings | 53 | — | ~56 _(jsoncpp/yaml-cpp; not first-party scope)_ |
| Distinct warning codes (first-party) | 16 | 14 | **2** |

_Latest reference build: 2026-05-15 → [`build/x64-debug-c4100-build.log`](build/x64-debug-c4100-build.log) (**full clean rebuild** after C4100 sweep, all uncommitted on top of `5ab57678`). Last pushed commit: `5ab57678` on `dhingora/warning-fixes`._

> **Calibration note (2026-05-15):** prior to the original clean rebuild we had been measuring against incremental builds, which under-counted by ~29 first-party warnings (mostly C4244 in view/widgets/compute TUs that no header sweep had touched). Numbers above are now the true post-sweep state. See progress-log entries "Clean-build calibration" and "C4100 sweep" for details.

Build succeeded; nothing here blocks compilation. **Only 2 categories remain live**: C4244 and C4996 (with 9 of 13 C4996 sites deliberately deferred per §4.3) — listed in §2 below in priority order.

---

## 2. Compile warnings — by code (first-party only)

### 2a. Live categories (post C4100 sweep, 2026-05-15 clean build)

| Remaining | Code | Description | Severity | Notes for next pass |
|---:|---|---|---|---|
| **187** | C4244 | conversion, possible loss of data (e.g. `int64_t` → `int`/`float`) | **medium** — silent truncation | Spread across packed-storage / rocprof / controller `.cpp` files; per-site review needed (some are intentional truncations of timestamps/durations to display widths, some are real narrowing bugs). Now the largest live category. |
| **9** | C4996 | deprecated/unsafe CRT (`strncpy`, `getenv`) — deferred subset | **medium** (security-flagged) | See §4.3 — 4× `strncpy` (one of which, `rocprofvis_controller_data.cpp:282`, hides a real null-termination contract bug needing maintainer review) + 5× `getenv` (need a small portable `_dupenv_s` wrapper). The 4 trivial `vsprintf`/`getch` sites were cleared 2026-05-15. |

**Total: 196 first-party** (down from 1083 baseline; cleanup has cleared 887 warnings and **14 categories outright**; the live category set is now exactly **C4244, C4996** — every shadowing/dead-local/signed-unsigned/size_t-narrowing/unreferenced-parameter category is at zero).

### 2b. Categories already cleared ✓

| Was | Code | Description | Cleared in | Live? |
|---:|---|---|---|---|
| ~~5~~ | C4065 | switch with `default` only, no `case` | `9c7a4ca6` | 0 ✓ |
| ~~4~~ | C4702 | unreachable code | `9c7a4ca6` | 0 ✓ |
| ~~3~~ | C4457 | declaration hides function parameter | `9c7a4ca6` | 0 ✓ |
| ~~2~~ | C4505 | unreferenced function with internal linkage | `9c7a4ca6` | 0 ✓ |
| ~~2~~ | C4389 | `==`/`!=` signed/unsigned mismatch | `9c7a4ca6` | 0 ✓ |
| ~~1~~ | C4458 | declaration hides class member | `9c7a4ca6` | 0 ✓ |
| ~~1~~ | C4101 | unreferenced local variable | `9c7a4ca6` | 0 ✓ |
| ~~1~~ | C4701 | potentially uninitialized local used (was the only **HIGH** severity item) | `9c7a4ca6` | 0 ✓ |
| ~~26~~+~~3~~+~~2~~ | C4456 | declaration of local hides previous local (the `for (T i ...) { T i; }` pattern) | `9c7a4ca6` + `2026-05-15` (uncommitted; test-file + view/test mop-up) | **0 ✓** |
| ~~28~~+~~3~~+~~1~~ | C4018 | signed/unsigned mismatch in comparison | `2026-05-15` (uncommitted; model-side + test-file + widgets mop-up) | **0 ✓** |
| ~~79~~+~~35~~ | C4267 | conversion from `size_t` to smaller type | `2026-05-15` (uncommitted; original model-side sweep + view/widgets/compute mop-up) | **0 ✓** |
| ~~172~~+~~5~~ | C4245 | signed/unsigned conversion in initialization/return | `2026-05-15` (uncommitted; model-side sweep + view/test/controller mop-up) | **0 ✓** |
| ~~18~~+~~4~~ | C4189 | local variable initialized but not referenced | `9c7a4ca6` + `2026-05-15` (uncommitted; view/test mop-up) | **0 ✓** |
| ~~481~~ | C4100 | unreferenced formal parameter | `2026-05-15` (uncommitted; scripted `Type name` → `Type /*name*/` across 30 files via `build/fix_c4100.ps1`) | **0 ✓** |
| ~~2~~+~~4~~ | C4996 | deprecated CRT function — partial cleanup of trivial sites only | `9c7a4ca6` + `2026-05-15` (uncommitted; `getch` + 3× `vsprintf`) | **9 remaining** — 4× `strncpy` + 5× `getenv`, deferred per §4.3 |

---

## 3. Compile warnings — by file (first-party hotspots, live clean-build, ≥5 warnings)

| Count | File |
|---:|---|
| 20 | `src/model/src/database/rocprofvis_db_packed_storage.cpp` |
| 19 | `src/model/src/database/rocprofvis_db_rocprof.cpp` |
| 15 | `src/model/src/database/rocprofvis_db_rocprof.h` |
| 14 | `src/controller/src/compute/rocprofvis_controller_trace_compute.cpp` |
| 13 | `src/controller/src/system/rocprofvis_controller_track.cpp` |
| 12 | `src/controller/src/system/rocprofvis_controller_table_system.cpp` |
| 12 | `src/model/src/database/rocprofvis_db_compute.cpp` |
| 10 | `src/model/src/datamodel/rocprofvis_dm_trace.cpp` |
| 8 | `src/model/src/database/rocprofvis_db_profile.cpp` |
| 7 | `src/view/src/compute/rocprofvis_compute_summary.cpp` |
| 7 | `src/model/src/database/rocprofvis_db_table_processor.cpp` |
| 5 | `src/model/src/database/rocprofvis_db_expression_filter.cpp` |
| 5 | `src/view/src/rocprofvis_utils.cpp` |

**Observation:** the C4100 sweep cleared the four model headers entirely (`rocprofvis_db.h`, `rocprofvis_db_compute.h`, `rocprofvis_db_profile.h`, `rocprofvis_dm_topology.h` all dropped from 161/86/70/63 → 0). Remaining noise is **187 C4244 narrowing-conversion warnings** spread across `.cpp` files (and one `rocprofvis_db_rocprof.h`), plus 9 deferred C4996 sites. Top 13 files account for 147 / 196 warnings (75 %).

### 3a. Per-category × per-file breakdown (live, clean build)

**C4244 (187 lossy conversion):**
- `rocprofvis_db_packed_storage.cpp`: 20 • `rocprofvis_db_rocprof.cpp`: 19 • `rocprofvis_db_rocprof.h`: 15 • `rocprofvis_controller_trace_compute.cpp`: 14 • `rocprofvis_controller_track.cpp`: 13 • `rocprofvis_db_compute.cpp`: 12 • `rocprofvis_controller_table_system.cpp`: 12 • `rocprofvis_dm_trace.cpp`: 10 • `rocprofvis_db_profile.cpp`: 8 • `rocprofvis_db_table_processor.cpp`: 7 • `rocprofvis_compute_summary.cpp`: 7 • `rocprofvis_db_expression_filter.cpp`: 5 • others: ≤4 each.

**C4996 (9 deferred deprecated CRT):**
- `rocprofvis_utils.cpp`: 5 (`getenv` — needs `_dupenv_s` wrapper) • `rocprofvis_controller_table_compute_pivot.cpp`: 2 (`strncpy`) • `rocprofvis_controller_data.cpp`: 1 (`strncpy` — **latent null-termination bug**, see §4.3) • `rocprofvis_controller_handle.cpp`: 1 (`strncpy`).

The full per-file listing is at the bottom of this document (Appendix A) but is now stale (incremental-build numbers); regenerate from `build/x64-debug-c4100-build.log` if needed.

---

## 4. High-priority compile warnings (full detail)

These are the warnings most worth fixing first because each one is either a likely bug or a security/safety smell.

### 4.1 C4701 — potentially uninitialized variable (1) — **HIGH** — ✓ **COMPLETE (1/1)**

| Status | File | Line | Detail |
|:---:|---|---:|---|
| ✓ | `src/model/src/database/rocprofvis_db_rocprof.cpp` | 225 | potentially uninitialized local variable `memact` used — fixed 2026-05-13 (value-initialization + lookup-miss skip; see §0) |

> Real risk of UB. Initialize `memact` at declaration or guarantee assignment on every path before use.

### 4.2 C4702 — unreachable code (4) — **medium** — ✓ **COMPLETE (4/4)**

| Status | File | Line |
|:---:|---|---:|
| ✓ | `src/model/src/database/rocprofvis_db_profile.cpp` | 1001 — fixed 2026-05-13 (`for (…) { …; break; }` → `if (!empty()) { auto it = begin(); … }`) |
| ✓ | `src/model/src/database/rocprofvis_db_profile.cpp` | 1029 — fixed 2026-05-13 (same pattern; right-neighbour twin) |
| ✓ | `src/model/src/datamodel/rocprofvis_dm_topology.cpp` | 192 — fixed 2026-05-13 (deleted redundant defensive assert after unconditional return) |
| ✓ | `src/model/src/database/rocprofvis_db_sqlite.cpp` | 55 — fixed 2026-05-13 (swap log/return) |

> Either delete the dead branch or fix the control flow that made it unreachable.

### 4.3 C4996 — deprecated/unsafe CRT (15) — **medium (security-flagged)** — 6/15 fixed; remaining 9 deferred (need API decisions or a portable wrapper)

> **Maintainer-decision needed:** `rocprofvis_controller_data.cpp:282` is *not* a simple deprecation cleanup — the `strncpy` there is hiding a real null-termination contract bug (the size-query branch returns `strlen(m_string)`, but the copy branch then fills the entire returned buffer with no room for `'\0'`). That site needs the API contract decided first (capacity-includes-null vs not) before any fix.

> **Portable wrapper needed:** the 5× `getenv` sites in `rocprofvis_utils.cpp` should be replaced via a single shared helper (e.g. `std::optional<std::string> GetEnv(const char*)` that uses `_dupenv_s` on MSVC and `std::getenv` elsewhere). Per-site `_dupenv_s` rewrites would scatter ownership-transfer code unnecessarily.

`strncpy` (does not always null-terminate), `vsprintf` (no length check), `getenv` (no thread-safety on Windows in older CRT), `getch` (POSIX name deprecated by MS).

| Status | File | Lines | Function | Fix |
|:---:|---|---|---|---|
| ✓✓ | `src/model/src/common/rocprofvis_c_interface.cpp` | ~~325~~, ~~350~~ | `strncpy` | `9c7a4ca6` (`memcpy` + explicit `'\0'`) |
| · | `src/controller/src/rocprofvis_controller_data.cpp` | 282 | `strncpy` | **deferred** — latent contract bug, needs maintainer review |
| · | `src/controller/src/rocprofvis_controller_handle.cpp` | 28 | `strncpy` | deferred — bundle with `controller_data.cpp` decision |
| · | `src/controller/src/compute/rocprofvis_controller_table_compute_pivot.cpp` | 368, 384 | `strncpy` | deferred — bundle with `controller_data.cpp` decision |
| ✓ | `src/model/src/test/main.cpp` | 33, ~~141~~ | `vsprintf`→`vsnprintf`, `getch`→`_getch` | 2026-05-15 (uncommitted) |
| ✓ | `src/model/src/tests/rocprofvis_dm_compute_tests.cpp` | ~~77~~ | `vsprintf`→`vsnprintf` | 2026-05-15 (uncommitted) |
| ✓ | `src/model/src/tests/rocprofvis_dm_system_tests.cpp` | ~~89~~ | `vsprintf`→`vsnprintf` | 2026-05-15 (uncommitted) |
| · | `src/view/src/rocprofvis_utils.cpp` | 344, 441, 467, 468, 469 | `getenv` | **deferred** — needs portable `GetEnv` wrapper, not 5× per-site `_dupenv_s` |

> Recommendation: prefer `std::string`/`std::vector<char>` and bounded copies (`strncpy_s` on MSVC; manual length-checked copy for portability). For env vars, write a small portable wrapper that uses `_dupenv_s` on MSVC and `std::getenv` elsewhere.

### 4.4 C4456/C4457/C4458 — variable shadowing (32) — ✓ **C4456 + C4457 + C4458 ALL COMPLETE**

You called this out specifically (`for (int i …) { int i …; }`). MSVC found these:

**C4456 — local hides previous local** (31) — ✓ **COMPLETE (31/31)**:

| Status | File | Line | Hidden symbol | Fix |
|:---:|---|---:|---|---|
| ✓ | `src/model/src/database/rocprofvis_db_packed_storage.cpp` | 445, 515 | `size` | `9c7a4ca6` |
| ✓ | `src/model/src/database/rocprofvis_db_packed_storage.cpp` | 450 | `numeric_string` | `9c7a4ca6` |
| ✓ | `src/model/src/database/rocprofvis_db_packed_storage.cpp` | 451 | `str` | `9c7a4ca6` |
| ✓ | `src/model/src/database/rocprofvis_db_packed_storage.cpp` | 516 | `value` | `9c7a4ca6` |
| ✓ | `src/model/src/database/rocprofvis_db_profile.cpp` | 751 | `i` | `9c7a4ca6` |
| ✓ | `src/model/src/database/rocprofvis_db_profile.cpp` | 1456, 1459 | `props` | `9c7a4ca6` |
| ✓ | `src/model/src/database/rocprofvis_db_profile.cpp` | 1705 | `it` | `9c7a4ca6` |
| ✓ | `src/model/src/database/rocprofvis_db_profile.cpp` | 2041, 2116 | `db_instance` | `9c7a4ca6` |
| ✓ | `src/model/src/database/rocprofvis_db_sqlite.cpp` | 335 | `conn` | `9c7a4ca6` |
| ✓ | `src/model/src/database/rocprofvis_db_table_processor.cpp` | 699, 748 | `it` | `9c7a4ca6` |
| ✓ | `src/model/src/test/main.cpp` | 265 | `i` | `9c7a4ca6` |
| ✓ | `src/model/src/test/main.cpp` | 291 | `id` → `record_id` | `2026-05-15` (uncommitted) |
| ✓ | `src/model/src/test/main.cpp` | 317 | `event_id` → `endpoint_event_id` | `2026-05-15` (uncommitted) |
| ✓ | `src/model/src/test/main.cpp` | 351 | `num_records` → `num_extdata_records` | `2026-05-15` (uncommitted) |
| ✓ | `src/model/src/tests/rocprofvis_dm_system_tests.cpp` | 636 | `i` | `9c7a4ca6` |
| ✓ | `src/controller/src/system/rocprofvis_controller_trace_system.cpp` | 285 | `category` | `9c7a4ca6` |
| ✓ | `src/controller/src/system/rocprofvis_controller_track.cpp` | 322, 399 | `i` | `9c7a4ca6` |
| ✓ | `src/controller/src/system/rocprofvis_controller_mem_mgmt.cpp` | 207 | `lock` | `9c7a4ca6` |
| ✓ | `src/controller/src/compute/rocprofvis_controller_trace_compute.cpp` | 515 | `id` | `9c7a4ca6` |
| ✓ | `src/controller/tests/rocprofvis_controller_system_tests.cpp` | 1162 | `result` (dropped inner type → assigns to outer) | `2026-05-15` (uncommitted) |
| ✓ | `src/view/src/rocprofvis_track_topology.cpp` | 387 | `j` | `9c7a4ca6` |
| ✓ | `src/view/src/rocprofvis_track_topology.cpp` | 409 | `k` | `9c7a4ca6` |
| ✓ | `src/view/src/rocprofvis_project.cpp` | 186 | `result` → `controller_result` | `2026-05-15` (uncommitted) |

**C4457 — local hides function parameter** (3) — ✓ **COMPLETE (3/3)**:

| Status | File | Line | Hidden parameter | Renamed to |
|:---:|---|---:|---|---|
| ✓ | `src/model/src/datamodel/rocprofvis_dm_table.cpp` | 155 | `handle` | `row_handle` |
| ✓ | `src/model/src/datamodel/rocprofvis_dm_trace.cpp` | 662 | `object` | `slice_object` |
| ✓ | `src/controller/src/system/rocprofvis_controller_track.cpp` | 824 | `value` | `sample_value` |

**C4458 — local hides class member** (1):

| File | Line | Hidden member |
|---|---:|---|
| `src/model/src/database/rocprofvis_db_cache.cpp` | 13 | `name` |

> The `i`/`j`/`k`/`it` ones are exactly the loop-counter shadowing pattern you asked about — fix them mechanically by renaming the inner one.

### 4.5 C4189 / C4101 — unused / dead locals (25) — ✓ **C4189 + C4101 ALL CLEARED IN BUILD (per-row table partially backfilled)**

> The current clean build emits **0 C4189 and 0 C4101**, so the category is genuinely complete. The per-row table below is partially stale (many `·` rows from the original audit were swept in bulk by `9c7a4ca6`'s C4189 cleanup pass without each row being individually re-marked). The 4 newly-mopped 2026-05-15 sites at the bottom are the only ones in this table fixed today.

These variables are written but never read. Either remove them, or — if they're held intentionally for RAII / debugging — make that explicit with `[[maybe_unused]]` or `static_cast<void>(x)`.

| Status | File | Line | Variable |
|:---:|---|---:|---|
| · | `src/model/src/datamodel/rocprofvis_dm_topology.cpp` | 78 | `result` |
| ✓ | `src/model/src/database/rocprofvis_db.cpp` | 28 | `ex` (C4101 — unreferenced caught exception) — fixed 2026-05-14 (drop name) |
| · | `src/model/src/database/rocprofvis_db.cpp` | 189 | `result` |
| · | `src/model/src/database/rocprofvis_db_profile.cpp` | 259 | `loaded_track_id` |
| · | `src/model/src/database/rocprofvis_db_profile.cpp` | 641 | `func` |
| ✓ | `src/model/src/database/rocprofvis_db_profile.cpp` | 992 | `timed_query` — fixed 2026-05-13 (deleted, alongside C4702 fix) |
| ✓ | `src/model/src/database/rocprofvis_db_profile.cpp` | 1020 | `timed_query` — fixed 2026-05-13 (deleted, alongside C4702 fix) |
| `src/model/src/database/rocprofvis_db_profile.cpp` | 1624 | `internal_future` |
| `src/model/src/database/rocprofvis_db_rocprof.cpp` | 690 | `result` |
| `src/model/src/database/rocprofvis_db_rocprof.cpp` | 743 | `num_columns` |
| `src/model/src/database/rocprofvis_db_rocprof.cpp` | 774 | `track_properties` |
| `src/model/src/database/rocprofvis_db_rocprof.cpp` | 952 | `track_queries_hash_value` |
| `src/model/src/database/rocprofvis_db_sqlite.cpp` | 671 | `zErrMsg` |
| `src/model/src/database/rocprofvis_db_table_processor.cpp` | 424 | `d` |
| `src/model/src/database/rocprofvis_db_table_processor.cpp` | 672 | `leftover_rows_count` |
| `src/model/src/database/rocprofvis_db_table_processor.cpp` | 844 | `size` |
| `src/model/src/database/rocprofvis_db_table_processor.cpp` | 964 | `delim` |
| `src/model/src/database/rocprofvis_db_version.cpp` | 227 | `type` |
| `src/controller/src/system/rocprofvis_controller_event.cpp` | 232 | `args` |
| `src/controller/src/system/rocprofvis_controller_graph.cpp` | 389 | `sample_last_value` |
| `src/controller/src/system/rocprofvis_controller_table_system.cpp` | 125 | `column` |
| ✓ | `src/controller/tests/rocprofvis_controller_system_tests.cpp` | 1206 | `total` — fixed 2026-05-15 (uncommitted; deleted) |
| ✓ | `src/view/src/rocprofvis_summary_view.cpp` | 895 | `text_width` — fixed 2026-05-15 (uncommitted; deleted, RHS pure) |
| ✓ | `src/view/src/rocprofvis_time_to_pixel.cpp` | 71 | `zoom_before_clamp` — fixed 2026-05-15 (uncommitted; deleted; intent flagged in §8) |
| ✓ | `src/view/src/rocprofvis_minimap.cpp` | 124 | `height` — fixed 2026-05-15 (uncommitted; deleted) |

> Several of these (`timed_query`, `internal_future`, `loaded_track_id`) look like *intent was lost* — someone meant to use the value but the using code was deleted. Worth eyeballing rather than just deleting.

### 4.6 C4505 — unreferenced static function (2) — dead code — ✓ **COMPLETE (2/2)**

| Status | File | Line | Function |
|:---:|---|---:|---|
| ✓ | `src/controller/src/system/rocprofvis_controller_segment.cpp` | 69 | `RocProfVis::Controller::AddSamples` — fixed 2026-05-14 (commented out; pending delete — see §0a) |
| ✓ | `src/controller/src/system/rocprofvis_controller_segment.cpp` | 92 | `RocProfVis::Controller::AddEvents` — fixed 2026-05-14 (commented out; pending delete — see §0a) |

> Either wire them up or delete them — they currently bloat the source and the function names suggest features that aren't actually being called.

### 4.7 C4389 — signed/unsigned `==`/`!=` (2) — **medium** — ✓ **COMPLETE (2/2)**

| Status | File | Line |
|:---:|---|---:|
| ✓ | `src/model/src/test/main.cpp` | 202 — fixed 2026-05-14 (`static_cast<uint32_t>(i)`) |
| ✓ | `src/model/src/tests/rocprofvis_dm_system_tests.cpp` | 509 — fixed 2026-05-14 (same) |

### 4.8 C4065 — `switch` with only `default` (5)

| File | Lines |
|---|---|
| `src/controller/src/system/rocprofvis_controller_event.cpp` | 473 |
| `src/controller/src/system/rocprofvis_controller_sample.cpp` | 204, 221, 268, 288 |

> Either replace with a plain `if`/block, or reinstate the `case` labels somebody removed.

---

## 5. Bulk warnings (mostly mechanical to fix)

These three categories are most of the noise (∼965 / 1083 ≈ 89 %). They're individually low-stakes but should be cleaned up in batches.

### 5.1 C4100 — unused parameter (481)

Concentrated in 4 database/datamodel headers (380 / 481 ≈ 79 %):

| Count | File |
|---:|---|
| 161 | `src/model/src/database/rocprofvis_db.h` |
| 86 | `src/model/src/database/rocprofvis_db_compute.h` |
| 70 | `src/model/src/database/rocprofvis_db_profile.h` |
| 63 | `src/model/src/datamodel/rocprofvis_dm_topology.h` |

> Likely virtual base-class methods with default-empty bodies. Fix options, in order of preference:
> 1. Drop the parameter name in the declaration: `Foo(int /*id*/)` or `Foo(int)`.
> 2. Use `[[maybe_unused]]` (C++17).
> 3. `(void)id;` at the top of the body.
> 4. Last resort: `#pragma warning(suppress: 4100)` per header.

### 5.2 C4244 / C4245 / C4267 / C4018 / C4389 — integer conversions (518)

`size_t` ↔ `int`, `int64_t` ↔ `int32_t`, `signed` ↔ `unsigned`. Hotspots:

- C4267 (`size_t` → smaller): `src/model/src/database/rocprofvis_db.h` (23), `rocprofvis_db_cache.h` (23)
- C4245 (signed→unsigned init): `src/model/src/database/rocprofvis_db_packed_storage.h` (132 — *one file dominates the whole category*)
- C4244 (lossy numeric): `src/model/src/database/rocprofvis_db_packed_storage.cpp` (20), `rocprofvis_db_rocprof.cpp` (19), `rocprofvis_db_rocprof.h` (15)
- C4018 (signed/unsigned `<`): `src/model/src/database/rocprofvis_db.h` (23 — again concentrated)

> These are easy to *suppress* and dangerous to *blanket-cast away*. The right fix per call site is one of:
> - Make the destination type wide enough.
> - Use the matching signedness everywhere (prefer unsigned only when the value is genuinely a count/bit pattern).
> - `static_cast<T>(x)` only after you've confirmed the truncation is intentional.
> - Replace loop counters comparing against `.size()` with `for (size_t i = 0; i < v.size(); ++i)` or range-for.

### 5.3 C4068 — unknown pragma (1) — N/A (thirdparty only)

The single C4068 site is `thirdparty/jsoncpp/double-conversion/string-to-double.cc:28` — `#pragma GCC ...` in third-party code. Out of scope for this audit; no first-party action.

---

## 6. Manual review — issues the compiler did not flag

Scope: every `.h`/`.hpp`/`.cpp` under `src/`. Findings below are not exhaustive — they're a focused pass for the patterns you described (latent bugs, ownership smells, long-term maintenance traps).

### 6.1 Misuse of `ROCPROFVIS_ASSERT(strcpy(...))` — **HIGH**

`src/view/src/compute/rocprofvis_compute_data_provider.cpp`, lines **228** and **248**:

```228:228:src/view/src/compute/rocprofvis_compute_data_provider.cpp
                                ROCPROFVIS_ASSERT(strcpy(label, data.c_str()));
```

```248:248:src/view/src/compute/rocprofvis_compute_data_provider.cpp
                                ROCPROFVIS_ASSERT(strcpy(label, data.c_str()));
```

Two problems:

1. `strcpy` returns the destination pointer, which is `nullptr` *only* if you passed `nullptr` in. Asserting it is **never useful** — the assertion always passes (when `label` is valid).
2. If `ROCPROFVIS_ASSERT` is compiled out in Release (typical), the entire side-effecting expression vanishes and `label` is never populated. Even if the macro keeps the expression, the form is misleading and easy to break later.

Additionally, in the **Y-axis** loop the truncation happens *after* the buffer is allocated, while in the X-axis loop it happens before:

```243:248:src/view/src/compute/rocprofvis_compute_data_provider.cpp
                                char* label = new char[data.length() + 1];
                                if (data.length() > PLOT_LABEL_MAX_LENGTH)
                                {
                                    data = data.substr(0, PLOT_LABEL_MAX_LENGTH) + "...";
                                }
                                ROCPROFVIS_ASSERT(strcpy(label, data.c_str()));
```

The truncated string is shorter so it still fits — but the inconsistency is a maintenance trap. Use `std::string` for `m_tick_labels[i]` if at all possible; otherwise use `memcpy(label, data.data(), data.size()+1)` and drop the `ROCPROFVIS_ASSERT` wrapper.

### 6.2 Unsafe C string functions — see also §4.3

`strcpy` (above), `strncpy` (7 sites in §4.3), `vsprintf` (3 sites), `getch` (1 site), `getenv` (5 sites). All are flagged by MSVC as deprecated; on Windows the `_s` variants exist, but for portability prefer `std::string` / `std::format` / `std::span` everywhere we control.

### 6.3 Many raw `new` / `delete` pairs (manual ownership, no smart pointers)

22 first-party `delete x;` sites; many obvious owners that could be `std::unique_ptr`:

| File | Line(s) | Notes |
|---|---|---|
| `src/controller/src/system/rocprofvis_controller_trace_system.cpp` | 73-87, 362, 391 | 8 raw `delete`s in one destructor — obvious `unique_ptr` candidates |
| `src/controller/src/compute/rocprofvis_controller_workload.cpp` | 29, 33, 483 | container-of-pointers cleanup |
| `src/controller/src/system/rocprofvis_controller_event.cpp` | 51, 287 | including child container teardown |
| `src/model/src/common/rocprofvis_c_interface.cpp` | 519, 520 | conditional delete of C-interface owned object |
| `src/controller/src/system/rocprofvis_controller_topology.cpp` | 110 | tree node teardown — replace with recursive `unique_ptr`s |
| `src/controller/src/rocprofvis_controller.cpp` | 206 | `delete trace;` inside loop |
| `src/controller/src/rocprofvis_controller_job_system.cpp` | 151 | `delete job;` — typical task-queue pattern |
| `src/controller/src/system/rocprofvis_controller_mem_mgmt.cpp` | 629 | `delete pool;` |
| `src/controller/src/system/rocprofvis_controller_timeline.cpp` | 34 | `delete graph;` |
| `src/view/src/rocprofvis_appwindow.cpp` | 81 | singleton teardown |
| `src/view/src/rocprofvis_event_manager.cpp` | 31 | singleton teardown |
| `src/view/src/rocprofvis_hotkey_manager.cpp` | 65 | singleton teardown |

> Long-term: migrate the obvious owners to `std::unique_ptr<T>` or `std::vector<std::unique_ptr<T>>`. This is also the quickest way to stop hand-writing destructors with N `delete`s.

### 6.4 Empty-bodied `catch (...)` — silently swallowed exceptions

Two sites in `src/model/src/database/rocprofvis_db_table_processor.cpp`:

```138:141:src/model/src/database/rocprofvis_db_table_processor.cpp
                        catch (...) {
                            spdlog::error("Unknown error!");
                            result = kRocProfVisDmResultUnknownError;
                        }
```

```426:428:src/model/src/database/rocprofvis_db_table_processor.cpp
            } catch (...) {
                numeric = false;
            }
```

The first one logs but discards the exception object — at minimum capture `std::exception_ptr` and log `e.what()` for `std::exception`. The second uses an exception as flow control (parsing-success test); convert to `std::from_chars` (no exceptions) or `std::strtol` with explicit error-checking.

### 6.5 `std::rand()` / `std::srand(time(nullptr))` in tests

`src/model/src/test/main.cpp` (5 sites) and `src/model/src/tests/rocprofvis_dm_system_tests.cpp` (5 sites). `std::rand` has poor quality and is not threadsafe; `time(nullptr)` seeds at second granularity so tests run within the same second produce identical sequences.

> Replace with `std::mt19937` seeded by `std::random_device` (or with a fixed seed for reproducibility). For *tests* especially, fixed-seed is preferred so failures are reproducible.

### 6.6 `getenv` portability and thread-safety — see also §4.3

5 sites in `src/view/src/rocprofvis_utils.cpp` (lines 344, 441, 467-469). `std::getenv` returns a pointer to a possibly-shared CRT buffer; on Windows it is not safe to call concurrently with `_putenv`. A small `RocProfVis::GetEnv(const char*) -> std::optional<std::string>` wrapper in `rocprofvis_utils.cpp` would centralize the platform `_dupenv_s` / `std::getenv` decision and the deprecation suppression.

### 6.7 `printf` in production code (1 — borderline)

`src/model/src/test/main.cpp:33` — only the test harness, low risk. Mentioned only because the rest of the codebase uses `spdlog`, so this is an inconsistency.

### 6.8 TODO / FIXME backlog (12)

Not bugs *per se* but worth tracking so they don't rot:

| File | Line | Note |
|---|---:|---|
| `src/view/src/rocprofvis_data_provider.cpp` | 2604 | "basic info might not be ready yet" |
| `src/view/src/rocprofvis_data_provider.cpp` | 2607 | `flow.direction = 0; // TODO: fix direction for RPD traces` |
| `src/view/src/rocprofvis_data_provider.cpp` | 3339, 3446 | "review the controller return codes" |
| `src/view/src/rocprofvis_multi_track_table.cpp` | 508 | "TODO handle event selection" |
| `src/view/src/rocprofvis_track_item.cpp` | 270 | request-cancellation TODO |
| `src/model/src/datamodel/rocprofvis_dm_trace.cpp` | 654 | API change TODO |
| `src/view/src/rocprofvis_timeline_view.cpp` | 1230 | move-this TODO |
| `src/view/src/rocprofvis_line_track_item.cpp` | 217 | hardcoded padding |
| `src/view/src/widgets/rocprofvis_infinite_scroll_table.cpp` | 260 | group-by detection TODO |
| `src/controller/src/system/rocprofvis_controller_event.cpp` | 676 | "review this" |
| `src/view/src/widgets/rocprofvis_editable_textfield.cpp` | 36 | `// FIXME: Assuming default is an empty string` |
| `src/view/src/compute/rocprofvis_compute_kernel_metric_table.cpp` | 207 | "important column" TODO |

### 6.9 `dynamic_cast<T*>(p)->member` without null-check

| File | Count | Notes |
|---|---:|---|
| `src/view/src/rocprofvis_data_provider.cpp` | 1 | dereference-after-cast |
| `src/view/src/rocprofvis_appwindow.cpp` | 1 | dereference-after-cast |

> If the cast can fail (otherwise `static_cast` would suffice), the result must be null-checked before dereference. Worth a manual look at the two sites.

### 6.10 Things checked and **not** found (good news)

- No `using namespace …;` in any first-party header.
- No empty `catch (...) {}` blocks (all have at least a logging or state-update line).
- No `goto`s in first-party code.
- No `std::system("…")` shell-out calls.
- No `assert(false)` / `assert(0)` placeholders left over.
- No `#pragma warning` suppressions, no `// NOLINT` markers — i.e. no warnings have been hidden, only ignored. (Good — the cleanup work is actually visible.)

---

## 7. Recommended cleanup order

The following order maximizes signal-to-noise and minimizes risk:

**Phase 1 (current focus): MSVC compile-time warnings only (§4).**
Manual-review findings (§6) are deferred until the build is warning-clean — see §8 for the decision note.

1. **High-impact compile-warning bugs first** (small, high value):
   - ~~C4701 in `rocprofvis_db_rocprof.cpp:225` (potentially uninitialized).~~ ✓ done 2026-05-13
   - ~~C4702 unreachable code (4 sites).~~ ✓ done 2026-05-13
   - C4701/C4702 family is now complete. ← next high-impact compile-warning category up next is C4996 (deprecated CRT, 15 sites) since the others scale up rapidly.
2. **Variable shadowing pass** (32 fixes, mechanical, low risk): rename inner `i`/`j`/`k`/`it`/`result`/etc. — §4.4.
3. **Dead code removal** (27 sites): C4189/C4101 unused locals + C4505 unused static functions — §4.5/§4.6.
4. **Deprecated/unsafe CRT** (15 sites): 2 done by hand (`c_interface.cpp` strncpy pair); remaining 13 paused, will be auto-handled in Phase 2 — §4.3. Note `rocprofvis_controller_data.cpp:283` flagged for human attention (real contract bug).
5. **Database header sweep** — fix unused-parameter and `size_t` truncation in:
   - `src/model/src/database/rocprofvis_db.h` (207 warnings)
   - `src/model/src/database/rocprofvis_db_packed_storage.h` (132)
   - `src/model/src/database/rocprofvis_db_compute.h` (88)
   - `src/model/src/database/rocprofvis_db_profile.h` (70)
   - `src/model/src/datamodel/rocprofvis_dm_topology.h` (72)
   This single sweep removes ~46 % of all warnings.
6. **Bulk integer-conversion pass** across remaining `.cpp` files (C4244/C4245/C4267/C4018/C4389).

**Phase 2 (after Phase 1 hits zero compile warnings): manual-review findings from §6.**
- `ROCPROFVIS_ASSERT(strcpy(...))` in `rocprofvis_compute_data_provider.cpp` (§6.1) — *picking this back up first when Phase 2 begins.*
- Empty-ish `catch (...)` in `rocprofvis_db_table_processor.cpp` (§6.4).
- Smart-pointer migration for the 22 raw `delete` sites (§6.3) — separate PR, larger blast radius.
- Remaining §6.x items in the order they appear.

---

## 8. Parked observations

> Things noticed in passing while fixing other warnings. Not bugs we're tracking right now — out of mind, but here for later if/when someone wants to pick them up. Add new entries to the **top** of this list.

### 2026-05-15 — `TimeToPixel`: `zoom_before_clamp` was captured but never read (apparent lost intent)

`src/view/src/rocprofvis_time_to_pixel.cpp:71` previously held `float zoom_before_clamp = m_zoom;` immediately before the max-zoom clamp at lines 72–76. The variable was never read anywhere in the file (or the rest of `src/`), so it was deleted to clear the C4189. The naming is conspicuous though — "save the value before clamp" reads as setup for either (a) a `spdlog::trace`/debug log comparing pre- vs post-clamp `m_zoom`, or (b) a "did the clamp actually fire?" check feeding an event/notification. Whatever the intent was, the using code never made it in. Worth a half-hour eyeball by the original author to decide whether the clamp instrumentation should actually exist (and if so, restore it properly with the log call); otherwise the deletion is the right call.

### 2026-05-15 — `Track::GetHistogramBucketValueAt` / `GetHistogramBucketNumEventsAt` parameter / map key type mismatch

`Track::GetHistogramBucketValueAt(size_t index)` and `Track::GetHistogramBucketNumEventsAt(size_t index)` (in `rocprofvis_dm_track.cpp:177` and `:190`) declare a `size_t index` parameter, but `m_track_params->histogram` is `std::map<uint32_t, ...>` (defined in `rocprofvis_common_types.h:140` and `:157`). The C4267 fix added `static_cast<uint32_t>(index)` at the `find()` call to silence the warning, matching what the compiler was implicitly doing already. **However:** if any caller ever passes a value > `UINT32_MAX` (e.g., a 64-bit timestamp used directly as a histogram bucket key), the lookup truncates and silently misses the bucket. Pick one of: (a) audit all callers, prove they stay in 32-bit range, narrow the parameter to `uint32_t`; (b) widen the underlying map key to `size_t` if 64-bit bucket indices are real; (c) leave as-is and document the contract. Same shape as the histogram-builder code in `rocprofvis_db_profile.cpp::BuildHistogram` — should be checked together.

### 2026-05-14 — `rocprofvis_controller_track.cpp:824` `sample_value` is fetched but never used

After the C4457 rename, `Track::SetObject`'s local `sample_value` is populated via `object->GetDouble(kRPVControllerSampleValue, 0, &sample_value)` (line 843) and then never read. The downstream `Segment::Insert` calls (lines 898/904/911/919) only pass the timestamp/level/object; the sample's measurement value is dropped on the floor. Looks like incomplete wiring — somebody meant for the segment to remember the value alongside the sample. Not safe to delete the `GetDouble` call (it has the side effect of updating `result`, which is checked at line 848). When the C4189 sweep picks this up, decide between (a) wiring `sample_value` into `Segment::Insert`, (b) keeping the side-effecting call but making the assignment explicitly discarded (`(void)sample_value;`), or (c) restructuring the `result =` chain so the call's success-check doesn't depend on having a target.

### 2026-05-14 — `rocprofvis_db.cpp:28` discards `std::exception::what()` on allocation failure

`Database::AddTrackProperties` catches `const std::exception&` and only emits the static `ERROR_MEMORY_ALLOCATION_FAILURE` string via `ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN`. The exception's `what()` text is silently dropped, so a future memory-pressure bug will look identical regardless of root cause (`bad_alloc`, vector allocator failure, custom-allocator failure, etc.). Add `spdlog::error("Track properties allocation failed: {}", ex.what());` before the assert when the exception text becomes useful again. Same shape probably exists in other `catch (const std::exception&)` blocks across the database/controller layer — worth a sweep.

### 2026-05-14 — `<cstring>` is being pulled in transitively across the codebase

We added `std::memcpy` calls in `rocprofvis_c_interface.cpp` without an explicit `#include <cstring>` and the file built. That means `<cstring>` is reaching this translation unit through one of the project headers it includes. This is convenient but fragile — any future header reorg that severs that transitive path will break the build in surprising ways. Worth a one-pass audit: anywhere we use `std::memcpy` / `std::memset` / `std::strlen` / etc., add `#include <cstring>` explicitly. Not blocking.

### 2026-05-13 — Defer all §6 manual-review findings until the build is warning-clean

Decision: focus Phase 1 exclusively on MSVC compile-time warnings (§4). All manual-review findings in §6 — `ROCPROFVIS_ASSERT(strcpy(...))` (§6.1), unsafe C string functions (§6.2), raw `new`/`delete` ownership (§6.3), empty-bodied `catch (...)` (§6.4), `std::rand` / `time(nullptr)` seeding in tests (§6.5), `getenv` portability (§6.6), `printf` in production code (§6.7), TODO/FIXME backlog (§6.8), unchecked `dynamic_cast` (§6.9) — are deferred and will be revisited only after the compile-warning count is at zero. Returning to §6.1 first when that day comes.

### 2026-05-13 — `ROCPROFVIS_ASSERT` macro is unbraced (footgun, `rocprofvis_core_assert.h:15`)

```
#define ROCPROFVIS_ASSERT(cond) if (!(cond)) ROCPROFVIS_ASSERT_LOG(#cond); assert(cond)
```

This expands to two statements with no surrounding `do { ... } while(0)` wrapper. Any caller that writes `if (foo) ROCPROFVIS_ASSERT(bar); else baz();` will get the `else` dangling off the wrong `if`. Same applies to `ROCPROFVIS_ASSERT_MSG`, `ROCPROFVIS_ASSERT_RETURN`, `ROCPROFVIS_ASSERT_MSG_RETURN`, `ROCPROFVIS_ASSERT_MSG_BREAK`, and `ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN`. Wrap each macro body in `do { ... } while(0)` (or `if (true) { ... } else (void)0` — both kill the dangling-else risk). One small file change, fixes the entire family.

### 2026-05-13 — `rocprofvis_db_profile.cpp`: `BuildCounterSliceLeft/RightNeighbourQuery` are 95% duplicate

### 2026-05-13 — `rocprofvis_db_profile.cpp`: `BuildCounterSliceLeft/RightNeighbourQuery` are 95% duplicate

`BuildCounterSliceLeftNeighbourQuery` and `BuildCounterSliceRightNeighbourQuery` differ only in three constants (`<` vs `>`, `DESC` vs `ASC`, `start` vs `end`). Easy candidate for a single helper that takes the comparison operator, sort direction, and pivot timestamp as parameters.

### 2026-05-13 — `rocprofvis_dm_topology.cpp`: `kRPVControllerTopologyNodePropertyValueIndexed` only handles `uint64_t` (lines 200–210)

In `Get(...)`, the case `kRPVControllerTopologyNodePropertyValueIndexed` reads the property value only when the variant holds `uint64_t`; for `double` and `std::string` it falls through to `ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(...)` and returns `kRocProfVisDmResultInvalidProperty`. So the function silently can't return non-`uint64_t` property values via this path. Looks like a feature gap rather than a bug, but worth a look from whoever owns the topology API.

### 2026-05-13 — `rocprofvis_db_sqlite.cpp`: `GetNullExceptionInt` / `GetNullExceptionString` triple-nested `if`

Both functions do three levels of `if (… != nullptr) { if (… != end) { if (… != end) … }}` for a single map-of-map lookup. Easy candidate for early-return / continue-on-failure flattening. Strictly a readability refactor — no behavior change.

---

## Appendix A — Per-file warning counts (full list)

```
 207  src/model/src/database/rocprofvis_db.h
 132  src/model/src/database/rocprofvis_db_packed_storage.h
  88  src/model/src/database/rocprofvis_db_compute.h
  72  src/model/src/datamodel/rocprofvis_dm_topology.h
  70  src/model/src/database/rocprofvis_db_profile.h
  36  src/model/src/database/rocprofvis_db_profile.cpp
  34  src/model/src/database/rocprofvis_db_rocprof.cpp
  29  src/model/src/common/rocprofvis_common_types.h
  27  src/model/src/database/rocprofvis_db_packed_storage.cpp
  27  src/model/src/database/rocprofvis_db_table_processor.cpp
  25  src/controller/src/system/rocprofvis_controller_track.cpp
  23  src/model/src/database/rocprofvis_db_cache.h
  22  src/model/src/database/rocprofvis_db_compute.cpp
  21  src/model/src/datamodel/rocprofvis_dm_track_slice.cpp
  18  src/controller/src/system/rocprofvis_controller_table_system.cpp
  18  src/model/src/datamodel/rocprofvis_dm_trace.cpp
  17  src/controller/src/compute/rocprofvis_controller_trace_compute.cpp
  15  src/model/src/database/rocprofvis_db_rocprof.h
  15  src/model/src/datamodel/rocprofvis_dm_base.cpp
  15  src/view/src/compute/rocprofvis_compute_summary.cpp
  12  src/model/src/test/main.cpp
  10  src/controller/src/system/rocprofvis_controller_sample.cpp
   9  src/controller/src/system/rocprofvis_controller_mem_mgmt.cpp
   9  src/view/src/rocprofvis_summary_view.cpp
   8  src/model/src/database/rocprofvis_db_sqlite.cpp
   8  src/model/src/datamodel/rocprofvis_dm_topology.cpp
   7  src/model/src/tests/rocprofvis_dm_system_tests.cpp
   7  src/model/src/database/rocprofvis_db_cache.cpp
   6  src/view/src/widgets/rocprofvis_compute_widget.cpp
   5  src/model/src/database/rocprofvis_db.cpp
   5  src/view/src/rocprofvis_utils.cpp
   5  src/view/src/rocprofvis_timeline_arrow.cpp
   5  src/controller/tests/rocprofvis_controller_system_tests.cpp
   5  src/model/src/database/rocprofvis_db_expression_filter.cpp
   5  src/controller/src/compute/rocprofvis_controller_table_compute_pivot.cpp
   5  src/model/src/database/rocprofvis_db_rocpd.cpp
   4  src/controller/src/system/rocprofvis_controller_trace_system.cpp
   4  src/view/src/rocprofvis_track_topology.cpp
   4  src/controller/src/system/rocprofvis_controller_summary.cpp
   4  src/controller/src/system/rocprofvis_controller_event.cpp
   3  src/model/src/datamodel/rocprofvis_dm_table.cpp
   3  src/controller/src/system/rocprofvis_controller_segment.cpp
   3  src/view/src/rocprofvis_minimap.cpp
   3  src/model/src/datamodel/rocprofvis_dm_event_track_slice.cpp
   3  src/model/src/tests/rocprofvis_dm_compute_tests.cpp
   2  src/view/src/compute/rocprofvis_compute_workload_view.cpp
   2  src/model/src/datamodel/rocprofvis_dm_flow_trace.cpp
   2  src/view/src/rocprofvis_time_to_pixel.cpp
   2  src/model/src/datamodel/rocprofvis_dm_table_row.cpp
   2  src/model/src/common/rocprofvis_c_interface.cpp
   2  src/model/src/datamodel/rocprofvis_dm_track.cpp
   2  src/controller/src/system/rocprofvis_controller_summary_metrics.cpp
   1  src/controller/src/rocprofvis_controller_data.cpp
   1  src/view/src/widgets/rocprofvis_infinite_scroll_table.cpp
   1  src/model/src/database/rocprofvis_db_query_builder.cpp
   1  src/view/src/compute/rocprofvis_compute_kernel_metric_table.cpp
   1  src/model/src/database/rocprofvis_db_track.cpp
   1  src/model/src/database/rocprofvis_db_version.cpp
   1  src/view/src/rocprofvis_timeline_view.cpp
   1  src/view/src/rocprofvis_track_item.cpp
   1  src/controller/src/system/rocprofvis_controller_topology.cpp
   1  src/controller/src/system/rocprofvis_controller_graph.cpp
   1  src/controller/src/rocprofvis_controller_future.cpp
   1  src/view/src/rocprofvis_project.cpp
   1  src/controller/src/rocprofvis_controller_handle.cpp
```

Plus 3 warnings inside the MSVC STL header `<algorithm>` (templated from our code; will disappear when the calling code's types are fixed).

---

## Appendix B — Caveats / scope

- This audit reflects only what an MSVC `/W4` Debug build catches on Windows. **Linux (gcc) and macOS (clang) builds were not run** as part of this audit and may surface additional warnings (notably `-Wsign-compare`, `-Wreorder`, `-Wmaybe-uninitialized`).
- Manual review §6 was pattern-based (regex sweeps across 302 first-party files), not a line-by-line read of every file. It is good at catching widespread patterns and known anti-patterns; it will not catch subtle algorithmic bugs.
- A real static analyzer (`clang-tidy`, `cppcheck`, MSVC `/analyze`) would surface a different — and largely complementary — set of issues. Worth running once as a follow-up.
