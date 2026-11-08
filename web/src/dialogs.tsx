import { useEffect, useState } from 'react'
import { ArrowUp, BookOpen, Database, FileText, Folder, FolderOpen, GitBranch, HardDrive, Info, Save, Settings } from 'lucide-react'
import { get, post, type FolderListing, type Status } from './api'
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

export function OpenDialog() {
  const set = useStore((s) => s.set)
  const recent = useStore((s) => s.recent)
  const [folder, setFolder] = useState('')
  const [path, setPath] = useState('')
  const listing = useAsync(() => get<FolderListing>('/api/files', { path: folder }), [folder])
  const close = () => set({ dialog: null })
  const open = (target: string) => {
    close()
    openTrace(target)
  }
  useEffect(() => {
    if (listing.data && !folder) setFolder(listing.data.path)
  }, [listing.data])

  return (
    <Modal
      title="Open a trace"
      icon={FolderOpen}
      width={760}
      onClose={close}
      footer={
        <>
          <input className="input grow mono" placeholder="Path to a .db, .rpd or .yaml file on the server" value={path} onChange={(e) => setPath(e.target.value)} onKeyDown={(e) => e.key === 'Enter' && path && open(path)} />
          <button className="btn" onClick={close}>Cancel</button>
          <button className="btn primary" disabled={!path} onClick={() => open(path)}>Open</button>
        </>
      }
    >
      <div className="row" style={{ marginBottom: 10 }}>
        <button className="icon-btn" title="Up one folder" disabled={!listing.data || listing.data.parent === listing.data.path} onClick={() => listing.data && setFolder(listing.data.parent)}>
          <ArrowUp size={16} />
        </button>
        <input className="input grow mono" value={folder} onChange={(e) => setFolder(e.target.value)} />
        {listing.data?.roots.length ? (
          <select className="select" value="" onChange={(e) => e.target.value && setFolder(e.target.value)}>
            <option value="">Drives</option>
            {listing.data.roots.map((root) => <option key={root} value={root}>{root}</option>)}
          </select>
        ) : null}
      </div>
      <div className="card" style={{ height: 340, overflow: 'auto', padding: 4 }}>
        {listing.error && <Empty icon={Folder} title="This folder cannot be read" hint={listing.error} />}
        {!listing.data && !listing.error && <Loading />}
        {listing.data?.entries.length === 0 && <Empty icon={Folder} title="No traces here" />}
        {listing.data?.entries.map((entry) => (
          <button
            key={entry.path}
            className={`list-item ${path === entry.path ? 'on' : ''}`}
            onClick={() => (entry.is_folder ? setFolder(entry.path) : setPath(entry.path))}
            onDoubleClick={() => !entry.is_folder && open(entry.path)}
          >
            {entry.is_folder ? <Folder size={16} /> : <Database size={16} color="var(--red)" />}
            <span className="grow ellipsis">{entry.name}</span>
            {!entry.is_folder && <span className="faint mono">{formatBytes(entry.size)}</span>}
          </button>
        ))}
      </div>
      {recent.length > 0 && (
        <>
          <div className="section-label" style={{ margin: '14px 0 6px' }}>Recent</div>
          {recent.slice(0, 4).map((item) => (
            <button key={item} className="list-item" onClick={() => open(item)}>
              <FileText size={15} />
              <span className="ellipsis">{fileName(item)}</span>
              <span className="faint ellipsis grow mono" style={{ fontSize: 11 }}>{item}</span>
            </button>
          ))}
        </>
      )}
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
