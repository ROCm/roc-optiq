import { useState } from 'react';
import type { TableData, Track } from '../api/types';
import { formatCount } from '../lib/format';

interface Props {
  tracks: Track[];
  selectedTrackIds: string[];
  table: TableData | null;
  busy: boolean;
  error: string | null;
  onFetch: (tableType: string, startRow: number, rowCount: number) => void;
}

/* Only the track-scoped tables; the rest need an operation-type selection. */
const TABLE_TYPES = [
  'events',
  'instrumented_events',
  'dispatch_events',
  'memory_allocation_events',
  'memory_copy_events',
  'sampled_events',
  'samples',
];

const PAGE_SIZE = 100;

export function TablePanel({
  selectedTrackIds,
  table,
  busy,
  error,
  onFetch,
}: Props) {
  const [tableType, setTableType] = useState('events');
  const [startRow, setStartRow] = useState(0);

  const go = (next: number) => {
    setStartRow(next);
    onFetch(tableType, next, PAGE_SIZE);
  };

  const hasSelection = selectedTrackIds.length > 0;
  const total = table?.total_rows ?? 0;
  const canPrev = startRow > 0;
  const canNext = table !== null && startRow + PAGE_SIZE < total;

  return (
    <>
      <div className="timeline-bar">
        <span>Table</span>
        <select
          value={tableType}
          onChange={(event) => setTableType(event.target.value)}
        >
          {TABLE_TYPES.map((type) => (
            <option key={type} value={type}>
              {type}
            </option>
          ))}
        </select>
        <button
          className="primary"
          onClick={() => go(0)}
          disabled={busy || !hasSelection}
        >
          {busy ? 'Fetching…' : 'Fetch'}
        </button>
        <span className="spacer" />
        {table && (
          <>
            <span>
              rows {startRow.toLocaleString()}–
              {Math.min(startRow + PAGE_SIZE, total).toLocaleString()} of{' '}
              {formatCount(total)}
            </span>
            <button onClick={() => go(Math.max(0, startRow - PAGE_SIZE))} disabled={!canPrev || busy}>
              Prev
            </button>
            <button onClick={() => go(startRow + PAGE_SIZE)} disabled={!canNext || busy}>
              Next
            </button>
          </>
        )}
      </div>

      {error && <div className="banner error">{error}</div>}

      {!hasSelection ? (
        <div className="empty">
          <h3>Pick a track first</h3>
          <p>
            Every table is scoped by a selection, and the middleware rejects an
            empty one rather than letting the query builder read past the end
            of it. Tick a track in the sidebar.
          </p>
        </div>
      ) : !table ? (
        <div className="empty">
          <h3>No rows loaded</h3>
          <p>
            Choose a table type and fetch. The result carries its schema and
            rows separately, so the columns below come from the response.
          </p>
        </div>
      ) : (
        <div className="table-scroll">
          <table>
            <thead>
              <tr>
                <th style={{ width: 60 }}>#</th>
                {table.columns.map((column) => (
                  <th key={column.name}>
                    {column.name}
                    <span style={{ color: 'var(--text-faint)', fontWeight: 400 }}>
                      {' '}
                      {column.type}
                    </span>
                  </th>
                ))}
              </tr>
            </thead>
            <tbody>
              {table.rows.map((row, index) => (
                <tr key={index}>
                  <td style={{ color: 'var(--text-faint)' }}>
                    {table.start_row + index}
                  </td>
                  {row.map((cell, cellIndex) => (
                    <td key={cellIndex} title={String(cell ?? '')}>
                      {cell === null || cell === undefined ? '—' : String(cell)}
                    </td>
                  ))}
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </>
  );
}
