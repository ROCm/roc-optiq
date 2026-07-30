import { useCallback, useEffect, useLayoutEffect, useRef, useState } from 'react';
import type { Track, TrackData, TrackEvent, TrackSample } from '../api/types';
import { entrySpan, isSampleEntry } from '../api/types';
import {
  colorForName,
  formatDuration,
  formatTimestamp,
  trackLabel,
  trackSubLabel,
} from '../lib/format';

export interface TimeRange {
  start: number;
  end: number;
}

interface HitBox {
  x: number;
  y: number;
  w: number;
  h: number;
  track: Track;
  entry: TrackEvent | TrackSample;
}

interface Props {
  tracks: Track[];
  data: Map<string, TrackData>;
  view: TimeRange;
  bounds: TimeRange;
  onViewChange: (view: TimeRange) => void;
  onSelect: (track: Track, entry: TrackEvent | TrackSample) => void;
  selectedId: string | null;
}

const GUTTER = 158;
const RULER_H = 28;
const ROW_H = 17;
const TRACK_PAD = 5;
const SAMPLE_TRACK_H = 46;
const MIN_SPAN = 100;

export function Timeline({
  tracks,
  data,
  view,
  bounds,
  onViewChange,
  onSelect,
  selectedId,
}: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const wrapRef = useRef<HTMLDivElement>(null);
  const hitsRef = useRef<HitBox[]>([]);
  const dragRef = useRef<{ x: number; view: TimeRange } | null>(null);

  const [size, setSize] = useState({ w: 0, h: 0 });
  const [dragging, setDragging] = useState(false);
  const [hover, setHover] = useState<{ x: number; y: number; hit: HitBox } | null>(
    null,
  );

  useLayoutEffect(() => {
    const wrap = wrapRef.current;
    if (!wrap) return;
    const observer = new ResizeObserver(([entry]) => {
      const { width, height } = entry.contentRect;
      setSize({ w: width, h: height });
    });
    observer.observe(wrap);
    return () => observer.disconnect();
  }, []);

  /* Lay the scene out and paint it. Hit boxes fall out of the same pass. */
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || size.w === 0 || size.h === 0) return;

    const dpr = window.devicePixelRatio || 1;
    canvas.width = Math.floor(size.w * dpr);
    canvas.height = Math.floor(size.h * dpr);
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    const hits: HitBox[] = [];
    const plotW = size.w - GUTTER;
    const span = Math.max(view.end - view.start, 1);
    const toX = (t: number) => GUTTER + ((t - view.start) / span) * plotW;

    ctx.fillStyle = '#0d1117';
    ctx.fillRect(0, 0, size.w, size.h);

    drawRuler(ctx, size.w, view, bounds.start, plotW);

    let y = RULER_H;
    for (const track of tracks) {
      const key = String(track.id);
      const entries = data.get(key);
      const height = trackHeight(entries);

      if (y > size.h) break;

      /* Alternating bands keep long track lists readable. */
      ctx.fillStyle = '#111721';
      ctx.fillRect(0, y, size.w, height);
      ctx.strokeStyle = '#1c2531';
      ctx.beginPath();
      ctx.moveTo(0, y + height + 0.5);
      ctx.lineTo(size.w, y + height + 0.5);
      ctx.stroke();

      drawGutterLabel(ctx, track, y, height, entries);

      if (entries) {
        if (entries.kind === 'samples') {
          drawSamples(ctx, entries, track, y, height, toX, plotW);
        } else {
          drawEvents(ctx, entries, track, y, toX, hits, selectedId);
        }
      } else {
        ctx.fillStyle = '#3f4b5b';
        ctx.font = '11px system-ui, sans-serif';
        ctx.fillText('loading…', GUTTER + 10, y + height / 2 + 4);
      }

      y += height;
    }

    /* Gutter divider, drawn last so track bands cannot paint over it. */
    ctx.fillStyle = '#151b23';
    ctx.fillRect(0, RULER_H, 1, size.h - RULER_H);
    ctx.strokeStyle = '#2a3441';
    ctx.beginPath();
    ctx.moveTo(GUTTER + 0.5, 0);
    ctx.lineTo(GUTTER + 0.5, size.h);
    ctx.stroke();

    hitsRef.current = hits;
  }, [tracks, data, view, bounds, size, selectedId]);

  const clampView = useCallback(
    (next: TimeRange): TimeRange => {
      const span = Math.min(
        Math.max(next.end - next.start, MIN_SPAN),
        bounds.end - bounds.start,
      );
      let start = next.start;
      if (start < bounds.start) start = bounds.start;
      if (start + span > bounds.end) start = bounds.end - span;
      return { start, end: start + span };
    },
    [bounds],
  );

  const handleWheel = useCallback(
    (event: React.WheelEvent<HTMLCanvasElement>) => {
      const canvas = canvasRef.current;
      if (!canvas) return;
      const rect = canvas.getBoundingClientRect();
      const px = event.clientX - rect.left;
      if (px < GUTTER) return;

      const plotW = size.w - GUTTER;
      const ratio = (px - GUTTER) / Math.max(plotW, 1);
      const span = view.end - view.start;
      /* Zoom about the cursor, so the point under it stays put. */
      const factor = event.deltaY > 0 ? 1.25 : 0.8;
      const nextSpan = span * factor;
      const anchor = view.start + ratio * span;
      onViewChange(
        clampView({
          start: anchor - ratio * nextSpan,
          end: anchor + (1 - ratio) * nextSpan,
        }),
      );
    },
    [view, size, clampView, onViewChange],
  );

  const handleMouseDown = (event: React.MouseEvent<HTMLCanvasElement>) => {
    const rect = event.currentTarget.getBoundingClientRect();
    if (event.clientX - rect.left < GUTTER) return;
    dragRef.current = { x: event.clientX, view };
    setDragging(true);
  };

  const handleMouseMove = (event: React.MouseEvent<HTMLCanvasElement>) => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const px = event.clientX - rect.left;
    const py = event.clientY - rect.top;

    const drag = dragRef.current;
    if (drag) {
      const plotW = size.w - GUTTER;
      const span = drag.view.end - drag.view.start;
      const shift = ((drag.x - event.clientX) / Math.max(plotW, 1)) * span;
      onViewChange(
        clampView({ start: drag.view.start + shift, end: drag.view.end + shift }),
      );
      return;
    }

    const hit = hitsRef.current.find(
      (box) => px >= box.x && px <= box.x + box.w && py >= box.y && py <= box.y + box.h,
    );
    setHover(hit ? { x: px, y: py, hit } : null);
  };

  const endDrag = () => {
    dragRef.current = null;
    setDragging(false);
  };

  const handleClick = (event: React.MouseEvent<HTMLCanvasElement>) => {
    const rect = event.currentTarget.getBoundingClientRect();
    const px = event.clientX - rect.left;
    const py = event.clientY - rect.top;
    const hit = hitsRef.current.find(
      (box) => px >= box.x && px <= box.x + box.w && py >= box.y && py <= box.y + box.h,
    );
    if (hit) onSelect(hit.track, hit.entry);
  };

  return (
    <div className="timeline-wrap" ref={wrapRef}>
      <canvas
        ref={canvasRef}
        className={dragging ? 'dragging' : ''}
        onWheel={handleWheel}
        onMouseDown={handleMouseDown}
        onMouseMove={handleMouseMove}
        onMouseUp={endDrag}
        onMouseLeave={() => {
          endDrag();
          setHover(null);
        }}
        onClick={handleClick}
      />
      {hover && <Tooltip hover={hover} origin={bounds.start} width={size.w} />}
    </div>
  );
}

function Tooltip({
  hover,
  origin,
  width,
}: {
  hover: { x: number; y: number; hit: HitBox };
  origin: number;
  width: number;
}) {
  const { hit } = hover;
  const [start, end] = entrySpan(hit.entry);
  const name = isSampleEntry(hit.entry)
    ? `${trackLabel(hit.track)} = ${hit.entry.value}`
    : (hit.entry.name ?? '(unnamed)');

  /* Flip to the left of the cursor near the right edge. */
  const flip = hover.x > width - 360;
  const style = {
    left: flip ? undefined : hover.x + 14,
    right: flip ? width - hover.x + 14 : undefined,
    top: hover.y + 16,
  };

  return (
    <div className="tooltip" style={style}>
      <div className="t-name">{name}</div>
      <div className="t-meta">
        {formatDuration(end - start)} @ {formatTimestamp(start, origin)}
      </div>
    </div>
  );
}

function trackHeight(entries: TrackData | undefined): number {
  if (!entries) return ROW_H + TRACK_PAD * 2;
  if (entries.kind === 'samples') return SAMPLE_TRACK_H;
  let levels = 1;
  for (const entry of entries.entries) {
    const level = (entry as TrackEvent).level ?? 0;
    if (level + 1 > levels) levels = level + 1;
  }
  return Math.min(levels, 12) * ROW_H + TRACK_PAD * 2;
}

function drawGutterLabel(
  ctx: CanvasRenderingContext2D,
  track: Track,
  y: number,
  height: number,
  entries: TrackData | undefined,
): void {
  ctx.save();
  ctx.beginPath();
  ctx.rect(0, y, GUTTER - 6, height);
  ctx.clip();

  ctx.fillStyle = '#e4e9f0';
  ctx.font = '11.5px system-ui, sans-serif';
  ctx.fillText(trackLabel(track), 10, y + 15);

  /* A one-level band has no room for a second line without spilling over. */
  const sub = trackSubLabel(track) ?? entries?.kind;
  if (sub && height >= 34) {
    ctx.fillStyle = '#5d6b7d';
    ctx.font = '10px system-ui, sans-serif';
    ctx.fillText(sub, 10, y + 28);
  }
  ctx.restore();
}

function drawEvents(
  ctx: CanvasRenderingContext2D,
  data: TrackData,
  track: Track,
  top: number,
  toX: (t: number) => number,
  hits: HitBox[],
  selectedId: string | null,
): void {
  ctx.save();
  ctx.beginPath();
  ctx.rect(GUTTER + 1, top, 1e6, 1e6);
  ctx.clip();
  ctx.font = '10.5px system-ui, sans-serif';
  ctx.textBaseline = 'middle';

  for (const raw of data.entries) {
    const entry = raw as TrackEvent;
    const x0 = toX(entry.start_timestamp);
    const x1 = toX(entry.end_timestamp);
    if (x1 < GUTTER || x0 > 1e5) continue;

    const level = Math.min(entry.level ?? 0, 11);
    const y = top + TRACK_PAD + level * ROW_H;
    const w = Math.max(x1 - x0, 1);
    const h = ROW_H - 2;

    const name = entry.name ?? '';
    const selected = selectedId === String(entry.id);
    ctx.fillStyle = selected ? '#ffffff' : colorForName(name);
    ctx.fillRect(x0, y, w, h);

    /* Only label a box wide enough to hold something legible. */
    if (w > 26) {
      ctx.save();
      ctx.beginPath();
      ctx.rect(x0 + 2, y, w - 4, h);
      ctx.clip();
      ctx.fillStyle = selected ? '#0d1117' : 'rgba(6, 10, 16, 0.86)';
      ctx.fillText(name, x0 + 4, y + h / 2);
      ctx.restore();
    }

    /* Hit boxes only pay off for targets a pointer can actually land on. */
    if (w >= 2) {
      hits.push({ x: x0, y, w, h, track, entry });
    }
  }
  ctx.restore();
}

function drawSamples(
  ctx: CanvasRenderingContext2D,
  data: TrackData,
  track: Track,
  top: number,
  height: number,
  toX: (t: number) => number,
  plotW: number,
): void {
  const samples = data.entries as TrackSample[];
  if (samples.length === 0) return;

  let min = track.min_value ?? Infinity;
  let max = track.max_value ?? -Infinity;
  if (!isFinite(min) || !isFinite(max)) {
    min = Infinity;
    max = -Infinity;
    for (const sample of samples) {
      if (sample.value < min) min = sample.value;
      if (sample.value > max) max = sample.value;
    }
  }
  const range = max - min || 1;

  ctx.save();
  ctx.beginPath();
  ctx.rect(GUTTER + 1, top, plotW, height);
  ctx.clip();

  const base = top + height - 4;
  const usable = height - 12;
  ctx.beginPath();
  ctx.moveTo(toX(samples[0].timestamp), base);
  for (const sample of samples) {
    const x = toX(sample.timestamp);
    const y = base - ((sample.value - min) / range) * usable;
    ctx.lineTo(x, y);
  }
  ctx.lineTo(toX(samples[samples.length - 1].timestamp), base);
  ctx.closePath();
  ctx.fillStyle = 'rgba(88, 166, 255, 0.22)';
  ctx.fill();

  ctx.beginPath();
  samples.forEach((sample, index) => {
    const x = toX(sample.timestamp);
    const y = base - ((sample.value - min) / range) * usable;
    if (index === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.strokeStyle = '#58a6ff';
  ctx.lineWidth = 1.2;
  ctx.stroke();
  ctx.restore();
}

function drawRuler(
  ctx: CanvasRenderingContext2D,
  width: number,
  view: TimeRange,
  origin: number,
  plotW: number,
): void {
  ctx.fillStyle = '#151b23';
  ctx.fillRect(0, 0, width, RULER_H);
  ctx.strokeStyle = '#2a3441';
  ctx.beginPath();
  ctx.moveTo(0, RULER_H + 0.5);
  ctx.lineTo(width, RULER_H + 0.5);
  ctx.stroke();

  const span = view.end - view.start;
  const target = 110;
  const rawStep = (span / plotW) * target;
  const step = niceStep(rawStep);
  const first = Math.ceil(view.start / step) * step;

  ctx.font = '10px ui-monospace, monospace';
  ctx.textBaseline = 'alphabetic';

  for (let t = first; t <= view.end; t += step) {
    const x = GUTTER + ((t - view.start) / span) * plotW;
    if (x < GUTTER) continue;
    ctx.strokeStyle = '#232c38';
    ctx.beginPath();
    ctx.moveTo(Math.floor(x) + 0.5, RULER_H);
    ctx.lineTo(Math.floor(x) + 0.5, 1e5);
    ctx.stroke();

    ctx.strokeStyle = '#3b4756';
    ctx.beginPath();
    ctx.moveTo(Math.floor(x) + 0.5, RULER_H - 6);
    ctx.lineTo(Math.floor(x) + 0.5, RULER_H);
    ctx.stroke();

    ctx.fillStyle = '#8b98a9';
    ctx.fillText(formatTimestamp(t, origin), x + 4, 17);
  }
}

/* Round a step up to the nearest 1, 2, or 5 times a power of ten. */
function niceStep(raw: number): number {
  const power = Math.pow(10, Math.floor(Math.log10(Math.max(raw, 1))));
  const scaled = raw / power;
  if (scaled <= 1) return power;
  if (scaled <= 2) return 2 * power;
  if (scaled <= 5) return 5 * power;
  return 10 * power;
}
