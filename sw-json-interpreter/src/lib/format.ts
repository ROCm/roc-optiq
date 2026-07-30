/* Timestamps are in the trace's native units, which for RPD traces are ns. */
export function formatDuration(ns: number): string {
  const abs = Math.abs(ns);
  if (abs < 1_000) return `${round(ns, 0)} ns`;
  if (abs < 1_000_000) return `${round(ns / 1_000, 2)} us`;
  if (abs < 1_000_000_000) return `${round(ns / 1_000_000, 2)} ms`;
  return `${round(ns / 1_000_000_000, 3)} s`;
}

export function formatTimestamp(ns: number, origin: number): string {
  return formatDuration(ns - origin);
}

function round(value: number, digits: number): string {
  return value.toFixed(digits).replace(/\.?0+$/, '');
}

export function formatCount(value: number): string {
  if (value < 1_000) return String(value);
  if (value < 1_000_000) return `${(value / 1_000).toFixed(1)}k`;
  return `${(value / 1_000_000).toFixed(1)}M`;
}

/*
 * Stable colour per name, so the same kernel keeps its colour across tracks
 * and across reloads. Hue only, to keep the palette coherent.
 */
export function colorForName(name: string): string {
  let hash = 0;
  for (let i = 0; i < name.length; i++) {
    hash = (hash * 31 + name.charCodeAt(i)) | 0;
  }
  const hue = Math.abs(hash) % 360;
  return `hsl(${hue} 55% 52%)`;
}

/* Ids can arrive as decimal strings, and must be echoed back as they came. */
export function idToString(id: number | string): string {
  return typeof id === 'string' ? id : String(id);
}

interface Labelled {
  id: number | string;
  main_name?: string;
  sub_name?: string;
  category?: string;
}

/*
 * Which field actually names a track varies by trace: RPD system traces leave
 * main_name empty and put "Thread 592" in sub_name with "CPU Thread" as the
 * category, so falling back through them beats showing a bare id.
 */
export function trackLabel(track: Labelled): string {
  return (
    track.main_name || track.sub_name || track.category || `track ${track.id}`
  );
}

export function trackSubLabel(track: Labelled): string | undefined {
  const label = trackLabel(track);
  return [track.sub_name, track.category].find(
    (candidate) => candidate && candidate !== label,
  );
}
