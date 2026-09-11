import { http } from '@kit.NetworkKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import nativeRender from 'libsplat.so';
export interface StreamChunk { file: string; count: number; bytes: number; bounds: number[]; }
export interface StreamManifest { version: number; bounds: number[]; chunks: StreamChunk[]; }
export function validateManifest(m: StreamManifest): void {
  if (!m || m.version !== 1 || !Array.isArray(m.bounds) || m.bounds.length !== 4 ||
    !m.bounds.every((v: number) => Number.isFinite(v) && Math.abs(v) <= 1e6) || m.bounds[3] <= 0 ||
    !Array.isArray(m.chunks) || m.chunks.length === 0 || m.chunks.length > 1024) { throw new Error('无效的流式清单'); }
  const names: string[] = [];
  for (const c of m.chunks) {
    if (!c || typeof c.file !== 'string' || !/^chunk-[0-9]{4}\.ply$/.test(c.file) || names.includes(c.file) ||
      !Number.isInteger(c.count) || c.count < 1 || c.count > 32768 || !Number.isInteger(c.bytes) || c.bytes < 1 || c.bytes > 2097152 ||
      !Array.isArray(c.bounds) || c.bounds.length !== 4 || !c.bounds.every((v: number) => Number.isFinite(v) && Math.abs(v) <= 1e6) || c.bounds[3] <= 0) {
      throw new Error('分块字段或资源预算无效');
    }
    names.push(c.file);
  }
}
export class StreamSession {
  manifest: StreamManifest | undefined = undefined;
  private request: http.HttpRequest | undefined = undefined;
  private directory: string = '';
  private base: string = '';
  private cache: Map<string, string> = new Map();
  private stopped: boolean = false;
  private busy: boolean = false;
  private published: string = '';
  active: boolean = true;
  private progress: (message: string) => void;
  constructor(progress: (message: string) => void) { this.progress = progress; }
  private async fetch(url: string, limit: number): Promise<ArrayBuffer> {
    const request = http.createHttp(); this.request = request;
    try {
      const result = await request.request(url, { expectDataType: http.HttpDataType.ARRAY_BUFFER,
        usingCache: false, maxLimit: limit, connectTimeout: 10000, readTimeout: 15000 });
      if (result.responseCode !== 200 || !(result.result instanceof ArrayBuffer) || result.result.byteLength > limit) {
        throw new Error(`HTTP ${result.responseCode} 或数据超过预算`);
      }
      return result.result as ArrayBuffer;
    } finally { request.destroy(); if (this.request === request) { this.request = undefined; } }
  }
  async open(url: string, cacheDir: string): Promise<void> {
    if (!/^https?:\/\/[A-Za-z0-9.\-:\[\]]+(?:\/[A-Za-z0-9._-]+)*\/scene\.json$/.test(url)) {
      throw new Error('请输入 HTTP(S) scene.json 地址，不含账号、查询参数或片段');
    }
    this.base = url.slice(0, url.lastIndexOf('/') + 1);
    const previous = await fs.listFile(cacheDir);
    for (const name of previous) {
      if (/^stream-[0-9]+$/.test(name)) { try { await fs.rmdir(`${cacheDir}/${name}`); } catch (_) {} }
    }
    this.directory = `${cacheDir}/stream-${Date.now()}`;
    await fs.mkdir(this.directory);
    const bytes = await this.fetch(url, 1048576);
    if (this.stopped) { return; }
    // Manifest is ASCII JSON emitted by the project converter.
    const data = new Uint8Array(bytes); let text = '';
    for (let i = 0; i < data.length; i++) { if (data[i] > 127) { throw new Error('清单应为 ASCII JSON'); } text += String.fromCharCode(data[i]); }
    const manifest = JSON.parse(text) as StreamManifest; validateManifest(manifest); this.manifest = manifest;
    this.progress(`流式清单就绪 · ${manifest.chunks.length} 块 · 常驻最多 8 块`);
  }
  stop(): void { this.stopped = true; this.request?.destroy(); }
  async dispose(): Promise<void> {
    this.stop();
    if (this.directory) { try { await fs.rmdir(this.directory); } catch (_) {} }
  }
  tick(camera: number[], fly: boolean): void {
    if (this.stopped || !this.active || this.busy || !this.manifest) { return; }
    this.pump(camera.slice(), fly);
  }
  private async pump(camera: number[], fly: boolean): Promise<void> {
    const m = this.manifest; if (!m) { return; }
    this.busy = true;
    try {
      const back = [Math.sin(camera[0]) * Math.cos(camera[1]), Math.sin(camera[1]), Math.cos(camera[0]) * Math.cos(camera[1])];
      const eye = [0, 1, 2].map((k: number) => m.bounds[k] + (camera[k + 3] + (fly ? 0 : back[k] * 3 * camera[2])) * m.bounds[3]);
      const score = (chunk: StreamChunk): number => {
        const dx = chunk.bounds[0] - eye[0]; const dy = chunk.bounds[1] - eye[1]; const dz = chunk.bounds[2] - eye[2];
        const distance = Math.max(0, Math.hypot(dx, dy, dz) - chunk.bounds[3]);
        return distance * (dx * back[0] + dy * back[1] + dz * back[2] < 0 ? 1 : 4);
      };
      const selected = m.chunks.slice().sort((a: StreamChunk, b: StreamChunk) => score(a) - score(b)).slice(0, 8);
      const missing = selected.find((c: StreamChunk) => !this.cache.has(c.file));
      if (missing) {
        this.progress(`下载 ${missing.file} · 缓存 ${this.cache.size} 块 / 最多 16 块`);
        const bytes = await this.fetch(this.base + missing.file, 2097152);
        if (this.stopped) { return; }
        if (bytes.byteLength !== missing.bytes) { throw new Error('分块长度与清单不符'); }
        const path = `${this.directory}/${missing.file}`;
        const f = await fs.open(path, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
        try {
          let offset = 0;
          while (offset < bytes.byteLength) {
            const n = await fs.write(f.fd, bytes.slice(offset)); if (n <= 0) { throw new Error('分块缓存写入失败'); } offset += n;
          }
        } finally { await fs.close(f); }
        if (this.stopped) { return; }
        this.cache.set(missing.file, path);
      }
      const paths: string[] = [];
      for (const c of selected) { const path = this.cache.get(c.file); if (path) { paths.push(path); } }
      const key = paths.slice().sort().join('|');
      if (paths.length && key !== this.published) { nativeRender.chunks(paths, m.bounds); this.published = key; }
      const candidates = Array.from(this.cache.keys());
      for (const file of candidates) {
        if (this.cache.size <= 16) { break; }
        if (!selected.some((c: StreamChunk) => c.file === file)) {
          const path = this.cache.get(file); this.cache.delete(file); if (path) { try { await fs.unlink(path); } catch (_) {} }
        }
      }
      this.progress(`流式 · 当前 ${paths.length}/${selected.length} 块 · 缓存 ${this.cache.size}/16 · 视角优先调度`);
    } catch (error) {
      if (!this.stopped) { this.progress(`流式暂停：${String(error)}；点击重试`); this.active = false; }
    } finally {
      this.busy = false;
      if (this.stopped && this.directory) { try { await fs.rmdir(this.directory); } catch (_) {} }
    }
  }
}
