/*
 * Drives the same call sequence the UI does, with the same parameter names,
 * against a running middleware server. Verifies the client's assumptions
 * about the wire format without needing a browser.
 *
 *   node scripts/smoke.mjs [baseUrl]
 */

const BASE = (process.argv[2] ?? 'http://127.0.0.1:8378').replace(/\/$/, '');
let nextId = 1;

async function request(method, params = {}) {
  const body = { id: nextId++, method, params };
  const response = await fetch(`${BASE}/rpc`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  return response.json();
}

async function call(method, params = {}) {
  const response = await request(method, params);
  if (!response.ok) {
    throw new Error(`${method}: ${response.error?.code} ${response.error?.message}`);
  }
  return response.result;
}

async function fetchAsync(method, params = {}) {
  let envelope = await call(method, params);
  while (envelope.status === 'pending') {
    await new Promise((resolve) => setTimeout(resolve, 150));
    envelope = await call('request.poll', {
      request_id: envelope.request_id,
      wait_ms: 500,
    });
  }
  if (envelope.status !== 'ready') {
    throw new Error(`${method} ended ${envelope.status}: ${envelope.error_code ?? ''}`);
  }
  return envelope.result;
}

function check(label, condition, detail = '') {
  const mark = condition ? 'PASS' : 'FAIL';
  console.log(`  [${mark}] ${label}${detail ? ` — ${detail}` : ''}`);
  if (!condition) process.exitCode = 1;
}

const tracePath = process.env.TRACE_PATH ?? 'sample/trace_70b_1024_32.rpd';

console.log(`sw-json-interpreter smoke test against ${BASE}\n`);

console.log('health');
const health = await fetch(`${BASE}/health`).then((r) => r.json());
check('GET /health answers ok', health.ok === true);

console.log('session.info');
const info = await call('session.info');
check('reports a protocol version', typeof info.protocol_version === 'number', `v${info.protocol_version}`);
check('lists methods', Array.isArray(info.methods) && info.methods.length > 0, `${info.methods.length} methods`);
check('reports capabilities', typeof info.capabilities === 'object');

console.log(`trace.open (${tracePath})`);
const opened = await fetchAsync('trace.open', { path: tracePath });
check('loads', opened.loaded === true);

console.log('timeline.info');
const timeline = await call('timeline.info');
check('has bounds', timeline.max_timestamp > timeline.min_timestamp);
check('has tracks', Array.isArray(timeline.tracks) && timeline.tracks.length > 0, `${timeline.num_tracks} tracks`);
const track = timeline.tracks.find((t) => (t.num_entries ?? 0) > 0);
check('a track carries entries', track !== undefined, track ? `${track.main_name} (${track.num_entries})` : '');

console.log('track.fetch');
const data = await fetchAsync('track.fetch', {
  track_id: track.id,
  start_time: timeline.min_timestamp,
  end_time: timeline.max_timestamp,
});
check('reports a kind', data.kind === 'events' || data.kind === 'samples', data.kind);
check('returns entries', Array.isArray(data.entries) && data.entries.length > 0, `${data.entries.length} entries`);
const entry = data.entries[0];
if (data.kind === 'events') {
  check('an event has a span', typeof entry.start_timestamp === 'number' && typeof entry.end_timestamp === 'number');
  check('an event has an id', entry.id !== undefined);
} else {
  check('a sample has a timestamp and value', typeof entry.timestamp === 'number' && typeof entry.value === 'number');
}

console.log('table.fetch');
const eventTracks = timeline.tracks.filter((t) => (t.num_entries ?? 0) > 0).slice(0, 4);
let table = null;
for (const candidate of eventTracks) {
  try {
    table = await fetchAsync('table.fetch', {
      table_type: 'events',
      track_ids: [Number(candidate.id)],
      start_row: 0,
      row_count: 25,
    });
    break;
  } catch {
    /* Not an event track; the next candidate may be one. */
  }
}
check('returns a schema', table !== null && Array.isArray(table.columns), table ? `${table.columns.length} columns` : 'no event track found');
check('rows are as wide as the schema', table !== null && table.rows.every((row) => row.length === table.columns.length));
check('reports a total', table !== null && typeof table.total_rows === 'number', table ? `${table.total_rows} rows` : '');

console.log('trace.status');
const status = await call('trace.status');
check('reads ready', status.state === 'ready', status.state);

console.log('error handling');
const bad = await request('no.such.method');
check('unknown method is reported, not thrown', bad.ok === false && bad.error.code === 'unknown_method');
const badTrack = await request('track.fetch', { track_id: 999999999 });
check('unknown track is reported', badTrack.ok === false, badTrack.error?.code);

console.log('trace.close');
const closed = await call('trace.close');
check('closes', closed.closed === true);

console.log(`\n${process.exitCode === 1 ? 'FAILURES' : 'All checks passed'}`);
