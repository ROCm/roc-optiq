import type { TimeFormat } from './store'

const pad = (value: number, width = 2) => String(Math.floor(value)).padStart(width, '0')

// A time relative to the trace start, in the user's chosen format.
export function formatTime(ns: number, format: TimeFormat): string {
  if (!Number.isFinite(ns)) return '–'
  switch (format) {
    case 's':
      return `${(ns / 1e9).toFixed(9)} s`
    case 'ms':
      return `${(ns / 1e6).toFixed(6)} ms`
    case 'us':
      return `${(ns / 1e3).toFixed(3)} µs`
    case 'ns':
      return `${Math.round(ns).toLocaleString()} ns`
    default: {
      const full = format === 'timecode'
      const total = Math.trunc(Math.abs(ns))
      const seconds = Math.floor(total / 1e9)
      const [h, m, s] = [Math.floor(seconds / 3600), Math.floor(seconds / 60) % 60, seconds % 60]
      return (
        (ns < 0 && total > 0 ? '-' : '') +
        (full || h ? `${pad(h)}:` : '') +
        (full || h || m ? `${pad(m)}:` : '') +
        `${pad(s)}.${pad(total % 1e9, 9)}`
      )
    }
  }
}

const UNITS: [number, string][] = [
  [1e9, 's'],
  [1e6, 'ms'],
  [1e3, 'µs'],
  [1, 'ns'],
]

// A duration in the unit that suits its size, e.g. "12.4 ms".
export function formatDuration(ns: number | null | undefined): string {
  if (ns === null || ns === undefined || !Number.isFinite(ns)) return '–'
  const [scale, unit] = UNITS.find(([s]) => Math.abs(ns) >= s) ?? UNITS[3]
  const value = ns / scale
  const digits = scale === 1 || Math.abs(value) >= 100 ? 0 : Math.abs(value) >= 10 ? 1 : 2
  return `${value.toFixed(digits)} ${unit}`
}

// A ruler label, precise enough to tell ticks interval apart.
export function formatTick(ns: number, interval: number): string {
  const [scale, unit] = UNITS.find(([s]) => Math.max(Math.abs(ns), interval) >= s) ?? UNITS[3]
  const digits = Math.max(0, Math.min(9, Math.ceil(Math.log10(scale / interval) - 1e-9)))
  return `${(ns / scale).toFixed(digits)} ${unit}`
}

export function formatNumber(value: number | null | undefined, digits = 2): string {
  if (value === null || value === undefined || !Number.isFinite(value)) return '–'
  return value.toLocaleString(undefined, { maximumFractionDigits: digits })
}

export function formatBytes(bytes: number): string {
  const units = ['B', 'KB', 'MB', 'GB', 'TB']
  let value = bytes
  let unit = 0
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024
    unit++
  }
  return `${value.toFixed(unit ? 1 : 0)} ${units[unit]}`
}

// A spacing of 1, 2 or 5 times a power of ten that splits span into about count parts.
export function niceInterval(span: number, count: number): number {
  const raw = span / Math.max(1, count)
  const power = 10 ** Math.floor(Math.log10(raw))
  const step = [1, 2, 5, 10].find((s) => s * power >= raw) ?? 10
  return step * power
}

// Event colors, picked by name so an event looks the same everywhere.
const PALETTE = [
  '#ff6b6b', '#ff9f43', '#f7c548', '#5fd39a', '#2ec4b6', '#48b5ff',
  '#6c8cff', '#a78bfa', '#f472b6', '#e8515a', '#f6ad55', '#4fd1c5',
]

export function colorFor(name: string): string {
  let hash = 2166136261
  for (let i = 0; i < name.length; i++) hash = Math.imul(hash ^ name.charCodeAt(i), 16777619)
  return PALETTE[(hash >>> 0) % PALETTE.length]
}

export const fileName = (path: string) => path.split(/[\\/]/).pop() ?? path

export function downloadCsv(name: string, header: string[], rows: string[][]) {
  const cell = (text: string) => (/[",\n]/.test(text) ? `"${text.replace(/"/g, '""')}"` : text)
  const csv = [header, ...rows].map((row) => row.map(cell).join(',')).join('\n')
  const link = document.createElement('a')
  link.href = URL.createObjectURL(new Blob([csv], { type: 'text/csv' }))
  link.download = name
  link.click()
  URL.revokeObjectURL(link.href)
}
