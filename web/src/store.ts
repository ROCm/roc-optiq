import { create } from 'zustand'
import type { EventDetails, Status, Timeline, Topology } from './api'

export type TimeFormat = 'timecode' | 'condensed' | 's' | 'ms' | 'us' | 'ns'

export interface Settings {
  theme: 'dark' | 'light'
  timeFormat: TimeFormat
  fontSize: number
  compactSidebar: boolean
  sidebar: boolean
  overview: boolean
  details: boolean
  flows: boolean
  flowStyle: 'chain' | 'fan'
  annotations: boolean
  normalizeVisible: boolean
  detailsHeight: number
}

export interface Range {
  start: number
  end: number
}

export interface EventRef {
  uuid: string
  trackId: string
  name: string
  start: number
  duration: number
  level: number
}

export interface Note {
  id: string
  time: number
  trackId: string | null
  title: string
  text: string
}

export interface Bookmark extends Range {
  scroll: number
}

// What a trace remembers between visits, the web's take on a project file.
interface Project {
  hidden: string[]
  order: string[]
  notes: Note[]
  bookmarks: (Bookmark | null)[]
}

export type Dialog = 'open' | 'settings' | 'about' | 'save' | null

interface State extends Project {
  status: Status | null
  // The server stopped answering; views reload when it is back (epoch changes).
  offline: boolean
  epoch: number
  settings: Settings
  recent: string[]
  timeline: Timeline | null
  topology: Topology | null
  view: Range
  scroll: number
  selectedTracks: string[]
  event: EventRef | null
  details: EventDetails | null
  // Events Ask Optiq pointed at, drawn with a glow until something clears them.
  highlights: string[]
  range: Range | null
  measuring: boolean
  scrollTo: string | null
  workloadId: number | null
  kernelId: number | null
  dialog: Dialog
  summary: boolean
  search: boolean
  assistant: boolean
  tab: string
  toast: { text: string; error?: boolean } | null
  set: (state: Partial<State>) => void
  setSettings: (settings: Partial<Settings>) => void
  notify: (text: string, error?: boolean) => void
  focusEvent: (event: EventRef) => void
  showTrace: (status: Status) => void
}

const DEFAULT_SETTINGS: Settings = {
  theme: 'dark',
  timeFormat: 'condensed',
  fontSize: 13,
  compactSidebar: false,
  sidebar: true,
  overview: true,
  details: true,
  flows: true,
  flowStyle: 'chain',
  annotations: true,
  normalizeVisible: false,
  detailsHeight: 300,
}

const read = <T>(key: string, fallback: T): T => {
  try {
    return { ...fallback, ...JSON.parse(localStorage.getItem(key) ?? 'null') }
  } catch {
    return fallback
  }
}

const projectKey = (path: string) => `optiq.project:${path}`
const emptyProject = (): Project => ({ hidden: [], order: [], notes: [], bookmarks: [] })
let toastTimer = 0

export const useStore = create<State>((set, get) => ({
  ...emptyProject(),
  status: null,
  offline: false,
  epoch: 0,
  settings: read('optiq.settings', DEFAULT_SETTINGS),
  recent: read<{ list: string[] }>('optiq.recent', { list: [] }).list,
  timeline: null,
  topology: null,
  view: { start: 0, end: 1 },
  scroll: 0,
  selectedTracks: [],
  event: null,
  details: null,
  highlights: [],
  range: null,
  measuring: false,
  scrollTo: null,
  workloadId: null,
  kernelId: null,
  dialog: null,
  summary: false,
  search: false,
  assistant: false,
  tab: 'event',
  toast: null,
  set,
  setSettings: (settings) => {
    const next = { ...get().settings, ...settings }
    localStorage.setItem('optiq.settings', JSON.stringify(next))
    set({ settings: next })
  },
  notify: (text, error) => {
    clearTimeout(toastTimer)
    set({ toast: { text, error } })
    toastTimer = window.setTimeout(() => set({ toast: null }), 3500)
  },
  // Selects an event and brings it to the middle of the timeline.
  focusEvent: (event) => {
    const { view, hidden } = get()
    const span = Math.max(event.duration * 3, Math.min(view.end - view.start, 1e6))
    const middle = event.start + event.duration / 2
    set({
      event,
      details: null,
      tab: 'event',
      view: { start: middle - span / 2, end: middle + span / 2 },
      hidden: hidden.filter((id) => id !== event.trackId),
      scrollTo: event.trackId,
      summary: false,
      search: false,
    })
  },
  showTrace: (status) => {
    const path = status.trace_file_path
    const recent = path ? [path, ...get().recent.filter((p) => p !== path)].slice(0, 10) : get().recent
    localStorage.setItem('optiq.recent', JSON.stringify({ list: recent }))
    set({
      ...read(projectKey(path), emptyProject()),
      status,
      recent,
      timeline: null,
      topology: null,
      selectedTracks: [],
      event: null,
      details: null,
      highlights: [],
      range: null,
      workloadId: null,
      kernelId: null,
      summary: false,
    })
  },
}))

useStore.subscribe((state, previous) => {
  const path = state.status?.trace_file_path
  const changed =
    state.hidden !== previous.hidden ||
    state.order !== previous.order ||
    state.notes !== previous.notes ||
    state.bookmarks !== previous.bookmarks
  if (path && changed) {
    const { hidden, order, notes, bookmarks } = state
    localStorage.setItem(projectKey(path), JSON.stringify({ hidden, order, notes, bookmarks }))
  }
})
