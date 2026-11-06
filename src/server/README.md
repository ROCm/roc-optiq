# ROCm Optiq JSON server

`roc-optiq-server` loads one system or compute trace and exposes the
`DataProvider`'s current state as JSON. It is the backend wrapper for a future
web client; it does not replace or alter the desktop UI.

Build only the server:

```console
cmake -S . -B build/server -DROCPROFVIS_ENABLE_GUI=OFF -DROCPROFVIS_ENABLE_WEB_SERVER=ON
cmake --build build/server --config Release --target roc-optiq-server
```

Run it:

```console
roc-optiq-server --port 8080 --allow-origin http://localhost:3000 trace.db
```

The server intentionally binds only to loopback addresses. It has no
authentication or TLS, and `--allow-origin` is only a browser CORS setting.

## API state

`GET /api/status` reports the load state and whether the trace is `system` or
`compute`. `GET /api/snapshot` returns only that trace kind; its inactive
section is `null`. Routes for the other trace kind return JSON `404` responses.

Metadata loaded with the trace is available from the timeline, histogram,
minimap, topology, track, workload, and kernel routes. Data that the desktop
loads on demand—track data, event details, tables, summaries, metric values,
kernel tables, and PC sampling—does not yet have an HTTP command API. Its read
route returns JSON `501` until that command API exists, rather than returning a
permanent, ambiguous `null`.

All model reads run on the provider thread between `DataProvider::Update()`
calls. HTTP workers never read a provider model directly. The HTTP worker and
connection queues are bounded, timed-out provider reads are cancelled before
execution when possible, and responses are marked `Cache-Control: no-store`.

64-bit identifiers are decimal strings because JavaScript numbers cannot
represent every 64-bit integer. Other unsigned 64-bit values become strings
only when they exceed JavaScript's safe-integer limit. Times retain their `_ns`
suffix. Non-finite floating-point data is serialized as `null`.
