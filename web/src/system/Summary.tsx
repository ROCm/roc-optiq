import { useMemo, useState } from 'react'
import { X } from 'lucide-react'
import { get, type Aggregate, type Table, type TopologyNode } from '../api'
import { Donut } from '../charts'
import { colorFor, formatDuration, formatNumber } from '../format'
import { useStore } from '../store'
import { ServerTable, SimpleTable } from '../table'
import { Empty, Loading, Segmented, useAsync } from '../ui'

const CATEGORIES = [
  { id: 'kernel', label: 'Kernel executions', table: 'summary_kernel_table', color: '#ed1c24' },
  { id: 'memory', label: 'Memory operations', table: 'summary_memory_table', color: '#48b5ff' },
  { id: 'thread', label: 'Thread operations', table: 'summary_thread_table', color: '#f7c548' },
] as const

type Category = (typeof CATEGORIES)[number]
const LEVEL_PARAM: Record<string, string> = { node: 'node', processor: 'agent', process: 'process', thread: 'thread' }

interface Scope {
  aggregate: Aggregate
  label: string
  depth: number
  path: Aggregate[]
}

// Every aggregate in the summary tree, labelled from the topology.
function flatten(root: Aggregate, names: Map<string, string>): Scope[] {
  const out: Scope[] = []
  const walk = (aggregate: Aggregate, depth: number, path: Aggregate[]) => {
    const label = aggregate.level === 'trace' ? 'Whole trace' : `${aggregate.level[0].toUpperCase()}${aggregate.level.slice(1)} · ${names.get(`${aggregate.level}${aggregate.id}`) ?? aggregate.id}`
    out.push({ aggregate, label, depth, path: [...path, aggregate] })
    aggregate.children.forEach((child) => walk(child, depth + 1, [...path, aggregate]))
  }
  walk(root, 0, [])
  return out
}

function topologyNames(nodes: TopologyNode[]) {
  const names = new Map<string, string>()
  const visit = (node: TopologyNode) => {
    names.set(`${node.type}${node.id}`, node.type === 'processor' ? `${node.label ?? ''} ${node.product_name ?? node.name}`.trim() : node.host_name || node.header || node.name)
    ;[...(node.processors ?? []), ...(node.processes ?? []), ...(node.threads ?? [])].forEach(visit)
  }
  nodes.forEach(visit)
  return names
}

export function Summary() {
  const timeline = useStore((s) => s.timeline)!
  const topology = useStore((s) => s.topology)
  const set = useStore((s) => s.set)
  const summary = useAsync(() => get<{ root: Aggregate }>('/api/summary'), [])
  const [scopeIndex, setScopeIndex] = useState(0)
  const scopes = useMemo(
    () => (summary.data ? flatten(summary.data.root, topologyNames(topology?.nodes ?? [])) : []),
    [summary.data, topology],
  )
  const scope = scopes[scopeIndex]

  return (
    <div className="sheet">
      <div className="dash">
        <div className="row">
          <h2 className="grow">Trace summary</h2>
          {scopes.length > 1 && (
            <select className="select" value={scopeIndex} onChange={(e) => setScopeIndex(Number(e.target.value))}>
              {scopes.map((s, i) => (
                <option key={i} value={i}>{'\u2003'.repeat(s.depth)}{s.label}</option>
              ))}
            </select>
          )}
          <button className="icon-btn" title="Close (Esc)" onClick={() => set({ summary: false })}>
            <X size={18} />
          </button>
        </div>
        {summary.error && <Empty title="The summary could not be loaded" hint={summary.error} />}
        {!summary.data && !summary.error && <Loading text="Summarizing the trace…" />}
        {scope && (
          <>
            <Overview aggregate={scope.aggregate} span={timeline.end_ns - timeline.start_ns} />
            <div className="split">
              {CATEGORIES.map((category) => (
                <CategoryCard key={category.id} category={category} scope={scope} />
              ))}
            </div>
            {scope.aggregate.counters.length > 0 && (
              <div className="card">
                <div className="card-title">Counters <span className="hint faint">{scope.aggregate.counters.length}</span></div>
                <SimpleTable
                  columns={['Device', 'Counter', 'Min', 'Mean', 'Max', 'Std dev', 'Units']}
                  rows={scope.aggregate.counters.map((c) => [c.device.toUpperCase(), c.name, c.min, c.mean, c.max, c.std_dev, c.units])}
                  maxHeight={360}
                />
              </div>
            )}
          </>
        )}
      </div>
    </div>
  )
}

function Overview({ aggregate, span }: { aggregate: Aggregate; span: number }) {
  const topology = useStore((s) => s.topology)
  const totals = CATEGORIES.map((c) => ({ ...c, events: aggregate.top_events[c.id] }))
  const busy = totals.reduce((sum, c) => sum + (c.events?.duration_sum_ns ?? 0), 0) || 1
  return (
    <>
      <div className="tiles">
        <div className="tile"><div className="section-label">Trace span</div><div className="value">{formatDuration(span)}</div></div>
        <div className="tile"><div className="section-label">Events</div><div className="value">{formatNumber(totals.reduce((s, c) => s + (c.events?.invocations ?? 0), 0), 0)}</div></div>
        <div className="tile"><div className="section-label">Nodes</div><div className="value">{topology?.nodes.length ?? '–'}</div></div>
        <div className="tile"><div className="section-label">GPUs</div><div className="value">{topology?.gpu_count ?? '–'}</div></div>
        <div className="tile"><div className="section-label">CPUs</div><div className="value">{topology?.cpu_count ?? '–'}</div></div>
      </div>
      <div className="card pad col">
        <div className="row"><span className="section-label grow">Time by category</span><span className="faint">{formatDuration(busy)} total</span></div>
        <div className="compose">
          {totals.map((c) => <div key={c.id} style={{ flex: c.events?.duration_sum_ns ?? 0, background: c.color }} title={c.label} />)}
        </div>
        <div className="legend">
          {totals.map((c) => (
            <span key={c.id}>
              <i className="dot" style={{ background: c.color }} />
              {c.label} · {((100 * (c.events?.duration_sum_ns ?? 0)) / busy).toFixed(1)}% · {formatNumber(c.events?.invocations ?? 0, 0)} calls
            </span>
          ))}
        </div>
      </div>
    </>
  )
}

function CategoryCard({ category, scope }: { category: Category; scope: Scope }) {
  const [mode, setMode] = useState<'bar' | 'pie'>('bar')
  const [chosen, setChosen] = useState<string | null>(null)
  const focusEvent = useStore((s) => s.focusEvent)
  const set = useStore((s) => s.set)
  const events = scope.aggregate.top_events[category.id]
  if (!events || !events.top.length) {
    return (
      <div className="card">
        <div className="card-title">{category.label}</div>
        <Empty title="Nothing recorded in this scope" />
      </div>
    )
  }
  const max = Math.max(...events.top.map((e) => e.duration_sum_ns))
  const criteria = Object.fromEntries(
    scope.path.filter((a) => LEVEL_PARAM[a.level] && a.id).map((a) => [LEVEL_PARAM[a.level], a.id]),
  )
  return (
    <div className="card">
      <div className="card-title">
        {category.label}
        <span className="hint row">
          <span className="faint">{formatDuration(events.duration_sum_ns)}</span>
          <Segmented
            value={mode}
            onChange={setMode}
            options={[{ id: 'bar', label: 'Bars' }, { id: 'pie', label: 'Pie' }]}
          />
        </span>
      </div>
      <div className="pad" style={{ paddingTop: 0 }}>
        {mode === 'pie' ? (
          <div className="row" style={{ justifyContent: 'center' }}>
            <Donut
              center={`${events.top.length}`}
              slices={events.top.map((e) => ({ label: e.name, value: e.duration_sum_ns, color: e.name === 'Others' ? '#55555f' : colorFor(e.name) }))}
            />
          </div>
        ) : (
          <div style={{ maxHeight: 300, overflow: 'auto' }}>
            {events.top.map((event) => (
              <div
                key={event.name}
                className={`hbar ${chosen === event.name ? 'on' : ''}`}
                onClick={() => event.name !== 'Others' && setChosen(chosen === event.name ? null : event.name)}
                title={event.name}
              >
                <span className="ellipsis">{event.name}</span>
                <span className="mono muted">{formatDuration(event.duration_sum_ns)} · {formatNumber(event.duration_pct * 100, 1)}% · ×{formatNumber(event.invocations, 0)}</span>
                <div className="track"><div style={{ width: `${(100 * event.duration_sum_ns) / max}%`, background: event.name === 'Others' ? 'var(--text-3)' : colorFor(event.name) }} /></div>
              </div>
            ))}
          </div>
        )}
      </div>
      {chosen && (
        <div style={{ height: 280, borderTop: '1px solid var(--line)' }}>
          <ServerTable
            deps={[category.table, chosen, JSON.stringify(criteria)]}
            exportName={`${category.id}-instances`}
            tools={<span className="badge neutral ellipsis" style={{ maxWidth: 260 }}>{chosen}</span>}
            load={(query) => get<Table>(`/api/tables/${category.table}`, { ...query, terms: chosen, substrings: 0, ...criteria })}
            onRow={(row) => {
              if (!row.__uuid || !row.__trackId) return
              set({ summary: false })
              focusEvent({ uuid: row.__uuid, trackId: row.__trackId, name: row.name ?? chosen, start: Number(row.start), duration: Number(row.duration ?? 0), level: 0 })
            }}
          />
        </div>
      )}
    </div>
  )
}