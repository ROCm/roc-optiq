import { Component, useEffect, useLayoutEffect, useRef, useState, type ReactNode } from 'react'
import { AlertTriangle, X, type LucideIcon } from 'lucide-react'

// Keeps one view's failure from blanking the whole app.
export class Boundary extends Component<{ children: ReactNode }, { error: Error | null }> {
  state = { error: null as Error | null }
  static getDerivedStateFromError(error: Error) {
    return { error }
  }
  render() {
    if (!this.state.error) return this.props.children
    return (
      <Empty
        icon={AlertTriangle}
        title="This view hit an unexpected error"
        hint={
          <button className="btn small" onClick={() => this.setState({ error: null })}>
            {this.state.error.message} · Retry
          </button>
        }
      />
    )
  }
}

export function AmdMark({ size = 22 }: { size?: number }) {
  return (
    <svg width={size} height={size} viewBox="18.277 9.137 5.723 5.726" aria-label="AMD">
      <path
        fill="var(--red)"
        d="M18.324 9.137l1.559 1.56h2.556v2.557L24 14.814V9.137zM19.881 11.01l-1.604 1.603v2.25h2.246l1.604-1.607h-2.246z"
      />
    </svg>
  )
}

export function Modal(props: {
  title: ReactNode
  icon?: LucideIcon
  onClose: () => void
  children: ReactNode
  footer?: ReactNode
  width?: number
}) {
  const Icon = props.icon
  useEffect(() => {
    const onKey = (event: KeyboardEvent) => event.key === 'Escape' && props.onClose()
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [props.onClose])
  return (
    <div className="backdrop" onMouseDown={(event) => event.target === event.currentTarget && props.onClose()}>
      <div className="modal" style={props.width ? { width: `min(${props.width}px, calc(100vw - 40px))` } : undefined}>
        <div className="modal-head">
          {Icon && <Icon size={18} color="var(--red)" />}
          <div className="grow">{props.title}</div>
          <button className="icon-btn small" onClick={props.onClose} title="Close">
            <X size={16} />
          </button>
        </div>
        <div className="modal-body">{props.children}</div>
        {props.footer && <div className="modal-foot">{props.footer}</div>}
      </div>
    </div>
  )
}

export type MenuItem =
  | { label: string; icon?: LucideIcon; onClick: () => void; disabled?: boolean; hint?: string }
  | { section: string }
  | 'divider'

export interface MenuState {
  x: number
  y: number
  items: MenuItem[]
}

// A popup menu at a screen point that closes on any outside click or Escape.
export function Menu({ menu, onClose }: { menu: MenuState | null; onClose: () => void }) {
  const ref = useRef<HTMLDivElement>(null)
  const [position, setPosition] = useState({ x: 0, y: 0 })
  useLayoutEffect(() => {
    if (!menu || !ref.current) return
    const { width, height } = ref.current.getBoundingClientRect()
    setPosition({
      x: Math.min(menu.x, window.innerWidth - width - 8),
      y: Math.min(menu.y, window.innerHeight - height - 8),
    })
  }, [menu])
  useEffect(() => {
    if (!menu) return
    const close = (event: Event) => {
      if (!(event.target instanceof Node && ref.current?.contains(event.target))) onClose()
    }
    const onKey = (event: KeyboardEvent) => event.key === 'Escape' && onClose()
    window.addEventListener('mousedown', close)
    window.addEventListener('keydown', onKey)
    window.addEventListener('blur', onClose)
    return () => {
      window.removeEventListener('mousedown', close)
      window.removeEventListener('keydown', onKey)
      window.removeEventListener('blur', onClose)
    }
  }, [menu, onClose])
  if (!menu) return null
  return (
    <div className="menu" ref={ref} style={{ left: position.x, top: position.y }}>
      {menu.items.map((item, index) => {
        if (item === 'divider') return <hr key={index} />
        if ('section' in item)
          return (
            <div key={index} className="label section-label">
              {item.section}
            </div>
          )
        const Icon = item.icon
        return (
          <button
            key={index}
            disabled={item.disabled}
            onClick={() => {
              onClose()
              item.onClick()
            }}
          >
            {Icon ? <Icon size={15} /> : <span style={{ width: 15 }} />}
            <span className="grow">{item.label}</span>
            {item.hint && <span className="faint mono">{item.hint}</span>}
          </button>
        )
      })}
    </div>
  )
}

export function Tabs<T extends string>(props: {
  tabs: { id: T; label: string; icon?: LucideIcon; count?: number | string; disabled?: boolean; title?: string }[]
  value: T
  onChange: (id: T) => void
  right?: ReactNode
}) {
  return (
    <div className="tabs">
      {props.tabs.map((tab) => {
        const Icon = tab.icon
        return (
          <button
            key={tab.id}
            className={`tab ${tab.id === props.value ? 'on' : ''}`}
            disabled={tab.disabled}
            title={tab.title}
            onClick={() => props.onChange(tab.id)}
          >
            {Icon && <Icon size={15} />}
            {tab.label}
            {tab.count !== undefined && <span className="count">{tab.count}</span>}
          </button>
        )
      })}
      {props.right && <div className="row" style={{ marginLeft: 'auto' }}>{props.right}</div>}
    </div>
  )
}

export function Segmented<T extends string>(props: {
  options: { id: T; label: string }[]
  value: T
  onChange: (id: T) => void
}) {
  return (
    <div className="segmented">
      {props.options.map((option) => (
        <button
          key={option.id}
          className={option.id === props.value ? 'on' : ''}
          onClick={() => props.onChange(option.id)}
        >
          {option.label}
        </button>
      ))}
    </div>
  )
}

export function Empty({ icon: Icon, title, hint }: { icon?: LucideIcon; title: string; hint?: ReactNode }) {
  return (
    <div className="empty">
      <div className="col" style={{ alignItems: 'center' }}>
        {Icon && <Icon size={28} />}
        <div style={{ color: 'var(--text-2)', fontWeight: 550 }}>{title}</div>
        {hint && <div>{hint}</div>}
      </div>
    </div>
  )
}

export function Loading({ text }: { text?: string }) {
  return (
    <div className="empty">
      <div className="row">
        <div className="spinner" />
        {text ?? 'Loading…'}
      </div>
    </div>
  )
}

interface Async<T> {
  data: T | undefined
  error: string | null
  loading: boolean
}

// Runs load whenever deps change and keeps the last result while the next loads.
// A load that returns null leaves the state empty.
export function useAsync<T>(load: () => Promise<T> | null, deps: unknown[]): Async<T> {
  const [state, setState] = useState<Async<T>>({ data: undefined, error: null, loading: false })
  useEffect(() => {
    let live = true
    const promise = load()
    if (!promise) {
      setState({ data: undefined, error: null, loading: false })
      return
    }
    setState((previous) => ({ ...previous, error: null, loading: true }))
    promise.then(
      (data) => live && setState({ data, error: null, loading: false }),
      (error: Error) => live && setState({ data: undefined, error: error.message, loading: false }),
    )
    return () => {
      live = false
    }
  }, deps)
  return state
}
