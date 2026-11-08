import { useState } from 'react'
import type { Ceiling } from './api'
import { formatNumber } from './format'

export interface Slice {
  label: string
  value: number
  color: string
}

export function Donut({ slices, size = 180, center }: { slices: Slice[]; size?: number; center?: string }) {
  const [hover, setHover] = useState<number | null>(null)
  const total = slices.reduce((sum, s) => sum + Math.max(0, s.value), 0) || 1
  const radius = size / 2 - 10
  let angle = -Math.PI / 2
  const point = (a: number, r: number) => [size / 2 + r * Math.cos(a), size / 2 + r * Math.sin(a)]
  return (
    <svg width={size} height={size} className="chart">
      {slices.map((slice, index) => {
        const sweep = (Math.max(0, slice.value) / total) * Math.PI * 2
        const [x0, y0] = point(angle, radius)
        const [x1, y1] = point(angle + Math.min(sweep, Math.PI * 2 - 1e-4), radius)
        angle += sweep
        return (
          <path
            key={index}
            d={`M${x0} ${y0} A${radius} ${radius} 0 ${sweep > Math.PI ? 1 : 0} 1 ${x1} ${y1}`}
            fill="none"
            stroke={slice.color}
            strokeWidth={hover === index ? 22 : 18}
            style={{ transition: 'stroke-width .15s', cursor: 'default' }}
            onMouseEnter={() => setHover(index)}
            onMouseLeave={() => setHover(null)}
          >
            <title>{`${slice.label}: ${((100 * slice.value) / total).toFixed(1)}%`}</title>
          </path>
        )
      })}
      <text x={size / 2} y={size / 2 - 4} textAnchor="middle" style={{ fill: 'var(--text)', fontSize: 18, fontWeight: 700 }}>
        {hover !== null ? `${((100 * slices[hover].value) / total).toFixed(1)}%` : center}
      </text>
      <text x={size / 2} y={size / 2 + 16} textAnchor="middle">
        {hover !== null ? slices[hover].label.slice(0, 22) : ''}
      </text>
    </svg>
  )
}

export interface RooflinePoint {
  x: number
  y: number
  label: string
  color: string
  active?: boolean
}

const PRECISIONS = ['fp4', 'fp6', 'fp8', 'fp16', 'bf16', 'fp32', 'fp64', 'i8', 'i32', 'i64']

// Roofline rates arrive in GFLOP/s; label them G, T or P.
const gigaFlops = (v: number) =>
  v >= 1e6 ? `${formatNumber(v / 1e6, 3)}P` : v >= 1e3 ? `${formatNumber(v / 1e3, 3)}T` : `${formatNumber(v, 3)}G`
const BANDWIDTH_COLORS: Record<string, string> = { hbm: '#ff6b6b', l2: '#f7c548', l1: '#48b5ff', lds: '#5fd39a' }

// Log-log roofline: bandwidth ceilings rise from the left, compute ceilings
// are flat, and kernels sit at their arithmetic intensity.
export function Roofline(props: {
  bandwidth: Ceiling[]
  compute: Ceiling[]
  points: RooflinePoint[]
  height?: number
}) {
  const [precision, setPrecision] = useState('fp32')
  const width = 640
  const height = props.height ?? 360
  const pad = { left: 66, right: 16, top: 14, bottom: 38 }
  const matches = (ceiling: Ceiling) => ceiling.compute.endsWith(precision)
  const ceilings = [...props.bandwidth, ...props.compute].filter(matches)
  const coordinates = [
    ...ceilings.flatMap((c) => [c.from, c.to]),
    ...props.points,
  ].filter((p): p is { x: number; y: number } => p.x !== null && p.y !== null && p.x > 0 && p.y > 0)
  const available = PRECISIONS.filter((p) => [...props.bandwidth, ...props.compute].some((c) => c.compute.endsWith(p)))
  if (!coordinates.length) return <div className="empty">No roofline data for this workload.</div>

  const extent = (values: number[]) => [
    10 ** Math.floor(Math.log10(Math.min(...values))),
    10 ** Math.ceil(Math.log10(Math.max(...values))),
  ]
  const [xMin, xMax] = extent(coordinates.map((p) => p.x))
  const [yMin, yMax] = extent(coordinates.map((p) => p.y))
  const x = (v: number) => pad.left + ((Math.log10(v) - Math.log10(xMin)) / (Math.log10(xMax) - Math.log10(xMin))) * (width - pad.left - pad.right)
  const y = (v: number) => height - pad.bottom - ((Math.log10(v) - Math.log10(yMin)) / (Math.log10(yMax) - Math.log10(yMin))) * (height - pad.top - pad.bottom)
  const decades = (min: number, max: number) =>
    Array.from({ length: Math.round(Math.log10(max / min)) + 1 }, (_, i) => min * 10 ** i)

  return (
    <div className="col">
      <div className="row" style={{ flexWrap: 'wrap', gap: 4 }}>
        {available.map((p) => (
          <button key={p} className={`btn small ${p === precision ? 'primary' : 'ghost'}`} onClick={() => setPrecision(p)}>
            {p.toUpperCase()}
          </button>
        ))}
      </div>
      <svg viewBox={`0 0 ${width} ${height}`} className="chart" style={{ width: '100%', maxHeight: height }}>
        {decades(xMin, xMax).map((v) => (
          <g key={`x${v}`}>
            <line className="gridline" x1={x(v)} x2={x(v)} y1={pad.top} y2={height - pad.bottom} />
            <text x={x(v)} y={height - pad.bottom + 16} textAnchor="middle">{formatNumber(v, 3)}</text>
          </g>
        ))}
        {decades(yMin, yMax).map((v) => (
          <g key={`y${v}`}>
            <line className="gridline" x1={pad.left} x2={width - pad.right} y1={y(v)} y2={y(v)} />
            <text x={pad.left - 8} y={y(v) + 4} textAnchor="end">{gigaFlops(v)}</text>
          </g>
        ))}
        <text x={(width + pad.left) / 2} y={height - 4} textAnchor="middle">Arithmetic intensity (FLOPs/byte)</text>
        <text transform={`translate(12 ${(height - pad.bottom + pad.top) / 2}) rotate(-90)`} textAnchor="middle">Performance (FLOP/s)</text>
        {ceilings.map((c, i) =>
          c.from.x && c.from.y && c.to.x && c.to.y ? (
            <g key={i}>
              <line
                x1={x(c.from.x)}
                y1={y(c.from.y)}
                x2={x(c.to.x)}
                y2={y(c.to.y)}
                stroke={BANDWIDTH_COLORS[c.bandwidth] ?? 'var(--text-2)'}
                strokeWidth={2}
                strokeDasharray={c.compute.startsWith('valu') ? '5 4' : undefined}
                opacity={0.9}
              >
                <title>{`${c.bandwidth.toUpperCase()} · ${c.compute.toUpperCase()} · ${formatNumber(c.throughput)}`}</title>
              </line>
            </g>
          ) : null,
        )}
        {props.points.map((p, i) =>
          p.x > 0 && p.y > 0 ? (
            <circle
              key={i}
              cx={x(p.x)}
              cy={y(p.y)}
              r={p.active ? 7 : 4.5}
              fill={p.color}
              stroke={p.active ? '#fff' : 'rgba(0,0,0,.4)'}
              strokeWidth={p.active ? 2 : 1}
            >
              <title>{`${p.label}\n${formatNumber(p.x, 3)} FLOPs/byte · ${formatNumber(p.y, 1)} GFLOP/s`}</title>
            </circle>
          ) : null,
        )}
      </svg>
      <div className="legend">
        {Object.entries(BANDWIDTH_COLORS).map(([name, color]) => (
          <span key={name}>
            <i className="dot" style={{ background: color }} /> {name.toUpperCase()}
          </span>
        ))}
        <span className="faint">dashed: VALU · solid: MFMA</span>
      </div>
    </div>
  )
}
