import { useEffect, useRef, useState } from 'react'
import { Activity, Copy, Info, ListTree, MousePointerClick, Rows3, ScrollText, StickyNote, Trash2, Trophy, Waves } from 'lucide-react'
import { get, type Statistics, type Table, type Track } from '../api'
import { colorFor, formatDuration, formatNumber, formatTime } from '../format'
import { useStore, type EventRef } from '../store'
import { ServerTable, SimpleTable, type TableQuery } from '../table'
import { Empty, Loading, Segmented, Tabs, useAsync } from '../ui'
import { trackColor } from './Timeline'

const TOP_TABLES = [
  { op: 'launch', table: 'top_instrumented_events_table', label: 'Instrumented thread events' },
  { op: 'dispatch', table: 'top_dispatch_events_table', label: 'Dispatches' },
  { op: 'memory_allocate', table: 'top_memory_allocation_events_table', label: 'Memory allocations' },
  { op: 'memory_copy', table: 'top_memory_copy_events_table', label: 'Memory copies' },
  { op: 'launch_sample', table: 'top_sampled_events_table', label: 'Sampled thread events' },
]

// The tracks a table covers: the selected ones of a kind, else every visible one.
function useScope(kind: 'events' | 'samples') {
  const timeline = useStore((s) => s.timeline)!
  const selected = useStore((s) => s.selectedTracks)
  const hidden = useStore((s) => s.hidden)
  const range = useStore((s) => s.range)
  const ofKind = timeline.tracks.filter((t) => t.data_type === kind)
  const chosen = ofKind.filter((t) => selected.includes(t.id))
  const tracks = chosen.length ? chosen : ofKind.filter((t) => !hidden.includes(t.id))
  const noun = kind === 'events' ? 'track' : 'counter'
  const label = `${chosen.length ? `${chosen.length} selected ${noun}${chosen.length > 1 ? 's' : ''}` : `All visible ${noun}s`}${range ? ' · selected range' : ''}`
  return { tracks, range: range ?? { start: timeline.start_ns, end: timeline.end_ns }, label }
}

function rowToEvent(row: Record<string, string>): EventRef | null {
  if (!row.__uuid || !row.__trackId) return null
  return { uuid: row.__uuid, trackId: row.__trackId, name: row.name ?? '', start: Number(row.start), duration: Number(row.duration ?? 0), level: 0 }
}

export function BottomPanel() {
  const tab = useStore((s) => s.tab)
  const height = useStore((s) => s.settings.detailsHeight)
  const notes = useStore((s) => s.notes)
  const event = useStore((s) => s.event)
  const selected = useStore((s) => s.selectedTracks)
  const set = useStore((s) => s.set)
  const setSettings = useStore((s) => s.setSettings)
  const [resizing, setResizing] = useState(false)
  const start = useRef(0)

  return (
    <>
      <div
        className={`resizer ${resizing ? 'active' : ''}`}
        onPointerDown={(e) => {
          e.currentTarget.setPointerCapture(e.pointerId)
          start.current = e.clientY + height
          setResizing(true)
        }}
        onPointerMove={(e) => resizing && setSettings({ detailsHeight: Math.min(Math.max(140, start.current - e.clientY), window.innerHeight - 260) })}
        onPointerUp={() => setResizing(false)}
      />
      <div className="bottom" style={{ height }}>
        <Tabs
          value={tab}
          onChange={(id) => set({ tab: id })}
          tabs={[
            { id: 'event', label: 'Event', icon: MousePointerClick, count: event ? 1 : undefined },
            { id: 'events', label: 'Events', icon: Rows3 },
            { id: 'samples', label: 'Samples', icon: Waves },
            { id: 'track', label: 'Track details', icon: Info, count: selected.length || undefined },
            { id: 'top', label: 'Top events', icon: Trophy },
            { id: 'notes', label: 'Annotations', icon: StickyNote, count: notes.length || undefined },
            { id: 'log', label: 'Log', icon: ScrollText },
          ]}
        />
        <div className="bottom-body">
          {tab === 'event' && <EventTab />}
          {tab === 'events' && <TrackTable kind="events" />}
          {tab === 'samples' && <TrackTable kind="samples" />}
          {tab === 'track' && <TrackTab />}
          {tab === 'top' && <TopEventsTab />}
          {tab === 'notes' && <NotesTab />}
          {tab === 'log' && <LogTab />}
        </div>
      </div>
    </>
  )
}

function TrackTable({ kind }: { kind: 'events' | 'samples' }) {
  const { tracks, range, label } = useScope(kind)
  const focusEvent = useStore((s) => s.focusEvent)
  if (!tracks.length) return <Empty icon={Rows3} title={`No ${kind === 'events' ? 'event' : 'counter'} tracks are visible`} />
  const ids = tracks.map((t) => t.id).join(',')
  return (
    <ServerTable
      deps={[kind, ids, range.start, range.end]}
      exportName={kind}
      groupable={kind === 'events'}
      tools={<span className="badge neutral">{label}</span>}
      load={(query: TableQuery) => get<Table>(`/api/tables/${kind === 'events' ? 'event_table' : 'sample_table'}`, { ...query, tracks: ids, start: range.start, end: range.end })}
      onRow={(row) => {
        const event = rowToEvent(row)
        if (event) focusEvent(event)
      }}
    />
  )
}

function EventTab() {
  const event = useStore((s) => s.event)
  const details = useStore((s) => s.details)
  const timeline = useStore((s) => s.timeline)!
  const format = useStore((s) => s.settings.timeFormat)
  const focusEvent = useStore((s) => s.focusEvent)
  const notify = useStore((s) => s.notify)
  if (!event) return <Empty icon={MousePointerClick} title="No event selected" hint="Click an event on the timeline, or a row in a table." />
  if (!details || details.uuid !== event.uuid) return <Loading text="Loading event details…" />
  const track = timeline.tracks.find((t) => t.id === details.track_id)
  const groups = details.ext.reduce<Record<string, [string, string][]>>((all, e) => {
    ;(all[e.category] ??= []).push([e.name, e.value])
    return all
  }, {})
  const goTo = (uuid: string, trackId: string, name: string, start: number, end: number, level: number) =>
    focusEvent({ uuid, trackId, name, start, duration: Math.max(0, end - start), level })

  return (
    <div className="pad col" style={{ gap: 16 }}>
      <div className="row" style={{ gap: 12, alignItems: 'flex-start' }}>
        <i className="dot" style={{ width: 12, height: 12, marginTop: 6, background: colorFor(details.name) }} />
        <div className="grow">
          <div style={{ fontSize: 16, fontWeight: 650, overflowWrap: 'anywhere' }}>{details.name}</div>
          <div className="row muted" style={{ flexWrap: 'wrap', marginTop: 4 }}>
            <span className="badge">{details.op.replace('_', ' ')}</span>
            <span className="mono">{formatDuration(details.duration_ns)}</span>
            <span className="faint">at</span>
            <span className="mono">{formatTime(details.start_ns - timeline.start_ns, format)}</span>
            <span className="faint">on</span>
            <span>{track?.name}</span>
          </div>
        </div>
        <button className="btn small" onClick={() => { navigator.clipboard.writeText(details.uuid); notify('Event id copied') }}>
          <Copy size={13} /> {details.uuid}
        </button>
      </div>
      <div className="split">
        {Object.entries(groups).map(([category, entries]) => (
          <div className="card" key={category}>
            <div className="card-title">{category}</div>
            <div className="kv pad" style={{ paddingTop: 0 }}>
              {entries.map(([name, value], i) => [<span key={`k${i}`}>{name}</span>, <span key={`v${i}`} className="mono">{value}</span>])}
            </div>
          </div>
        ))}
        {details.args.length > 0 && (
          <div className="card">
            <div className="card-title">Arguments</div>
            <SimpleTable columns={['#', 'Name', 'Value', 'Type']} rows={details.args.map((a) => [a.position, a.name, a.value, a.data_type])} />
          </div>
        )}
        {details.flows.length > 0 && (
          <div className="card">
            <div className="card-title"><Activity size={15} /> Flows <span className="hint faint">click to follow</span></div>
            <SimpleTable
              columns={['Name', 'Track', 'Start', 'Level']}
              rows={details.flows.map((f) => [f.name, timeline.tracks.find((t) => t.id === f.track_id)?.name ?? f.track_id, formatTime(f.start_ns - timeline.start_ns, format), f.level])}
              selected={(row) => row[0] === details.name}
              onRow={(i) => {
                const f = details.flows[i]
                goTo(f.uuid, f.track_id, f.name, f.start_ns, f.end_ns, f.level)
              }}
            />
          </div>
        )}
        {details.call_stack.length > 0 && (
          <div className="card">
            <div className="card-title"><ListTree size={15} /> Call stack</div>
            <SimpleTable columns={['Function', 'File', 'PC', 'Address']} rows={details.call_stack.map((f) => [f.name, f.file, f.pc, f.address])} />
          </div>
        )}
      </div>
    </div>
  )
}

function Statistic({ track }: { track: Track }) {
  const range = useStore((s) => s.range)
  const supported = track.kind === 'queue' || track.kind === 'counter'
  const stats = useAsync(
    () => (supported ? get<Statistics>(`/api/tracks/${track.id}/statistics`, { start: range?.start, end: range?.end }) : null),
    [track.id, range?.start, range?.end],
  )
  if (!supported) return null
  if (!stats.data) return <span className="faint">{stats.error ?? 'Computing…'}</span>
  return (
    <>
      {stats.data.stats.map((s) => [
        <span key={`k${s.name}`}>{s.name}{range ? ' (range)' : ''}</span>,
        <span key={`v${s.name}`} className="mono">{formatNumber(s.value, 3)} {s.units}</span>,
      ])}
    </>
  )
}

function TrackTab() {
  const timeline = useStore((s) => s.timeline)!
  const selected = useStore((s) => s.selectedTracks)
  const event = useStore((s) => s.event)
  const format = useStore((s) => s.settings.timeFormat)
  const ids = selected.length ? selected : event ? [event.trackId] : []
  const tracks = timeline.tracks.filter((t) => ids.includes(t.id))
  if (!tracks.length) return <Empty icon={Info} title="No track selected" hint="Click a track name to see its details; Ctrl+click adds more." />
  return (
    <div className="pad split">
      {tracks.map((track) => (
        <div className="card" key={track.id}>
          <div className="card-title">
            <i className="dot" style={{ background: trackColor(track) }} />
            <span className="ellipsis">{track.name}</span>
            <span className="badge neutral hint">{track.kind.replace('_', ' ')}</span>
          </div>
          <div className="kv pad" style={{ paddingTop: 0 }}>
            <span>Category</span><span>{track.category}</span>
            <span>Owner</span><span>{track.main_name}</span>
            <span>Entries</span><span className="mono">{track.entry_count.toLocaleString()}</span>
            <span>First</span><span className="mono">{formatTime(track.start_ns - timeline.start_ns, format)}</span>
            <span>Last</span><span className="mono">{formatTime(track.end_ns - timeline.start_ns, format)}</span>
            {track.data_type === 'samples' && (
              <>
                <span>Value range</span>
                <span className="mono">{formatNumber(track.min_value)} – {formatNumber(track.max_value)}</span>
              </>
            )}
            {track.operations.length > 0 && (<><span>Operations</span><span>{track.operations.join(', ').replace(/_/g, ' ')}</span></>)}
            <Statistic track={track} />
          </div>
        </div>
      ))}
    </div>
  )
}

function TopEventsTab() {
  const { tracks, range, label } = useScope('events')
  const available = TOP_TABLES.filter((t) => tracks.some((track) => track.operations.includes(t.op)))
  const [choice, setChoice] = useState(available[0]?.table)
  const current = available.find((t) => t.table === choice) ?? available[0]
  if (!current) return <Empty icon={Trophy} title="No event tracks with top events in scope" />
  const ids = tracks.filter((t) => t.operations.includes(current.op)).map((t) => t.id).join(',')
  return (
    <ServerTable
      deps={[current.table, ids, range.start, range.end]}
      exportName={current.table}
      tools={
        <>
          <select className="select small" value={current.table} onChange={(e) => setChoice(e.target.value)}>
            {available.map((t) => <option key={t.table} value={t.table}>{t.label}</option>)}
          </select>
          <span className="badge neutral">{label}</span>
        </>
      }
      load={(query) => get<Table>(`/api/tables/${current.table}`, { ...query, tracks: ids, start: range.start, end: range.end })}
    />
  )
}

// The server's recent log, the web's Log Viewer.
function LogTab() {
  const [lines, setLines] = useState<string[]>([])
  const [level, setLevel] = useState('all')
  const body = useRef<HTMLDivElement>(null)
  useEffect(() => {
    let live = true
    const load = () => get<{ lines: string[] }>('/api/logs').then((reply) => live && setLines(reply.lines), () => undefined)
    load()
    const timer = setInterval(load, 2000)
    return () => {
      live = false
      clearInterval(timer)
    }
  }, [])
  useEffect(() => {
    body.current?.scrollTo({ top: body.current.scrollHeight })
  }, [lines])
  const shown = lines.filter((line) => level === 'all' || line.includes(`[${level}]`))
  return (
    <div className="grid-wrap">
      <div className="grid-tools">
        <Segmented value={level} onChange={setLevel} options={['all', 'info', 'warning', 'error'].map((id) => ({ id, label: id }))} />
        <span className="faint">{shown.length} lines</span>
        <div className="grow" />
        <button className="icon-btn small" title="Copy the log" onClick={() => navigator.clipboard.writeText(shown.join(''))}>
          <Copy size={14} />
        </button>
      </div>
      <div className="grid code" ref={body} style={{ padding: '6px 12px' }}>
        {shown.map((line, i) => (
          <div key={i} className={/\[(error|critical)\]/.test(line) ? 'log-error' : /\[warning\]/.test(line) ? 'log-warn' : ''}>
            {line.trimEnd()}
          </div>
        ))}
      </div>
    </div>
  )
}

function NotesTab() {
  const notes = useStore((s) => s.notes)
  const timeline = useStore((s) => s.timeline)!
  const format = useStore((s) => s.settings.timeFormat)
  const set = useStore((s) => s.set)
  if (!notes.length) return <Empty icon={StickyNote} title="No annotations yet" hint="Right-click the timeline and choose Add note here." />
  const update = (id: string, patch: object) => set({ notes: notes.map((n) => (n.id === id ? { ...n, ...patch } : n)) })
  return (
    <div className="pad col">
      {[...notes].sort((a, b) => a.time - b.time).map((note) => (
        <div className="card row" key={note.id} style={{ padding: '8px 10px', alignItems: 'flex-start' }}>
          <StickyNote size={16} color="#f7c548" style={{ marginTop: 6 }} />
          <div className="grow col" style={{ gap: 4 }}>
            <input className="input small" value={note.title} onChange={(e) => update(note.id, { title: e.target.value })} />
            <input className="input small" placeholder="Note text" value={note.text} onChange={(e) => update(note.id, { text: e.target.value })} />
          </div>
          <button
            className="btn small"
            onClick={() => {
              const { view } = useStore.getState()
              const half = (view.end - view.start) / 2
              set({ view: { start: note.time - half, end: note.time + half }, scrollTo: note.trackId })
            }}
          >
            {formatTime(note.time - timeline.start_ns, format)}
          </button>
          <button className="icon-btn small" title="Delete" onClick={() => set({ notes: notes.filter((n) => n.id !== note.id) })}>
            <Trash2 size={14} />
          </button>
        </div>
      ))}
    </div>
  )
}
