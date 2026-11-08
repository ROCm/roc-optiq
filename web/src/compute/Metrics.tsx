import { useMemo, useState } from 'react'
import { Pin, PinOff } from 'lucide-react'
import type { Kernel, MetricTable, MetricValue, Workload } from '../api'
import { formatNumber } from '../format'
import { SimpleTable } from '../table'
import { Empty, Loading } from '../ui'
import { shortName, useMetrics } from './ComputeView'

const PINS_KEY = 'optiq.pinned-metrics'
const byId = (metrics?: MetricValue[]) => new Map((metrics ?? []).map((m) => [m.metric_id, m]))

function MetricTableCard(props: {
  table: MetricTable
  values: Map<string, MetricValue>
  pinned: string[]
  onPin: (id: string) => void
}) {
  const { table, values } = props
  return (
    <div className="card" style={{ overflow: 'hidden' }}>
      <div className="card-title">{table.name} <span className="hint faint">{table.entries.length}</span></div>
      <div className="grid" style={{ maxHeight: 460 }}>
        <table>
          <thead>
            <tr>
              <th style={{ width: 28 }} />
              <th>Metric</th>
              {table.value_names.map((v) => <th key={v}>{v}</th>)}
              <th>Unit</th>
            </tr>
          </thead>
          <tbody>
            {table.entries.map((entry) => {
              const value = values.get(entry.metric_id)
              const pinned = props.pinned.includes(entry.metric_id)
              return (
                <tr key={entry.metric_id}>
                  <td>
                    <button className={`icon-btn small ${pinned ? 'on' : ''}`} title={pinned ? 'Unpin' : 'Pin to the top'} onClick={() => props.onPin(entry.metric_id)}>
                      {pinned ? <PinOff size={12} /> : <Pin size={12} />}
                    </button>
                  </td>
                  <td title={entry.description}>{entry.name}</td>
                  {table.value_names.map((v) => <td key={v} className="num">{formatNumber(value?.values[v], 3)}</td>)}
                  <td className="faint">{entry.unit}</td>
                </tr>
              )
            })}
          </tbody>
        </table>
      </div>
    </div>
  )
}

export function TableView({ workload, kernel }: { workload: Workload; kernel: Kernel }) {
  const [category, setCategory] = useState(workload.metrics[0]?.id)
  const [pinned, setPinned] = useState<string[]>(() => JSON.parse(localStorage.getItem(PINS_KEY) ?? '[]'))
  const current = workload.metrics.find((c) => c.id === category) ?? workload.metrics[0]
  const metrics = useMetrics(workload.id, [kernel.id], current ? String(current.id) : '', 14)
  const pinnedCategories = [...new Set(pinned.map((id) => id.split('.')[0]))].join(',')
  const pinnedMetrics = useMetrics(workload.id, [kernel.id], pinnedCategories, 15)
  const values = useMemo(() => byId(metrics.data?.kernels[0]?.metrics), [metrics.data])
  const pinnedValues = useMemo(() => byId(pinnedMetrics.data?.kernels[0]?.metrics), [pinnedMetrics.data])
  const togglePin = (id: string) => {
    const next = pinned.includes(id) ? pinned.filter((p) => p !== id) : [...pinned, id]
    localStorage.setItem(PINS_KEY, JSON.stringify(next))
    setPinned(next)
  }
  const pinnedRows = pinned.flatMap((id) => {
    const value = pinnedValues.get(id)
    const shown = value && Object.entries(value.values).map(([k, v]) => `${k} ${formatNumber(v, 3)}`).join(' · ')
    return value ? [[value.name, shown!, value.unit]] : []
  })
  if (!current) return <Empty title="This profile has no metric tables" />

  return (
    <div className="dash">
      <div className="row" style={{ flexWrap: 'wrap', gap: 6 }}>
        {workload.metrics.map((c) => (
          <button key={c.id} className={`btn small ${c.id === current.id ? 'primary' : ''}`} onClick={() => setCategory(c.id)}>
            {c.id}. {c.name}
          </button>
        ))}
      </div>
      {pinned.length > 0 && (
        <div className="card">
          <div className="card-title"><Pin size={15} /> Pinned metrics <span className="hint faint ellipsis" style={{ maxWidth: 360 }}>{shortName(kernel.name)}</span></div>
          <SimpleTable columns={['Metric', 'Values', 'Unit']} rows={pinnedRows} />
        </div>
      )}
      {metrics.loading && !metrics.data ? (
        <Loading text="Loading metrics…" />
      ) : (
        <div className="split">
          {current.tables.map((table) => (
            <MetricTableCard key={table.id} table={table} values={values} pinned={pinned} onPin={togglePin} />
          ))}
        </div>
      )}
    </div>
  )
}

interface Side {
  workload: number
  kernel: number
}

function SidePicker({ label, side, workloads, onChange }: { label: string; side: Side; workloads: Workload[]; onChange: (side: Side) => void }) {
  const workload = workloads.find((w) => w.id === side.workload) ?? workloads[0]
  return (
    <div className="row grow">
      <span className="badge" style={{ minWidth: 70, justifyContent: 'center' }}>{label}</span>
      {workloads.length > 1 && (
        <select className="select" value={workload.id} onChange={(e) => onChange({ workload: Number(e.target.value), kernel: workloads.find((w) => w.id === Number(e.target.value))!.kernels[0].id })}>
          {workloads.map((w) => <option key={w.id} value={w.id}>{w.name}</option>)}
        </select>
      )}
      <select className="select grow" value={side.kernel} onChange={(e) => onChange({ ...side, kernel: Number(e.target.value) })}>
        {workload.kernels.map((k) => <option key={k.id} value={k.id}>{shortName(k.name)}</option>)}
      </select>
    </div>
  )
}

export function Comparison({ workloads, workload, kernel }: { workloads: Workload[]; workload: Workload; kernel: Kernel }) {
  const other = workload.kernels.find((k) => k.id !== kernel.id) ?? kernel
  const [baseline, setBaseline] = useState<Side>({ workload: workload.id, kernel: kernel.id })
  const [target, setTarget] = useState<Side>({ workload: workload.id, kernel: other.id })
  const [category, setCategory] = useState(workload.metrics[0]?.id ?? 2)
  const [threshold, setThreshold] = useState(10)
  const catalog = workloads.find((w) => w.id === baseline.workload) ?? workload
  const tables = catalog.metrics.find((c) => c.id === category)?.tables ?? []
  const valueNames = [...new Set(tables.flatMap((t) => t.value_names))]
  const [valueName, setValueName] = useState('Avg')
  const value = valueNames.includes(valueName) ? valueName : valueNames[0]
  const left = useMetrics(baseline.workload, [baseline.kernel], String(category), 16)
  const right = useMetrics(target.workload, [target.kernel], String(category), 17)
  const [a, b] = [byId(left.data?.kernels[0]?.metrics), byId(right.data?.kernels[0]?.metrics)]

  const rows = tables.flatMap((t) =>
    t.entries.map((entry) => {
      const x = a.get(entry.metric_id)?.values[value] ?? null
      const y = b.get(entry.metric_id)?.values[value] ?? null
      const delta = x !== null && y !== null ? y - x : null
      const percent = delta !== null && x ? (100 * delta) / Math.abs(x) : null
      return { entry, table: t.name, x, y, delta, percent }
    }),
  )

  return (
    <div className="dash">
      <div className="card pad col">
        <SidePicker label="Baseline" side={baseline} workloads={workloads} onChange={setBaseline} />
        <SidePicker label="Target" side={target} workloads={workloads} onChange={setTarget} />
        <div className="row" style={{ flexWrap: 'wrap' }}>
          <select className="select" value={category} onChange={(e) => setCategory(Number(e.target.value))}>
            {catalog.metrics.map((c) => <option key={c.id} value={c.id}>{c.id}. {c.name}</option>)}
          </select>
          <select className="select" value={value} onChange={(e) => setValueName(e.target.value)}>
            {valueNames.map((v) => <option key={v}>{v}</option>)}
          </select>
          <span className="grow" />
          <span className="muted">Highlight changes over</span>
          <input className="input" type="number" style={{ width: 80 }} value={threshold} min={0} onChange={(e) => setThreshold(Number(e.target.value))} />
          <span className="muted">%</span>
        </div>
      </div>
      <div className="card" style={{ overflow: 'hidden' }}>
        {left.loading || right.loading ? (
          <Loading text="Comparing…" />
        ) : (
          <div className="grid" style={{ maxHeight: 'calc(100vh - 330px)' }}>
            <table>
              <thead>
                <tr><th>Table</th><th>Metric</th><th>Baseline</th><th>Target</th><th>Difference</th><th>Change</th><th>Unit</th></tr>
              </thead>
              <tbody>
                {rows.map(({ entry, table, x, y, delta, percent }) => {
                  const flagged = percent !== null && Math.abs(percent) >= threshold
                  const tone = flagged ? (percent! > 0 ? 'var(--ok)' : 'var(--red-hi)') : undefined
                  return (
                    <tr key={entry.metric_id}>
                      <td className="faint">{table}</td>
                      <td title={entry.description}>{entry.name}</td>
                      <td className="num">{formatNumber(x, 3)}</td>
                      <td className="num">{formatNumber(y, 3)}</td>
                      <td className="num" style={{ color: tone }}>{delta === null ? '–' : `${delta > 0 ? '+' : ''}${formatNumber(delta, 3)}`}</td>
                      <td className="num" style={{ color: tone, fontWeight: flagged ? 650 : undefined }}>{percent === null ? '–' : `${percent > 0 ? '+' : ''}${percent.toFixed(1)}%`}</td>
                      <td className="faint">{entry.unit}</td>
                    </tr>
                  )
                })}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  )
}
