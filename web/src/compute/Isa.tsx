import { useMemo, useState } from 'react'
import { get, type Kernel, type PcSampling, type Workload } from '../api'
import { formatNumber } from '../format'
import { Empty, Loading, useAsync } from '../ui'

// Loads the ISA, the sample counts and a source file, then shows samples per
// instruction beside the source line each instruction came from.
export function Isa({ workload, kernel }: { workload: Workload; kernel: Kernel }) {
  const [sourceFile, setSourceFile] = useState<string | null>(null)
  const [selected, setSelected] = useState<string | null>(null)
  const path = `/api/compute/workloads/${workload.id}/kernels/${kernel.id}/pc_sampling`
  const data = useAsync(async () => {
    await get(path, { layer: 'isa' })
    await get(path, { layer: 'stalls' })
    return get<PcSampling>(path, { layer: 'source', source_file: sourceFile })
  }, [path, sourceFile])

  const model = useMemo(() => {
    const d = data.data
    if (!d) return null
    const instructions = d.code_objects.flatMap((c) => c.kernel_symbols.flatMap((s) => s.instructions))
    const states = new Map(d.sample_states.map((s) => [s.instruction_uuid, s]))
    const lookups = new Map(d.stall_reason_lookups.map((l) => [l.uuid, l.text]))
    const reasons = new Map<string, { text: string; count: number }[]>()
    for (const r of d.stall_reasons) {
      reasons.set(r.sample_state_uuid, [...(reasons.get(r.sample_state_uuid) ?? []), { text: lookups.get(r.lookup_uuid) ?? r.lookup_uuid, count: r.count }])
    }
    const lineOf = new Map(d.instruction_source_lines.map((m) => [m.instruction_uuid, m.source_line_uuid]))
    const max = Math.max(1, ...d.sample_states.map((s) => s.total_count))
    return { instructions, states, reasons, lineOf, max }
  }, [data.data])

  if (data.error) return <Empty title="ISA could not be loaded" hint={data.error} />
  if (!model || !data.data) return <Loading text="Loading ISA and samples…" />
  if (!model.instructions.length) return <Empty title="No ISA for this kernel" />
  const file = data.data.source_files.find((f) => f.uuid === sourceFile) ?? data.data.source_files[0]
  const line = selected ? model.lineOf.get(selected) : undefined
  const state = selected ? model.states.get(selected) : undefined
  const stalls = state ? model.reasons.get(state.uuid) ?? [] : []

  return (
    <div className="dash" style={{ gridTemplateColumns: 'minmax(0, 1.2fr) minmax(0, 1fr)', alignItems: 'start' }}>
      <div className="card" style={{ overflow: 'hidden' }}>
        <div className="card-title">Instructions <span className="hint faint">{model.instructions.length} · {data.data.sample_states_loaded ? 'with PC samples' : 'no samples'}</span></div>
        <div className="code" style={{ maxHeight: 'calc(100vh - 260px)', overflow: 'auto' }}>
          {model.instructions.map((instruction) => {
            const sample = model.states.get(instruction.uuid)
            return (
              <div key={instruction.uuid} className={`code-line ${selected === instruction.uuid ? 'on' : ''}`} onClick={() => setSelected(instruction.uuid)}>
                <span className="faint">{instruction.offset.toString(16).padStart(6, '0')}</span>
                <span className="ellipsis">{instruction.text}</span>
                <span className="row" style={{ gap: 6 }}>
                  <span className="heat grow" style={{ marginTop: 0 }}>
                    <span style={{ width: `${(100 * (sample?.total_count ?? 0)) / model.max}%`, background: 'linear-gradient(90deg, #ff9f43, var(--red))' }} />
                  </span>
                  <span className="faint" style={{ width: 46, textAlign: 'right' }}>{sample ? formatNumber(sample.total_count, 0) : ''}</span>
                </span>
              </div>
            )
          })}
        </div>
      </div>
      <div className="col" style={{ gap: 16 }}>
        {state && (
          <div className="card pad col">
            <span className="section-label">Selected instruction</span>
            <div className="kv">
              <span>Samples</span><span className="mono">{formatNumber(state.total_count, 0)}</span>
              <span>Issued</span><span className="mono">{formatNumber(state.issue_count, 0)}</span>
              <span>Stalled</span><span className="mono">{formatNumber(state.stall_count, 0)}</span>
            </div>
            {stalls.sort((a, b) => b.count - a.count).map((s) => (
              <div key={s.text} className="hbar" style={{ cursor: 'default' }}>
                <span className="ellipsis">{s.text}</span>
                <span className="mono muted">{formatNumber(s.count, 0)}</span>
                <div className="track"><div style={{ width: `${(100 * s.count) / Math.max(1, state.stall_count ?? state.total_count)}%`, background: 'var(--red)' }} /></div>
              </div>
            ))}
          </div>
        )}
        <div className="card" style={{ overflow: 'hidden' }}>
          <div className="card-title">
            Source
            {data.data.source_files.length > 1 && (
              <select className="select small hint" value={file?.uuid} onChange={(e) => setSourceFile(e.target.value)}>
                {data.data.source_files.map((f) => <option key={f.uuid} value={f.uuid}>{f.path}</option>)}
              </select>
            )}
          </div>
          {file?.lines.length ? (
            <div className="code" style={{ maxHeight: 'calc(100vh - 360px)', overflow: 'auto' }}>
              {file.lines.map((l) => (
                <div key={l.uuid} className={`code-line ${l.uuid === line ? 'on' : ''}`} style={{ gridTemplateColumns: '48px 1fr' }}>
                  <span className="faint">{l.line_number}</span>
                  <span>{l.content}</span>
                </div>
              ))}
            </div>
          ) : (
            <Empty title="No source was captured" />
          )}
        </div>
      </div>
    </div>
  )
}
