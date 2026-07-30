import { useState } from 'react';
import type { ExchangeLogEntry } from '../api/client';

interface Props {
  log: ExchangeLogEntry[];
  onSend: (raw: string) => void;
  onClear: () => void;
  busy: boolean;
}

const PRESETS = [
  '{"method":"session.info"}',
  '{"method":"trace.status"}',
  '{"method":"timeline.info"}',
  '{"method":"trace.topology"}',
  '{"method":"request.list"}',
];

/*
 * The raw protocol view: every exchange the client has made, plus a way to
 * hand-write one. This is the part that makes the JSON boundary legible.
 */
export function JsonConsole({ log, onSend, onClear, busy }: Props) {
  const [draft, setDraft] = useState('{"method":"session.info"}');

  const submit = (event: React.FormEvent) => {
    event.preventDefault();
    if (draft.trim()) onSend(draft);
  };

  return (
    <>
      <div className="timeline-bar">
        <form
          onSubmit={submit}
          style={{ display: 'flex', gap: 7, flex: 1, minWidth: 320 }}
        >
          <input
            className="mono"
            value={draft}
            onChange={(event) => setDraft(event.target.value)}
            placeholder='{"method":"session.info","params":{}}'
            style={{ flex: 1 }}
            spellCheck={false}
          />
          <button type="submit" className="primary" disabled={busy}>
            Send
          </button>
        </form>
        <button onClick={onClear} disabled={log.length === 0}>
          Clear
        </button>
      </div>

      <div className="timeline-bar" style={{ gap: 6 }}>
        <span style={{ color: 'var(--text-faint)' }}>Presets</span>
        {PRESETS.map((preset) => (
          <button
            key={preset}
            onClick={() => setDraft(preset)}
            style={{ fontSize: 11, padding: '3px 8px' }}
          >
            {JSON.parse(preset).method}
          </button>
        ))}
      </div>

      <div className="console">
        {log.length === 0 ? (
          <div className="empty">
            <h3>No exchanges yet</h3>
            <p>
              Every request this app makes is recorded here with its response,
              so you can watch the protocol as the UI drives it.
            </p>
          </div>
        ) : (
          log.map((entry) => <Exchange key={entry.seq} entry={entry} />)
        )}
      </div>
    </>
  );
}

function Exchange({ entry }: { entry: ExchangeLogEntry }) {
  const [open, setOpen] = useState(false);
  const time = new Date(entry.at).toLocaleTimeString();

  return (
    <div className="exchange">
      <div className="exchange-head" onClick={() => setOpen(!open)}>
        <span style={{ color: 'var(--text-faint)', width: 12 }}>
          {open ? '▾' : '▸'}
        </span>
        <span className="m">{entry.method}</span>
        <span className={`badge ${entry.ok ? 'ok' : 'err'}`}>
          {entry.ok ? 'ok' : 'error'}
        </span>
        <span className="badge">{entry.transport}</span>
        <span className="spacer" />
        <span className="t">
          {entry.elapsedMs.toFixed(1)} ms · {time}
        </span>
      </div>
      {open && (
        <>
          <div className="pre-label">Request</div>
          <pre>{JSON.stringify(entry.request, null, 2)}</pre>
          <div className="pre-label">Response</div>
          <pre>{truncate(JSON.stringify(entry.response, null, 2))}</pre>
        </>
      )}
    </div>
  );
}

/* A table page or a busy track runs to megabytes; do not paint all of it. */
function truncate(text: string): string {
  const LIMIT = 20_000;
  if (text.length <= LIMIT) return text;
  return `${text.slice(0, LIMIT)}\n\n… ${(text.length - LIMIT).toLocaleString()} more characters`;
}
