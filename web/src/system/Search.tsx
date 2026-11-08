import { useState } from 'react'
import { Search as SearchIcon, X } from 'lucide-react'
import { get, type Table } from '../api'
import { useStore } from '../store'
import { ServerTable } from '../table'
import { Empty } from '../ui'

// One term, or several when the text is a list of "quoted" terms, as on the desktop.
function parseTerms(text: string): string[] {
  const value = text.trim()
  if (!value.startsWith('"')) return value ? [value] : []
  return [...value.matchAll(/"([^"]+)"/g)].map((m) => m[1])
}

export function Search() {
  const set = useStore((s) => s.set)
  const focusEvent = useStore((s) => s.focusEvent)
  const range = useStore((s) => s.range)
  const [text, setText] = useState('')
  const [submitted, setSubmitted] = useState('')
  const [options, setOptions] = useState({ substrings: true, category: false, partial: false, inRange: false })
  const terms = parseTerms(submitted)
  const close = () => set({ search: false })
  const toggle = (key: keyof typeof options) => setOptions({ ...options, [key]: !options[key] })

  return (
    <div className="backdrop" style={{ placeItems: 'start center', paddingTop: '9vh' }} onMouseDown={(e) => e.target === e.currentTarget && close()}>
      <div className="modal" style={{ width: 'min(980px, calc(100vw - 40px))', height: '70vh' }} onKeyDown={(e) => e.key === 'Escape' && close()}>
        <div className="modal-head" style={{ paddingBottom: 10 }}>
          <SearchIcon size={18} color="var(--red)" />
          <input
            className="input grow"
            style={{ height: 38, fontSize: 15 }}
            autoFocus
            placeholder='Search events by name, e.g. hipMemcpy or "gemm" "copy"'
            value={text}
            onChange={(e) => setText(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && setSubmitted(text)}
          />
          <button className="btn primary" onClick={() => setSubmitted(text)}>Search</button>
          <button className="icon-btn" onClick={close} title="Close">
            <X size={16} />
          </button>
        </div>
        <div className="row" style={{ padding: '0 18px 10px', gap: 16, flexWrap: 'wrap' }}>
          <label className="check"><input type="checkbox" checked={options.substrings} onChange={() => toggle('substrings')} /> Match substrings</label>
          <label className="check"><input type="checkbox" checked={options.category} onChange={() => toggle('category')} /> Include category</label>
          <label className="check"><input type="checkbox" checked={options.partial} onChange={() => toggle('partial')} /> Match any term</label>
          <label className="check" style={{ opacity: range ? 1 : 0.5 }}>
            <input type="checkbox" disabled={!range} checked={options.inRange && !!range} onChange={() => toggle('inRange')} /> Selected range only
          </label>
        </div>
        <div style={{ flex: 1, minHeight: 0, borderTop: '1px solid var(--line)' }}>
          {terms.length ? (
            <ServerTable
              deps={[terms.join('\n'), options.substrings, options.category, options.partial, options.inRange && range?.start, range?.end]}
              exportName="search"
              load={(query) =>
                get<Table>('/api/tables/event_search_table', {
                  ...query,
                  sort: query.sort || 1,
                  terms: terms.join('\n'),
                  substrings: options.substrings ? 1 : 0,
                  category: options.category ? 1 : 0,
                  partial: options.partial ? 1 : 0,
                  start: options.inRange ? range?.start : undefined,
                  end: options.inRange ? range?.end : undefined,
                })
              }
              onRow={(row) => {
                if (!row.__uuid || !row.__trackId) return
                close()
                focusEvent({ uuid: row.__uuid, trackId: row.__trackId, name: row.name, start: Number(row.start), duration: Number(row.duration ?? 0), level: 0 })
              }}
            />
          ) : (
            <Empty icon={SearchIcon} title="Find any event in the trace" hint="Press Enter to search. Click a result to jump to it." />
          )}
        </div>
      </div>
    </div>
  )
}
