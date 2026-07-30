import type { MwEnvelope, MwResponse, RequestStatus } from './types';

/*
 * Client for the middleware, over either transport.
 *
 * The two are interchangeable at the protocol level: the same JSON document
 * goes in and comes back, so the only difference here is plumbing. HTTP opens
 * a connection per call; the WebSocket keeps one up and correlates replies by
 * the `id` the server echoes back.
 */

export type Transport = 'http' | 'websocket';

export interface ExchangeLogEntry {
  seq: number;
  at: number;
  transport: Transport;
  method: string;
  request: unknown;
  response: unknown;
  elapsedMs: number;
  ok: boolean;
}

export type LogListener = (entry: ExchangeLogEntry) => void;
export type StatusListener = (connected: boolean) => void;

export class MiddlewareError extends Error {
  code: string;

  constructor(code: string, message: string) {
    super(message);
    this.name = 'MiddlewareError';
    this.code = code;
  }
}

const POLL_INTERVAL_MS = 200;

export class MiddlewareClient {
  private baseUrl: string;
  private transport: Transport;
  private socket: WebSocket | null = null;
  private nextId = 1;
  private logSeq = 0;
  private pending = new Map<
    number,
    { resolve: (value: MwResponse) => void; reject: (reason: Error) => void }
  >();
  private logListeners = new Set<LogListener>();
  private statusListeners = new Set<StatusListener>();

  constructor(baseUrl: string, transport: Transport = 'http') {
    this.baseUrl = baseUrl.replace(/\/$/, '');
    this.transport = transport;
  }

  getTransport(): Transport {
    return this.transport;
  }

  onLog(listener: LogListener): () => void {
    this.logListeners.add(listener);
    return () => this.logListeners.delete(listener);
  }

  onStatus(listener: StatusListener): () => void {
    this.statusListeners.add(listener);
    return () => this.statusListeners.delete(listener);
  }

  private emitLog(entry: Omit<ExchangeLogEntry, 'seq'>): void {
    const full = { ...entry, seq: ++this.logSeq };
    this.logListeners.forEach((listener) => listener(full));
  }

  private emitStatus(connected: boolean): void {
    this.statusListeners.forEach((listener) => listener(connected));
  }

  /* Open the WebSocket. HTTP needs no setup, so this is a no-op for it. */
  async connect(): Promise<void> {
    if (this.transport === 'http') {
      await this.health();
      this.emitStatus(true);
      return;
    }
    await this.openSocket();
  }

  private openSocket(): Promise<void> {
    return new Promise((resolve, reject) => {
      const url = this.baseUrl.replace(/^http/, 'ws') + '/ws';
      const socket = new WebSocket(url);

      const failed = () => {
        this.socket = null;
        this.emitStatus(false);
        reject(new Error(`could not open a WebSocket to ${url}`));
      };

      socket.onopen = () => {
        this.socket = socket;
        this.emitStatus(true);
        resolve();
      };
      socket.onerror = failed;
      socket.onclose = () => {
        this.socket = null;
        this.emitStatus(false);
        /* Fail anything still outstanding rather than leaving it hanging. */
        this.pending.forEach(({ reject: rejectPending }) =>
          rejectPending(new Error('the connection closed')),
        );
        this.pending.clear();
      };
      socket.onmessage = (event) => this.handleSocketMessage(event.data);
    });
  }

  private handleSocketMessage(data: unknown): void {
    if (typeof data !== 'string') return;
    let parsed: MwResponse;
    try {
      parsed = JSON.parse(data) as MwResponse;
    } catch {
      return;
    }
    const id = typeof parsed.id === 'number' ? parsed.id : -1;
    const waiter = this.pending.get(id);
    if (waiter) {
      this.pending.delete(id);
      waiter.resolve(parsed);
    }
  }

  disconnect(): void {
    if (this.socket) {
      this.socket.close();
      this.socket = null;
    }
    this.emitStatus(false);
  }

  isConnected(): boolean {
    if (this.transport === 'http') return true;
    return this.socket?.readyState === WebSocket.OPEN;
  }

  async health(): Promise<boolean> {
    const response = await fetch(`${this.baseUrl}/health`);
    if (!response.ok) throw new Error(`health check returned ${response.status}`);
    return true;
  }

  /* One request, one response, with the exchange recorded for the console. */
  async request<T = unknown>(
    method: string,
    params: Record<string, unknown> = {},
  ): Promise<MwResponse<T>> {
    const id = this.nextId++;
    const body = { id, method, params };
    const started = performance.now();

    let response: MwResponse<T>;
    if (this.transport === 'websocket') {
      response = (await this.sendOverSocket(id, body)) as MwResponse<T>;
    } else {
      response = (await this.sendOverHttp(body)) as MwResponse<T>;
    }

    this.emitLog({
      at: Date.now(),
      transport: this.transport,
      method,
      request: body,
      response,
      elapsedMs: performance.now() - started,
      ok: response.ok,
    });
    return response;
  }

  private async sendOverHttp(body: unknown): Promise<MwResponse> {
    const response = await fetch(`${this.baseUrl}/rpc`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    return (await response.json()) as MwResponse;
  }

  private sendOverSocket(id: number, body: unknown): Promise<MwResponse> {
    return new Promise((resolve, reject) => {
      if (!this.socket || this.socket.readyState !== WebSocket.OPEN) {
        reject(new Error('the WebSocket is not open'));
        return;
      }
      this.pending.set(id, { resolve, reject });
      this.socket.send(JSON.stringify(body));
    });
  }

  /* Unwrap the response, turning a protocol error into a thrown one. */
  async call<T = unknown>(
    method: string,
    params: Record<string, unknown> = {},
  ): Promise<T> {
    const response = await this.request<T>(method, params);
    if (!response.ok || response.result === undefined) {
      const error = response.error;
      throw new MiddlewareError(
        error?.code ?? 'unknown_error',
        error?.message ?? 'the request failed without an error body',
      );
    }
    return response.result;
  }

  /*
   * Fetching methods answer with an envelope rather than the payload. Poll
   * until it reaches a terminal state, reporting progress along the way.
   *
   * A terminal result is delivered exactly once and the request is then
   * retired, so this must not poll again after reading one.
   */
  async fetchAsync<T = unknown>(
    method: string,
    params: Record<string, unknown> = {},
    onProgress?: (progress: number, message: string) => void,
  ): Promise<T> {
    const first = await this.call<MwEnvelope<T>>(method, params);
    let envelope = first;

    while (envelope.status === 'pending') {
      onProgress?.(envelope.progress ?? 0, envelope.message ?? '');
      await delay(POLL_INTERVAL_MS);
      envelope = await this.call<MwEnvelope<T>>('request.poll', {
        request_id: envelope.request_id,
        wait_ms: 500,
      });
    }

    return terminalResult(method, envelope);
  }
}

function terminalResult<T>(method: string, envelope: MwEnvelope<T>): T {
  const status: RequestStatus = envelope.status;
  if (status === 'ready') {
    if (envelope.result === undefined) {
      throw new MiddlewareError('empty_result', `${method} finished with no result`);
    }
    return envelope.result;
  }
  if (status === 'cancelled') {
    throw new MiddlewareError('cancelled', `${method} was cancelled`);
  }
  throw new MiddlewareError(
    envelope.error_code ?? 'fetch_failed',
    envelope.message || `${method} failed`,
  );
}

function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
