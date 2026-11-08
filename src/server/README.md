# ROCm Optiq JSON server

`roc-optiq-server` wraps the `DataProvider` in an HTTP server: it loads a
system or compute trace and answers JSON requests for what the provider holds,
fetching on demand what the desktop UI would fetch. It is the backend of the web
client in [`web/`](../../web/README.md); it does not replace or alter the
desktop UI.

Build only the server:

```console
cmake -S . -B build/server -DROCPROFVIS_ENABLE_GUI=OFF -DROCPROFVIS_ENABLE_WEB_SERVER=ON
cmake --build build/server --config Release --target roc-optiq-server
```

Run it, optionally with a trace and the built web app:

```console
roc-optiq-server --port 8080 --web web/dist trace.db
```

Then open `http://localhost:8080`. Without a trace the client opens one through
the API. `--allow-origin <origin>` lets a page served elsewhere read the API.

The server binds only to loopback addresses and insists on owning its port. It
has no authentication or TLS, and `--allow-origin` is only a browser CORS
setting. Routes that change something are `POST` with a JSON body, which a page
from another origin cannot send without a CORS preflight the server refuses.

## API

General:

| Route | |
|---|---|
| `GET /api/status` | Load state, trace kind (`system` or `compute`), trace path, version |
| `GET /api/snapshot` | Status plus the active trace kind's metadata |
| `GET /api/files?path=` | Folders and trace files in a server folder (default: the trace's) |
| `POST /api/open {path}` | Load another trace in place of the current one |

System traces:

| Route | |
|---|---|
| `GET /api/timeline`, `/api/topology`, `/api/minimap`, `/api/histogram` | Tracks, topology tree, density |
| `GET /api/track_data?tracks=&start=&end=&px=` | Events or samples of several tracks, merged to `px` pixels |
| `GET /api/events/<uuid>?track=` | An event's properties, arguments, flows and call stack |
| `GET /api/tables/<slot>?...` | A page of `event_table`, `sample_table`, `event_search_table`, `summary_*_table` or `top_*_events_table` |
| `GET /api/summary` | The summary tree, fetched on first use |
| `GET /api/tracks/<id>/statistics?start=&end=` | Queue utilization or counter statistics |
| `POST /api/save_selection {path, start_ns, end_ns}` | Write a time range to a new trace |
| `POST /api/cleanup {rebuild}` | Fast or full database cleanup |

Table queries take `offset`, `limit` (1–10000), `sort` (column index) and
`order` (`asc` or `desc`). Track tables also take `tracks`, `start`, `end`,
`filter` (a SQL condition), `group` and `group_cols`; searches take
newline-separated `terms` and the `substrings`, `category` and `partial` flags.
Any table can be scoped with `node`, `agent`, `process` and `thread` ids. A query
the database rejects returns the table with an `error` message.

Compute traces:

| Route | |
|---|---|
| `GET /api/compute` | Profiler info and every workload, kernel, metric catalog and roofline |
| `GET /api/compute/workloads/<w>[/kernels/<k>]` | One workload or kernel |
| `GET /api/compute/workloads/<w>/metrics?kernels=&metrics=&client=` | Metric values (`c`, `c.t` or `c.t.e` ids) for kernels, or for the workload |
| `GET /api/compute/workloads/<w>/kernel_table?metrics=&sort=&order=&filter_<column>=` | The kernel table with `c.t.e:value` columns |
| `GET /api/compute/workloads/<w>/kernels/<k>/pc_sampling?layer=` | ISA, `source` or `stalls` layer of PC sampling |

A route for the other trace kind returns `409`, and any data route returns `503`
until a trace is ready.

## How requests run

The provider and its models belong to the thread that calls
`DataProvider::Update()`, so every request is queued for that thread. A request
for data the provider loads on demand starts the fetch there and is answered
once the fetch finishes; a fetch the provider cannot take yet (another query of
the same table type is running) is retried for up to a minute. HTTP workers never
read a model directly, worker and connection queues are bounded, and API
responses are marked `Cache-Control: no-store`.

64-bit identifiers are decimal strings because JavaScript numbers cannot
represent every 64-bit integer. Other unsigned 64-bit values become strings
only when they exceed JavaScript's safe-integer limit. Times retain their `_ns`
suffix. Non-finite floating-point data is serialized as `null`.
