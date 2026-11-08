import { useEffect, useState } from 'react'
import {
  AlertTriangle, BarChart3, BookOpen, Database, FileText, FolderOpen, GitBranch, History, Info, Maximize, Menu as MenuIcon,
  Moon, PanelBottom, PanelLeft, PanelTop, Save, Search as SearchIcon, Settings, Sun, Waypoints, Wrench,
} from 'lucide-react'
import { get, post, type Status } from './api'
import { ComputeView } from './compute/ComputeView'
import { AboutDialog, OpenDialog, RESOURCES, SaveDialog, SettingsDialog, openTrace } from './dialogs'
import { fileName } from './format'
import { useStore } from './store'
import { Search } from './system/Search'
import { Summary } from './system/Summary'
import { SystemView } from './system/SystemView'
import { AmdMark, Boundary, Menu, type MenuItem, type MenuState } from './ui'

// Hands a newly opened trace to the store, or updates the current one's state.
async function poll() {
  try {
    const status = await get<Status>('/api/status')
    const current = useStore.getState().status
    if (!current || current.trace_file_path !== status.trace_file_path) useStore.getState().showTrace(status)
    else useStore.setState({ status })
  } catch {
    if (!useStore.getState().status) setTimeout(poll, 2000)
  }
}

export function App() {
  const status = useStore((s) => s.status)
  const settings = useStore((s) => s.settings)
  const dialog = useStore((s) => s.dialog)
  const summary = useStore((s) => s.summary)
  const search = useStore((s) => s.search)
  const toast = useStore((s) => s.toast)
  const set = useStore((s) => s.set)
  const ready = status?.state === 'ready'
  const system = ready && status.trace_type === 'system'

  const loading = status?.state === 'loading'
  useEffect(() => {
    poll()
  }, [])
  useEffect(() => {
    if (!loading) return
    const timer = setInterval(poll, 250)
    return () => clearInterval(timer)
  }, [loading, status?.trace_file_path])
  useEffect(() => {
    document.documentElement.dataset.theme = settings.theme
    document.documentElement.style.setProperty('--fs', `${settings.fontSize}px`)
  }, [settings.theme, settings.fontSize])
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if ((event.ctrlKey || event.metaKey) && (event.key === 'k' || event.key === 'f') && system) {
        event.preventDefault()
        set({ search: true })
      } else if (event.key === 'F11') {
        event.preventDefault()
        document.fullscreenElement ? document.exitFullscreen() : document.documentElement.requestFullscreen()
      } else if (event.key === 'Escape' && useStore.getState().summary) {
        set({ summary: false })
      }
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [system])

  return (
    <div className="app">
      <TopBar />
      {!status ? (
        <Splash text="Connecting to the Optiq server…" />
      ) : status.state === 'loading' ? (
        <Splash text={status.progress_message || 'Loading the trace…'} title={fileName(status.trace_file_path)} />
      ) : !ready ? (
        <Welcome />
      ) : (
        <Boundary key={status.trace_file_path}>
          {status.trace_type === 'system' ? <SystemView /> : <ComputeView />}
        </Boundary>
      )}
      {system && summary && <Summary />}
      {system && search && <Search />}
      {dialog === 'open' && <OpenDialog />}
      {dialog === 'settings' && <SettingsDialog />}
      {dialog === 'about' && <AboutDialog />}
      {dialog === 'save' && <SaveDialog />}
      {toast && (
        <div className={`toast ${toast.error ? 'error' : ''}`}>
          {toast.error && <AlertTriangle size={16} color="var(--red)" />}
          {toast.text}
        </div>
      )}
    </div>
  )
}

function TopBar() {
  const status = useStore((s) => s.status)
  const settings = useStore((s) => s.settings)
  const summary = useStore((s) => s.summary)
  const range = useStore((s) => s.range)
  const recent = useStore((s) => s.recent)
  const set = useStore((s) => s.set)
  const setSettings = useStore((s) => s.setSettings)
  const notify = useStore((s) => s.notify)
  const [menu, setMenu] = useState<MenuState | null>(null)
  const ready = status?.state === 'ready'
  const system = ready && status.trace_type === 'system'

  const cleanup = async (rebuild: boolean) => {
    notify(rebuild ? 'Running a full database cleanup…' : 'Running a fast database cleanup…')
    try {
      await post('/api/cleanup', { rebuild })
      notify('Database cleanup finished')
    } catch (error) {
      notify((error as Error).message, true)
    }
  }

  const openMenu = (event: React.MouseEvent) => {
    const box = event.currentTarget.getBoundingClientRect()
    const items: MenuItem[] = [
      { label: 'Open…', icon: FolderOpen, onClick: () => set({ dialog: 'open' }) },
      ...(recent.length ? [{ section: 'Recent' } as MenuItem] : []),
      ...recent.slice(0, 5).map((path) => ({ label: fileName(path), icon: History, onClick: () => openTrace(path) })),
      'divider',
      { label: 'Save trace selection…', icon: Save, disabled: !system || !range, onClick: () => set({ dialog: 'save' }) },
      { label: 'Database: fast cleanup', icon: Wrench, disabled: !system, onClick: () => cleanup(false) },
      { label: 'Database: full cleanup', icon: Database, disabled: !system, onClick: () => cleanup(true) },
      'divider',
      { label: 'Show topology sidebar', icon: PanelLeft, disabled: !system, hint: settings.sidebar ? '✓' : '', onClick: () => setSettings({ sidebar: !settings.sidebar }) },
      { label: 'Show timeline overview', icon: PanelTop, disabled: !system, hint: settings.overview ? '✓' : '', onClick: () => setSettings({ overview: !settings.overview }) },
      { label: 'Show details panel', icon: PanelBottom, disabled: !system, hint: settings.details ? '✓' : '', onClick: () => setSettings({ details: !settings.details }) },
      { label: 'Fullscreen', icon: Maximize, hint: 'F11', onClick: () => (document.fullscreenElement ? document.exitFullscreen() : document.documentElement.requestFullscreen()) },
      'divider',
      { label: 'Preferences', icon: Settings, onClick: () => set({ dialog: 'settings' }) },
      { label: 'About', icon: Info, onClick: () => set({ dialog: 'about' }) },
    ]
    setMenu({ x: box.right - 230, y: box.bottom + 6, items })
  }

  return (
    <header className="topbar">
      <div className="brand" onClick={() => set({ summary: false })}>
        <AmdMark />
        <b>ROCm</b>
        <span>Optiq</span>
      </div>
      {status?.trace_file_path && (
        <button className="trace-chip btn ghost" style={{ height: 30 }} title={status.trace_file_path} onClick={() => set({ dialog: 'open' })}>
          <FileText size={14} />
          <span className="ellipsis">{fileName(status.trace_file_path)}</span>
          {ready && <span className="badge">{status.trace_type}</span>}
        </button>
      )}
      <div className="grow" />
      {system && (
        <button className="search-button" onClick={() => set({ search: true })}>
          <SearchIcon size={15} />
          Search events
          <kbd>Ctrl K</kbd>
        </button>
      )}
      <div className="grow" />
      {system && (
        <>
          <button className={`btn small ${summary ? 'primary' : ''}`} onClick={() => set({ summary: !summary })}>
            <BarChart3 size={15} /> Summary
          </button>
          <button className={`icon-btn ${settings.flows ? 'on' : ''}`} title="Show flow arrows" onClick={() => setSettings({ flows: !settings.flows })}>
            <Waypoints size={17} />
          </button>
          <button className={`icon-btn ${settings.sidebar ? 'on' : ''}`} title="Topology sidebar" onClick={() => setSettings({ sidebar: !settings.sidebar })}>
            <PanelLeft size={17} />
          </button>
          <button className={`icon-btn ${settings.details ? 'on' : ''}`} title="Details panel" onClick={() => setSettings({ details: !settings.details })}>
            <PanelBottom size={17} />
          </button>
        </>
      )}
      <button className="icon-btn" title="Toggle theme" onClick={() => setSettings({ theme: settings.theme === 'dark' ? 'light' : 'dark' })}>
        {settings.theme === 'dark' ? <Sun size={17} /> : <Moon size={17} />}
      </button>
      <button className="icon-btn" title="Menu" onClick={openMenu}>
        <MenuIcon size={18} />
      </button>
      <Menu menu={menu} onClose={() => setMenu(null)} />
    </header>
  )
}

function Splash({ text, title }: { text: string; title?: string }) {
  return (
    <div className="loading">
      <AmdMark size={40} />
      {title && <div style={{ fontSize: 18, fontWeight: 650 }}>{title}</div>}
      <div className="progress" />
      <div className="muted">{text}</div>
    </div>
  )
}

function Welcome() {
  const status = useStore((s) => s.status)
  const recent = useStore((s) => s.recent)
  const set = useStore((s) => s.set)
  return (
    <div className="welcome">
      <div className="hero">
        <div className="col" style={{ gap: 16 }}>
          <div className="row" style={{ gap: 12 }}>
            <AmdMark size={30} />
            <span className="badge">ROCm</span>
          </div>
          <h1>
            See every kernel.<br />
            <em>Optimize</em> with Optiq.
          </h1>
          <p>
            Explore system traces from the ROCm Systems Profiler and kernel profiles from the ROCm Compute Profiler —
            timelines, flows, summaries, rooflines and ISA — straight from the browser.
          </p>
          {status?.state === 'error' && (
            <div className="row" style={{ color: 'var(--red-hi)' }}>
              <AlertTriangle size={16} /> Could not load {fileName(status.trace_file_path)}. {status.progress_message}
            </div>
          )}
          <div className="row">
            <button className="btn primary" style={{ height: 40, padding: '0 18px' }} onClick={() => set({ dialog: 'open' })}>
              <FolderOpen size={17} /> Open trace
            </button>
            <a className="btn" style={{ height: 40 }} href={RESOURCES[0].docs} target="_blank" rel="noreferrer">
              <BookOpen size={16} /> Documentation
            </a>
          </div>
        </div>
        <div className="hero-grid">
          <div className="card">
            <div className="card-title"><History size={16} /> Recent traces</div>
            <div style={{ padding: '0 6px 8px' }}>
              {recent.length ? (
                recent.map((path) => (
                  <button key={path} className="list-item" onClick={() => openTrace(path)} title={path}>
                    <Database size={15} color="var(--red)" />
                    <span className="col grow" style={{ gap: 0, minWidth: 0 }}>
                      <span className="ellipsis">{fileName(path)}</span>
                      <span className="faint ellipsis mono" style={{ fontSize: 11 }}>{path}</span>
                    </span>
                  </button>
                ))
              ) : (
                <div className="faint" style={{ padding: '4px 10px 10px' }}>Traces you open appear here.</div>
              )}
            </div>
          </div>
          <div className="card">
            <div className="card-title"><BookOpen size={16} /> Resources</div>
            <div style={{ padding: '0 6px 8px' }}>
              {RESOURCES.map((r) => (
                <div key={r.title} className="list-item" style={{ cursor: 'default' }}>
                  <span className="col grow" style={{ gap: 0 }}>
                    <span>{r.title}</span>
                    <span className="faint" style={{ fontSize: 12 }}>{r.text}</span>
                  </span>
                  <a className="icon-btn small" href={r.docs} target="_blank" rel="noreferrer" title="Documentation"><BookOpen size={14} /></a>
                  <a className="icon-btn small" href={r.code} target="_blank" rel="noreferrer" title="Source"><GitBranch size={14} /></a>
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
