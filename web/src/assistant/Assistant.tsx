import { useEffect, useRef, useState } from 'react'
import { ArrowUp, Copy, KeyRound, Sparkles, Square, Trash2, X } from 'lucide-react'
import { get, post } from '../api'
import { useStore } from '../store'
import { briefing, runTool, type OverviewChart } from './tools'

interface ChatMessage {
  role: 'user' | 'assistant' | 'tool'
  content: string
  name?: string
  tool_call_id?: string
  tool_calls?: { id: string; name: string; arguments: string }[]
}

type Item =
  | { kind: 'user' | 'answer' | 'error'; text: string }
  | { kind: 'chart'; chart: OverviewChart }

interface Status {
  model: string
  endpoint_url: string
  has_token: boolean
  tools: { name: string; label: string }[]
}

interface Reply {
  reply: string
  tool_calls: ChatMessage['tool_calls']
  error?: string
}

// The same budget and closing nudge as the desktop panel.
const MAX_TOOL_ROUNDS = 20
const FINAL_NUDGE =
  'Do not call any more tools. Write your answer now, reading the numbers you already gathered and saying what they mean for this workload.'
const EXPLAIN =
  'Explain this view. Call trace_overview and get_summary so the explanation uses real kernel names and numbers, keep it to the overview pass, and offer the deeper dives as next steps.'
const OVERRIDES_KEY = 'optiq.assistant'

const escapeHtml = (text: string) => text.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c]!)

// Enough Markdown for an answer: headings, lists, emphasis, code and links.
function markdown(source: string): string {
  const inline = (text: string) =>
    escapeHtml(text)
      .replace(/`([^`]+)`/g, '<code>$1</code>')
      .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
      .replace(/(^|[^*\w])\*([^*\n]+)\*/g, '$1<em>$2</em>')
      .replace(/\[([^\]]+)\]\((https?:[^)\s]+)\)/g, '<a href="$2" target="_blank" rel="noreferrer">$1</a>')
  const out: string[] = []
  let list: 'ul' | 'ol' | null = null
  let code: string[] | null = null
  const close = () => {
    if (list) out.push(`</${list}>`)
    list = null
  }
  for (const line of source.split('\n')) {
    if (line.trim().startsWith('```')) {
      if (code) {
        out.push(`<pre><code>${escapeHtml(code.join('\n'))}</code></pre>`)
        code = null
      } else {
        close()
        code = []
      }
      continue
    }
    if (code) {
      code.push(line)
      continue
    }
    const heading = /^(#{1,4})\s+(.*)$/.exec(line)
    const bullet = /^\s*[-*]\s+(.*)$/.exec(line)
    const numbered = /^\s*\d+[.)]\s+(.*)$/.exec(line)
    if (heading) {
      close()
      out.push(`<h4>${inline(heading[2])}</h4>`)
    } else if (bullet || numbered) {
      const kind = bullet ? 'ul' : 'ol'
      if (list !== kind) {
        close()
        out.push(`<${kind}>`)
        list = kind
      }
      out.push(`<li>${inline((bullet ?? numbered)![1])}</li>`)
    } else if (line.trim()) {
      close()
      out.push(`<p>${inline(line)}</p>`)
    } else close()
  }
  close()
  return out.join('')
}

function OverviewCard({ chart }: { chart: OverviewChart }) {
  return (
    <div className="ai-card">
      <div className="section-label">Timeline overview</div>
      <div className="ai-histogram">
        {chart.bins.map((v, i) => <i key={i} style={{ height: `${Math.max(2, v)}%` }} />)}
      </div>
      {chart.tracks.map((track) => (
        <div key={track.name} className="ai-heat" title={track.name}>
          <span className="ellipsis">{track.name}</span>
          <div>{track.bins.map((v, i) => <i key={i} style={{ opacity: 0.08 + (v / 100) * 0.92 }} />)}</div>
        </div>
      ))}
      <div className="faint" style={{ fontSize: 11 }}>Busiest tracks over the whole trace.</div>
    </div>
  )
}

export function Assistant() {
  const set = useStore((s) => s.set)
  const [status, setStatus] = useState<Status | null>(null)
  const [items, setItems] = useState<Item[]>([])
  const [steps, setSteps] = useState<string[]>([])
  const [working, setWorking] = useState<string | null>(null)
  const [input, setInput] = useState('')
  const [overrides, setOverrides] = useState<{ endpoint_url?: string; model?: string; token?: string }>(() =>
    JSON.parse(localStorage.getItem(OVERRIDES_KEY) ?? '{}'),
  )
  const [setup, setSetup] = useState(false)
  const conversation = useRef<ChatMessage[]>([])
  const stopped = useRef(false)
  const transcript = useRef<HTMLDivElement>(null)

  useEffect(() => {
    get<Status>('/api/assistant').then(setStatus, () => setStatus(null))
  }, [])
  useEffect(() => {
    transcript.current?.scrollTo({ top: transcript.current.scrollHeight, behavior: 'smooth' })
  }, [items, working])

  const configured = !!status && (!!(overrides.endpoint_url || status.endpoint_url) && !!(overrides.token || status.has_token))
  const label = (name: string) => status?.tools.find((t) => t.name === name)?.label ?? `Running ${name}...`
  const add = (item: Item) => setItems((all) => [...all, item])

  const chat = (final: boolean) => post<Reply>('/api/assistant/chat', { messages: conversation.current, final, ...overrides })

  // One question: tool rounds until the model stops asking, then the write-up.
  const ask = async (question: string) => {
    if (working || !question.trim()) return
    stopped.current = false
    setSteps([])
    add({ kind: 'user', text: question === EXPLAIN ? 'Explain this view' : question })
    const first = !conversation.current.some((m) => m.role === 'user')
    conversation.current.push({ role: 'user', content: `${first ? `Briefing for the current Optiq view:\n${briefing()}\n` : ''}User question:\n${question}\n` })
    const turnStart = conversation.current.length
    let rounds = 0
    let final = false
    try {
      while (!stopped.current) {
        setWorking(final ? 'Working out what it means...' : 'Thinking...')
        const result = await chat(final)
        if (stopped.current) break
        if (result.tool_calls?.length && !final) {
          if (++rounds > MAX_TOOL_ROUNDS) {
            final = true
            conversation.current.push({ role: 'user', content: FINAL_NUDGE })
            continue
          }
          conversation.current.push({ role: 'assistant', content: result.reply ?? '', tool_calls: result.tool_calls })
          for (const call of result.tool_calls) {
            setWorking(label(call.name))
            let content: string
            try {
              const output = await runTool(call.name, JSON.parse(call.arguments || '{}'))
              content = output.content
              if (output.chart) add({ kind: 'chart', chart: output.chart })
              if (output.nextSteps) setSteps(output.nextSteps)
            } catch (error) {
              content = `Error: ${(error as Error).message}`
            }
            conversation.current.push({ role: 'tool', tool_call_id: call.id, name: call.name, content: content || '(empty)' })
          }
          continue
        }
        if (!final && rounds > 0) {
          final = true
          conversation.current.push({ role: 'user', content: FINAL_NUDGE })
          continue
        }
        const answer = result.reply || "I gathered the data but could not put an answer together. Ask me something more specific and I'll dig in again."
        // Keep the question and the answer; the tool rows have done their job.
        conversation.current = [...conversation.current.slice(0, turnStart), { role: 'assistant', content: answer }]
        add({ kind: 'answer', text: answer })
        break
      }
    } catch (error) {
      conversation.current = conversation.current.slice(0, turnStart)
      add({ kind: 'error', text: (error as Error).message })
    } finally {
      setWorking(null)
    }
  }

  const clear = () => {
    stopped.current = true
    conversation.current = []
    setItems([])
    setSteps([])
    set({ highlights: [] })
  }
  const saveOverrides = (next: typeof overrides) => {
    localStorage.setItem(OVERRIDES_KEY, JSON.stringify(next))
    setOverrides(next)
  }

  return (
    <aside className="assistant">
      <div className="assistant-head">
        <Sparkles size={17} color="var(--red)" />
        <div className="grow">
          <div style={{ fontWeight: 650 }}>Ask Optiq</div>
          <div className="faint" style={{ fontSize: 11.5 }}>{overrides.model || status?.model || 'not configured'}</div>
        </div>
        <button className="icon-btn small" title="Endpoint and key" onClick={() => setSetup(!setup)}><KeyRound size={14} /></button>
        <button
          className="icon-btn small"
          title="Copy the conversation"
          onClick={() => navigator.clipboard.writeText(items.flatMap((i) => (i.kind === 'chart' ? [] : [`${i.kind === 'user' ? 'You' : 'Optiq'}: ${i.text}`])).join('\n\n'))}
        >
          <Copy size={14} />
        </button>
        <button className="icon-btn small" title="Clear the conversation" onClick={clear}><Trash2 size={14} /></button>
        <button className="icon-btn small" title="Close" onClick={() => set({ assistant: false })}><X size={14} /></button>
      </div>
      {(setup || (status && !configured)) && (
        <div className="assistant-setup col">
          <div className="faint" style={{ fontSize: 12 }}>
            {status?.endpoint_url ? `Using the desktop app's endpoint ${status.endpoint_url}${status.has_token ? ' and its stored key' : ', but no key is stored for it'}. ` : 'No endpoint is configured in the desktop app. '}
            Anything set here applies to this browser only.
          </div>
          <input className="input small mono" placeholder={status?.endpoint_url || 'https://… OpenAI-compatible endpoint'} value={overrides.endpoint_url ?? ''} onChange={(e) => saveOverrides({ ...overrides, endpoint_url: e.target.value })} />
          <input className="input small mono" placeholder={status?.model || 'model'} value={overrides.model ?? ''} onChange={(e) => saveOverrides({ ...overrides, model: e.target.value })} />
          <input className="input small mono" type="password" placeholder={status?.has_token ? 'stored key in use' : 'API key'} value={overrides.token ?? ''} onChange={(e) => saveOverrides({ ...overrides, token: e.target.value })} />
        </div>
      )}
      <div className="assistant-body" ref={transcript}>
        {!items.length && (
          <div className="assistant-intro">
            <Sparkles size={26} color="var(--red)" />
            <div style={{ fontSize: 15, fontWeight: 650 }}>Ask about this trace</div>
            <div className="muted">I read the timeline overview first, then dig into whatever looks worst — and point the timeline at what I find.</div>
          </div>
        )}
        {items.map((item, i) =>
          item.kind === 'chart' ? (
            <OverviewCard key={i} chart={item.chart} />
          ) : item.kind === 'user' ? (
            <div key={i} className="ai-user">{item.text}</div>
          ) : item.kind === 'error' ? (
            <div key={i} className="ai-error">{item.text}</div>
          ) : (
            <div key={i} className="ai-answer" dangerouslySetInnerHTML={{ __html: markdown(item.text) }} />
          ),
        )}
        {working && (
          <div className="row faint" style={{ fontSize: 12.5 }}>
            <div className="spinner" style={{ width: 13, height: 13 }} /> {working}
          </div>
        )}
        {!working && steps.length > 0 && (
          <div className="col" style={{ gap: 6 }}>
            {steps.map((step, i) => (
              <button key={step} className="ai-step" onClick={() => ask(step)}>
                <span className="faint">{i + 1}.</span> {step}
              </button>
            ))}
          </div>
        )}
      </div>
      <div className="assistant-foot">
        {!items.length && (
          <button className="btn primary" style={{ width: '100%' }} disabled={!configured || !!working} onClick={() => ask(EXPLAIN)}>
            <Sparkles size={15} /> Explain this view
          </button>
        )}
        <div className="assistant-input">
          <textarea
            rows={2}
            placeholder="Ask about this trace…"
            value={input}
            disabled={!configured}
            onChange={(e) => setInput(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault()
                ask(input)
                setInput('')
              }
            }}
          />
          {working ? (
            <button className="icon-btn on" title="Stop" onClick={() => (stopped.current = true)}><Square size={14} /></button>
          ) : (
            <button className="icon-btn on" title="Send (Enter)" disabled={!input.trim() || !configured} onClick={() => { ask(input); setInput('') }}><ArrowUp size={16} /></button>
          )}
        </div>
      </div>
    </aside>
  )
}
