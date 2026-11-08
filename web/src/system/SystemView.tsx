import { useEffect, useMemo, useRef, useState } from 'react'
import { ChevronRight, Cpu, Eye, EyeOff, Gauge, Layers, Locate, MemoryStick, Search, Server, Workflow } from 'lucide-react'
import { get, type EventDetails, type Minimap, type Timeline as TimelineData, type Topology, type TopologyNode, type Track } from '../api'
import { useStore } from '../store'
import { Loading, Menu, type MenuState } from '../ui'
import { BottomPanel } from './BottomPanel'
import { Timeline, clampView, trackColor } from './Timeline'

export function SystemView() {
  const timeline = useStore((s) => s.timeline)
  const settings = useStore((s) => s.settings)
  const event = useStore((s) => s.event)
  const set = useStore((s) => s.set)
  const notify = useStore((s) => s.notify)

  useEffect(() => {
    Promise.all([get<TimelineData>('/api/timeline'), get<Topology>('/api/topology')]).then(
      ([loaded, topology]) => {
        const ids = loaded.tracks.map((t) => t.id)
        const saved = useStore.getState().order
        const order = saved.length === ids.length && ids.every((id) => saved.includes(id)) ? saved : ids
        set({ timeline: loaded, topology, order, view: { start: loaded.start_ns, end: loaded.end_ns }, scroll: 0 })
      },
      (error: Error) => notify(`Could not load the timeline: ${error.message}`, true),
    )
  }, [])

  useEffect(() => {
    if (!event) return
    let live = true
    get<EventDetails>(`/api/events/${event.uuid}`, { track: event.trackId }).then(
      (details) => live && set({ details }),
      (error: Error) => live && notify(`Could not load the event: ${error.message}`, true),
    )
    return () => {
      live = false
    }
  }, [event?.uuid])

  if (!timeline) return <Loading text="Building the timeline…" />
  return (
    <div className="system" style={{ gridTemplateColumns: settings.sidebar ? 'auto 1fr' : '1fr' }}>
      {settings.sidebar && <Sidebar />}
      <div className="main">
        {settings.overview && <Overview />}
        <Timeline />
        {settings.details && <BottomPanel />}
      </div>
    </div>
  )
}

interface TreeItem {
  key: string
  label: string
  icon?: typeof Cpu
  track?: Track
  children: TreeItem[]
}

function trackIds(item: TreeItem): string[] {
  return item.track ? [item.track.id] : item.children.flatMap(trackIds)
}

// The topology as the sidebar shows it: nodes, their processors (queues and
// counters) and processes (threads and streams), then any track left over.
function buildTree(topology: Topology, tracks: Track[]): TreeItem[] {
  const byId = new Map(tracks.map((t) => [t.id, t]))
  const used = new Set<string>()
  const leaf = (node: TopologyNode, prefix: string): TreeItem[] => {
    const track = node.track_id ? byId.get(node.track_id) : undefined
    if (!track) return []
    used.add(track.id)
    return [{ key: `${prefix}/${node.type}${node.id}/${track.id}`, label: track.name, track, children: [] }]
  }
  const group = (key: string, label: string, icon: typeof Cpu, children: TreeItem[]): TreeItem[] =>
    children.length ? [{ key, label: `${label} (${children.length})`, icon, children }] : []
  const roots = topology.nodes.map((node) => {
    const nodeKey = `node${node.id}`
    const processors = (node.processors ?? []).map((processor) => {
      const key = `${nodeKey}/p${processor.id}`
      const queues = (processor.queues ?? []).flatMap((q) => leaf(q, key))
      const counters = (processor.counters ?? []).flatMap((c) => leaf(c, key))
      return {
        key,
        label: `${processor.label ?? processor.processor_type?.toUpperCase()} · ${processor.product_name || processor.name}`,
        icon: processor.processor_type === 'gpu' ? Gauge : Cpu,
        children: [...queues, ...group(`${key}/counters`, 'Counters', Layers, counters)],
      }
    })
    const processes = (node.processes ?? []).map((process) => {
      const key = `${nodeKey}/proc${process.id}`
      return {
        key,
        label: process.header || `${process.command} (${process.pid})`,
        icon: Workflow,
        children: [
          ...(process.threads ?? []).flatMap((t) => leaf(t, key)),
          ...(process.streams ?? []).flatMap((s) => leaf(s, key)),
        ],
      }
    })
    return { key: nodeKey, label: node.host_name || node.name, icon: Server, children: [...processors, ...processes].filter((c) => c.children.length) }
  })
  const rest = tracks.filter((t) => !used.has(t.id)).map((track) => ({ key: `rest/${track.id}`, label: track.name, track, children: [] }))
  return [...roots, ...group('rest', 'Uncategorized', MemoryStick, rest)]
}

function Sidebar() {
  const timeline = useStore((s) => s.timeline)!
  const topology = useStore((s) => s.topology)
  const hidden = useStore((s) => s.hidden)
  const selectedTracks = useStore((s) => s.selectedTracks)
  const compact = useStore((s) => s.settings.compactSidebar)
  const set = useStore((s) => s.set)
  const [query, setQuery] = useState('')
  const [closed, setClosed] = useState<Set<string>>(new Set())
  const [menu, setMenu] = useState<MenuState | null>(null)
  const tree = useMemo(() => (topology ? buildTree(topology, timeline.tracks) : []), [topology, timeline])
  const hiddenSet = new Set(hidden)

  const setVisible = (ids: string[], visible: boolean) => {
    const next = new Set(useStore.getState().hidden)
    ids.forEach((id) => (visible ? next.delete(id) : next.add(id)))
    set({ hidden: [...next] })
  }
  const goTo = (track: Track) => set({ hidden: hidden.filter((id) => id !== track.id), scrollTo: track.id, selectedTracks: [track.id] })
  const matches = (item: TreeItem): boolean =>
    !query || item.label.toLowerCase().includes(query.toLowerCase()) || item.children.some(matches)

  const openMenu = (item: TreeItem, event: React.MouseEvent) => {
    event.preventDefault()
    const state = useStore.getState()
    const ids = trackIds(item)
    const { track } = item
    setMenu({
      x: event.clientX,
      y: event.clientY,
      items: track
        ? [
            { label: 'Go to track', icon: Locate, onClick: () => goTo(track) },
            hiddenSet.has(track.id)
              ? { label: 'Show track', icon: Eye, onClick: () => setVisible([track.id], true) }
              : { label: 'Hide track', icon: EyeOff, onClick: () => setVisible([track.id], false) },
            { label: 'Show all tracks', icon: Eye, disabled: !hidden.length, onClick: () => set({ hidden: [] }) },
            { label: 'Hide all but this track', icon: EyeOff, onClick: () => set({ hidden: timeline.tracks.map((t) => t.id).filter((id) => id !== track.id) }) },
            'divider',
            { label: 'Show selected tracks', icon: Eye, disabled: !state.selectedTracks.length, onClick: () => setVisible(state.selectedTracks, true) },
            { label: 'Hide selected tracks', icon: EyeOff, disabled: !state.selectedTracks.length, onClick: () => setVisible(state.selectedTracks, false) },
          ]
        : [
            { label: 'Show all tracks below', icon: Eye, onClick: () => setVisible(ids, true) },
            { label: 'Hide all tracks below', icon: EyeOff, onClick: () => setVisible(ids, false) },
          ],
    })
  }

  const render = (item: TreeItem, depth: number): React.ReactNode => {
    if (!matches(item)) return null
    const ids = trackIds(item)
    const hiddenCount = ids.filter((id) => hiddenSet.has(id)).length
    const open = query ? true : !closed.has(item.key)
    const Icon = item.icon
    return (
      <div key={item.key}>
        <div
          className={`tree-row ${item.track && hiddenSet.has(item.track.id) ? 'hidden-track' : ''} ${item.track && selectedTracks.includes(item.track.id) ? 'on' : ''}`}
          style={{ paddingLeft: 6 + depth * 14 }}
          onClick={() => {
            if (item.track) goTo(item.track)
            else {
              const next = new Set(closed)
              next.has(item.key) ? next.delete(item.key) : next.add(item.key)
              setClosed(next)
            }
          }}
          onContextMenu={(event) => openMenu(item, event)}
          title={item.label}
        >
          {item.children.length ? (
            <ChevronRight size={14} className={`chev ${open ? 'open' : ''}`} />
          ) : (
            <span style={{ width: 14 }} />
          )}
          {item.track ? <i className="dot" style={{ background: trackColor(item.track) }} /> : Icon && <Icon size={14} className="faint" />}
          <span className="name">{item.label}</span>
          {!compact && ids.length > 0 && (
            <button
              className={`icon-btn small eye ${hiddenCount ? 'off' : ''}`}
              title={hiddenCount === ids.length ? 'Show' : 'Hide'}
              onClick={(event) => {
                event.stopPropagation()
                setVisible(ids, hiddenCount === ids.length)
              }}
            >
              {hiddenCount === ids.length ? <EyeOff size={13} /> : <Eye size={13} style={{ opacity: hiddenCount ? 0.5 : 1 }} />}
            </button>
          )}
        </div>
        {open && item.children.map((child) => render(child, depth + 1))}
      </div>
    )
  }

  return (
    <aside className="sidebar">
      <div className="sidebar-head">
        <span className="section-label grow">System topology</span>
        <span className="faint">{timeline.tracks.length - hidden.length}/{timeline.tracks.length}</span>
      </div>
      <div style={{ padding: '0 10px 8px', position: 'relative' }}>
        <Search size={14} className="faint" style={{ position: 'absolute', left: 20, top: 9 }} />
        <input className="input small" style={{ width: '100%', paddingLeft: 28 }} placeholder="Filter tracks" value={query} onChange={(event) => setQuery(event.target.value)} />
      </div>
      <div className="tree">{tree.map((item) => render(item, 0))}</div>
      <Menu menu={menu} onClose={() => setMenu(null)} />
    </aside>
  )
}

// Event density across the whole trace, with the view as a window to drag.
function Overview() {
  const timeline = useStore((s) => s.timeline)!
  const hidden = useStore((s) => s.hidden)
  const normalizeVisible = useStore((s) => s.settings.normalizeVisible)
  const setSettings = useStore((s) => s.setSettings)
  const set = useStore((s) => s.set)
  const canvas = useRef<HTMLCanvasElement>(null)
  const [minimap, setMinimap] = useState<Minimap | null>(null)
  const drag = useRef<{ offset: number } | { anchor: number } | null>(null)

  useEffect(() => {
    get<Minimap>('/api/minimap').then(setMinimap, () => undefined)
  }, [])

  const bins = useMemo(() => {
    if (!minimap) return []
    const hiddenSet = new Set(hidden)
    const events = new Set(timeline.tracks.filter((t) => t.data_type === 'events').map((t) => t.id))
    let sum: number[] = []
    for (const [id, values] of Object.entries(minimap.tracks)) {
      if (!events.has(id) || (normalizeVisible && hiddenSet.has(id))) continue
      sum = values.map((v, i) => (sum[i] ?? 0) + v)
    }
    const max = Math.max(1e-12, ...sum)
    return sum.map((v) => v / max)
  }, [minimap, hidden, normalizeVisible, timeline])

  useEffect(() => {
    const draw = () => {
      const element = canvas.current
      if (!element) return
      const dpr = window.devicePixelRatio || 1
      const [width, height] = [element.clientWidth, element.clientHeight]
      element.width = width * dpr
      element.height = height * dpr
      const context = element.getContext('2d')!
      context.scale(dpr, dpr)
      const { view, range } = useStore.getState()
      const total = timeline.end_ns - timeline.start_ns || 1
      const toX = (t: number) => ((t - timeline.start_ns) / total) * width
      if (bins.length) {
        const gradient = context.createLinearGradient(0, 0, 0, height)
        gradient.addColorStop(0, 'rgba(255, 75, 79, 0.95)')
        gradient.addColorStop(1, 'rgba(237, 28, 36, 0.08)')
        context.beginPath()
        context.moveTo(0, height)
        bins.forEach((v, i) => context.lineTo(((i + 0.5) / bins.length) * width, height - 4 - v * (height - 12)))
        context.lineTo(width, height)
        context.closePath()
        context.fillStyle = gradient
        context.fill()
      }
      if (range) {
        context.fillStyle = 'rgba(237, 28, 36, 0.25)'
        context.fillRect(toX(range.start), 0, Math.max(1, toX(range.end) - toX(range.start)), height)
      }
      const [x0, x1] = [toX(view.start), toX(view.end)]
      context.fillStyle = 'rgba(0, 0, 0, 0.45)'
      context.fillRect(0, 0, Math.max(0, x0), height)
      context.fillRect(x1, 0, width - x1, height)
      context.strokeStyle = 'rgba(255, 255, 255, 0.85)'
      context.lineWidth = 1.5
      context.beginPath()
      context.roundRect(x0, 1, Math.max(3, x1 - x0), height - 2, 4)
      context.stroke()
    }
    draw()
    const observer = new ResizeObserver(draw)
    observer.observe(canvas.current!)
    const unsubscribe = useStore.subscribe((s, p) => (s.view !== p.view || s.range !== p.range) && draw())
    return () => {
      observer.disconnect()
      unsubscribe()
    }
  }, [bins, timeline])

  const timeAt = (clientX: number) => {
    const box = canvas.current!.getBoundingClientRect()
    return timeline.start_ns + ((clientX - box.left) / box.width) * (timeline.end_ns - timeline.start_ns)
  }

  return (
    <div className="overview">
      <canvas
        ref={canvas}
        style={{ cursor: 'ew-resize' }}
        onPointerDown={(event) => {
          event.currentTarget.setPointerCapture(event.pointerId)
          const time = timeAt(event.clientX)
          const { view } = useStore.getState()
          drag.current = time >= view.start && time <= view.end ? { offset: time - view.start } : { anchor: time }
        }}
        onPointerMove={(event) => {
          const current = drag.current
          if (!current) return
          const time = timeAt(event.clientX)
          const { view } = useStore.getState()
          if ('offset' in current) {
            const start = time - current.offset
            set({ view: clampView(start, start + view.end - view.start) })
          } else {
            set({ view: clampView(Math.min(current.anchor, time), Math.max(current.anchor, time)) })
          }
        }}
        onPointerUp={(event) => {
          const current = drag.current
          drag.current = null
          if (current && 'anchor' in current && Math.abs(timeAt(event.clientX) - current.anchor) < (timeline.end_ns - timeline.start_ns) / 500) {
            const { view } = useStore.getState()
            const span = view.end - view.start
            set({ view: clampView(current.anchor - span / 2, current.anchor + span / 2) })
          }
        }}
      />
      <button
        className="btn small ghost"
        style={{ position: 'absolute', right: 6, top: 4, fontSize: 11, height: 20 }}
        title="Normalize the density over all tracks or only the visible ones"
        onClick={() => setSettings({ normalizeVisible: !normalizeVisible })}
      >
        {normalizeVisible ? 'Visible tracks' : 'All tracks'}
      </button>
    </div>
  )
}
