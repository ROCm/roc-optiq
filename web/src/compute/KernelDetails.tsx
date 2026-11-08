import { useLayoutEffect, useMemo, useRef, useState } from 'react'
import { ArrowDown, ArrowUp, Plus, X } from 'lucide-react'
import { get, type Kernel, type KernelTable, type MemoryBlock, type MetricRef, type MetricValue, type Workload } from '../api'
import { Roofline } from '../charts'
import { fileName, formatNumber } from '../format'
import { useStore } from '../store'
import { Empty, Loading, useAsync } from '../ui'
import { SOL, SolTable, pairs, shortName, useMetrics } from './ComputeView'

const MEMORY_CHART_CATEGORY = '3'

export function KernelDetails({ workload, kernel }: { workload: Workload; kernel: Kernel }) {
  const sol = useMetrics(workload.id, [kernel.id], SOL, 12)
  const points = kernel.roofline.filter((p) => p.x && p.y)
  return (
    <div className="dash">
      <KernelTableCard workload={workload} kernel={kernel} />
      <div className="split">
        <div className="card">
          <div className="card-title">System speed-of-light <span className="hint faint ellipsis" style={{ maxWidth: 320 }}>{shortName(kernel.name)}</span></div>
          {sol.data?.kernels[0] ? <SolTable metrics={sol.data.kernels[0].metrics} /> : sol.error ? <Empty title={sol.error} /> : <Loading />}
        </div>
        <div className="card">
          <div className="card-title">Kernel roofline</div>
          <div className="pad" style={{ paddingTop: 0 }}>
            <Roofline
              bandwidth={workload.roofline.bandwidth_ceilings}
              compute={workload.roofline.compute_ceilings}
              points={points.map((p) => ({ x: p.x!, y: p.y!, label: `${p.type.toUpperCase()} intensity`, color: { hbm: '#ff6b6b', l2: '#f7c548', l1: '#48b5ff', lds: '#5fd39a' }[p.type] ?? '#fff', active: true }))}
            />
          </div>
        </div>
      </div>
      <MemoryChart workload={workload} kernel={kernel} />
    </div>
  )
}

function KernelTableCard({ workload, kernel }: { workload: Workload; kernel: Kernel }) {
  const set = useStore((s) => s.set)
  const [selectors, setSelectors] = useState<{ id: string; label: string }[]>([])
  const [sort, setSort] = useState<{ column: number; order: 'asc' | 'desc' }>({ column: 2, order: 'desc' })
  const [inputs, setInputs] = useState<Record<number, string>>({})
  const [filters, setFilters] = useState<Record<number, string>>({})
  const table = useAsync(
    () =>
      get<KernelTable>(`/api/compute/workloads/${workload.id}/kernel_table`, {
        metrics: selectors.map((s) => s.id).join(','),
        sort: sort.column,
        order: sort.order,
        ...Object.fromEntries(Object.entries(filters).filter(([, v]) => v.trim()).map(([c, v]) => [`filter_${c}`, v.trim()])),
      }),
    [workload.id, selectors, sort, filters],
  )
  const columns = table.data?.columns ?? []
  const rows = table.data?.rows ?? []
  const maxima = columns.map((_, c) => Math.max(1e-12, ...rows.map((r) => Number(r[c]) || 0)))
  const label = (name: string) =>
    ({ kernel_name: 'Kernel', duration_ns_sum: 'Duration (ns)', dispatch_count: 'Dispatches' })[name] ??
    selectors.find((s) => `metric_${s.id.replace(/[^a-z0-9]+/gi, '_').toLowerCase()}` === name.toLowerCase())?.label ??
    name

  return (
    <div className="card" style={{ overflow: 'hidden' }}>
      <div className="card-title">
        Kernels
        <span className="hint row" style={{ flexWrap: 'wrap', justifyContent: 'flex-end' }}>
          {selectors.map((s) => (
            <span key={s.id} className="badge neutral" title={s.id}>
              {s.label}
              <button className="icon-btn small" style={{ width: 16, height: 16 }} onClick={() => setSelectors(selectors.filter((x) => x.id !== s.id))}>
                <X size={11} />
              </button>
            </span>
          ))}
          <MetricPicker workload={workload} onAdd={(id, text) => !selectors.some((s) => s.id === id) && setSelectors([...selectors, { id, label: text }])} />
          {table.loading && <div className="spinner" />}
        </span>
      </div>
      {table.data?.error && <div className="pad" style={{ color: 'var(--red-hi)', paddingTop: 0 }}>{table.data.error}</div>}
      <div className="grid" style={{ maxHeight: 340 }}>
        <table>
          <thead>
            <tr>
              {columns.map((c, i) =>
                c.name.startsWith('__') ? null : (
                  <th key={c.name} onClick={() => setSort({ column: i, order: sort.column === i && sort.order === 'desc' ? 'asc' : 'desc' })}>
                    <span className="row" style={{ gap: 4 }}>
                      {label(c.name)}
                      {sort.column === i && (sort.order === 'asc' ? <ArrowUp size={12} /> : <ArrowDown size={12} />)}
                    </span>
                  </th>
                ),
              )}
            </tr>
            <tr className="filters">
              {columns.map((c, i) =>
                c.name.startsWith('__') ? null : (
                  <th key={c.name}>
                    <input
                      className="input small mono"
                      placeholder={i === 1 ? 'contains' : '> 0'}
                      value={inputs[i] ?? ''}
                      onChange={(e) => setInputs({ ...inputs, [i]: e.target.value })}
                      onKeyDown={(e) => e.key === 'Enter' && setFilters(inputs)}
                    />
                  </th>
                ),
              )}
            </tr>
          </thead>
          <tbody>
            {rows.map((row) => (
              <tr key={row[0]} className={`clickable ${Number(row[0]) === kernel.id ? 'on' : ''}`} onClick={() => set({ kernelId: Number(row[0]) })}>
                {row.map((value, c) =>
                  columns[c]?.name.startsWith('__') ? null : c === 1 ? (
                    <td key={c} title={value}>{shortName(value)}</td>
                  ) : (
                    <td key={c} className="num bar-cell">
                      <i style={{ width: `${(100 * (Number(value) || 0)) / maxima[c]}%` }} />
                      <span>{formatNumber(Number(value), 2)}</span>
                    </td>
                  ),
                )}
              </tr>
            ))}
          </tbody>
        </table>
        {!rows.length && !table.loading && <Empty title="No kernels match" />}
      </div>
    </div>
  )
}

// Picks a metric value to add as a kernel table column, category by category.
function MetricPicker({ workload, onAdd }: { workload: Workload; onAdd: (selector: string, label: string) => void }) {
  const [category, setCategory] = useState(workload.metrics[0]?.id)
  const tables = workload.metrics.find((c) => c.id === category)?.tables ?? []
  const [tableId, setTableId] = useState<number | undefined>()
  const table = tables.find((t) => t.id === tableId) ?? tables[0]
  const [entryId, setEntryId] = useState<number | undefined>()
  const entry = table?.entries.find((e) => e.id === entryId) ?? table?.entries[0]
  const [valueName, setValueName] = useState<string | undefined>()
  const value = table?.value_names.includes(valueName ?? '') ? valueName! : table?.value_names[0]
  if (!workload.metrics.length) return null
  return (
    <>
      <select className="select small" style={{ maxWidth: 150 }} value={category} onChange={(e) => { setCategory(Number(e.target.value)); setTableId(undefined); setEntryId(undefined) }}>
        {workload.metrics.map((c) => <option key={c.id} value={c.id}>{c.name}</option>)}
      </select>
      <select className="select small" style={{ maxWidth: 150 }} value={table?.id} onChange={(e) => { setTableId(Number(e.target.value)); setEntryId(undefined) }}>
        {tables.map((t) => <option key={t.id} value={t.id}>{t.name}</option>)}
      </select>
      <select className="select small" style={{ maxWidth: 170 }} value={entry?.id} onChange={(e) => setEntryId(Number(e.target.value))}>
        {table?.entries.map((x) => <option key={x.id} value={x.id}>{x.name}</option>)}
      </select>
      <select className="select small" value={value} onChange={(e) => setValueName(e.target.value)}>
        {table?.value_names.map((v) => <option key={v}>{v}</option>)}
      </select>
      <button className="btn small" disabled={!entry || !value} onClick={() => entry && value && onAdd(`${entry.metric_id}:${value}`, `${entry.name} ${value}${entry.unit ? ` (${entry.unit})` : ''}`)}>
        <Plus size={13} /> Column
      </button>
    </>
  )
}

interface ChartItem {
  metric: string
  title: string
  unit: string
}

interface ChartBlock {
  id: string
  column: number
  order: number
  title: string
  items: ChartItem[]
  children: ChartBlock[]
}

interface ChartArrow {
  from: string
  to: string
  direction: string
  metric: string
  title: string
}

// The layout the desktop app ships per GPU architecture, used when the database has none.
const LAYOUTS = import.meta.glob('../../../resources/memory_chart/gfx*.json', { eager: true, import: 'default' }) as Record<
  string,
  { blocks: RawBlock[]; arrows: (Omit<ChartArrow, 'metric' | 'title'> & { metric?: string; metric_name?: string; title?: string })[] }
>

interface RawBlock {
  id: string
  column: number
  order?: number
  title: string
  content: { metric: string | MetricRef; metric_name?: string; title: string; unit?: string }[]
  children?: RawBlock[]
}

function chooseLayout(workload: Workload): { blocks: ChartBlock[]; arrows: ChartArrow[] } | null {
  const byName = new Map(
    (workload.metrics.find((c) => String(c.id) === MEMORY_CHART_CATEGORY)?.tables ?? []).flatMap((t) => t.entries.map((e) => [e.name, e.metric_id] as const)),
  )
  const resolve = (metric: string | MetricRef | undefined, name?: string) =>
    !metric ? '' : typeof metric === 'string' ? (name && byName.get(name)) || metric : metric.metric_id ?? (byName.get(metric.metric_name) || '')
  const block = (raw: RawBlock | MemoryBlock): ChartBlock => ({
    id: raw.id,
    column: raw.column,
    order: raw.order ?? 0,
    title: raw.title,
    items: (raw.content as RawBlock['content']).map((c) => ({ metric: resolve(c.metric, c.metric_name), title: c.title, unit: c.unit ?? '' })),
    children: ((raw.children ?? []) as RawBlock[]).map(block),
  })
  if (workload.memory_chart.blocks.length) {
    return {
      blocks: workload.memory_chart.blocks.map(block),
      arrows: workload.memory_chart.arrows.map((a) => ({ ...a, metric: resolve(a.metric) })),
    }
  }
  const arch = pairs(workload.system_info).find(([k]) => k === 'gpu arch')?.[1] ?? ''
  const layouts = Object.entries(LAYOUTS).map(([path, layout]) => [fileName(path).replace('.json', ''), layout] as const)
  const match =
    layouts.find(([name]) => name === arch) ?? layouts.find(([name]) => name.endsWith('x') && arch.startsWith(name.slice(0, -1)))
  if (!match) return null
  return {
    blocks: match[1].blocks.map(block),
    arrows: match[1].arrows.map((a) => ({ ...a, title: a.title ?? '', metric: resolve(a.metric, a.metric_name) })),
  }
}

function MemoryChart({ workload, kernel }: { workload: Workload; kernel: Kernel }) {
  const layout = useMemo(() => chooseLayout(workload), [workload])
  const ids = (blocks: ChartBlock[]): string[] => blocks.flatMap((b) => [...b.items.map((i) => i.metric), ...ids(b.children)])
  const categories = layout
    ? [...new Set([...ids(layout.blocks), ...layout.arrows.map((a) => a.metric)].filter(Boolean).map((id) => id.split('.')[0]))].join(',')
    : ''
  const metrics = useMetrics(workload.id, [kernel.id], categories, 13)
  const values = useMemo(
    () => new Map<string, MetricValue>((metrics.data?.kernels[0]?.metrics ?? []).map((m) => [m.metric_id, m])),
    [metrics.data],
  )
  const container = useRef<HTMLDivElement>(null)
  const [geometry, setGeometry] = useState<Record<string, DOMRect>>({})

  useLayoutEffect(() => {
    const element = container.current
    if (!element) return
    const measure = () => {
      const origin = element.getBoundingClientRect()
      const next: Record<string, DOMRect> = {}
      element.querySelectorAll<HTMLElement>('[data-block]').forEach((node) => {
        const box = node.getBoundingClientRect()
        next[node.dataset.block!] = new DOMRect(box.left - origin.left + element.scrollLeft, box.top - origin.top, box.width, box.height)
      })
      setGeometry(next)
    }
    measure()
    const observer = new ResizeObserver(measure)
    observer.observe(element)
    return () => observer.disconnect()
  }, [layout, values])

  if (!layout) return <div className="card"><div className="card-title">Memory chart</div><Empty title="Memory chart is N/A for this workload." /></div>
  const valueOf = (metric: string, unit = '') => {
    const value = values.get(metric)
    const number = value ? value.values.Value ?? Object.values(value.values)[0] : null
    return number === null || number === undefined ? 'N/A' : `${formatNumber(number, 2)}${unit || value?.unit ? ` ${unit || value?.unit}` : ''}`
  }
  const columns = [...new Set(layout.blocks.map((b) => b.column))].sort((a, b) => a - b)
  const renderBlock = (b: ChartBlock) => (
    <div key={b.id} className="memblock" data-block={b.id}>
      <div className="memblock-title">{b.title}</div>
      {b.items.map((item, i) => (
        <div key={i} className="metric-line"><span>{item.title}</span><b>{valueOf(item.metric, item.unit)}</b></div>
      ))}
      {b.children.map(renderBlock)}
    </div>
  )
  const pairsOf = new Map<string, ChartArrow[]>()
  for (const arrow of layout.arrows) {
    const key = `${arrow.from}>${arrow.to}`
    pairsOf.set(key, [...(pairsOf.get(key) ?? []), arrow])
  }

  return (
    <div className="card" style={{ overflow: 'hidden' }}>
      <div className="card-title">Memory chart <span className="hint faint">{metrics.loading ? 'Loading metrics…' : shortName(kernel.name)}</span></div>
      <div className="memchart" ref={container} style={{ position: 'relative', gap: 150 }}>
        <svg style={{ position: 'absolute', inset: 0, width: '100%', height: '100%', pointerEvents: 'none', overflow: 'visible' }}>
          <defs>
            <marker id="arrow" viewBox="0 0 8 8" refX="7" refY="4" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
              <path d="M0 0 L8 4 L0 8 z" fill="#ff7a59" />
            </marker>
          </defs>
          {[...pairsOf.entries()].map(([key, arrows]) => {
            const [a, b] = [geometry[arrows[0].from], geometry[arrows[0].to]]
            if (!a || !b) return null
            const forward = b.left >= a.right
            const [x0, y0] = forward ? [a.right, a.top + a.height / 2] : [a.left + a.width / 2, a.bottom]
            const [x1, y1] = forward ? [b.left, b.top + b.height / 2] : [b.left + b.width / 2, b.top]
            const bend = Math.max(30, Math.abs(x1 - x0) / 2)
            const heads = new Set(arrows.map((r) => r.direction))
            const labelled = arrows.filter((r) => r.metric || r.title)
            return (
              <g key={key}>
                <path
                  d={forward ? `M${x0} ${y0} C${x0 + bend} ${y0} ${x1 - bend} ${y1} ${x1} ${y1}` : `M${x0} ${y0} L${x1} ${y1}`}
                  stroke="#ff7a59"
                  strokeOpacity={0.7}
                  strokeWidth={1.6}
                  fill="none"
                  markerEnd={heads.has('forward') || heads.has('both') ? 'url(#arrow)' : undefined}
                  markerStart={heads.has('backward') || heads.has('both') ? 'url(#arrow)' : undefined}
                />
                <foreignObject x={x0 + (x1 - x0) * 0.62 - 58} y={y0 + (y1 - y0) * 0.75 - 9 * labelled.length} width={116} height={18 * labelled.length + 4}>
                  <div style={{ display: 'grid', gap: 2 }}>
                    {labelled.map((r, i) => (
                      <div key={i} className="badge neutral" style={{ height: 16, fontSize: 10, textTransform: 'none', justifyContent: 'space-between', background: 'var(--panel-2)', border: '1px solid var(--line)' }} title={r.title}>
                        <span className="ellipsis" style={{ maxWidth: 58 }}>{r.title}</span>
                        <b style={{ color: 'var(--text)' }}>{r.metric ? valueOf(r.metric).replace(/ .*/, '') : ''}</b>
                      </div>
                    ))}
                  </div>
                </foreignObject>
              </g>
            )
          })}
        </svg>
        {columns.map((column) => (
          <div className="memcol" key={column} style={{ position: 'relative' }}>
            {layout.blocks.filter((b) => b.column === column).sort((a, b) => a.order - b.order).map(renderBlock)}
          </div>
        ))}
      </div>
    </div>
  )
}
