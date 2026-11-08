# ROCm Optiq web

The browser client for ROCm Optiq. It is a React and TypeScript app that reads
everything from [`roc-optiq-server`](../src/server/README.md) over its JSON API,
so the trace stays on the server and the page only asks for what is on screen.

## Run it

One process serves the built app and the API:

```console
npm ci
npm run build
roc-optiq-server --web web/dist [trace.db]
```

Then open `http://localhost:8080`.

For development, run a server on port 8080 and the Vite dev server, which
reloads on save and forwards `/api` to the server:

```console
roc-optiq-server [trace.db]
npm run dev
```

Then open `http://localhost:5173`. `npm run build` also type-checks.

## Where things are

| File | |
|---|---|
| `src/api.ts` | The server's JSON types and `get`/`post` |
| `src/store.ts` | App state; settings and per-trace annotations, bookmarks and track layout persist in local storage |
| `src/App.tsx` | Top bar, welcome and loading screens, routing by trace kind |
| `src/system/` | System traces: timeline and toolbar, topology sidebar, overview, details panel, summary, search |
| `src/assistant/` | Ask Optiq: the conversation loop and the tools it runs against the API |
| `src/compute/` | Compute profiles: summary, kernel details and memory chart, table view, comparison, ISA |
| `src/table.tsx`, `src/charts.tsx`, `src/ui.tsx` | Shared tables, charts and controls |

The timeline draws on a canvas from the store directly, and asks the server for
each visible track's events at the current zoom with `/api/track_data`.

Ask Optiq uses the endpoint and key configured in the desktop app; the server
relays each round (see the server README). Its key button sets a different
endpoint, model or key for this browser only.
