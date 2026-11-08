import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react'
import {
  ChevronRight, Copy, Crosshair, Eye, EyeOff, Maximize2, Save, StickyNote, Trash2, Waypoints, X, ZoomIn,
} from 'lucide-react'
import { get, type Track, type TrackData } from '../api'
import { colorFor, formatDuration, formatNumber, formatTick, formatTime, niceInterval } from '../format'
import { useStore, type Note, type Range } from '../store'
import { Menu, type MenuState } from '../ui'

const RULER = 30
const HEAD = 236
const ROW = 18
const COUNTER_HEIGHT = 56
const MAX_ROWS = 24
const MIN_SPAN = 50
const BATCH = 48

interface Lane {
  track: Track
  top: number
  height: number
  rows: number
}

interface Cached {
  start: number
  end: number
  nsPerPixel: number
  data: TrackData
}

interface Hit {
  lane: Lane
  index: number
}

type Drag =
  | { kind: 'pan'; x: number; y: number; view: Range; scroll: number; moved: boolean }
  | { kind: 'range'; anchor: number }

export const trackColor = (track: Track) => colorFor(track.category + track.name)

// Keeps a view inside the trace, give or take a margin, and wider than a few ns.
export function clampView(start: number, end: number): Range {
  const { timeline } = useStore.getState()
  if (!timeline) return { start, end }
  const total = timeline.end_ns - timeline.start_ns
  let span = Math.min(Math.max(end - start, MIN_SPAN), total * 1.2)
  const low = timeline.start_ns - total * 0.1
  const high = timeline.end_ns + total * 0.1
  start = Math.min(Math.max(start, low), high - span)
  span = Math.min(span, high - start)
  return { start, end: start + span }
}

export function Timeline() {
  const timeline = useStore((s) => s.timeline)!
  const order = useStore((s) => s.order)
  const hidden = useStore((s) => s.hidden)
  const selectedTracks = useStore((s) => s.selectedTracks)
  const scroll = useStore((s) => s.scroll)
  const scrollTo = useStore((s) => s.scrollTo)
  const set = useStore((s) => s.set)
  const notify = useStore((s) => s.notify)

  const root = useRef<HTMLDivElement>(null)
  const canvas = useRef<HTMLCanvasElement>(null)
  const tooltip = useRef<HTMLDivElement>(null)
  const cache = useRef(new Map<string, Cached>())
  const fetching = useRef(false)
  const frame = useRef(0)
  const drag = useRef<Drag | null>(null)
  const hover = useRef<{ x: number; y: number; hit: Hit | null } | null>(null)
  const lanesRef = useRef<Lane[]>([])
  const [size, setSize] = useState({ width: 800, height: 400 })
  const [levels, setLevels] = useState<Record<string, number>>({})
  const [collapsed, setCollapsed] = useState<Set<string>>(new Set())
  const [menu, setMenu] = useState<MenuState | null>(null)
  const [editing, setEditing] = useState<{ note: Note; x: number; y: number } | null>(null)
  const [loading, setLoading] = useState(false)

  const byId = useMemo(() => new Map(timeline.tracks.map((t) => [t.id, t])), [timeline])
  const lanes = useMemo(() => {
    let top = 0
    const hiddenSet = new Set(hidden)
    return order.flatMap((id) => {
      const track = byId.get(id)
      if (!track || hiddenSet.has(id)) return []
      const counter = track.data_type === 'samples'
      const rows = counter ? 0 : collapsed.has(id) ? 1 : Math.min(MAX_ROWS, (levels[id] ?? 0) + 1)
      const height = counter ? COUNTER_HEIGHT : Math.max(44, rows * ROW + 10)
      const lane = { track, top, height, rows }
      top += height
      return [lane]
    })
  }, [order, hidden, byId, levels, collapsed])
  lanesRef.current = lanes
  const contentHeight = lanes.length ? lanes[lanes.length - 1].top + lanes[lanes.length - 1].height : 0
  const graphWidth = Math.max(1, size.width - HEAD)
  const viewportHeight = Math.max(1, size.height - RULER)
  const maxScroll = Math.max(0, contentHeight - viewportHeight + 24)

  const setScroll = useCallback(
    (value: number) => set({ scroll: Math.min(Math.max(0, value), maxScroll) }),
    [set, maxScroll],
  )

  // Drawing reads straight from the store so panning never waits on React.
  const draw = useCallback(() => {
    const element = canvas.current
    if (!element) return
    const context = element.getContext('2d')!
    const dpr = window.devicePixelRatio || 1
    const width = element.clientWidth
    const height = element.clientHeight
    if (element.width !== Math.round(width * dpr) || element.height !== Math.round(height * dpr)) {
      element.width = Math.round(width * dpr)
      element.height = Math.round(height * dpr)
    }
    const state = useStore.getState()
    const { view, range, event: selected, details, notes, selectedTracks: chosen, settings } = state
    const css = getComputedStyle(document.documentElement)
    const color = (name: string) => css.getPropertyValue(name).trim()
    const [text2, text3, line, line2, panel2, red] = ['--text-2', '--text-3', '--line', '--line-2', '--panel-2', '--red'].map(color)
    const scale = width / (view.end - view.start)
    const toX = (t: number) => (t - view.start) * scale
    const scrollTop = state.scroll

    context.setTransform(dpr, 0, 0, dpr, 0, 0)
    context.clearRect(0, 0, width, height)
    context.font = '500 11px "Inter Variable", system-ui, sans-serif'
    context.textBaseline = 'middle'

    const interval = niceInterval(view.end - view.start, width / 120)
    const firstTick = Math.ceil(view.start / interval) * interval

    context.save()
    context.beginPath()
    context.rect(0, RULER, width, height - RULER)
    context.clip()
    context.strokeStyle = line
    context.lineWidth = 1
    for (let t = firstTick; t <= view.end; t += interval) {
      const x = Math.round(toX(t)) + 0.5
      context.beginPath()
      context.moveTo(x, RULER)
      context.lineTo(x, height)
      context.stroke()
    }

    const laneTop = (lane: Lane) => RULER + lane.top - scrollTop
    const eventY = (lane: Lane, level: number) => laneTop(lane) + 5 + level * ROW
    for (const lane of lanesRef.current) {
      const y0 = laneTop(lane)
      if (y0 > height || y0 + lane.height < RULER) continue
      if (chosen.includes(lane.track.id)) {
        context.fillStyle = 'rgba(237, 28, 36, 0.06)'
        context.fillRect(0, y0, width, lane.height)
      }
      context.fillStyle = line
      context.fillRect(0, y0 + lane.height - 1, width, 1)
      const data = cache.current.get(lane.track.id)?.data
      if (data?.events) {
        for (const event of data.events) {
          if (event.level >= lane.rows) continue
          const x0 = toX(event.start_ns)
          const x1 = toX(event.start_ns + event.duration_ns)
          if (x1 < 0 || x0 > width) continue
          const w = Math.max(1, x1 - x0)
          const y = eventY(lane, event.level)
          const combined = event.child_count > 1
          context.globalAlpha = combined ? 0.5 : 0.92
          context.fillStyle = colorFor(combined ? event.top_combined_name || event.name : event.name)
          context.beginPath()
          context.roundRect(x0, y, w, ROW - 3, w > 6 ? 3 : 0)
          context.fill()
          context.globalAlpha = 1
          const left = Math.max(x0, 0) + 5
          const room = Math.min(x1, width) - left - 4
          if (room > 18) {
            const label = combined ? `${event.top_combined_name || event.name} +${event.child_count - 1}` : event.name
            const chars = Math.floor(room / 6.3)
            context.fillStyle = 'rgba(10, 10, 14, 0.92)'
            context.fillText(label.length > chars ? `${label.slice(0, Math.max(1, chars - 1))}…` : label, left, y + (ROW - 3) / 2 + 0.5)
          }
          if (selected?.uuid === event.uuid) {
            context.save()
            context.strokeStyle = '#fff'
            context.lineWidth = 2
            context.shadowColor = red
            context.shadowBlur = 14
            context.strokeRect(x0 - 1, y - 1, w + 2, ROW - 1)
            context.restore()
          }
        }
      } else if (data?.samples && data.samples.length) {
        const { min_value: low, max_value: high } = lane.track
        const span = high - low || 1
        const base = y0 + lane.height - 6
        const yOf = (v: number) => base - ((v - low) / span) * (lane.height - 16)
        const tone = trackColor(lane.track)
        context.beginPath()
        let firstX: number | null = null
        let lastX = 0
        for (const sample of data.samples) {
          const xa = toX(sample.start_ns)
          const xb = toX(sample.end_ns)
          if (xb < -2) continue
          if (xa > width + 2) break
          const yv = yOf(sample.value)
          if (firstX === null) {
            firstX = xa
            context.moveTo(xa, yv)
          } else context.lineTo(xa, yv)
          context.lineTo(xb, yv)
          lastX = xb
        }
        if (firstX !== null) {
          context.strokeStyle = tone
          context.lineWidth = 1.5
          context.stroke()
          context.lineTo(lastX, base)
          context.lineTo(firstX, base)
          context.closePath()
          const gradient = context.createLinearGradient(0, y0, 0, base)
          gradient.addColorStop(0, `${tone}55`)
          gradient.addColorStop(1, `${tone}05`)
          context.fillStyle = gradient
          context.fill()
        }
        context.fillStyle = text3
        context.fillText(`max ${formatNumber(high)}`, 6, y0 + 9)
      }
    }

    // Flows, as a chain through the selected event's flow list in time order.
    if (settings.flows && details && selected && details.uuid === selected.uuid && details.flows.length > 1) {
      const points = details.flows
        .map((flow) => {
          const lane = lanesRef.current.find((l) => l.track.id === flow.track_id)
          return lane && { flow, y: eventY(lane, Math.min(flow.level, Math.max(0, lane.rows - 1))) + (ROW - 3) / 2 }
        })
        .filter((p): p is NonNullable<typeof p> => !!p)
        .sort((a, b) => a.flow.start_ns - b.flow.start_ns)
      context.strokeStyle = '#ff7a59'
      context.fillStyle = '#ff7a59'
      context.lineWidth = 1.6
      for (let i = 1; i < points.length; i++) {
        const from = points[i - 1]
        const to = points[i]
        const x0 = toX(Math.max(from.flow.start_ns, Math.min(from.flow.end_ns, to.flow.start_ns)))
        const x1 = toX(to.flow.start_ns)
        const bend = Math.max(30, Math.abs(x1 - x0) / 2)
        context.beginPath()
        context.moveTo(x0, from.y)
        context.bezierCurveTo(x0 + bend, from.y, x1 - bend, to.y, x1, to.y)
        context.stroke()
        context.beginPath()
        context.moveTo(x1, to.y)
        context.lineTo(x1 - 8, to.y - 4)
        context.lineTo(x1 - 8, to.y + 4)
        context.fill()
        context.beginPath()
        context.arc(x0, from.y, 2.5, 0, Math.PI * 2)
        context.fill()
      }
    }

    if (range) {
      const x0 = toX(range.start)
      const x1 = toX(range.end)
      context.fillStyle = 'rgba(237, 28, 36, 0.09)'
      context.fillRect(x0, RULER, x1 - x0, height - RULER)
      context.fillStyle = red
      context.fillRect(Math.round(x0), RULER, 1, height - RULER)
      context.fillRect(Math.round(x1), RULER, 1, height - RULER)
    }

    for (const note of notes) {
      const x = toX(note.time)
      if (x < -10 || x > width + 10) continue
      const lane = lanesRef.current.find((l) => l.track.id === note.trackId)
      context.setLineDash([3, 4])
      context.strokeStyle = 'rgba(247, 197, 72, 0.6)'
      context.beginPath()
      context.moveTo(x + 0.5, lane ? laneTop(lane) : RULER)
      context.lineTo(x + 0.5, lane ? laneTop(lane) + lane.height : height)
      context.stroke()
      context.setLineDash([])
    }

    const cursor = hover.current
    if (cursor && !drag.current) {
      context.fillStyle = line2
      context.fillRect(Math.round(cursor.x), RULER, 1, height - RULER)
    }
    context.restore()

    // The ruler sits on top: ticks, the selected range and notes.
    context.fillStyle = panel2
    context.fillRect(0, 0, width, RULER)
    context.fillStyle = line
    context.fillRect(0, RULER - 1, width, 1)
    context.fillStyle = text2
    for (let t = firstTick; t <= view.end; t += interval) {
      const x = Math.round(toX(t)) + 0.5
      context.fillRect(x - 0.5, RULER - 7, 1, 6)
      context.fillText(formatTick(t - timeline.start_ns, interval), x + 4, 11)
    }
    if (range) {
      const x0 = toX(range.start)
      const x1 = toX(range.end)
      context.fillStyle = 'rgba(237, 28, 36, 0.28)'
      context.fillRect(x0, 0, x1 - x0, RULER - 1)
      const label = formatDuration(range.end - range.start)
      const labelWidth = context.measureText(label).width + 14
      const middle = Math.min(Math.max((x0 + x1) / 2, labelWidth / 2 + 2), width - labelWidth / 2 - 2)
      context.fillStyle = red
      context.beginPath()
      context.roundRect(middle - labelWidth / 2, 18, labelWidth, 11, 5)
      context.fill()
      context.fillStyle = '#fff'
      context.textAlign = 'center'
      context.font = '650 10px "Inter Variable", system-ui, sans-serif'
      context.fillText(label, middle, 23.5)
      context.textAlign = 'left'
      context.font = '500 11px "Inter Variable", system-ui, sans-serif'
    }
    for (const note of notes) {
      const x = toX(note.time)
      if (x < -10 || x > width + 10) continue
      context.fillStyle = '#f7c548'
      context.beginPath()
      context.moveTo(x, RULER - 2)
      context.lineTo(x - 5, RULER - 9)
      context.lineTo(x - 5, 17)
      context.lineTo(x + 5, 17)
      context.lineTo(x + 5, RULER - 9)
      context.fill()
    }
    if (cursor && cursor.y > RULER) {
      const label = formatTick(view.start + cursor.x / scale - timeline.start_ns, (view.end - view.start) / width)
      const labelWidth = context.measureText(label).width + 12
      context.fillStyle = color('--text')
      context.beginPath()
      context.roundRect(cursor.x - labelWidth / 2, 3, labelWidth, 14, 4)
      context.fill()
      context.fillStyle = color('--bg')
      context.textAlign = 'center'
      context.fillText(label, cursor.x, 10.5)
      context.textAlign = 'left'
    }
  }, [timeline])

  const schedule = useCallback(() => {
    cancelAnimationFrame(frame.current)
    frame.current = requestAnimationFrame(draw)
  }, [draw])

  // Fetches, a batch at a time, any visible track whose cached data does not
  // cover the view at this zoom.
  const pump = useCallback(async () => {
    if (fetching.current) return
    const { view, scroll: scrollTop } = useStore.getState()
    const dpr = window.devicePixelRatio || 1
    const span = view.end - view.start
    const nsPerPixel = span / (graphWidth * dpr)
    const start = Math.max(timeline.start_ns, view.start - span)
    const end = Math.min(timeline.end_ns, view.end + span)
    if (!(end > start)) return
    const ids = lanesRef.current
      .filter((lane) => lane.top + lane.height > scrollTop - 200 && lane.top < scrollTop + viewportHeight + 200)
      .filter((lane) => {
        const cached = cache.current.get(lane.track.id)
        return (
          !cached ||
          Math.max(view.start, timeline.start_ns) < cached.start ||
          Math.min(view.end, timeline.end_ns) > cached.end ||
          cached.nsPerPixel > nsPerPixel * 1.6
        )
      })
      .map((lane) => lane.track.id)
      .slice(0, BATCH)
    if (!ids.length) return
    const pixels = Math.min(8192, Math.max(1, Math.round((end - start) / nsPerPixel)))
    fetching.current = true
    setLoading(true)
    try {
      const reply = await get<{ tracks: TrackData[] }>('/api/track_data', { tracks: ids.join(','), start, end, px: pixels })
      const deepest: Record<string, number> = {}
      for (const data of reply.tracks) {
        cache.current.set(data.track_id, { start, end, nsPerPixel: (end - start) / pixels, data })
        if (data.events) deepest[data.track_id] = data.events.reduce((m, e) => Math.max(m, e.level), 0)
      }
      setLevels((previous) => {
        const grown = Object.entries(deepest).filter(([id, level]) => level > (previous[id] ?? 0))
        return grown.length ? { ...previous, ...Object.fromEntries(grown) } : previous
      })
      fetching.current = false
      schedule()
      setTimeout(pump, 0)
    } catch (error) {
      fetching.current = false
      notify(`Could not load track data: ${(error as Error).message}`, true)
    } finally {
      setLoading(false)
    }
  }, [timeline, graphWidth, viewportHeight, notify, schedule])

  useLayoutEffect(() => {
    const element = root.current!
    const measure = () => setSize({ width: element.clientWidth, height: element.clientHeight })
    measure()
    const observer = new ResizeObserver(measure)
    observer.observe(element)
    return () => observer.disconnect()
  }, [])

  useEffect(() => {
    let timer = 0
    const unsubscribe = useStore.subscribe((state, previous) => {
      schedule()
      if (state.summary || state.search || state.dialog) tooltip.current!.style.display = 'none'
      if (state.view !== previous.view || state.scroll !== previous.scroll) {
        clearTimeout(timer)
        timer = window.setTimeout(pump, 40)
      }
    })
    schedule()
    pump()
    return () => {
      unsubscribe()
      clearTimeout(timer)
    }
  }, [schedule, pump, lanes])

  useEffect(() => {
    if (!scrollTo) return
    const lane = lanes.find((l) => l.track.id === scrollTo)
    if (lane && (lane.top < scroll || lane.top + lane.height > scroll + viewportHeight)) {
      setScroll(lane.top - viewportHeight / 3)
    }
    set({ scrollTo: null })
  }, [scrollTo, lanes])

  // Pointer helpers, in canvas coordinates.
  const timeAt = (x: number) => {
    const { view } = useStore.getState()
    return view.start + (x / graphWidth) * (view.end - view.start)
  }
  const local = (event: { clientX: number; clientY: number }) => {
    const box = canvas.current!.getBoundingClientRect()
    return { x: event.clientX - box.left, y: event.clientY - box.top }
  }
  const hitTest = (x: number, y: number): Hit | null => {
    const { scroll: scrollTop, view } = useStore.getState()
    const contentY = y - RULER + scrollTop
    const lane = lanesRef.current.find((l) => contentY >= l.top && contentY < l.top + l.height)
    const events = lane && cache.current.get(lane.track.id)?.data.events
    if (!lane || !events) return lane ? { lane, index: -1 } : null
    const level = Math.floor((contentY - lane.top - 5) / ROW)
    const scale = graphWidth / (view.end - view.start)
    const slack = 3 / scale
    const time = timeAt(x)
    for (let i = events.length - 1; i >= 0; i--) {
      const event = events[i]
      if (event.level === level && time >= event.start_ns - slack && time <= event.start_ns + event.duration_ns + slack) {
        return { lane, index: i }
      }
    }
    return { lane, index: -1 }
  }

  const showTooltip = (clientX: number, clientY: number, hit: Hit | null, x: number) => {
    const element = tooltip.current!
    const data = hit && cache.current.get(hit.lane.track.id)?.data
    const { timeFormat } = useStore.getState().settings
    let html = ''
    if (hit && data?.events && hit.index >= 0) {
      const event = data.events[hit.index]
      const name = event.child_count > 1 ? `${event.child_count} events · ${event.top_combined_name || event.name}` : event.name
      html = `<div class="name">${escape(name)}</div><div class="kv"><span>Duration</span><span class="mono">${formatDuration(event.duration_ns)}</span><span>Start</span><span class="mono">${formatTime(event.start_ns - timeline.start_ns, timeFormat)}</span>${event.child_count > 1 ? '<span>Click</span><span>to zoom in</span>' : ''}</div>`
    } else if (hit && data?.samples) {
      const time = timeAt(x)
      const sample = data.samples.find((s) => time >= s.start_ns && time < s.end_ns)
      if (sample) {
        html = `<div class="name">${escape(hit.lane.track.name)}</div><div class="kv"><span>Value</span><span class="mono">${formatNumber(sample.value, 4)}</span><span>At</span><span class="mono">${formatTime(sample.start_ns - timeline.start_ns, timeFormat)}</span></div>`
      }
    }
    element.style.display = html ? 'block' : 'none'
    if (html) {
      element.innerHTML = html
      const left = Math.min(clientX + 14, window.innerWidth - element.offsetWidth - 8)
      const top = clientY + 18 + element.offsetHeight > window.innerHeight ? clientY - element.offsetHeight - 10 : clientY + 18
      element.style.transform = `translate(${left}px, ${top}px)`
    }
  }

  const selectHit = (hit: Hit | null, additive: boolean) => {
    const { selectedTracks: chosen } = useStore.getState()
    const data = hit && cache.current.get(hit.lane.track.id)?.data
    if (hit && data?.events && hit.index >= 0) {
      const event = data.events[hit.index]
      if (event.child_count > 1) {
        set({ view: clampView(event.start_ns - event.duration_ns * 0.1, event.start_ns + event.duration_ns * 1.1) })
        return
      }
      set({
        event: { uuid: event.uuid, trackId: hit.lane.track.id, name: event.name, start: event.start_ns, duration: event.duration_ns, level: event.level },
        details: null,
        tab: 'event',
      })
    } else if (hit) {
      const id = hit.lane.track.id
      set({ selectedTracks: additive ? (chosen.includes(id) ? chosen.filter((t) => t !== id) : [...chosen, id]) : [id] })
    } else {
      set({ event: null, details: null })
    }
  }

  const zoomAt = (x: number, factor: number) => {
    const { view } = useStore.getState()
    const anchor = timeAt(x)
    set({ view: clampView(anchor - (anchor - view.start) * factor, anchor + (view.end - anchor) * factor) })
  }

  const onPointerDown = (event: React.PointerEvent) => {
    if (event.button !== 0) return
    const { x, y } = local(event)
    root.current?.focus()
    const note = noteAt(x, y)
    if (note) {
      setEditing({ note, x: x + HEAD, y: RULER + 4 })
      return
    }
    event.currentTarget.setPointerCapture(event.pointerId)
    const state = useStore.getState()
    if (y < RULER || event.ctrlKey || event.metaKey) {
      const anchor = timeAt(x)
      drag.current = { kind: 'range', anchor }
      set({ range: { start: anchor, end: anchor } })
    } else {
      drag.current = { kind: 'pan', x, y, view: state.view, scroll: state.scroll, moved: false }
    }
  }

  const onPointerMove = (event: React.PointerEvent) => {
    const { x, y } = local(event)
    const current = drag.current
    if (current?.kind === 'pan') {
      const dx = x - current.x
      const dy = y - current.y
      if (Math.abs(dx) + Math.abs(dy) > 3) current.moved = true
      const shift = (dx / graphWidth) * (current.view.end - current.view.start)
      set({ view: clampView(current.view.start - shift, current.view.end - shift) })
      setScroll(current.scroll - dy)
    } else if (current?.kind === 'range') {
      const time = timeAt(x)
      set({ range: { start: Math.min(current.anchor, time), end: Math.max(current.anchor, time) } })
    } else {
      const hit = y > RULER ? hitTest(x, y) : null
      hover.current = { x, y, hit }
      showTooltip(event.clientX, event.clientY, hit, x)
      canvas.current!.style.cursor = hit && hit.index >= 0 ? 'pointer' : y < RULER ? 'text' : 'grab'
    }
    schedule()
  }

  const onPointerUp = (event: React.PointerEvent) => {
    const current = drag.current
    drag.current = null
    const { x, y } = local(event)
    if (current?.kind === 'pan' && !current.moved) {
      selectHit(hitTest(x, y), event.ctrlKey || event.metaKey || event.shiftKey)
    } else if (current?.kind === 'range') {
      const { range, view } = useStore.getState()
      if (range && ((range.end - range.start) / (view.end - view.start)) * graphWidth < 3) set({ range: null })
    }
  }

  const noteAt = (x: number, y: number) => {
    if (y > RULER) return null
    const { view, notes } = useStore.getState()
    const scale = graphWidth / (view.end - view.start)
    return notes.find((note) => Math.abs((note.time - view.start) * scale - x) < 7) ?? null
  }

  const onWheel = (event: WheelEvent) => {
    event.preventDefault()
    const { x } = local(event)
    if (event.shiftKey) {
      setScroll(useStore.getState().scroll + event.deltaY)
    } else if (Math.abs(event.deltaX) > Math.abs(event.deltaY)) {
      const { view } = useStore.getState()
      const shift = (event.deltaX / graphWidth) * (view.end - view.start)
      set({ view: clampView(view.start + shift, view.end + shift) })
    } else {
      zoomAt(x, Math.exp(event.deltaY * (event.ctrlKey ? 0.01 : 0.0022)))
    }
  }

  // Registered by hand: React's wheel listener is passive, and a pinch arrives
  // as Ctrl+wheel, which would otherwise zoom the whole page.
  const wheel = useRef(onWheel)
  wheel.current = onWheel
  useEffect(() => {
    const element = canvas.current!
    const listener = (event: WheelEvent) => wheel.current(event)
    element.addEventListener('wheel', listener, { passive: false })
    return () => element.removeEventListener('wheel', listener)
  }, [])

  const addNote = (time: number, trackId: string | null) => {
    const note: Note = { id: crypto.randomUUID(), time, trackId, title: 'Note', text: '' }
    set({ notes: [...useStore.getState().notes, note] })
    const { view } = useStore.getState()
    setEditing({ note, x: HEAD + ((time - view.start) / (view.end - view.start)) * graphWidth, y: RULER + 4 })
  }

  const onContextMenu = (event: React.MouseEvent) => {
    event.preventDefault()
    const { x, y } = local(event)
    const time = timeAt(x)
    const hit = y > RULER ? hitTest(x, y) : null
    const state = useStore.getState()
    setMenu({
      x: event.clientX,
      y: event.clientY,
      items: [
        { label: 'Add note here', icon: StickyNote, onClick: () => addNote(time, hit?.lane.track.id ?? null) },
        { label: 'Zoom to selection', icon: ZoomIn, hint: 'Z', disabled: !state.range, onClick: () => zoomToSelection() },
        { label: 'Save selection as trace…', icon: Save, disabled: !state.range, onClick: () => set({ dialog: 'save' }) },
        'divider',
        { label: 'Copy time', icon: Copy, onClick: () => navigator.clipboard.writeText(formatTime(time - timeline.start_ns, state.settings.timeFormat)) },
        { label: state.settings.flows ? 'Hide flows' : 'Show flows', icon: Waypoints, onClick: () => state.setSettings({ flows: !state.settings.flows }) },
        { label: 'Reset zoom', icon: Maximize2, onClick: () => set({ view: { start: timeline.start_ns, end: timeline.end_ns } }) },
        'divider',
        { label: 'Unselect all tracks', icon: X, disabled: !state.selectedTracks.length, onClick: () => set({ selectedTracks: [] }) },
        { label: 'Unselect event and range', icon: Crosshair, disabled: !state.event && !state.range, onClick: () => set({ event: null, details: null, range: null }) },
      ],
    })
  }

  const zoomToSelection = () => {
    const { range, event } = useStore.getState()
    const target = range ?? (event && { start: event.start, end: event.start + event.duration })
    if (target) {
      const pad = (target.end - target.start) * 0.05
      set({ view: clampView(target.start - pad, target.end + pad) })
    }
  }

  // Desktop hotkeys: A/D pan, W/S zoom, arrows scroll, Esc clears, M marks the
  // selected event's range, Z zooms to it, 0-9 recall and Ctrl+0-9 save views.
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      const target = event.target as HTMLElement
      if (target.closest('input, textarea, select, [contenteditable]') || useStore.getState().dialog) return
      const state = useStore.getState()
      const { view } = state
      const span = view.end - view.start
      const boost = event.shiftKey ? 3 : 1
      const pointer = hover.current?.x ?? graphWidth / 2
      const key = event.key.toLowerCase()
      if (/^[0-9]$/.test(key)) {
        const slot = Number(key)
        const bookmarks = [...state.bookmarks]
        if (event.ctrlKey || event.metaKey) {
          event.preventDefault()
          bookmarks[slot] = { ...view, scroll: state.scroll }
          set({ bookmarks })
          notify(`Saved view to bookmark ${slot}`)
        } else if (bookmarks[slot]) {
          const { start, end, scroll: saved } = bookmarks[slot]!
          set({ view: { start, end } })
          setScroll(saved)
        }
        return
      }
      if (event.ctrlKey || event.metaKey || event.altKey) return
      const actions: Record<string, () => void> = {
        a: () => set({ view: clampView(view.start - span * 0.1 * boost, view.end - span * 0.1 * boost) }),
        d: () => set({ view: clampView(view.start + span * 0.1 * boost, view.end + span * 0.1 * boost) }),
        w: () => zoomAt(pointer, 1 / (1 + 0.25 * boost)),
        s: () => zoomAt(pointer, 1 + 0.25 * boost),
        arrowup: () => setScroll(state.scroll - 60 * boost),
        arrowdown: () => setScroll(state.scroll + 60 * boost),
        escape: () => set({ event: null, details: null, range: null }),
        z: zoomToSelection,
        m: () => {
          if (state.range) set({ range: null })
          else if (state.event) set({ range: { start: state.event.start, end: state.event.start + state.event.duration } })
          else notify('No events selected to mark.')
        },
      }
      actions.arrowleft = actions.a
      actions.arrowright = actions.d
      if (actions[key]) {
        event.preventDefault()
        actions[key]()
      }
    }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [graphWidth, setScroll])

  const toggleTrack = (id: string, event: React.MouseEvent) => {
    const additive = event.ctrlKey || event.metaKey || event.shiftKey
    set({
      selectedTracks: additive
        ? selectedTracks.includes(id) ? selectedTracks.filter((t) => t !== id) : [...selectedTracks, id]
        : selectedTracks.length === 1 && selectedTracks[0] === id ? [] : [id],
    })
  }

  const trackMenu = (track: Track, event: React.MouseEvent) => {
    event.preventDefault()
    const state = useStore.getState()
    setMenu({
      x: event.clientX,
      y: event.clientY,
      items: [
        { section: track.name },
        { label: 'Hide track', icon: EyeOff, onClick: () => set({ hidden: [...state.hidden, track.id] }) },
        { label: 'Hide all but this track', icon: Eye, onClick: () => set({ hidden: state.order.filter((id) => id !== track.id) }) },
        { label: 'Show all tracks', icon: Eye, disabled: !state.hidden.length, onClick: () => set({ hidden: [] }) },
        { label: 'Hide selected tracks', icon: EyeOff, disabled: !state.selectedTracks.length, onClick: () => set({ hidden: [...new Set([...state.hidden, ...state.selectedTracks])], selectedTracks: [] }) },
        'divider',
        { label: 'Add note on this track', icon: StickyNote, onClick: () => addNote((state.view.start + state.view.end) / 2, track.id) },
        { label: 'Fit track data', icon: Maximize2, onClick: () => set({ view: clampView(track.start_ns, track.end_ns) }) },
      ],
    })
  }

  const moveTrack = (id: string, before: string) => {
    if (id === before) return
    const next = order.filter((t) => t !== id)
    next.splice(next.indexOf(before), 0, id)
    set({ order: next })
  }

  const visibleLanes = lanes.filter((l) => l.top + l.height > scroll && l.top < scroll + viewportHeight)

  return (
    <div
      className="timeline"
      ref={root}
      tabIndex={0}
      onMouseLeave={() => {
        hover.current = null
        tooltip.current!.style.display = 'none'
        schedule()
      }}
    >
      <div className="corner" style={{ width: HEAD }}>
        <span className="grow">{lanes.length} tracks</span>
        {loading && <div className="spinner" style={{ width: 12, height: 12 }} />}
      </div>
      <div className="heads" style={{ width: HEAD }} onWheel={(event) => setScroll(scroll + event.deltaY)}>
        {visibleLanes.map((lane) => {
          const { track } = lane
          const counter = track.data_type === 'samples'
          return (
            <div
              key={track.id}
              className={`head ${selectedTracks.includes(track.id) ? 'on' : ''}`}
              style={{ top: lane.top - scroll, height: lane.height }}
              draggable
              onDragStart={(event) => event.dataTransfer.setData('text/track', track.id)}
              onDragOver={(event) => event.preventDefault()}
              onDrop={(event) => moveTrack(event.dataTransfer.getData('text/track'), track.id)}
              onClick={(event) => toggleTrack(track.id, event)}
              onContextMenu={(event) => trackMenu(track, event)}
              title={`${track.name}\n${track.main_name}\n${track.entry_count.toLocaleString()} entries`}
            >
              {counter ? (
                <i className="dot" style={{ background: trackColor(track), marginTop: 5 }} />
              ) : (
                <button
                  className="icon-btn small"
                  style={{ width: 16, height: 16, marginTop: 1 }}
                  onClick={(event) => {
                    event.stopPropagation()
                    const next = new Set(collapsed)
                    next.has(track.id) ? next.delete(track.id) : next.add(track.id)
                    setCollapsed(next)
                  }}
                >
                  <ChevronRight size={13} style={{ transform: collapsed.has(track.id) ? undefined : 'rotate(90deg)', transition: 'transform .15s' }} />
                </button>
              )}
              <div className="grow">
                <div className="title ellipsis">{track.name}</div>
                <div className="sub ellipsis">{track.main_name}</div>
              </div>
              <div className="tools">
                <button
                  className="icon-btn small"
                  title="Hide track"
                  onClick={(event) => {
                    event.stopPropagation()
                    set({ hidden: [...hidden, track.id] })
                  }}
                >
                  <EyeOff size={13} />
                </button>
              </div>
            </div>
          )
        })}
      </div>
      <canvas
        ref={canvas}
        style={{ left: HEAD, width: graphWidth, height: size.height }}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={onPointerUp}
        onDoubleClick={(event) => {
          const { x, y } = local(event)
          const hit = hitTest(x, y)
          const data = hit && cache.current.get(hit.lane.track.id)?.data
          if (hit && data?.events && hit.index >= 0) {
            const e = data.events[hit.index]
            set({ view: clampView(e.start_ns - e.duration_ns * 0.15, e.start_ns + e.duration_ns * 1.15) })
          }
        }}
        onContextMenu={onContextMenu}
      />
      <div className="tooltip" ref={tooltip} style={{ display: 'none', left: 0, top: 0 }} />
      {editing && (
        <NoteEditor
          note={editing.note}
          x={Math.min(editing.x, size.width - 270)}
          y={editing.y}
          onClose={() => setEditing(null)}
        />
      )}
      <Menu menu={menu} onClose={() => setMenu(null)} />
    </div>
  )
}

function NoteEditor({ note, x, y, onClose }: { note: Note; x: number; y: number; onClose: () => void }) {
  const set = useStore((s) => s.set)
  const update = (patch: Partial<Note>) =>
    set({ notes: useStore.getState().notes.map((n) => (n.id === note.id ? { ...n, ...patch } : n)) })
  const current = useStore((s) => s.notes.find((n) => n.id === note.id))
  if (!current) return null
  return (
    <div className="note-pop" style={{ left: x, top: y }} onPointerDown={(event) => event.stopPropagation()}>
      <div className="row" style={{ marginBottom: 8 }}>
        <StickyNote size={15} color="#f7c548" />
        <input className="input small grow" value={current.title} autoFocus onChange={(event) => update({ title: event.target.value })} />
        <button className="icon-btn small" title="Delete note" onClick={() => { set({ notes: useStore.getState().notes.filter((n) => n.id !== note.id) }); onClose() }}>
          <Trash2 size={14} />
        </button>
        <button className="icon-btn small" title="Close" onClick={onClose}>
          <X size={14} />
        </button>
      </div>
      <textarea className="input" rows={4} style={{ width: '100%' }} placeholder="Write a note…" value={current.text} onChange={(event) => update({ text: event.target.value })} />
    </div>
  )
}

function escape(text: string) {
  return text.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!)
}
