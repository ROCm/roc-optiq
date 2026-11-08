// Ask Optiq's tools, run in the browser against the server API. Names,
// arguments and output shape follow the desktop assistant, whose tool schema
// and instructions the server sends with every round.

import { get, type Aggregate, type EventDetails, type Minimap, type Statistics, type Table, type Track } from '../api'
import { formatDuration } from '../format'
import { useStore } from '../store'
import { clampView } from '../system/Timeline'

type Args = Record<string, unknown>

export interface OverviewChart {
  bins: number[]
  tracks: { name: string; bins: number[] }[]
}

export interface ToolResult {
  content: string
  chart?: OverviewChart
  nextSteps?: string[]
}

const TOP_TABLES: Record<string, [string, string]> = {
  dispatch: ['top_dispatch_events_table', 'dispatch'],
  memory_copy: ['top_memory_copy_events_table', 'memory_copy'],
  memory_alloc: ['top_memory_allocation_events_table', 'memory_allocate'],
  instrumented: ['top_instrumented_events_table', 'launch'],
  sampled: ['top_sampled_events_table', 'launch_sample'],
}
const SUMMARY_TABLES: Record<string, string> = {
  kernel: 'summary_kernel_table',
  memory: 'summary_memory_table',
  thread: 'summary_thread_table',
}
const OPERATION_IDS: Record<string, string> = { instrumented: '1', dispatch: '2', memory_alloc: '3', memory_copy: '4', sampled: '5' }
const SCOPE_PARAMS: Record<string, string> = { node: 'node', processor: 'agent', process: 'process', thread: 'thread' }
const COLUMNS = (
  'name category duration start end id __uuid PID TID queue stream node size address SrcAddr value counter ' +
  'arguments GridSizeX GridSizeY GridSizeZ WGSizeX WGSizeY WGSizeZ LDSSize ScratchSize StaticLDSSize ' +
  'StaticScratchSize AgentAbsoluteIndex AgentType AgentTypeIndex AgentName SrcAgentAbsoluteIndex SrcAgentType ' +
  'SrcAgentTypeIndex SrcAgentName __trackId __streamTrackId'
).split(' ')
const MAX_ROWS = 200
const OVERVIEW_TRACKS = 8

// Per-trace caches for what costs a round trip and never changes.
let cache: { path: string; minimap?: Promise<Minimap>; summary?: Promise<{ root: Aggregate }> } = { path: '' }
function cached<K extends 'minimap' | 'summary'>(key: K, load: () => NonNullable<(typeof cache)[K]>) {
  const path = useStore.getState().status?.trace_file_path ?? ''
  if (cache.path !== path) cache = { path }
  return (cache[key] ??= load()) as NonNullable<(typeof cache)[K]>
}

const state = () => useStore.getState()
const timeline = () => state().timeline!
const ns = (value: number) => formatDuration(value)
const number = (value: unknown) => (typeof value === 'number' ? value : typeof value === 'string' && value.trim() ? Number(value) : NaN)
const text = (value: unknown) => (typeof value === 'string' ? value : value === undefined || value === null ? '' : String(value))

// The window a query reads: the arguments, else the selected range, else the trace.
function window(args: Args) {
  const { range } = state()
  const t = timeline()
  const start = number(args.start_ns)
  const end = number(args.end_ns)
  return {
    start: Number.isFinite(start) ? start : range?.start ?? t.start_ns,
    end: Number.isFinite(end) ? end : range?.end ?? t.end_ns,
  }
}

function trackIds(args: Args, fallback: (track: Track) => boolean): string {
  const asked = Array.isArray(args.track_ids) ? args.track_ids.map(String) : []
  const known = new Set(timeline().tracks.map((t) => t.id))
  const ids = asked.length ? asked.filter((id) => known.has(id)) : timeline().tracks.filter(fallback).map((t) => t.id)
  if (asked.length && !ids.length) throw new Error(`None of track_ids ${asked.join(', ')} exist. Call list_tracks.`)
  return ids.join(',')
}

const quote = (value: string) => `'${value.replace(/'/g, "''")}'`
const likeEscape = (value: string) => value.replace(/[\\%_]/g, (c) => `\\${c}`)

function column(name: unknown, what: string): string {
  const value = text(name)
  if (!COLUMNS.includes(value)) throw new Error(`Unknown ${what} column "${value}". Use one of: ${COLUMNS.join(', ')}.`)
  return value
}

// The "filters" argument as a table filter, with every column and operator
// checked against a whitelist and every value quoted.
function filter(args: Args): string {
  if (args.filters === undefined) return ''
  if (!Array.isArray(args.filters)) throw new Error('filters must be an array of {column, op, value}.')
  if (args.filters.length > 8) throw new Error('At most 8 filters.')
  return args.filters
    .map((entry: Record<string, unknown>) => {
      const name = column(entry.column, 'filter')
      const op = text(entry.op) || '='
      if (entry.value === undefined) throw new Error(`Filter on "${name}" is missing value.`)
      if (op === 'contains' || op === 'starts_with') {
        return `${name} LIKE ${quote(`${op === 'contains' ? '%' : ''}${likeEscape(text(entry.value))}%`)} ESCAPE '\\'`
      }
      if (!['=', '!=', '<', '<=', '>', '>='].includes(op)) throw new Error('Unknown filter op. Use =, !=, <, <=, >, >=, contains, or starts_with.')
      return `${name} ${op} ${typeof entry.value === 'number' ? entry.value : quote(text(entry.value))}`
    })
    .join(' AND ')
}

// Named-value rows, internal columns hidden except the two goto needs.
function formatTable(table: Table, limit: number): string {
  if (table.error) return `error: ${table.error}`
  if (!table.columns?.length) return 'No rows returned.'
  const shown = table.columns.map((c, i) => ({ ...c, i })).filter((c) => !c.name.startsWith('__') || c.name === '__uuid' || c.name === '__trackId')
  const lines = [`columns: ${shown.map((c) => c.name).join(' ')}`]
  table.rows.slice(0, limit).forEach((row, r) => {
    lines.push(`${r + 1}. ${shown.map((c) => `${c.name}=${c.type === 'string' || c.name === '__uuid' ? `"${row[c.i] ?? ''}"` : row[c.i] ?? ''}`).join(' ')}`)
  })
  const rest = table.total_row_count - Math.min(limit, table.rows.length)
  if (rest > 0) lines.push(`... ${rest} more rows not shown`)
  return lines.join('\n')
}

// Fetches a table page, resolving sort_by to a column of that table first.
async function query(name: string, params: Record<string, string | number | undefined>, args: Args, defaultSort?: string) {
  const limit = Math.min(Math.max(1, number(args.limit) || 20), MAX_ROWS)
  const order = args.sort_order === 'asc' || args.sort_order === 'desc' ? args.sort_order : 'desc'
  const base = { ...params, filter: filter(args), group: args.group_by ? column(args.group_by, 'group_by') : undefined }
  let sort: number | undefined
  const sortBy = text(args.sort_by) || defaultSort
  if (sortBy) {
    const header = await get<Table>(`/api/tables/${name}`, { ...base, limit: 1 })
    const index = header.columns?.findIndex((c) => c.name.toLowerCase() === sortBy.toLowerCase()) ?? -1
    if (index < 0 && args.sort_by) return `sort_by "${sortBy}" is not a column here. Columns: ${(header.columns ?? []).map((c) => c.name).join(', ')}`
    sort = index >= 0 ? index : undefined
  }
  const table = await get<Table>(`/api/tables/${name}`, { ...base, limit, offset: number(args.offset) || 0, sort, order })
  return formatTable(table, limit)
}

function summaryScope(root: Aggregate, args: Args): Aggregate {
  const level = text(args.level)
  if (!level || level === 'trace') return root
  const id = text(args.id)
  const find = (a: Aggregate): Aggregate | null => (a.level === level && a.id === id ? a : a.children.map(find).find(Boolean) ?? null)
  const found = find(root)
  if (!found) throw new Error(`No ${level} with id ${id} in the summary. Use a level and id from a sub_metrics list.`)
  return found
}

function scopeLabel(scope: Aggregate) {
  const names = new Map<string, string>()
  const visit = (node: { type: string; id: string; name: string; header: string; host_name?: string; product_name?: string; label?: string | null; processors?: never[]; processes?: never[]; threads?: never[] }) => {
    names.set(`${node.type}${node.id}`, node.type === 'processor' ? `${node.label ?? ''} ${node.product_name ?? node.name}`.trim() : node.host_name || node.header || node.name)
    ;[...(node.processors ?? []), ...(node.processes ?? []), ...(node.threads ?? [])].forEach(visit)
  }
  ;(state().topology?.nodes ?? []).forEach((n) => visit(n as never))
  return names.get(`${scope.level}${scope.id}`) ?? scope.id ?? 'trace'
}

const categoryTotal = (scope: Aggregate, category: 'kernel' | 'memory' | 'thread') => scope.top_events[category]?.duration_sum_ns ?? 0

async function getSummary(args: Args): Promise<ToolResult> {
  const summary = await cached('summary', () => get<{ root: Aggregate }>('/api/summary'))
  const scope = summaryScope(summary.root, args)
  const lines = [`scope: level=${scope.level}${scope.id ? ` id=${scope.id} label="${scopeLabel(scope)}"` : ''}`]
  for (const category of ['kernel', 'memory', 'thread'] as const) {
    const events = scope.top_events[category]
    if (!events?.duration_sum_ns) continue
    lines.push(`${category}: total_ns=${ns(events.duration_sum_ns)} invocations=${events.invocations}`)
    events.top.forEach((e, i) =>
      lines.push(`  ${i + 1}. name="${e.name}" share_pct=${(e.duration_pct * 100).toFixed(1)} invocations=${e.invocations} total_ns=${ns(e.duration_sum_ns)}`),
    )
  }
  lines.push(`counters: ${scope.counters.length}${scope.counters.length ? ' (read them with get_summary_counter_stats)' : ''}`)
  if (scope.children.length) {
    lines.push('sub_metrics:')
    for (const sub of scope.children) {
      lines.push(
        `  level=${sub.level} id=${sub.id} label="${scopeLabel(sub)}" kernels_ns=${categoryTotal(sub, 'kernel')} memory_ns=${categoryTotal(sub, 'memory')} thread_ns=${categoryTotal(sub, 'thread')} counters=${sub.counters.length}`,
      )
    }
    lines.push('note: the values above cover this scope only. Call get_summary with a sub_metrics level and id to read one of them.')
  }
  return { content: lines.join('\n') }
}

async function summaryEventMetrics(args: Args): Promise<ToolResult> {
  const summary = await cached('summary', () => get<{ root: Aggregate }>('/api/summary'))
  const scope = summaryScope(summary.root, args)
  const category = (text(args.category) || 'kernel') as 'kernel' | 'memory' | 'thread'
  const top = scope.top_events[category]?.top ?? []
  const name = text(args.name).toLowerCase()
  const matches = name ? top.filter((e) => e.name.toLowerCase() === name || e.name.toLowerCase().includes(name)) : top
  const exact = matches.filter((e) => e.name.toLowerCase() === name)
  const chosen = exact.length ? exact : matches
  if (!chosen.length) return { content: `No ${category} named "${args.name}" in this scope's summary. Names kept: ${top.map((e) => e.name).join(' | ')}` }
  return {
    content: chosen
      .map((e) => `name="${e.name}" invocations=${e.invocations} total_ns=${ns(e.duration_sum_ns)} avg_ns=${ns(e.duration_avg_ns)} min_ns=${ns(e.duration_min_ns)} max_ns=${ns(e.duration_max_ns)} share_pct=${(e.duration_pct * 100).toFixed(1)}`)
      .join('\n'),
  }
}

async function counterStats(args: Args): Promise<ToolResult> {
  const summary = await cached('summary', () => get<{ root: Aggregate }>('/api/summary'))
  const scope = summaryScope(summary.root, args)
  const name = text(args.name).toLowerCase()
  const all = scope.counters.filter((c) => !name || c.name.toLowerCase().includes(name))
  const offset = number(args.offset) || 0
  const limit = Math.min(number(args.limit) || 20, MAX_ROWS)
  const page = all.slice(offset, offset + limit)
  if (!page.length) return { content: name ? `No counter matches "${args.name}" at this scope.` : 'No counters at this scope.' }
  const lines = page.map((c) => `${c.name}${c.units ? ` [${c.units}]` : ''} class=${c.device} mean=${c.mean} min=${c.min} max=${c.max} std_dev=${c.std_dev}`)
  if (all.length > offset + page.length) lines.push(`showing ${offset + 1}-${offset + page.length} of ${all.length}; page on with offset`)
  return { content: lines.join('\n') }
}

// Sums equal-width bins into fewer.
function downsample(values: number[], bins: number) {
  if (!values.length) return []
  return Array.from({ length: bins }, (_, i) => {
    const from = Math.floor((i * values.length) / bins)
    const to = Math.max(from + 1, Math.floor(((i + 1) * values.length) / bins))
    return values.slice(from, to).reduce((s, v) => s + v, 0)
  })
}

async function traceOverview(args: Args): Promise<ToolResult> {
  const minimap = await cached('minimap', () => get<Minimap>('/api/minimap'))
  const t = timeline()
  const span = t.end_ns - t.start_ns
  const bins = Math.min(Math.max(8, number(args.bins) || 32), 128)
  const byId = new Map(t.tracks.map((track) => [track.id, track]))
  const scale = (row: number[]) => {
    const max = Math.max(1e-12, ...row)
    return row.map((v) => Math.round((100 * v) / max))
  }
  const windowOf = (index: number) => {
    const width = span / bins
    return `${Math.round(t.start_ns + index * width)}-${Math.round(t.start_ns + (index + 1) * width)}`
  }
  const lines = [`trace_window_ns: ${t.start_ns}-${t.end_ns} (${ns(span)})`, `bins: ${bins}`, `bin_width_ns: ${ns(span / bins)}`]
  if (args.track_id !== undefined) {
    const track = byId.get(text(args.track_id))
    if (!track) throw new Error(`No track ${args.track_id}. Call list_tracks.`)
    const row = downsample(minimap.tracks[track.id] ?? [], bins)
    const peak = row.indexOf(Math.max(...row))
    lines.push(`track: id=${track.id} name="${track.name}" events=${track.entry_count}`, `activity_0_100: ${scale(row).join(' ')}`, `busiest_window_ns: ${windowOf(peak)}`)
    return { content: lines.join('\n') }
  }
  const eventTracks = t.tracks.filter((track) => track.data_type === 'events')
  const ranked = eventTracks.filter((track) => track.kind !== 'stream').sort((a, b) => b.entry_count - a.entry_count)
  const total = ranked.reduce((s, track) => s + track.entry_count, 0)
  let combined: number[] = []
  for (const track of eventTracks) combined = (minimap.tracks[track.id] ?? []).map((v, i) => (combined[i] ?? 0) + v)
  const row = downsample(combined, bins)
  const peak = row.indexOf(Math.max(...row))
  const quiet = row.indexOf(Math.min(...row))
  lines.push(
    `activity_0_100: ${scale(row).join(' ')}`,
    `busiest_window_ns: ${windowOf(peak)}`,
    `quietest_window_ns: ${windowOf(quiet)}`,
    'note: activity_0_100 and both windows are estimates from the timeline histogram. Use them to choose where to look; total_events and busiest_tracks are exact.',
    `total_events: ${total}`,
  )
  const streams = eventTracks.filter((track) => track.kind === 'stream')
  if (streams.length) lines.push(`stream_tracks: ${streams.length} tracks. They repeat the queue tracks' kernels and copies grouped by HIP stream, so total_events and busiest_tracks leave them out.`)
  lines.push('busiest_tracks:')
  ranked.slice(0, OVERVIEW_TRACKS).forEach((track, i) =>
    lines.push(`  ${i + 1}. track_id=${track.id} events=${track.entry_count} share_pct=${total ? ((100 * track.entry_count) / total).toFixed(1) : 0} name="${track.name}"`),
  )
  const chart: OverviewChart = {
    bins: scale(downsample(combined, 48)),
    tracks: ranked.slice(0, 5).map((track) => ({ name: track.name, bins: scale(downsample(minimap.tracks[track.id] ?? [], 48)) })),
  }
  return { content: lines.join('\n'), chart }
}

async function listTracks(): Promise<ToolResult> {
  return {
    content: timeline()
      .tracks.map((track) => `track_id=${track.id} type=${track.data_type === 'samples' ? 'Counter' : track.kind} name="${track.name}" owner="${track.main_name}" entries=${track.entry_count}${track.operations.length ? ` events=${track.operations.join('|')}` : ''}`)
      .join('\n'),
  }
}

async function eventDetails(args: Args): Promise<ToolResult> {
  const uuid = text(args.event_uuid)
  if (!/^\d+$/.test(uuid)) throw new Error('event_uuid must be the __uuid digits as a quoted string.')
  const track = text(args.track_id) || timeline().tracks[0].id
  const d = await get<EventDetails>(`/api/events/${uuid}`, { track })
  const lines = [`name="${d.name}" op=${d.op} track_id=${d.track_id} start_ns=${d.start_ns} duration_ns=${d.duration_ns} (${ns(d.duration_ns)}) level=${d.level}`]
  if (d.args.length) lines.push('arguments:', ...d.args.map((a) => `  ${a.name}=${a.value} (${a.data_type})`))
  if (d.ext.length) lines.push('properties:', ...d.ext.map((e) => `  ${e.name}=${e.value}`))
  if (d.flows.length) lines.push('flows:', ...d.flows.map((f) => `  __uuid="${f.uuid}" __trackId=${f.track_id} name="${f.name}" start_ns=${f.start_ns} end_ns=${f.end_ns}`))
  if (d.call_stack.length) lines.push('call_stack:', ...d.call_stack.map((f, i) => `  ${i}. ${f.name}${f.file ? ` ${f.file}` : ''}`))
  return { content: lines.join('\n') }
}

async function trackStatistics(args: Args): Promise<ToolResult> {
  const id = text(args.track_id)
  const track = timeline().tracks.find((t) => t.id === id)
  if (!track) throw new Error(`No track ${id}. Call list_tracks.`)
  if (track.kind !== 'queue' && track.kind !== 'counter') return { content: `track_statistics covers queue and counter tracks; "${track.name}" is a ${track.kind} track. Use track_events.` }
  const { start, end } = window(args)
  const stats = await get<Statistics>(`/api/tracks/${id}/statistics`, { start, end })
  return { content: [`track_id=${id} name="${track.name}" window_ns=${start}-${end}`, ...stats.stats.map((s) => `${s.name}: ${s.value} ${s.units}`)].join('\n') }
}

async function gotoTool(args: Args): Promise<ToolResult> {
  const events = (Array.isArray(args.events) ? args.events : []) as { event_uuid?: unknown; track_id?: unknown }[]
  if (args.event_uuid !== undefined) events.unshift({ event_uuid: args.event_uuid, track_id: args.track_id })
  const uuids = events.map((e) => text(e.event_uuid)).filter((u) => /^\d+$/.test(u))
  let start = number(args.start_ns)
  let end = number(args.end_ns)
  const notes: string[] = []
  const focal = events.find((e) => /^\d+$/.test(text(e.event_uuid)))
  if (focal) {
    const track = text(focal.track_id) || timeline().tracks[0].id
    const details = await get<EventDetails>(`/api/events/${text(focal.event_uuid)}`, { track })
    if (!Number.isFinite(start)) [start, end] = [details.start_ns, details.start_ns + details.duration_ns]
    state().set({
      event: { uuid: details.uuid, trackId: details.track_id, name: details.name, start: details.start_ns, duration: details.duration_ns, level: details.level },
      details,
      tab: 'event',
      hidden: state().hidden.filter((id) => id !== details.track_id),
      scrollTo: details.track_id,
    })
    notes.push(`Selected "${details.name}" on track ${details.track_id}.`)
  }
  if (Number.isFinite(start) && Number.isFinite(end) && end > start) {
    const { view } = state()
    const pad = (end - start) * (args.zoom ? 0.15 : 0)
    const span = args.zoom || end - start > view.end - view.start ? end - start + pad * 2 : view.end - view.start
    const middle = (start + end) / 2
    state().set({ view: clampView(middle - span / 2, middle + span / 2) })
    notes.push(`View at ${start}-${end}${args.zoom ? ', zoomed' : ''}.`)
  }
  state().set({ highlights: uuids })
  if (uuids.length > 1) notes.push(`Highlighted ${uuids.length} events.`)
  return { content: notes.join(' ') || 'Nothing to move to: pass a range or an event.' }
}

const PANELS: [RegExp, string][] = [
  [/^(overview|minimap|map|histogram|activity)/, 'overview'],
  [/^(topology|sidebar|navbar|tree|tracks|left)/, 'sidebar'],
  [/^(details|analysis|tables?|event ?table|bottom)/, 'details'],
  [/^(summary|stats)/, 'summary'],
  [/^(log|console|output)/, 'log'],
  [/^(annotations?|notes?|sticky)/, 'notes'],
]

async function showPanel(args: Args): Promise<ToolResult> {
  const name = text(args.panel).toLowerCase().replace(/^the\s+/, '').trim()
  const panel = PANELS.find(([pattern]) => pattern.test(name))?.[1]
  const visible = args.visible !== false
  const { setSettings, set } = state()
  if (!panel) return { content: `No panel called "${args.panel}". Panels: overview, topology, details, summary, log, annotations.` }
  if (panel === 'overview' || panel === 'sidebar' || panel === 'details') setSettings({ [panel]: visible })
  else if (panel === 'summary') set({ summary: visible })
  else if (visible) {
    setSettings({ details: true })
    set({ tab: panel })
  }
  return { content: `${visible ? 'Opened' : 'Closed'} the ${panel} panel.` }
}

const TABS: [string, string][] = [
  ['event', 'Event'], ['events', 'Events'], ['samples', 'Samples'], ['track', 'Track details'],
  ['top', 'Top events'], ['notes', 'Annotations'], ['log', 'Log'],
]

async function switchTab(args: Args): Promise<ToolResult> {
  const name = text(args.name).toLowerCase()
  if (!name) return { content: `Details panel tabs: ${TABS.map(([, label]) => label).join(', ')}. One trace is open.` }
  const tab = TABS.find(([id, label]) => label.toLowerCase().includes(name) || name.includes(id))
  if (!tab) return { content: `No tab matches "${args.name}". Tabs: ${TABS.map(([, label]) => label).join(', ')}.` }
  state().setSettings({ details: true })
  state().set({ tab: tab[0] })
  return { content: `Switched to the ${tab[1]} tab.` }
}

async function annotate(args: Args): Promise<ToolResult> {
  const time = number(args.time_ns)
  if (!Number.isFinite(time)) throw new Error('time_ns is required.')
  const trackId = args.track_id === undefined ? null : text(args.track_id)
  const note = { id: crypto.randomUUID(), time, trackId, title: text(args.title) || 'Note', text: text(args.text) }
  state().set({ notes: [...state().notes, note] })
  const { range } = state()
  const outside = range && (time < range.start || time > range.end)
  return { content: `Pinned "${note.title}" at ${time} ns${trackId ? ` on track ${trackId}` : ''}.${outside ? ' That is outside the selected range.' : ''}` }
}

async function bookmark(args: Args): Promise<ToolResult> {
  const { bookmarks, view, scroll, set } = state()
  const slot = number(args.slot)
  const list = () => bookmarks.flatMap((b, i) => (b ? [`${i}: ${Math.round(b.start)}-${Math.round(b.end)}`] : [])).join(', ') || 'none'
  if (args.action === 'list') return { content: `Bookmarks: ${list()}` }
  if (!(slot >= 0 && slot <= 9)) throw new Error('slot must be 0 to 9.')
  const next = [...bookmarks]
  if (args.action === 'save') next[slot] = { ...view, scroll }
  else if (args.action === 'remove') next[slot] = null
  else if (args.action === 'goto') {
    const saved = bookmarks[slot]
    if (!saved) return { content: `Bookmark ${slot} is empty. Saved: ${list()}` }
    set({ view: { start: saved.start, end: saved.end }, scroll: saved.scroll })
    return { content: `Went to bookmark ${slot}.` }
  } else throw new Error('action must be save, goto, remove or list.')
  set({ bookmarks: next })
  return { content: `${args.action === 'save' ? 'Saved' : 'Removed'} bookmark ${slot}.` }
}

async function measure(args: Args): Promise<ToolResult> {
  if (args.clear) {
    state().set({ range: null })
    return { content: 'Cleared the measurement.' }
  }
  const start = number(args.start_ns)
  const end = number(args.end_ns)
  if (!(end > start)) throw new Error('measure needs start_ns < end_ns.')
  state().set({ range: { start, end } })
  return { content: `Measured ${ns(end - start)} from ${start} to ${end}.` }
}

export async function runTool(name: string, args: Args): Promise<ToolResult> {
  const range = window(args)
  const content = (value: string) => ({ content: value })
  switch (name) {
    case 'trace_overview':
      return traceOverview(args)
    case 'get_summary':
      return getSummary(args)
    case 'get_summary_event_metrics':
      return summaryEventMetrics(args)
    case 'get_summary_counter_stats':
      return counterStats(args)
    case 'list_tracks':
      return listTracks()
    case 'top_events': {
      const entry = TOP_TABLES[text(args.category)]
      if (!entry) throw new Error('category must be dispatch, memory_copy, memory_alloc, instrumented or sampled.')
      const tracks = trackIds(args, (t) => t.operations.includes(entry[1]))
      if (!tracks) return content(`No track carries ${args.category} events.`)
      return content(await query(entry[0], { tracks, start: range.start, end: range.end }, args, 'DurationTotal'))
    }
    case 'event_instances': {
      const table = SUMMARY_TABLES[text(args.category) || 'kernel']
      if (!table) throw new Error('category must be kernel, memory or thread.')
      if (!text(args.name)) throw new Error('name is required.')
      const scope = SCOPE_PARAMS[text(args.level)]
      const params = { terms: text(args.name), substrings: 0, start: args.start_ns === undefined ? undefined : range.start, end: args.end_ns === undefined ? undefined : range.end, ...(scope && args.id !== undefined ? { [scope]: text(args.id) } : {}) }
      return content(await query(table, params, { ...args, filters: undefined, group_by: undefined }, 'duration'))
    }
    case 'search_events': {
      const terms = (Array.isArray(args.terms) ? args.terms : [args.terms]).map(text).filter(Boolean)
      if (!terms.length) throw new Error('terms is required.')
      const categories = Array.isArray(args.categories) ? args.categories.map((c) => OPERATION_IDS[text(c)]).filter(Boolean) : []
      const result = await query(
        'event_search_table',
        { terms: terms.join('\n'), substrings: args.match === 'equals' ? 0 : 1, partial: args.combine === 'any' ? 1 : 0, category: args.include_category ? 1 : 0, start: range.start, end: range.end },
        { ...args, filters: undefined, group_by: undefined },
        'duration',
      )
      return content(categories.length ? `${result}\nnote: categories ${(args.categories as string[]).join(', ')} are matched by name only in this client.` : result)
    }
    case 'track_events':
      return content(await query('event_table', { tracks: trackIds(args, (t) => t.data_type === 'events'), start: range.start, end: range.end }, args))
    case 'track_samples':
      return content(await query('sample_table', { tracks: trackIds(args, (t) => t.data_type === 'samples'), start: range.start, end: range.end }, args))
    case 'event_details':
      return eventDetails(args)
    case 'track_statistics':
      return trackStatistics(args)
    case 'goto':
      return gotoTool(args)
    case 'show_panel':
      return showPanel(args)
    case 'switch_tab':
      return switchTab(args)
    case 'flow_arrows':
      state().setSettings({ flows: args.visible !== false, ...(args.style === 'fan' || args.style === 'chain' ? { flowStyle: args.style } : {}) })
      return content(`Flow arrows ${args.visible !== false ? 'on' : 'off'}${args.style ? `, ${args.style} style` : ''}.`)
    case 'annotate':
      return annotate(args)
    case 'bookmark':
      return bookmark(args)
    case 'measure':
      return measure(args)
    case 'reset_view': {
      const t = timeline()
      state().set({ view: { start: t.start_ns, end: t.end_ns } })
      return content('Reset the view to the whole trace.')
    }
    case 'offer_next_steps': {
      const steps = (Array.isArray(args.steps) ? args.steps : []).map(text).filter(Boolean).slice(0, 3)
      return { content: `Offered ${steps.length} next steps.`, nextSteps: steps }
    }
    default:
      return content(`Unknown tool "${name}".`)
  }
}

// What the model is told about the trace before the first question.
export function briefing(): string {
  const { timeline: t, topology, event, range, selectedTracks, view } = state()
  if (!t) return 'No trace is loaded.'
  const kinds = t.tracks.reduce<Record<string, number>>((all, track) => ({ ...all, [track.kind]: (all[track.kind] ?? 0) + 1 }), {})
  const lines = [
    `trace: ${state().status?.trace_file_path}`,
    `span_ns: ${t.start_ns}-${t.end_ns} (${ns(t.end_ns - t.start_ns)})`,
    `topology: nodes=${topology?.nodes.length ?? 0} gpus=${topology?.gpu_count ?? 0} cpus=${topology?.cpu_count ?? 0}`,
    `tracks: ${t.tracks.length} (${Object.entries(kinds).map(([k, n]) => `${k}=${n}`).join(', ')})`,
    `view_ns: ${Math.round(view.start)}-${Math.round(view.end)}`,
  ]
  if (range) lines.push(`selected_range_ns: ${Math.round(range.start)}-${Math.round(range.end)} (${ns(range.end - range.start)})`)
  if (event) lines.push(`selected_event: __uuid="${event.uuid}" __trackId=${event.trackId} name="${event.name}" start_ns=${event.start} duration_ns=${event.duration}`)
  if (selectedTracks.length) lines.push(`selected_tracks: ${selectedTracks.join(', ')}`)
  lines.push('summary: not in the briefing; call get_summary.')
  return lines.join('\n')
}
