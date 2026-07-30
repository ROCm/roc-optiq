# ROCm Optiq Middleware

A JSON request/response layer over the controller, so a frontend can drive a
system trace without linking C++ or speaking the controller's property-bag ABI.

The controller's C ABI is deliberately granular: a caller reads one property at
a time off an opaque handle, with the property enum deciding both the meaning
and the getter that is legal to use. That is a good fit for the C++ View, which
is compiled against the same headers, and a bad fit for anything across a
process or language boundary. This layer trades that granularity for a small
set of coarse, intent-level calls -- open a trace, describe the timeline, fetch
a track, fetch a table -- each of which answers with a self-contained JSON
document.

**Scope: system traces.** Compute traces, the profiler launcher, and remote/SSH
sessions are not exposed. `session.info` reports what a build supports, so a
client can feature-detect rather than assume.

## Layout

| Path | Contents |
| --- | --- |
| `inc/rocprofvis_middleware.h` | Public C ABI: allocate a session, hand it a JSON string, get a JSON string back |
| `src/rocprofvis_mw_session.*` | Session state, request registry, async polling, method dispatch |
| `src/rocprofvis_mw_methods.cpp` | The methods themselves |
| `src/rocprofvis_mw_serialize.*` | Controller handle to JSON encoders |
| `src/rocprofvis_mw_enums.*` | Stable string spellings for controller enums |
| `src/rocprofvis_mw_json.*` | Defensive helpers over `jt::Json` |
| `tools/rocprofvis_mw_stdio_main.cpp` | Transport: newline-delimited JSON on stdin/stdout |
| `tools/http/rocprofvis_mw_net.*` | Cross-platform TCP sockets (winsock2 / BSD), non-blocking |
| `tools/http/rocprofvis_mw_http.*` | HTTP/1.1 request parser and response writer |
| `tools/http/rocprofvis_mw_websocket.*` | RFC 6455 handshake and frame codec |
| `tools/http/rocprofvis_mw_http_main.cpp` | Transport: HTTP and WebSocket on one port |
| `tests/` | Protocol tests and transport-codec tests |

The core is transport agnostic and knows nothing about either adapter: both
reduce to handing a JSON string to `rocprofvis_mw_request` and writing the
answer back. A third transport would add a file under `tools/` and change
nothing else.

## Talking to it

Two adapters ship. Both speak the same protocol, so the choice is only about
plumbing: [stdio](#stdio) for a child process, [HTTP or
WebSocket](#http-and-websocket) for anything over a socket.

### stdio

Build `roc-optiq-middleware-stdio` and write one JSON object per line:

```bash
printf '%s\n' \
  '{"id":1,"method":"trace.open","params":{"path":"sample/trace.rpd","wait_ms":60000}}' \
  '{"id":2,"method":"timeline.info","params":{}}' \
  | ./roc-optiq-middleware-stdio
```

Each line in, one line out. stdout carries the protocol and nothing else; logs
go to stderr.

### HTTP and WebSocket

Build `roc-optiq-middleware-http` and run it. It binds loopback by default:

```bash
./roc-optiq-middleware-http --host 127.0.0.1 --port 8378 --verbose
```

One port serves both transports:

| Endpoint | Purpose |
| --- | --- |
| `POST /rpc` | One JSON request in the body, one JSON response back |
| `GET /ws` | Upgrade to a WebSocket carrying the same documents |
| `GET /health` | Liveness check, answers `{"ok":true}` without touching the session |
| `GET /` | Lists the endpoints above, for someone who arrived in a browser |

`POST /rpc` is a plain request/response, which makes it the easy one to reach
for:

```bash
curl -s localhost:8378/rpc \
  -d '{"id":1,"method":"trace.open","params":{"path":"sample/trace.rpd","wait_ms":60000}}'
```

The WebSocket carries exactly the same documents, one JSON object per message,
and is the better fit for a UI: the connection stays up, so a client can leave
several fetches in flight and poll them without paying for a new connection
each time. Replies always come back as text frames. Fragmented messages are
reassembled, and ping/pong and close are handled for you.

Both transports share **one session**, which is the point -- open a trace over
`POST /rpc` and a WebSocket client sees it too. It also means the caveat from
[Async requests](#async-requests) applies across transports: requests are
served strictly one at a time, so a call made with a long `wait_ms` blocks
every other client for its duration. Prefer polling to inline waits when more
than one client is connected.

Responses carry permissive CORS headers (`Access-Control-Allow-Origin: *`) and
`OPTIONS` preflights are answered, so a browser frontend can call it from a dev
server on another port.

> **This is not a hardened server.** There is no authentication, no TLS, and no
> rate limiting, and any origin may call it. Anyone who can reach the port can
> read any trace the process can open, and `table.export_csv` and
> `trace.save_trimmed` let them write files. Keep it on loopback. Passing
> `--host 0.0.0.0` exposes all of that to the network, and it logs a warning
> saying so.

Inbound size is capped -- 32 KB of HTTP headers, 8 MB of HTTP body, and 16 MB
for a WebSocket message whether it arrives in one frame or many -- so a bad
length field fails one request instead of the process. Responses are not
capped: a large `table.fetch` is buffered per connection and drained as the
socket accepts it.

### Embedding

To skip both adapters, the whole ABI is four functions:

```c
rocprofvis_mw_session_t* session = rocprofvis_mw_session_alloc();
char* response = rocprofvis_mw_request(session, "{\"method\":\"session.info\"}");
rocprofvis_mw_string_free(response);
rocprofvis_mw_session_free(session);
```

A session is single-threaded: do not call `rocprofvis_mw_request` concurrently
on one session. The controller's worker threads run underneath, which is how
work overlaps; see [Async requests](#async-requests).

## Message shape

A request is an object with `method`, optional `params`, and an optional `id`:

```json
{ "id": 17, "method": "track.fetch", "params": { "track_id": 4 } }
```

`id` is opaque. It is echoed back untouched, in whatever JSON type it arrived
as, so a client can correlate responses without the server ascribing meaning to
it.

A response always carries `ok`, and either `result` or `error`. It is always
well formed, including for input that is not JSON at all:

```json
{ "id": 17, "method": "track.fetch", "ok": true, "result": { } }
{ "id": 17, "method": "track.fetch", "ok": false,
  "error": { "code": "unknown_track", "message": "no track with id 4" } }
```

`code` is a stable identifier worth branching on. `message` is for humans and
may be reworded.

### Numbers and ids

Ids in a trace can exceed 2^53, which is the largest integer a JSON number
holds exactly. Past that point an id is emitted as a **decimal string**:

```json
{ "id": "1152921528229174913", "name": "OpTable::writeRows" }
```

So for any id-shaped field, accept both a number and a string, and **echo the
value back exactly as it arrived**. Reformatting a large id through a JSON
number rounds it, and rounding lands on a neighbouring event rather than on
nothing -- silently wrong data. Sending an oversized id as a number is
therefore rejected with `invalid_argument` rather than answered.

Timestamps are JSON numbers in the trace's native units. Absent properties are
omitted rather than sent as zero, so `contains` is the way to test for one.

## Async requests

Fetches are asynchronous underneath. Any fetching method answers with a request
envelope rather than the payload:

```json
{ "request_id": 4, "status": "pending", "progress": 40, "message": "..." }
```

`status` is `pending`, `ready`, `error`, or `cancelled`. When it is `ready` the
envelope also carries `result`; when it is `error` it carries `error_code`.

There are two ways to get the payload.

**Wait inline.** Pass `wait_ms` in `params` and the call blocks up to that long,
returning a completed envelope if the work finished in time. Simplest, and what
the tests use.

**Poll.** Omit `wait_ms`, keep the `request_id`, and call `request.poll` (which
itself accepts `wait_ms`). This keeps the client responsive and lets several
fetches be in flight at once.

Either way a terminal result is **delivered exactly once** and the request is
then retired: the registry only tracks work still in flight. Polling a second
time reports `unknown_request`; a client that needs the data again re-issues the
fetch. `request.list` shows what is outstanding and `request.cancel` drops one.

Every call advances all outstanding requests before doing its own work, so any
request refreshes everyone's `progress`.

## Methods

`session.info` lists the methods a build actually has, which is more reliable
than this table.

### Session and trace

| Method | Purpose |
| --- | --- |
| `session.info` | Protocol version, capabilities, method list |
| `trace.open` | Open a trace by `path`. Async |
| `trace.close` | Close it and reset the session for reuse |
| `trace.status` | State (`empty`/`loading`/`ready`/`error`), paths, outstanding request count |
| `trace.topology` | Nodes, processors, processes, threads, queues, streams, counters |
| `trace.histogram` | Event density over the trace, aggregate and per track |
| `trace.save_trimmed` | Write a time-clipped copy. Async |
| `trace.cleanup` | Release cached trace data. Async |

Topology comes back as flat, id-keyed collections rather than a tree, so a
client can index it directly; parents are referenced by id (`node_id`,
`process_id`, `track_id`, and so on).

### Timeline and data

| Method | Purpose |
| --- | --- |
| `timeline.info` | Track metadata and the trace's time bounds |
| `track.fetch` | Raw entries for one track over a time range. Async |
| `graph.fetch` | Level-of-detail entries for one track at `x_resolution`. Async |
| `table.fetch` | A page of a table. Async |
| `table.export_csv` | Write a table to `path` as CSV. Async |
| `summary.fetch` | Aggregation tree over a time range. Async |
| `event.ext_data` | An event's extended properties. Async |
| `event.flow` | An event's flow-control links. Async |
| `event.callstack` | An event's callstack frames. Async |
| `analysis.track_statistics` | `counter_statistics` or `queue_utilization` for one track. Async |

`track.fetch` and `graph.fetch` report `track_type` (or `graph_type`) and a
`kind` of `events` or `samples`, because the two decode differently.

### Requests

| Method | Purpose |
| --- | --- |
| `request.poll` | Fetch the state of one request, optionally waiting |
| `request.cancel` | Cancel and retire one request |
| `request.list` | What is still outstanding |

## Tables

`table.fetch` takes a `table_type` and a **selection**, plus the usual paging
(`start_row`, `row_count`), ordering (`sort_column`, `sort_order`), and clauses
(`where`, `filter`, `group`, `group_columns`).

A table is also clipped to `start_time` and `end_time`. Both default to the
trace's own bounds, so omitting them means the whole thing rather than the
empty range.

Every table is scoped by a selection, and it must not be empty:

| `table_type` | Selection |
| --- | --- |
| `events`, `instrumented_events`, `dispatch_events`, `memory_allocation_events`, `memory_copy_events`, `sampled_events` | `track_ids`, all of them event tracks |
| `samples` | `track_ids`, all of them sample tracks |
| `search_results`, `summary_kernel_instances` | `operation_types`, plus optional `string_table_filters` |

Both rules are enforced here, up front, and a violation is reported against the
offending id. Underneath, the query builder reads the first element of the
selection without checking that one exists, and a track of the wrong kind is
dropped while packing rather than reported -- so an unchecked request would take
the process down instead of returning an error.

The result carries the schema and the rows separately:

```json
{ "columns": [ { "name": "duration", "type": "uint64" } ],
  "total_rows": 91043, "start_row": 0,
  "rows": [ [ 162455 ] ] }
```

`rows` are positional and every row is as wide as `columns`, so a client can zip
the two. `total_rows` is the size of the whole query, not of the page returned.

## Error codes

| Code | Meaning |
| --- | --- |
| `parse_error` | The request was not a well-formed JSON object |
| `unknown_method` | No such method in this build |
| `invalid_argument` | A parameter is missing, malformed, or inconsistent |
| `no_trace` | No trace is open |
| `trace_loading` | A trace is still loading |
| `trace_not_ready` | A trace is open but not usable |
| `trace_already_open` | `trace.open` while one is already open |
| `open_failed` | The trace could not be opened |
| `unsupported_trace` | Recognised, but not a system trace |
| `no_timeline` | The trace has no timeline |
| `unknown_track` | No track with that id |
| `table_unavailable` | That table does not exist for this trace |
| `unknown_request` | No outstanding request with that id, possibly already delivered |
| `memory_alloc_error` | Allocation failed |

A failure inside the controller is reported under the controller's own result
name (`timeout`, `cancelled`, `not_supported`, and so on) rather than being
flattened into one of the above.

## Tests

Two suites, split by what they need.

`roc-optiq-middleware-tests` speaks the protocol rather than calling internals,
so it doubles as worked examples of each flow. It needs a trace:

```bash
ctest -R roc-optiq-middleware-tests
```

Or directly, against either sample trace:

```bash
roc-optiq-middleware-tests --input_file sample/trace_70b_1024_32.rpd
roc-optiq-middleware-tests --input_file sample/rocpd-transpose.db
```

`roc-optiq-middleware-transport-tests` covers the HTTP parser and the WebSocket
codec on their own. No trace, no sockets, so it runs in well under a second:

```bash
ctest -R roc-optiq-middleware-transport-tests
```

It pins the parts that are easy to get subtly wrong and expensive to debug over
a live connection -- the RFC 6455 worked example, SHA-1 across a block
boundary, every payload-length encoding at its boundary, masking, fragment
reassembly, pipelined requests, and each malformed-input status code.
