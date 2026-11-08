import { useEffect, useMemo, useRef, useState, type ReactNode } from 'react'
import { ArrowDown, ArrowUp, Download, Filter, ListFilter, X } from 'lucide-react'
import type { Column, Table } from './api'
import { downloadCsv } from './format'
import { useStore } from './store'
import { Empty } from './ui'

export interface TableQuery {
  offset: number
  limit: number
  sort: number
  order: 'asc' | 'desc'
  filter: string
  group: string
}

const PAGE = 200
const ROW = 28
const EXPORT_PAGE = 10000
const isHidden = (column: Column) => column.name.startsWith('__')
const isNumeric = (column?: Column) => column?.type === 'uint64' || column?.type === 'double'

// Per-column inputs become SQL the way the desktop table's filter row writes it:
// text columns match a substring, number columns take a comparison ("> 1000").
function buildFilter(columns: Column[], inputs: Record<string, string>, where: string): string {
  const clauses = Object.entries(inputs).flatMap(([name, text]) => {
    const column = columns.find((c) => c.name === name)
    const value = text.trim()
    if (!column || !value) return []
    if (!isNumeric(column)) return [`${name} LIKE '%${value.replace(/'/g, "''")}%'`]
    return [`${name} ${/^[<>=!]/.test(value) ? value : `= ${value}`}`]
  })
  if (where.trim()) clauses.push(`(${where.trim()})`)
  return clauses.join(' AND ')
}

// A table the server pages: rows load as they scroll into view.
export function ServerTable(props: {
  load: (query: TableQuery) => Promise<Table>
  deps: unknown[]
  onRow?: (row: Record<string, string>) => void
  exportName: string
  groupable?: boolean
  tools?: ReactNode
}) {
  const notify = useStore((s) => s.notify)
  const [sort, setSort] = useState<{ column: number; order: 'asc' | 'desc' }>({ column: 0, order: 'asc' })
  const [inputs, setInputs] = useState<Record<string, string>>({})
  const [applied, setApplied] = useState({ inputs: {} as Record<string, string>, where: '', group: '' })
  const [where, setWhere] = useState('')
  const [showFilters, setShowFilters] = useState(false)
  const [columns, setColumns] = useState<Column[]>([])
  const [total, setTotal] = useState(0)
  const [pages, setPages] = useState(new Map<number, string[][]>())
  const [error, setError] = useState<string | null>(null)
  const [loading, setLoading] = useState(false)
  const [viewport, setViewport] = useState({ top: 0, height: 400 })
  const ref = useRef<HTMLDivElement>(null)
  const requested = useRef(new Set<number>())
  const generation = useRef(0)
  const columnsRef = useRef<Column[]>([])

  const filter = buildFilter(columnsRef.current, applied.inputs, applied.where)
  const query = (offset: number, limit: number): TableQuery => ({
    offset,
    limit,
    sort: sort.column,
    order: sort.order,
    filter,
    group: applied.group,
  })

  const fetchPage = (page: number) => {
    if (requested.current.has(page)) return
    requested.current.add(page)
    const current = generation.current
    setLoading(true)
    props
      .load(query(page * PAGE, PAGE))
      .then(
        (table) => {
          if (current !== generation.current) return
          setError(table.error ?? null)
          columnsRef.current = table.columns ?? []
          setColumns(table.columns ?? [])
          setTotal(table.total_row_count ?? 0)
          setPages((previous) => new Map(previous).set(page, table.rows ?? []))
        },
        (failure: Error) => current === generation.current && setError(failure.message),
      )
      .finally(() => current === generation.current && setLoading(false))
  }

  useEffect(() => {
    generation.current++
    requested.current.clear()
    setPages(new Map())
    setTotal(0)
    ref.current?.scrollTo({ top: 0 })
    fetchPage(0)
  }, [...props.deps, sort.column, sort.order, filter, applied.group])

  useEffect(() => {
    const element = ref.current
    if (!element) return
    const observer = new ResizeObserver(() => setViewport((v) => ({ ...v, height: element.clientHeight })))
    observer.observe(element)
    return () => observer.disconnect()
  }, [])

  const first = Math.max(0, Math.floor(viewport.top / ROW) - 10)
  const last = Math.min(total, Math.ceil((viewport.top + viewport.height) / ROW) + 10)
  useEffect(() => {
    for (let page = Math.floor(first / PAGE); page * PAGE < last; page++) fetchPage(page)
  }, [first, last, pages])

  const visible = columns.map((column, index) => ({ column, index })).filter(({ column }) => !isHidden(column))
  const rowAt = (index: number) => pages.get(Math.floor(index / PAGE))?.[index % PAGE]
  const record = (row: string[]) => Object.fromEntries(columns.map((c, i) => [c.name, row[i]]))
  const apply = () => setApplied({ inputs, where, group: applied.group })

  const exportAll = async () => {
    notify(`Exporting ${total.toLocaleString()} rows…`)
    try {
      const rows: string[][] = []
      for (let offset = 0; offset < total; offset += EXPORT_PAGE) {
        rows.push(...(await props.load(query(offset, EXPORT_PAGE))).rows)
      }
      const shown = visible.map(({ index }) => index)
      downloadCsv(
        `${props.exportName}.csv`,
        shown.map((i) => columns[i].name),
        rows.map((row) => shown.map((i) => row[i] ?? '')),
      )
    } catch (failure) {
      notify(`Export failed: ${(failure as Error).message}`, true)
    }
  }

  return (
    <div className="grid-wrap">
      <div className="grid-tools">
        {props.tools}
        <span className="faint">
          {total.toLocaleString()} {total === 1 ? 'row' : 'rows'}
        </span>
        {loading && <div className="spinner" />}
        <div className="grow" />
        {props.groupable && (
          <select
            className="select small"
            value={applied.group}
            title="Group rows by a column"
            onChange={(event) => {
              setSort({ column: 0, order: 'asc' })
              setApplied({ ...applied, group: event.target.value })
            }}
          >
            <option value="">No grouping</option>
            {visible.map(({ column }) => (
              <option key={column.name} value={column.name}>
                Group by {column.name}
              </option>
            ))}
          </select>
        )}
        <button className={`icon-btn small ${showFilters ? 'on' : ''}`} title="Filter rows" onClick={() => setShowFilters(!showFilters)}>
          <ListFilter size={15} />
        </button>
        <button className="icon-btn small" title="Export to CSV" disabled={!total} onClick={exportAll}>
          <Download size={15} />
        </button>
      </div>
      {showFilters && (
        <div className="grid-tools">
          <Filter size={14} className="faint" />
          <input
            className="input small grow mono"
            placeholder="SQL condition, e.g. duration > 1000000 AND name LIKE '%Memcpy%'"
            value={where}
            onChange={(event) => setWhere(event.target.value)}
            onKeyDown={(event) => event.key === 'Enter' && apply()}
          />
          <button className="btn small" onClick={apply}>
            Apply
          </button>
          <button
            className="btn small ghost"
            onClick={() => {
              setInputs({})
              setWhere('')
              setApplied({ inputs: {}, where: '', group: '' })
            }}
          >
            <X size={13} /> Clear
          </button>
        </div>
      )}
      {error && <div className="grid-tools" style={{ color: 'var(--red-hi)' }}>{error}</div>}
      <div className="grid" ref={ref} onScroll={(event) => setViewport({ top: event.currentTarget.scrollTop, height: event.currentTarget.clientHeight })}>
        {!columns.length && !loading ? (
          <Empty title="No rows" />
        ) : (
          <table>
            <thead>
              <tr>
                {visible.map(({ column, index }) => (
                  <th
                    key={column.name}
                    onClick={() =>
                      setSort({ column: index, order: sort.column === index && sort.order === 'asc' ? 'desc' : 'asc' })
                    }
                  >
                    <span className="row" style={{ gap: 4 }}>
                      {column.name}
                      {sort.column === index && (sort.order === 'asc' ? <ArrowUp size={12} /> : <ArrowDown size={12} />)}
                    </span>
                  </th>
                ))}
              </tr>
              {showFilters && (
                <tr className="filters">
                  {visible.map(({ column }) => (
                    <th key={column.name}>
                      <input
                        className="input small mono"
                        placeholder={isNumeric(column) ? '> 0' : 'contains'}
                        value={inputs[column.name] ?? ''}
                        onChange={(event) => setInputs({ ...inputs, [column.name]: event.target.value })}
                        onKeyDown={(event) => event.key === 'Enter' && apply()}
                      />
                    </th>
                  ))}
                </tr>
              )}
            </thead>
            <tbody>
              <tr style={{ height: first * ROW }} />
              {Array.from({ length: Math.max(0, last - first) }, (_, offset) => {
                const index = first + offset
                const row = rowAt(index)
                return (
                  <tr
                    key={index}
                    className={props.onRow && row ? 'clickable' : ''}
                    onClick={() => row && props.onRow?.(record(row))}
                  >
                    {visible.map(({ column, index: c }) => (
                      <td key={column.name} className={isNumeric(column) ? 'num' : ''} title={row?.[c]}>
                        {row ? row[c] : <span className="faint">·</span>}
                      </td>
                    ))}
                  </tr>
                )
              })}
              <tr style={{ height: Math.max(0, total - last) * ROW }} />
            </tbody>
          </table>
        )}
      </div>
    </div>
  )
}

// A small table held in memory: sortable, with optional bar columns and a
// selected row.
export function SimpleTable(props: {
  columns: string[]
  rows: (string | number | null)[][]
  onRow?: (index: number) => void
  selected?: (row: (string | number | null)[]) => boolean
  bars?: number[]
  maxHeight?: number | string
}) {
  const [sort, setSort] = useState<{ column: number; order: 1 | -1 } | null>(null)
  const isNumber = (column: number) => props.rows.some((row) => typeof row[column] === 'number')
  const order = useMemo(() => {
    const indices = props.rows.map((_, i) => i)
    if (!sort) return indices
    const value = (i: number) => props.rows[i][sort.column]
    return indices.sort((a, b) => {
      const [x, y] = [value(a), value(b)]
      const [nx, ny] = [Number(x), Number(y)]
      const compare = !Number.isNaN(nx) && !Number.isNaN(ny) ? nx - ny : String(x ?? '').localeCompare(String(y ?? ''))
      return compare * sort.order
    })
  }, [props.rows, sort])
  const maxima = useMemo(
    () =>
      Object.fromEntries(
        (props.bars ?? []).map((c) => [c, Math.max(1e-12, ...props.rows.map((row) => Number(row[c]) || 0))]),
      ),
    [props.rows, props.bars],
  )
  const show = (value: string | number | null) =>
    value === null || value === undefined ? '–' : typeof value === 'number' ? value.toLocaleString(undefined, { maximumFractionDigits: 3 }) : value

  return (
    <div className="grid" style={{ maxHeight: props.maxHeight }}>
      <table>
        <thead>
          <tr>
            {props.columns.map((name, column) => (
              <th
                key={column}
                onClick={() => setSort({ column, order: sort?.column === column && sort.order === 1 ? -1 : 1 })}
              >
                <span className="row" style={{ gap: 4 }}>
                  {name}
                  {sort?.column === column && (sort.order === 1 ? <ArrowUp size={12} /> : <ArrowDown size={12} />)}
                </span>
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {order.map((index) => {
            const row = props.rows[index]
            return (
              <tr
                key={index}
                className={`${props.onRow ? 'clickable' : ''} ${props.selected?.(row) ? 'on' : ''}`}
                onClick={() => props.onRow?.(index)}
              >
                {row.map((value, column) =>
                  column in maxima ? (
                    <td key={column} className="num bar-cell">
                      <i style={{ width: `${(100 * (Number(value) || 0)) / maxima[column]}%` }} />
                      <span>{show(value)}</span>
                    </td>
                  ) : (
                    <td key={column} className={isNumber(column) ? 'num' : ''} title={String(value ?? '')}>
                      {show(value)}
                    </td>
                  ),
                )}
              </tr>
            )
          })}
        </tbody>
      </table>
      {!props.rows.length && <Empty title="No rows" />}
    </div>
  )
}
