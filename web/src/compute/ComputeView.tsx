import { useEffect, useMemo, useState } from 'react'
import { Binary, Cpu, GitCompare, Info, LayoutDashboard, Microscope, Table2 } from 'lucide-react'
import { get, type Compute, type Kernel, type Metrics, type MetricValue, type Workload } from '../api'
import { Donut, Roofline } from '../charts'
import { colorFor, formatDuration, formatNumber } from '../format'
import { useStore } from '../store'
import { SimpleTable } from '../table'
import { Boundary, Empty, Loading, Segmented, Tabs, useAsync } from '../ui'
import { Isa } from './Isa'
import { KernelDetails } from './KernelDetails'
import { Comparison, TableView } from './Metrics'

export const SOL = '2.1'

// A workload's values, from rows of keys and values or of key-value pairs.
export function pairs(rows: string[][]): [string, string][] {
  if (rows.length === 2 && rows[0].length > 2) return rows[0].map((key, i) => [key, rows[1][i] ?? ''])
  return rows.map((row) => [row[0] ?? '', row.slice(1).join(' ')])
}

export function useMetrics(workload: number, kernels: number[] | null, metrics: string, client: number) {
  return useAsync(
    () =>
      metrics
        ? get<Metrics>(`/api/compute/workloads/${workload}/metrics`, { kernels: kernels?.join(','), metrics, client })
        : null,
    [workload, kernels?.join(','), metrics, client],
  )
}

export const shortName = (name: string) => {
  const plain = name.replace(/^void\s+/, '')
  const cut = plain.indexOf('(')
  return cut > 0 ? plain.slice(0, cut) : plain
}

type Tab = 'summary' | 'kernel' | 'tables' | 'compare' | 'profile' | 'isa'

export function ComputeView() {
  const workloadId = useStore((s) => s.workloadId)
  const kernelId = useStore((s) => s.kernelId)
  const set = useStore((s) => s.set)
  const compute = useAsync(() => get<Compute>('/api/compute'), [])
  const [tab, setTab] = useState<Tab>('summary')
  const workload = compute.data?.workloads.find((w) => w.id === workloadId) ?? compute.data?.workloads[0]
  const kernel = workload?.kernels.find((k) => k.id === kernelId) ?? busiest(workload)

  useEffect(() => {
    if (workload && workload.id !== workloadId) set({ workloadId: workload.id })
    if (kernel && kernel.id !== kernelId) set({ kernelId: kernel.id })
  }, [workload, kernel])

  if (compute.error) return <Empty title="This compute profile could not be loaded" hint={compute.error} />
  if (!compute.data || !workload) return <Loading text="Reading the profile…" />
  if (!kernel) return <Empty title="This workload has no kernels" />
  const hasMetrics = workload.metrics.length > 0

  return (
    <div className="compute">
      <div className="compute-bar">
        <Cpu size={16} color="var(--red)" />
        {compute.data.workloads.length > 1 ? (
          <select className="select" value={workload.id} onChange={(e) => set({ workloadId: Number(e.target.value), kernelId: null })}>
            {compute.data.workloads.map((w) => <option key={w.id} value={w.id}>{w.name}</option>)}
          </select>
        ) : (
          <span style={{ fontWeight: 650 }}>{workload.name}</span>
        )}
        <span className="faint">/</span>
        <select className="select grow" style={{ maxWidth: 640 }} value={kernel.id} onChange={(e) => set({ kernelId: Number(e.target.value) })}>
          {[...workload.kernels]
            .sort((a, b) => b.dispatch.duration_total_ns - a.dispatch.duration_total_ns)
            .map((k) => (
              <option key={k.id} value={k.id}>
                {shortName(k.name)} — {formatDuration(k.dispatch.duration_total_ns)}
              </option>
            ))}
        </select>
        <span className="badge neutral">{workload.kernels.length} kernels</span>
      </div>
      <Tabs
        value={tab}
        onChange={setTab}
        tabs={[
          { id: 'summary', label: 'Summary', icon: LayoutDashboard },
          { id: 'kernel', label: 'Kernel details', icon: Microscope },
          { id: 'tables', label: 'Table view', icon: Table2, disabled: !hasMetrics, title: hasMetrics ? undefined : 'This profile has no metric tables' },
          { id: 'compare', label: 'Baseline comparison', icon: GitCompare, disabled: !hasMetrics },
          { id: 'profile', label: 'Profile details', icon: Info },
          { id: 'isa', label: 'ISA view', icon: Binary, disabled: !kernel.has_isa_lines, title: kernel.has_isa_lines ? undefined : 'No ISA was captured for this kernel' },
        ]}
      />
      <div className="compute-body">
        <Boundary key={tab}>
          {tab === 'summary' && <ComputeSummary workload={workload} kernel={kernel} />}
          {tab === 'kernel' && <KernelDetails workload={workload} kernel={kernel} />}
          {tab === 'tables' && <TableView workload={workload} kernel={kernel} />}
          {tab === 'compare' && <Comparison workloads={compute.data.workloads} workload={workload} kernel={kernel} />}
          {tab === 'profile' && <ProfileDetails compute={compute.data} workload={workload} />}
          {tab === 'isa' && <Isa workload={workload} kernel={kernel} />}
        </Boundary>
      </div>
    </div>
  )
}

function busiest(workload?: Workload): Kernel | undefined {
  return workload?.kernels.reduce<Kernel | undefined>((a, k) => (!a || k.dispatch.duration_total_ns > a.dispatch.duration_total_ns ? k : a), undefined)
}

const DISPATCH_METRICS = [
  { id: 'duration_total_ns', label: 'Total duration', time: true },
  { id: 'invocation_count', label: 'Invocations', time: false },
  { id: 'duration_mean_ns', label: 'Mean duration', time: true },
  { id: 'duration_median_ns', label: 'Median duration', time: true },
  { id: 'duration_min_ns', label: 'Min duration', time: true },
  { id: 'duration_max_ns', label: 'Max duration', time: true },
] as const

function ComputeSummary({ workload, kernel }: { workload: Workload; kernel: Kernel }) {
  const set = useStore((s) => s.set)
  const [metric, setMetric] = useState<(typeof DISPATCH_METRICS)[number]['id']>('duration_total_ns')
  const [mode, setMode] = useState<'bar' | 'pie'>('bar')
  const [intensity, setIntensity] = useState('hbm')
  const sol = useMetrics(workload.id, null, SOL, 11)
  const chosen = DISPATCH_METRICS.find((m) => m.id === metric)!
  const ranked = useMemo(() => [...workload.kernels].sort((a, b) => b.dispatch[metric] - a.dispatch[metric]), [workload, metric])
  const top = ranked.slice(0, 10)
  const max = Math.max(1, ...top.map((k) => k.dispatch[metric]))
  const total = workload.kernels.reduce((s, k) => s + k.dispatch.duration_total_ns, 0)
  const show = (v: number) => (chosen.time ? formatDuration(v) : formatNumber(v, 0))

  return (
    <div className="dash">
      <div className="tiles">
        <div className="tile"><div className="section-label">Kernels</div><div className="value">{workload.kernels.length}</div></div>
        <div className="tile"><div className="section-label">Dispatches</div><div className="value">{formatNumber(workload.kernels.reduce((s, k) => s + k.dispatch.invocation_count, 0), 0)}</div></div>
        <div className="tile"><div className="section-label">GPU time</div><div className="value">{formatDuration(total)}</div></div>
        <div className="tile"><div className="section-label">Architecture</div><div className="value">{pairs(workload.system_info).find(([k]) => k === 'gpu arch')?.[1] ?? '–'}</div></div>
        <div className="tile"><div className="section-label">GPU</div><div className="value ellipsis">{pairs(workload.system_info).find(([k]) => k === 'gpu model')?.[1] ?? '–'}</div></div>
      </div>
      <div className="split">
        <div className="card">
          <div className="card-title">
            Top kernels
            <span className="hint row">
              <select className="select small" value={metric} onChange={(e) => setMetric(e.target.value as typeof metric)}>
                {DISPATCH_METRICS.map((m) => <option key={m.id} value={m.id}>{m.label}</option>)}
              </select>
              <Segmented value={mode} onChange={setMode} options={[{ id: 'bar', label: 'Bars' }, { id: 'pie', label: 'Pie' }]} />
            </span>
          </div>
          <div className="pad" style={{ paddingTop: 0 }}>
            {mode === 'pie' ? (
              <div className="row" style={{ justifyContent: 'center' }}>
                <Donut size={220} center={chosen.label} slices={top.map((k) => ({ label: shortName(k.name), value: k.dispatch[metric], color: colorFor(k.name) }))} />
              </div>
            ) : (
              top.map((k) => (
                <div key={k.id} className={`hbar ${k.id === kernel.id ? 'on' : ''}`} onClick={() => set({ kernelId: k.id })} title={k.name}>
                  <span className="ellipsis">{shortName(k.name)}</span>
                  <span className="mono muted">{show(k.dispatch[metric])}</span>
                  <div className="track"><div style={{ width: `${(100 * k.dispatch[metric]) / max}%`, background: colorFor(k.name) }} /></div>
                </div>
              ))
            )}
          </div>
        </div>
        <div className="card">
          <div className="card-title">
            Workload roofline
            <span className="hint">
              <Segmented value={intensity} onChange={setIntensity} options={['hbm', 'l2', 'l1'].map((id) => ({ id, label: id.toUpperCase() }))} />
            </span>
          </div>
          <div className="pad" style={{ paddingTop: 0 }}>
            <Roofline
              bandwidth={workload.roofline.bandwidth_ceilings}
              compute={workload.roofline.compute_ceilings}
              points={workload.kernels.flatMap((k) =>
                k.roofline
                  .filter((p) => p.type === intensity && p.x && p.y)
                  .map((p) => ({ x: p.x!, y: p.y!, label: shortName(k.name), color: colorFor(k.name), active: k.id === kernel.id })),
              )}
            />
          </div>
        </div>
      </div>
      <div className="card">
        <div className="card-title">System speed-of-light <span className="hint faint">whole workload</span></div>
        {sol.data?.workload ? <SolTable metrics={sol.data.workload.metrics} /> : <Loading />}
      </div>
    </div>
  )
}

export function SolTable({ metrics }: { metrics: MetricValue[] }) {
  return (
    <SimpleTable
      columns={['Metric', 'Avg', 'Peak', 'Pct of peak', 'Unit']}
      rows={metrics.map((m) => [m.name, m.values.Avg ?? null, m.values.Peak ?? null, m.values['Pct of Peak'] ?? null, m.unit])}
      bars={[3]}
      maxHeight={420}
    />
  )
}

function ProfileDetails({ compute, workload }: { compute: Compute; workload: Workload }) {
  const sections: [string, [string, string][]][] = [
    ['System information', pairs(workload.system_info)],
    ['Profiling configuration', pairs(workload.profiling_config)],
  ]
  return (
    <div className="dash">
      <div className="card">
        <div className="card-title">Analysis information</div>
        <div className="kv pad" style={{ paddingTop: 0 }}>
          <span>ROCm Compute Profiler</span><span className="mono">{compute.profiler.version || '–'}</span>
          <span>Git revision</span><span className="mono">{compute.profiler.git_version || '–'}</span>
          <span>Database schema</span><span className="mono">{compute.profiler.schema_version || '–'}</span>
        </div>
      </div>
      <div className="split">
        {sections.map(([title, rows]) => (
          <div className="card" key={title}>
            <div className="card-title">{title}</div>
            <div className="kv pad" style={{ paddingTop: 0 }}>
              {rows.flatMap(([key, value], i) => [<span key={`k${i}`}>{key}</span>, <span key={`v${i}`} className="mono">{value || '–'}</span>])}
            </div>
          </div>
        ))}
      </div>
    </div>
  )
}
