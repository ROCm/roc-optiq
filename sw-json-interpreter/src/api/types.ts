/*
 * Wire types for the ROCm Optiq middleware protocol.
 *
 * These mirror src/middleware/README.md. Only the subset this proof of
 * concept uses is modelled; anything else stays as raw JSON.
 */

/* Ids past 2^53 arrive as decimal strings, so every id-shaped field is both. */
export type TraceId = number | string;

export interface MwError {
  code: string;
  message: string;
}

export interface MwResponse<T = unknown> {
  id?: unknown;
  method?: string;
  ok: boolean;
  result?: T;
  error?: MwError;
}

export type RequestStatus = 'pending' | 'ready' | 'error' | 'cancelled';

/* Every fetching method answers with this rather than the payload. */
export interface MwEnvelope<T = unknown> {
  request_id: number;
  status: RequestStatus;
  progress?: number;
  message?: string;
  result?: T;
  error_code?: string;
}

export interface SessionInfo {
  protocol_version: number;
  trace_state: string;
  methods: string[];
  capabilities: Record<string, boolean>;
}

export interface TraceStatus {
  state: 'empty' | 'loading' | 'ready' | 'error';
  paths: string[];
  pending_requests: number;
}

export interface Track {
  id: TraceId;
  type?: string;
  min_timestamp?: number;
  max_timestamp?: number;
  num_entries?: number;
  category?: string;
  main_name?: string;
  sub_name?: string;
  description?: string;
  min_value?: number;
  max_value?: number;
}

export interface TimelineInfo {
  min_timestamp: number;
  max_timestamp: number;
  num_tracks: number;
  tracks: Track[];
}

/* A track carries either events or samples, and the two decode differently. */
export interface TrackEvent {
  id: TraceId;
  start_timestamp: number;
  end_timestamp: number;
  level?: number;
  num_children?: number;
  name?: string;
  category?: string;
}

export interface TrackSample {
  id: TraceId;
  timestamp: number;
  end_timestamp?: number;
  value: number;
}

export interface TrackData {
  /* track.fetch names the first, graph.fetch the second. */
  track_type?: string;
  graph_type?: string;
  kind: 'events' | 'samples';
  entries: Array<TrackEvent | TrackSample>;
}

export interface TableColumn {
  name: string;
  type: string;
}

export interface TableData {
  columns: TableColumn[];
  total_rows: number;
  start_row: number;
  rows: unknown[][];
}

export function isSampleEntry(
  entry: TrackEvent | TrackSample,
): entry is TrackSample {
  return (entry as TrackSample).timestamp !== undefined;
}

/* Start and end for either entry kind, so the renderer can stay generic. */
export function entrySpan(entry: TrackEvent | TrackSample): [number, number] {
  if (isSampleEntry(entry)) {
    return [entry.timestamp, entry.end_timestamp ?? entry.timestamp];
  }
  return [entry.start_timestamp, entry.end_timestamp];
}
