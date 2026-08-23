# Python analysis scripts

ROCm Optiq can run a Python source string against the currently loaded
**system trace**. Scripts use the injected `optiq` module to read tracks
and tables, then publish text with `optiq.result.text`.

This is an in-app interpreter, not `python my_script.py`. There is no
`sys.path` of your choosing, no pip, and no access to the process
filesystem APIs. Enable scripting at build time with
`ROCPROFVIS_ENABLE_SCRIPTING=ON`.

## How to run a script

1. Open a system trace (`.rpd` or equivalent).
2. Open the **Script** editor from the timeline or compute toolbar.
3. Write (or keep) a Python source string and click **Run**.

The editor sends the source plus the current selection (tracks and time
range) to the controller. Execution happens on a dedicated interpreter
thread. Fetch calls look synchronous in Python; the bindings wait on
the controller Future and copy progress back to the UI.

If nothing is selected, `optiq.selection` covers every track and the
full timeline range.

A script with no loaded controller can still run (for example
`optiq.result.text('hello')`). `optiq.trace` and `optiq.selection` are
then `None`, and `optiq.table()` raises.

## The `optiq` module

`optiq` is injected into the script globals. You do not need to import
it. `import optiq` is also allowed.

| Name | Type | Description |
|------|------|-------------|
| `optiq.trace` | `Trace` or `None` | All tracks on the loaded controller. |
| `optiq.selection` | `Selection` or `None` | Tracks and time range from the editor (or all tracks / full timeline). |
| `optiq.table()` | `Table` | Allocates a **private** query table. Does not touch the UI Event Table or Sample Table. |
| `optiq.result.text(s)` | function | Append a line of text to the script result. Call more than once; lines are joined with newlines. |

`print` is not available. Use `optiq.result.text`.

### Constants

| Constant | Meaning |
|----------|---------|
| `optiq.TRACK_TYPE_SAMPLES` | Sample / counter track. |
| `optiq.TRACK_TYPE_EVENTS` | Interval event track. |
| `optiq.TABLE_TYPE_EVENTS` | Event-table query (default for `Table.fetch`). |
| `optiq.TABLE_TYPE_SAMPLES` | Sample-table query. |
| `optiq.SORT_ASCENDING` | Default sort order for `Table.fetch`. |
| `optiq.SORT_DESCENDING` | Descending sort order. |

---

## `optiq.Trace`

Loaded system trace.

| Attribute | Type | Description |
|-----------|------|-------------|
| `tracks` | `list[Track]` | Every track on the controller, in controller order. |

---

## `optiq.Selection`

What the UI asked the script to consider.

| Attribute | Type | Description |
|-----------|------|-------------|
| `tracks` | `list[Track]` | Selected tracks, or all tracks if the context did not name any. |
| `start` | `float` | Selection start timestamp (same units as the timeline). |
| `end` | `float` | Selection end timestamp. |

---

## `optiq.Track`

One timeline track. Attributes are read from the live controller handle.

| Attribute | Type | Description |
|-----------|------|-------------|
| `id` | `int` | Track id. |
| `type` | `int` | `TRACK_TYPE_EVENTS` or `TRACK_TYPE_SAMPLES`. |
| `name` | `str` | Main display name. |
| `sub_name` | `str` | Secondary name (may be empty). |
| `min_time` | `float` | Earliest timestamp on the track. |
| `max_time` | `float` | Latest timestamp on the track. |
| `num_entries` | `int` | Declared entry count (not a fetch). |

### `Track.events(start=None, end=None)`

Fetch items in `[start, end]` and return a `list` of copied `Event`
objects. Omitting `start` / `end` uses the track's `min_time` /
`max_time`.

Works for both event tracks and sample tracks. On a sample track,
`Event.start` is the sample timestamp and `Event.end` is the sample end
(or the same timestamp if there is no separate end).

The call blocks until the controller fetch finishes. Keyboard interrupt
/ cancel is checked between wait slices.

```python
events = track.events(start=optiq.selection.start, end=optiq.selection.end)
```

---

## `optiq.Event`

A **copy** of one fetched track item. It is not a live controller
handle.

| Attribute | Type | Description |
|-----------|------|-------------|
| `id` | `int` | Event or sample id. |
| `start` | `float` | Start timestamp. |
| `end` | `float` | End timestamp. |
| `name` | `str` | Event name. Empty for samples. |

Duration is `e.end - e.start`. Gap between consecutive events on a
track is `events[i].start - events[i - 1].end`.

---

## `optiq.Table`

Private query table allocated with `optiq.table()`. Each call to
`optiq.table()` creates a new table. The object is freed when Python
drops the last reference.

Requires a loaded **system** trace. Compute traces raise
`RuntimeError`.

### `Table.fetch(...)`

Run a table query and return a `list` of `dict` rows. The list is also
cached on the table for `rows()`.

Keyword arguments:

| Argument | Default | Description |
|----------|---------|-------------|
| `tracks` | all matching tracks | Sequence of `optiq.Track`. Only tracks whose type matches the table type are used. |
| `start` | timeline min | Query start timestamp. |
| `end` | timeline max | Query end timestamp. |
| `where` | `""` | SQL-shaped `WHERE` fragment (same language as the Event / Sample Table). |
| `filter` | `""` | Expression filter string. |
| `group` | `""` | Group-by expression. |
| `group_columns` | `""` | Grouped column list. |
| `sort_column` | `0` | Column index to sort. |
| `sort_order` | `SORT_ASCENDING` | `SORT_ASCENDING` or `SORT_DESCENDING`. |
| `start_index` | `0` | First row to return. |
| `count` | `10000` | Maximum rows to return. |
| `type` | `"events"` | `"events"` / `"samples"`, or `TABLE_TYPE_EVENTS` / `TABLE_TYPE_SAMPLES`. |

At least one track of the matching type is required. Otherwise
`RuntimeError` is raised.

Each row is a dict keyed by **column header** strings. Cell values are
`int`, `float`, `str`, or `None` depending on the column type.

```python
table = optiq.table()
rows = table.fetch(
    tracks=optiq.selection.tracks,
    start=optiq.selection.start,
    end=optiq.selection.end,
    type="events",
    count=256,
)
optiq.result.text(str(len(rows)))
if rows:
    optiq.result.text(str(sorted(rows[0].keys())))
```

### `Table.rows()`

Return the list from the last successful `fetch`, or `[]` if `fetch`
has not been called.

---

## Allowed Python

The interpreter uses an isolated configuration and a restricted
`__import__`. This is **not** a security jail; it is a guardrail so
analysis scripts stay in-process and cannot casually open files or
launch processes.

### Import allowlist

These top-level modules may be imported (submodules of the same top
name are allowed, for example `collections.abc`):

`math`, `statistics`, `decimal`, `fractions`, `itertools`,
`functools`, `operator`, `collections`, `heapq`, `dataclasses`,
`typing`, `enum`, `json`, `re`, `datetime`, `textwrap`, `string`,
`optiq`

Allowlisted stdlib modules are also pre-imported into the script
globals, so `math.sqrt(16)` works without `import math`.

Disallowed imports fail with `ImportError` (for example `os`,
`subprocess`, `ctypes`, `sys`, `pathlib`, `numpy`).

### Builtins

A reduced builtin set is provided (`len`, `range`, `sum`, `min`,
`max`, `sorted`, `enumerate`, exceptions, and similar). There is no
`print`, `open`, `exec`, `eval`, or `__import__` except the restricted
one used by `import`.

---

## Examples

### Report how many tracks are loaded

```python
optiq.result.text(str(len(optiq.trace.tracks)))
```

### Test even spacing on the first selected event track

This is the default script in the editor.

```python
track = None
for t in optiq.selection.tracks:
    if t.type == optiq.TRACK_TYPE_EVENTS and t.num_entries > 0:
        track = t
        break
if track is None:
    optiq.result.text('No event track in the selection')
else:
    events = track.events(start=optiq.selection.start, end=optiq.selection.end)
    if len(events) < 2:
        optiq.result.text('Need at least two events on ' + track.name)
    else:
        gaps = [events[i].start - events[i - 1].end for i in range(1, len(events))]
        mean = sum(gaps) / len(gaps)
        max_dev = max(abs(g - mean) for g in gaps)
        even = max_dev <= (abs(mean) * 0.1)
        optiq.result.text(track.name)
        optiq.result.text('events=' + str(len(events)))
        optiq.result.text('mean_gap=' + str(mean))
        optiq.result.text('max_dev=' + str(max_dev))
        optiq.result.text('even' if even else 'uneven')
```

### Histogram of event count per track

Skip sample tracks. Each bin is a pair of **track id** and **event
count** (`num_entries` on the track, no fetch). Output is CSV so the
two values are easy to plot or paste elsewhere.

```python
pairs = []
for t in optiq.trace.tracks:
    if t.type == optiq.TRACK_TYPE_SAMPLES:
        continue
    pairs.append((t.id, t.num_entries))

optiq.result.text('track_id,event_count')
for track_id, count in pairs:
    optiq.result.text(str(track_id) + ',' + str(count))

max_count = max((count for _, count in pairs), default=0)
width = 40
for track_id, count in pairs:
    bar = 0 if max_count == 0 else int(round(width * count / max_count))
    optiq.result.text(str(track_id) + '\t' + ('#' * bar) + ' ' + str(count))
```

To count only events inside the current selection instead of the
declared track total, replace `t.num_entries` with
`len(t.events(start=optiq.selection.start, end=optiq.selection.end))`.

### Query the first event tracks through a private table

```python
event_tracks = [t for t in optiq.trace.tracks if t.type == optiq.TRACK_TYPE_EVENTS]
table = optiq.table()
rows = []
for track in event_tracks:
    rows = table.fetch(
        tracks=[track],
        start=track.min_time,
        end=track.max_time,
        count=32,
    )
    if rows:
        break
optiq.result.text(str(len(rows)))
```

---

## Errors and cancellation

- Controller failures surface as `RuntimeError` with a short message
  and a numeric result code, for example
  `track_fetch_async failed (3)`.
- A disallowed `import` raises `ImportError`.
- Cancel (Stop in the editor) interrupts the interpreter. In-flight
  fetches are cancelled between wait slices. Cancel is best-effort;
  the script may still finish the current Python statement.

The UI shows the joined `optiq.result.text` output on success, or the
error message on failure.

## What is not available yet

- Publishing a result table or plot back to the view (`optiq.result`
  is text-only).
- Compute-trace (`rocprof-compute`) queries via `optiq.table()`.
- `numpy` and other third-party packages.
- Writing files, launching processes, or talking to a system Python
  install.
