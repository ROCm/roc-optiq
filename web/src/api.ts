// The roc-optiq-server API. 64-bit ids arrive as decimal strings and stay strings.

import { useStore } from './store'

export interface Status {
  state: 'init' | 'loading' | 'ready' | 'error'
  trace_type: 'system' | 'compute'
  progress_message: string
  trace_file_path: string
  pending_requests: number
  version: string
}

export interface Track {
  id: string
  name: string
  data_type: 'events' | 'samples'
  kind: 'unknown' | 'queue' | 'stream' | 'instrumented_thread' | 'sampled_thread' | 'counter'
  category: string
  main_name: string
  sub_name: string
  start_ns: number
  end_ns: number
  entry_count: number
  min_value: number
  max_value: number
  operations: string[]
  agent_or_pid: string
  queue_id_or_tid: string
  topology: { node_id: string; process_id: string; device_id: string; id: string }
}

export interface Timeline {
  start_ns: number
  end_ns: number
  tracks: Track[]
}

export interface TraceEvent {
  uuid: string
  op: string
  name: string
  start_ns: number
  duration_ns: number
  level: number
  child_count: number
  top_combined_name: string
}

export interface Sample {
  start_ns: number
  end_ns: number
  value: number
}

export interface TrackData {
  track_id: string
  start_ns: number
  end_ns: number
  events?: TraceEvent[]
  samples?: Sample[]
}

export interface Flow {
  uuid: string
  name: string
  track_id: string
  start_ns: number
  end_ns: number
  level: number
}

export interface EventDetails {
  uuid: string
  op: string
  track_id: string
  name: string
  start_ns: number
  duration_ns: number
  level: number
  args: { position: number; name: string; value: string; data_type: string }[]
  ext: { category: string; name: string; value: string }[]
  flows: Flow[]
  call_stack: { uuid: string; name: string; file: string; pc: string; address: string }[]
}

export interface Column {
  name: string
  type?: 'uint64' | 'double' | 'string' | 'object'
}

export interface Table {
  columns: Column[]
  rows: string[][]
  total_row_count: number
  error?: string
}

export interface TopologyNode {
  type: 'node' | 'processor' | 'process' | 'thread' | 'stream' | 'queue' | 'counter'
  id: string
  name: string
  header: string
  track_id: string | null
  host_name?: string
  os_name?: string
  os_release?: string
  product_name?: string
  processor_type?: string
  label?: string | null
  pid?: string
  command?: string
  units?: string
  description?: string
  processors?: TopologyNode[]
  processes?: TopologyNode[]
  queues?: TopologyNode[]
  counters?: TopologyNode[]
  threads?: TopologyNode[]
  streams?: TopologyNode[]
}

export interface Topology {
  gpu_count: number
  cpu_count: number
  nodes: TopologyNode[]
}

export interface Minimap {
  start_ns: number
  end_ns: number
  tracks: Record<string, number[]>
}

export interface TopEvent {
  name: string
  invocations: number
  duration_sum_ns: number
  duration_min_ns: number
  duration_max_ns: number
  duration_pct: number
  duration_avg_ns: number
}

export interface TopEvents {
  invocations: number
  duration_sum_ns: number
  top: TopEvent[]
}

export interface SummaryCounter {
  device: string
  id: string
  name: string
  units: string
  min: number
  max: number
  mean: number
  std_dev: number
}

export interface Aggregate {
  level: 'trace' | 'node' | 'processor' | 'process' | 'thread'
  id: string | null
  top_events: Record<'kernel' | 'memory' | 'thread', TopEvents | null>
  counters: SummaryCounter[]
  children: Aggregate[]
}

export interface Statistics {
  state: string
  stats: { name: string; value: number | null; units: string }[]
}

export interface FolderListing {
  path: string
  parent: string
  roots: string[]
  places: { name: string; path: string }[]
  entries: { name: string; path: string; is_folder: boolean; size: number }[]
}

export interface MetricEntry {
  metric_id: string
  id: number
  name: string
  description: string
  unit: string
}

export interface MetricTable {
  id: number
  name: string
  value_names: string[]
  entries: MetricEntry[]
}

export interface MetricCategory {
  id: number
  name: string
  tables: MetricTable[]
}

export interface Kernel {
  id: number
  name: string
  has_isa_lines: boolean
  dispatch: Record<
    | 'invocation_count'
    | 'duration_total_ns'
    | 'duration_min_ns'
    | 'duration_max_ns'
    | 'duration_mean_ns'
    | 'duration_median_ns',
    number
  >
  roofline: { x: number | null; y: number | null; type: string }[]
}

export interface Ceiling {
  bandwidth: string
  compute: string
  from: { x: number | null; y: number | null }
  to: { x: number | null; y: number | null }
  throughput: number | null
}

export interface MetricRef {
  valid: boolean
  metric_id: string | null
  metric_name: string
}

export interface MemoryBlock {
  id: string
  column: number
  order: number
  title: string
  content: { metric: MetricRef; title: string; category: string; unit: string }[]
  children: MemoryBlock[]
}

export interface MemoryChart {
  blocks: MemoryBlock[]
  arrows: { from: string; to: string; direction: string; metric: MetricRef; title: string }[]
}

export interface Workload {
  id: number
  name: string
  system_info: string[][]
  profiling_config: string[][]
  metrics: MetricCategory[]
  memory_chart: MemoryChart
  kernels: Kernel[]
  roofline: { bandwidth_ceilings: Ceiling[]; compute_ceilings: Ceiling[] }
}

export interface Compute {
  profiler: { version: string; git_version: string; schema_version: string }
  workloads: Workload[]
}

export interface MetricValue {
  metric_id: string
  name: string
  unit: string
  values: Record<string, number | null>
}

export interface Metrics {
  workload: { metrics: MetricValue[] } | null
  kernels: ({ kernel_id: number; metrics: MetricValue[] } | null)[]
}

export interface KernelTable {
  columns: Column[]
  rows: string[][]
  error?: string
}

export interface PcSampling {
  sample_states_loaded: boolean
  code_objects: {
    kernel_symbols: { instructions: { uuid: string; offset: number; text: string }[] }[]
  }[]
  source_files: {
    uuid: string
    path: string
    lines: { uuid: string; line_number: number; content: string }[]
  }[]
  instruction_source_lines: { instruction_uuid: string; source_line_uuid: string }[]
  sample_states: {
    uuid: string
    instruction_uuid: string
    total_count: number
    issue_count: number | null
    stall_count: number | null
  }[]
  stall_reasons: { sample_state_uuid: string; lookup_uuid: string; count: number }[]
  stall_reason_lookups: { uuid: string; text: string }[]
}

type Query = Record<string, string | number | boolean | null | undefined>

// Builds against a remote server when VITE_OPTIQ_SERVER is set at build time.
const BASE: string = import.meta.env.VITE_OPTIQ_SERVER ?? ''

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  let response: Response
  try {
    response = await fetch(BASE + path, init)
  } catch {
    useStore.getState().set({ offline: true })
    throw new Error('The Optiq server is not reachable.')
  }
  const body = await response.json().catch(() => ({}))
  if (!response.ok) throw new Error(body.error ?? response.statusText)
  return body as T
}

export function get<T>(path: string, query: Query = {}): Promise<T> {
  const params = new URLSearchParams()
  for (const [key, value] of Object.entries(query)) {
    if (value !== undefined && value !== null && value !== '') params.set(key, String(value))
  }
  const text = params.toString()
  return request<T>(text ? `${path}?${text}` : path)
}

export function post<T>(path: string, body: object): Promise<T> {
  return request<T>(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  })
}
