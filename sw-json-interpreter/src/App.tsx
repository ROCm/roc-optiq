import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import { MiddlewareClient, MiddlewareError } from './api/client';
import type { ExchangeLogEntry, Transport } from './api/client';
import type {
  SessionInfo,
  TableData,
  TimelineInfo,
  Track,
  TraceStatus,
  TrackData,
  TrackEvent,
  TrackSample,
} from './api/types';
import type { TimeRange } from './components/Timeline';
import { Timeline } from './components/Timeline';
import { JsonConsole } from './components/JsonConsole';
import { TablePanel } from './components/TablePanel';
import { DetailsPanel } from './components/DetailsPanel';
import type { ExtDataItem } from './components/DetailsPanel';
import { formatCount, formatDuration, trackLabel, trackSubLabel } from './lib/format';

type Tab = 'timeline' | 'table' | 'console';

const DEFAULT_URL = 'http://127.0.0.1:8378';

/*
 * The path is resolved by the server, so the useful default differs per
 * machine. Set VITE_TRACE_PATH in .env.local, which is not committed.
 */
const DEFAULT_TRACE_PATH: string =
  import.meta.env.VITE_TRACE_PATH || 'sample/trace_70b_1024_32.rpd';
const AUTO_SELECT_TRACKS = 6;
const FETCH_DEBOUNCE_MS = 260;

export default function App() {
  const [url, setUrl] = useState(DEFAULT_URL);
  const [transport, setTransport] = useState<Transport>('http');
  const [client, setClient] = useState<MiddlewareClient | null>(null);
  const [connected, setConnected] = useState(false);
  const [session, setSession] = useState<SessionInfo | null>(null);

  const [tracePath, setTracePath] = useState(DEFAULT_TRACE_PATH);
  const [timeline, setTimeline] = useState<TimelineInfo | null>(null);
  const [selectedTracks, setSelectedTracks] = useState<string[]>([]);
  const [trackData, setTrackData] = useState<Map<string, TrackData>>(new Map());
  const [filter, setFilter] = useState('');

  const [view, setView] = useState<TimeRange>({ start: 0, end: 1 });
  const [selection, setSelection] = useState<{
    track: Track;
    entry: TrackEvent | TrackSample;
  } | null>(null);
  const [extData, setExtData] = useState<ExtDataItem[] | null>(null);
  const [extBusy, setExtBusy] = useState(false);

  const [table, setTable] = useState<TableData | null>(null);
  const [tableBusy, setTableBusy] = useState(false);
  const [tableError, setTableError] = useState<string | null>(null);

  const [tab, setTab] = useState<Tab>('timeline');
  const [log, setLog] = useState<ExchangeLogEntry[]>([]);
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState('Not connected');
  const [error, setError] = useState<string | null>(null);

  const fetchGeneration = useRef(0);

  const bounds: TimeRange = useMemo(
    () =>
      timeline
        ? { start: timeline.min_timestamp, end: timeline.max_timestamp }
        : { start: 0, end: 1 },
    [timeline],
  );

  /* ------------------------------------------------------------ connect */

  /* Read the timeline for whatever trace the session currently holds. */
  const loadTimeline = useCallback(async (active: MiddlewareClient) => {
    setStatus('Reading the timeline…');
    const info = await active.call<TimelineInfo>('timeline.info');
    setTimeline(info);
    setView({ start: info.min_timestamp, end: info.max_timestamp });

    /* Show something immediately rather than an empty canvas. */
    const initial = info.tracks
      .filter((track) => (track.num_entries ?? 0) > 0)
      .slice(0, AUTO_SELECT_TRACKS)
      .map((track) => String(track.id));
    setSelectedTracks(initial);
    setStatus(
      `${info.num_tracks} tracks · ${formatDuration(
        info.max_timestamp - info.min_timestamp,
      )}`,
    );
    setTab('timeline');
  }, []);

  const connect = useCallback(async () => {
    setError(null);
    setBusy(true);
    setStatus('Connecting…');
    try {
      const next = new MiddlewareClient(url, transport);
      next.onLog((entry) => setLog((prev) => [entry, ...prev].slice(0, 300)));
      next.onStatus(setConnected);
      await next.connect();

      const info = await next.call<SessionInfo>('session.info');
      setClient(next);
      setSession(info);
      setConnected(true);
      setStatus(`Connected over ${transport}`);

      /*
       * One session is shared across every client and transport, so a trace
       * may already be open. Adopt it, rather than showing no trace and then
       * failing an open with trace_already_open.
       */
      const trace = await next.call<TraceStatus>('trace.status');
      if (trace.state === 'ready') {
        if (trace.paths.length > 0) setTracePath(trace.paths[0]);
        await loadTimeline(next);
      }
    } catch (caught) {
      setClient(null);
      setConnected(false);
      setStatus('Not connected');
      setError(
        `${describe(caught)} — is roc-optiq-middleware-http running on ${url}?`,
      );
    } finally {
      setBusy(false);
    }
  }, [url, transport, loadTimeline]);

  const disconnect = useCallback(() => {
    client?.disconnect();
    setClient(null);
    setConnected(false);
    setSession(null);
    setTimeline(null);
    setTrackData(new Map());
    setSelectedTracks([]);
    setSelection(null);
    setTable(null);
    setStatus('Not connected');
  }, [client]);

  /* --------------------------------------------------------- open trace */

  const openTrace = useCallback(async () => {
    if (!client) return;
    setBusy(true);
    setError(null);
    setTimeline(null);
    setTrackData(new Map());
    setSelection(null);
    setTable(null);

    try {
      /*
       * Poll rather than passing wait_ms: requests are served one at a time,
       * so a long inline wait would stall every other client.
       */
      await client.fetchAsync('trace.open', { path: tracePath }, (progress, message) =>
        setStatus(`Opening trace… ${progress}% ${message}`),
      );
      await loadTimeline(client);
    } catch (caught) {
      setError(describe(caught));
      setStatus('Connected');
    } finally {
      setBusy(false);
    }
  }, [client, tracePath, loadTimeline]);

  const closeTrace = useCallback(async () => {
    if (!client) return;
    try {
      await client.call('trace.close');
    } catch (caught) {
      setError(describe(caught));
    }
    setTimeline(null);
    setTrackData(new Map());
    setSelectedTracks([]);
    setSelection(null);
    setTable(null);
    setStatus('Connected');
  }, [client]);

  /* ------------------------------------------------- fetch visible data */

  useEffect(() => {
    if (!client || !timeline || selectedTracks.length === 0) return;

    const generation = ++fetchGeneration.current;
    const timer = setTimeout(async () => {
      for (const trackId of selectedTracks) {
        if (generation !== fetchGeneration.current) return;
        const track = timeline.tracks.find((item) => String(item.id) === trackId);
        if (!track) continue;
        try {
          /*
           * graph.fetch rather than track.fetch: it returns entries at the
           * asked-for pixel resolution instead of every raw one, which for a
           * busy track is the difference between 19k entries in 2s and 1.5M
           * in 72s. The entries carry real ids either way, so an event can
           * still be inspected.
           */
          const data = await client.fetchAsync<TrackData>('graph.fetch', {
            track_id: track.id,
            start_time: view.start,
            end_time: view.end,
            x_resolution: plotResolution(),
          });
          if (generation !== fetchGeneration.current) return;
          setTrackData((prev) => new Map(prev).set(trackId, data));
        } catch (caught) {
          if (generation !== fetchGeneration.current) return;
          setError(`track ${trackId}: ${describe(caught)}`);
        }
      }
    }, FETCH_DEBOUNCE_MS);

    return () => clearTimeout(timer);
  }, [client, timeline, selectedTracks, view]);

  /* Drop data for tracks that are no longer shown. */
  useEffect(() => {
    setTrackData((prev) => {
      const next = new Map<string, TrackData>();
      selectedTracks.forEach((id) => {
        const existing = prev.get(id);
        if (existing) next.set(id, existing);
      });
      return next;
    });
  }, [selectedTracks]);

  /* -------------------------------------------------------- interaction */

  const toggleTrack = (id: string) => {
    setSelectedTracks((prev) =>
      prev.includes(id) ? prev.filter((item) => item !== id) : [...prev, id],
    );
  };

  const onSelectEntry = (track: Track, entry: TrackEvent | TrackSample) => {
    setSelection({ track, entry });
    setExtData(null);
  };

  const loadExtData = useCallback(async () => {
    if (!client || !selection) return;
    setExtBusy(true);
    try {
      const result = await client.fetchAsync<{ entries: ExtDataItem[] }>(
        'event.ext_data',
        { event_id: selection.entry.id },
      );
      setExtData(result.entries ?? []);
    } catch (caught) {
      setError(describe(caught));
      setExtData([]);
    } finally {
      setExtBusy(false);
    }
  }, [client, selection]);

  const fetchTable = useCallback(
    async (tableType: string, startRow: number, rowCount: number) => {
      if (!client) return;
      setTableBusy(true);
      setTableError(null);
      try {
        const result = await client.fetchAsync<TableData>('table.fetch', {
          table_type: tableType,
          track_ids: selectedTracks.map((id) => Number(id)),
          start_row: startRow,
          row_count: rowCount,
        });
        setTable(result);
      } catch (caught) {
        setTable(null);
        setTableError(describe(caught));
      } finally {
        setTableBusy(false);
      }
    },
    [client, selectedTracks],
  );

  const sendRaw = useCallback(
    async (raw: string) => {
      if (!client) return;
      setBusy(true);
      try {
        const parsed = JSON.parse(raw) as {
          method?: string;
          params?: Record<string, unknown>;
        };
        if (!parsed.method) throw new Error('the document needs a "method"');
        await client.request(parsed.method, parsed.params ?? {});
      } catch (caught) {
        setError(describe(caught));
      } finally {
        setBusy(false);
      }
    },
    [client],
  );

  const resetZoom = () => setView(bounds);

  const visibleTracks = useMemo(() => {
    if (!timeline) return [];
    const needle = filter.trim().toLowerCase();
    if (!needle) return timeline.tracks;
    return timeline.tracks.filter((track) =>
      `${track.main_name ?? ''} ${track.sub_name ?? ''} ${track.category ?? ''}`
        .toLowerCase()
        .includes(needle),
    );
  }, [timeline, filter]);

  const shownTracks = useMemo(
    () =>
      selectedTracks
        .map((id) => timeline?.tracks.find((track) => String(track.id) === id))
        .filter((track): track is Track => track !== undefined),
    [selectedTracks, timeline],
  );

  /* -------------------------------------------------------------- render */

  return (
    <div className="app">
      <header className="topbar">
        <div className="brand">
          sw-json-interpreter<span>ROCm Optiq middleware client</span>
        </div>

        <span className={`dot ${connected ? 'on' : busy ? 'busy' : 'off'}`} />
        <span style={{ color: 'var(--text-dim)', fontSize: 12 }}>{status}</span>

        <span className="spacer" />

        <input
          className="mono"
          value={url}
          onChange={(event) => setUrl(event.target.value)}
          disabled={connected}
          style={{ width: 190 }}
          spellCheck={false}
        />
        <select
          value={transport}
          onChange={(event) => setTransport(event.target.value as Transport)}
          disabled={connected}
        >
          <option value="http">HTTP</option>
          <option value="websocket">WebSocket</option>
        </select>
        {connected ? (
          <button onClick={disconnect}>Disconnect</button>
        ) : (
          <button className="primary" onClick={connect} disabled={busy}>
            Connect
          </button>
        )}
      </header>

      {error && (
        <div className="banner error">
          <span style={{ flex: 1 }}>{error}</span>
          <button onClick={() => setError(null)}>Dismiss</button>
        </div>
      )}

      <div className="body">
        <aside className="sidebar">
          <div className="section">
            <h2>Trace</h2>
            <div className="field">
              <label>Path on the server</label>
              <input
                className="mono"
                value={tracePath}
                onChange={(event) => setTracePath(event.target.value)}
                disabled={!connected || busy}
                spellCheck={false}
              />
            </div>
            <div className="row">
              <button
                className="primary"
                onClick={openTrace}
                disabled={!connected || busy}
                style={{ flex: 1 }}
              >
                {busy ? 'Working…' : 'Open'}
              </button>
              <button onClick={closeTrace} disabled={!timeline || busy}>
                Close
              </button>
            </div>
            {!connected && (
              <p className="hint" style={{ marginTop: 9, marginBottom: 0 }}>
                Start the server with{' '}
                <code>roc-optiq-middleware-http --port 8378</code>, then
                connect. The path is resolved on the server, not here.
              </p>
            )}
          </div>

          {session && (
            <div className="section">
              <h2>Session</h2>
              <div className="hint">
                Protocol v{session.protocol_version} · {session.methods.length}{' '}
                methods
                <br />
                {Object.entries(session.capabilities)
                  .filter(([, enabled]) => enabled)
                  .map(([name]) => name)
                  .join(', ') || 'no capabilities reported'}
              </div>
            </div>
          )}

          {timeline && (
            <>
              <div className="section" style={{ paddingBottom: 9 }}>
                <h2>
                  Tracks ({selectedTracks.length}/{timeline.num_tracks})
                </h2>
                <input
                  placeholder="Filter tracks…"
                  value={filter}
                  onChange={(event) => setFilter(event.target.value)}
                  style={{ width: '100%' }}
                />
              </div>
              <div className="tracks">
                {visibleTracks.map((track) => {
                  const id = String(track.id);
                  const on = selectedTracks.includes(id);
                  return (
                    <div
                      key={id}
                      className={`track-row ${on ? 'on' : ''}`}
                      onClick={() => toggleTrack(id)}
                    >
                      <input type="checkbox" checked={on} readOnly />
                      <div style={{ flex: 1, minWidth: 0 }}>
                        <div className="track-name">{trackLabel(track)}</div>
                        {trackSubLabel(track) && (
                          <div className="track-sub">{trackSubLabel(track)}</div>
                        )}
                      </div>
                      <span className="track-count">
                        {formatCount(track.num_entries ?? 0)}
                      </span>
                    </div>
                  );
                })}
              </div>
            </>
          )}
        </aside>

        <main className="main">
          <nav className="tabs">
            {(['timeline', 'table', 'console'] as Tab[]).map((name) => (
              <div
                key={name}
                className={`tab ${tab === name ? 'on' : ''}`}
                onClick={() => setTab(name)}
              >
                {name === 'console' ? 'JSON console' : name}
                {name === 'console' && log.length > 0 && (
                  <span className="badge" style={{ marginLeft: 7 }}>
                    {log.length}
                  </span>
                )}
              </div>
            ))}
          </nav>

          <div className="pane">
            {tab === 'timeline' &&
              (timeline ? (
                <>
                  <div className="timeline-bar">
                    <button onClick={resetZoom}>Reset zoom</button>
                    <span>
                      {formatDuration(view.end - view.start)} shown of{' '}
                      {formatDuration(bounds.end - bounds.start)}
                    </span>
                    <span className="spacer" />
                    <span className="hint">
                      scroll to zoom · drag to pan · click an event for details
                    </span>
                  </div>
                  <Timeline
                    tracks={shownTracks}
                    data={trackData}
                    view={view}
                    bounds={bounds}
                    onViewChange={setView}
                    onSelect={onSelectEntry}
                    selectedId={
                      selection ? String(selection.entry.id) : null
                    }
                  />
                </>
              ) : (
                <div className="empty">
                  <h3>No trace open</h3>
                  <p>
                    {connected
                      ? 'Enter a path the server can reach and press Open. The trace loads asynchronously, so progress is reported while it works.'
                      : 'Connect to a running middleware server to begin.'}
                  </p>
                </div>
              ))}

            {tab === 'table' && (
              <TablePanel
                tracks={shownTracks}
                selectedTrackIds={selectedTracks}
                table={table}
                busy={tableBusy}
                error={tableError}
                onFetch={fetchTable}
              />
            )}

            {tab === 'console' && (
              <JsonConsole
                log={log}
                onSend={sendRaw}
                onClear={() => setLog([])}
                busy={busy || !client}
              />
            )}
          </div>

          {selection && tab === 'timeline' && (
            <DetailsPanel
              track={selection.track}
              entry={selection.entry}
              origin={bounds.start}
              extData={extData}
              extBusy={extBusy}
              onLoadExt={loadExtData}
              onClose={() => setSelection(null)}
            />
          )}
        </main>
      </div>
    </div>
  );
}

/*
 * Level of detail to ask for. The window's width is an upper bound on the
 * plot's, and asking for more detail than there are pixels only costs a
 * little bandwidth, where asking for less loses events that would be visible.
 */
function plotResolution(): number {
  return Math.max(Math.round(window.innerWidth), 800);
}

function describe(caught: unknown): string {
  if (caught instanceof MiddlewareError) return `${caught.code}: ${caught.message}`;
  if (caught instanceof Error) return caught.message;
  return String(caught);
}
