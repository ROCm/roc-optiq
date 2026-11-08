import { useEffect, useState } from 'react'
import { ArrowUp, BookOpen, Database, Folder, FolderOpen, GitBranch, HardDrive, History, Info, Save, Settings } from 'lucide-react'
import { get, post, type FolderListing, type Status } from './api'
import { AssistantSettings } from './assistant/Assistant'
import { fileName, formatBytes, formatDuration, formatTime } from './format'
import { useStore, type TimeFormat } from './store'
import { AmdMark, Empty, Loading, Modal, Segmented, useAsync } from './ui'

export const RESOURCES = [
  { title: 'ROCm Optiq', text: 'Workflow, installation and feature guides.', docs: 'https://rocm.docs.amd.com/projects/roc-optiq/en/latest/', code: 'https://github.com/ROCm/roc-optiq' },
  { title: 'ROCm Systems Profiler', text: 'System-wide traces, timelines and runtime activity.', docs: 'https://rocm.docs.amd.com/projects/rocprofiler-systems/en/latest/', code: 'https://github.com/ROCm/rocm-systems/tree/develop/projects/rocprofiler-systems' },
  { title: 'ROCm Compute Profiler', text: 'Kernel, memory and hardware counter analysis.', docs: 'https://rocm.docs.amd.com/projects/rocprofiler-compute/en/latest/', code: 'https://github.com/ROCm/rocm-systems/tree/develop/projects/rocprofiler-compute' },
]

export const HOTKEYS: [string, string][] = [
  ['A / D or ← / →', 'Pan left / right'],
  ['W / S or mouse wheel', 'Zoom in / out'],
  ['↑ / ↓ or Shift+wheel', 'Scroll tracks'],
  ['Drag', 'Pan the timeline'],
  ['Ctrl+drag or drag the ruler', 'Select a time range'],
  ['Z', 'Zoom to the selection'],
  ['M', 'Mark the selected event as the range'],
  ['Esc', 'Clear the selection'],
  ['Ctrl+0…9 / 0…9', 'Save / recall a bookmarked view'],
  ['Shift', 'Move faster'],
  ['Ctrl+K', 'Search events'],
]

export async function openTrace(path: string) {
  const { notify, showTrace } = useStore.getState()
  try {
    showTrace(await post<Status>('/api/open', { path }))
  } catch (error) {
    notify((error as Error).message, true)
  }
}

const parentOf = (path: string) => path.replace(/[\\/][^\\/]*$/, '') || path

// Clickable segments of a folder path, Windows or POSIX.
function crumbs(path: string) {
  const windows = /^[A-Za-z]:/.test(path)
  const parts = path.split(/[\\/]+/).filter(Boolean)
  const join = (count: number) =>
    windows ? `${parts.slice(0, count).join('\\')}${count === 1 ? '\\' : ''}` : `/${parts.slice(0, count).join('/')}`
  return [...(windows ? [] : [{ name: '/', path: '/' }]), ...parts.map((name, i) => ({ name, path: join(i + 1) }))]
}

// The server's file system, which is this machine's while the server is local.
export function OpenDialog() {
  const set = useStore((s) => s.set)
  const recent = useStore((s) => s.recent)
  const [folder, setFolder] = useState('')
  const [path, setPath] = useState('')
  const [filter, setFilter] = useState('')
  const listing = useAsync(() => get<FolderListing>('/api/files', { path: folder }), [folder])
  const data = listing.data
  const entries = (data?.entries ?? []).filter((e) => e.name.toLowerCase().includes(filter.toLowerCase()))
  const close = () => set({ dialog: null })
  const open = (target: string) => {
    close()
    openTrace(target)
  }
  const go = (target: string) => {
    setFolder(target)
    setFilter('')
  }
  const choose = (entry: (typeof entries)[number]) => (entry.is_folder ? go(entry.path) : open(entry.path))
  useEffect(() => {
    if (data && !folder) setFolder(data.path)
  }, [data])
  const onKey = (event: React.KeyboardEvent) => {
    const index = entries.findIndex((e) => e.path === path)
    if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
      event.preventDefault()
      const next = entries[Math.min(entries.length - 1, Math.max(0, index + (event.key === 'ArrowDown' ? 1 : -1)))]
      if (next) setPath(next.path)
    } else if (event.key === 'Enter' && index >= 0) {
      choose(entries[index])
    } else if (event.key === 'Backspace' && !filter && data) {
      go(data.parent)
    }
  }
  const side = (title: string, items: { name: string; path: string }[], icon: typeof Folder) =>
    items.length > 0 && (
      <>
        <div className="section-label" style={{ margin: '10px 8px 4px' }}>{title}</div>
        {items.map((item) => {
          const Icon = icon
          return (
            <button key={item.path} className={`list-item ${data?.path === item.path ? 'on' : ''}`} style={{ padding: '6px 8px' }} onClick={() => go(item.path)} title={item.path}>
              <Icon size={15} />
              <span className="ellipsis">{item.name}</span>
            </button>
          )
        })}
      </>
    )

  return (
    <Modal
      title="Open a trace"
      icon={FolderOpen}
      width={900}
      onClose={close}
      footer={
        <>
          <input className="input grow mono" placeholder="Path to a .db, .rpd or .yaml file" value={path} onChange={(e) => setPath(e.target.value)} onKeyDown={(e) => e.key === 'Enter' && path && open(path)} />
          <button className="btn" onClick={close}>Cancel</button>
          <button className="btn primary" disabled={!path || entries.some((e) => e.path === path && e.is_folder)} onClick={() => open(path)}>Open</button>
        </>
      }
    >
      <div className="picker" onKeyDown={onKey}>
        <nav className="picker-side">
          {side('Places', data?.places ?? [], Folder)}
          {side('Drives', (data?.roots ?? []).map((root) => ({ name: root, path: root })), HardDrive)}
          {side('Recent folders', [...new Set(recent.map(parentOf))].slice(0, 5).map((p) => ({ name: fileName(p), path: p })), History)}
        </nav>
        <div className="col" style={{ minWidth: 0, gap: 8 }}>
          <div className="row">
            <button className="icon-btn" title="Up one folder (Backspace)" disabled={!data || data.parent === data.path} onClick={() => data && go(data.parent)}>
              <ArrowUp size={16} />
            </button>
            <div className="crumbs grow" ref={(element) => element?.scrollTo({ left: element.scrollWidth })}>
              {data && crumbs(data.path).map((c, i, all) => (
                <button key={c.path} className={i === all.length - 1 ? 'on' : ''} onClick={() => go(c.path)}>{c.name}</button>
              ))}
            </div>
            <input className="input small" style={{ width: 160 }} placeholder="Filter" value={filter} onChange={(e) => setFilter(e.target.value)} autoFocus />
          </div>
          <div className="card picker-list" tabIndex={0}>
            {listing.error && <Empty icon={Folder} title="This folder cannot be read" hint={listing.error} />}
            {!data && !listing.error && <Loading />}
            {data && entries.length === 0 && <Empty icon={Folder} title={filter ? 'Nothing matches the filter' : 'No folders or traces here'} />}
            {entries.map((entry) => (
              <button
                key={entry.path}
                className={`list-item ${path === entry.path ? 'on' : ''}`}
                onClick={() => (entry.is_folder ? go(entry.path) : setPath(entry.path))}
                onDoubleClick={() => choose(entry)}
              >
                {entry.is_folder ? <Folder size={16} /> : <Database size={16} color="var(--red)" />}
                <span className="grow ellipsis">{entry.name}</span>
                {!entry.is_folder && <span className="faint mono">{formatBytes(entry.size)}</span>}
              </button>
            ))}
          </div>
        </div>
      </div>
    </Modal>
  )
}

const TIME_FORMATS: { id: TimeFormat; label: string }[] = [
  { id: 'timecode', label: 'Timecode' },
  { id: 'condensed', label: 'Condensed' },
  { id: 's', label: 's' },
  { id: 'ms', label: 'ms' },
  { id: 'us', label: 'µs' },
  { id: 'ns', label: 'ns' },
]

export function SettingsDialog() {
  const settings = useStore((s) => s.settings)
  const setSettings = useStore((s) => s.setSettings)
  const set = useStore((s) => s.set)
  const close = () => set({ dialog: null })
  return (
    <Modal title="Preferences" icon={Settings} onClose={close} footer={<button className="btn primary" onClick={close}>Done</button>}>
      <div className="col" style={{ gap: 18 }}>
        <div className="col">
          <span className="section-label">Display</span>
          <div className="row"><span className="grow">Theme</span>
            <Segmented value={settings.theme} onChange={(theme) => setSettings({ theme })} options={[{ id: 'dark', label: 'Dark' }, { id: 'light', label: 'Light' }]} />
          </div>
          <div className="row"><span className="grow">Font size</span>
            <input type="range" min={11} max={16} value={settings.fontSize} onChange={(e) => setSettings({ fontSize: Number(e.target.value) })} style={{ accentColor: 'var(--red)' }} />
            <span className="mono" style={{ width: 36 }}>{settings.fontSize}px</span>
          </div>
          <label className="check"><input type="checkbox" checked={settings.compactSidebar} onChange={(e) => setSettings({ compactSidebar: e.target.checked })} /> Compact topology sidebar</label>
        </div>
        <div className="col">
          <span className="section-label">Units</span>
          <div className="row"><span className="grow">Time format</span>
            <Segmented value={settings.timeFormat} onChange={(timeFormat) => setSettings({ timeFormat })} options={TIME_FORMATS} />
          </div>
          <div className="faint">Preview: <span className="mono">{formatTime(3723456789012, settings.timeFormat)}</span></div>
        </div>
        <AssistantSettings />
        <div className="col">
          <span className="section-label">Keyboard and mouse</span>
          <div className="kv">
            {HOTKEYS.flatMap(([keys, action]) => [<span key={keys} className="mono">{keys}</span>, <span key={action}>{action}</span>])}
          </div>
        </div>
      </div>
    </Modal>
  )
}

export function AboutDialog() {
  const status = useStore((s) => s.status)
  const set = useStore((s) => s.set)
  return (
    <Modal title="About ROCm Optiq" icon={Info} onClose={() => set({ dialog: null })}>
      <div className="col" style={{ gap: 16 }}>
        <div className="row" style={{ gap: 14 }}>
          <AmdMark size={44} />
          <div>
            <div style={{ fontSize: 20, fontWeight: 700 }}>ROCm Optiq</div>
            <div className="muted">Web client · server {status?.version ?? 'unknown'}</div>
          </div>
        </div>
        <p className="muted" style={{ margin: 0 }}>
          Visualizes traces from the ROCm Systems Profiler and profiles from the ROCm Compute Profiler. The
          trace stays on the server; this page only asks it for what is on screen.
        </p>
        {RESOURCES.map((r) => (
          <div key={r.title} className="row">
            <span className="grow">{r.title}</span>
            <a className="btn small" href={r.docs} target="_blank" rel="noreferrer"><BookOpen size={13} /> Docs</a>
            <a className="btn small" href={r.code} target="_blank" rel="noreferrer"><GitBranch size={13} /> Source</a>
          </div>
        ))}
        <div className="faint">Copyright Advanced Micro Devices, Inc. MIT License.</div>
      </div>
    </Modal>
  )
}

export function SaveDialog() {
  const status = useStore((s) => s.status)!
  const range = useStore((s) => s.range)
  const timeline = useStore((s) => s.timeline)
  const format = useStore((s) => s.settings.timeFormat)
  const set = useStore((s) => s.set)
  const notify = useStore((s) => s.notify)
  const [path, setPath] = useState(status.trace_file_path.replace(/(\.\w+)$/, '_selection$1'))
  const [busy, setBusy] = useState(false)
  const close = () => set({ dialog: null })
  const save = async () => {
    if (!range) return
    setBusy(true)
    try {
      await post('/api/save_selection', { path, start_ns: range.start, end_ns: range.end })
      notify(`Saved ${fileName(path)}`)
      close()
    } catch (error) {
      notify((error as Error).message, true)
    } finally {
      setBusy(false)
    }
  }
  return (
    <Modal
      title="Save trace selection"
      icon={Save}
      onClose={close}
      footer={
        <>
          <button className="btn" onClick={close}>Cancel</button>
          <button className="btn primary" disabled={!range || !path || busy} onClick={save}>
            {busy ? <div className="spinner" /> : <HardDrive size={15} />} Save
          </button>
        </>
      }
    >
      {range && timeline ? (
        <div className="col" style={{ gap: 12 }}>
          <div className="muted">
            Writes the events between <span className="mono">{formatTime(range.start - timeline.start_ns, format)}</span> and{' '}
            <span className="mono">{formatTime(range.end - timeline.start_ns, format)}</span> ({formatDuration(range.end - range.start)}) to a new
            database on the server.
          </div>
          <input className="input mono" value={path} onChange={(e) => setPath(e.target.value)} />
        </div>
      ) : (
        <Empty title="Select a time range first" hint="Ctrl+drag on the timeline or drag along its ruler." />
      )}
    </Modal>
  )
}
