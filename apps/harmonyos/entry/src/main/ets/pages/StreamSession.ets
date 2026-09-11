import { BusinessError } from '@kit.BasicServicesKit';
import { http } from '@kit.NetworkKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import nativeRender from 'libsplat.so';
export interface StreamChunk { file: string; count: number; bytes: number; bounds: number[]; }
export interface StreamLeaf { bounds: number[]; lods: number[][]; }
export interface StreamManifest { version: number; bounds: number[]; chunks: StreamChunk[]; leaves?: StreamLeaf[]; levels?: number; }
function validBounds(b: number[]): boolean {
  return Array.isArray(b) && b.length === 4 && b.every((v: number) => Number.isFinite(v) && Math.abs(v) <= 1e6) && b[3] > 0;
}
export function validateManifest(m: StreamManifest): void {
  if (!m || (m.version !== 1 && m.version !== 2) || !validBounds(m.bounds) || !Array.isArray(m.chunks) || !m.chunks.length || m.chunks.length > 1024) { throw new Error('无效的流式清单'); }
  const names: string[] = [];
  for (const c of m.chunks) {
    if (!c || typeof c.file !== 'string' || !/^chunk-[0-9]{4}\.(ply|sog)$/.test(c.file) || names.includes(c.file) ||
      !Number.isInteger(c.count) || c.count < 1 || c.count > 4000000 || !Number.isInteger(c.bytes) || c.bytes < 1 || c.bytes > 33554432 || !validBounds(c.bounds)) { throw new Error('分块字段或资源预算无效'); }
    names.push(c.file);
  }
  if (m.version === 1 && m.chunks.reduce((n: number, c: StreamChunk) => n + c.count, 0) > 4000000) { throw new Error('平面清单超过 400 万预算，请使用分层清单'); }
  if (m.version === 2) {
    if (!m.leaves || !Array.isArray(m.leaves) || !m.leaves.length || m.leaves.length > 10000 || !m.levels || !Number.isInteger(m.levels) || m.levels < 1 || m.levels > 16) { throw new Error('无效的 LOD 层级'); }
    let coarse = 0;
    for (const leaf of m.leaves) {
      if (!validBounds(leaf.bounds) || !Array.isArray(leaf.lods) || leaf.lods.length !== m.levels) { throw new Error('无效的 LOD 节点'); }
      for (const r of leaf.lods) {
        if (!Array.isArray(r) || r.length !== 3 || !r.every((v: number) => Number.isInteger(v)) || r[0] < -1 || r[0] >= m.chunks.length || r[1] < 0 || r[2] < 0 ||
          (r[0] >= 0 && (r[2] === 0 || r[1] + r[2] > m.chunks[r[0]].count)) || (r[0] === -1 && (r[1] !== 0 || r[2] !== 0))) { throw new Error('LOD 分块范围无效'); }
      }
      const last = leaf.lods[m.levels - 1]; coarse += last[2];
    }
    if (coarse > 4000000) { throw new Error('基础覆盖超过 400 万高斯预算'); }
  }
}
// Each leaf always has one representation. A missing refinement retains its nearest cached coarse LOD.
export function selectLods(m: StreamManifest, camera: number[], fly: boolean, budget: number): number[] {
  if (!m.leaves || !m.levels) { return []; }
  const levels = m.leaves.map(() => (m.levels as number) - 1);
  let count = m.leaves.reduce((n: number, l: StreamLeaf) => n + l.lods[(m.levels as number) - 1][2], 0);
  const back = [Math.sin(camera[0]) * Math.cos(camera[1]), Math.sin(camera[1]), Math.cos(camera[0]) * Math.cos(camera[1])];
  const eye = [0, 1, 2].map((k: number) => m.bounds[k] + (camera[k + 3] + (fly ? 0 : back[k] * 3 * camera[2])) * m.bounds[3]);
  const importance = m.leaves.map((l: StreamLeaf) => {
    const v = [0, 1, 2].map((k: number) => l.bounds[k] - eye[k]);
    const facing = -(v[0] * back[0] + v[1] * back[1] + v[2] * back[2]);
    const distance = Math.max(m.bounds[3] * .001, Math.hypot(v[0], v[1], v[2]) - l.bounds[3]);
    return l.bounds[3] * l.bounds[3] / (distance * distance) * (facing > -l.bounds[3] ? 1 : .01);
  });
  // Re-evaluate density at each level so large nearby leaves receive detail first.
  for (let pass = 0; pass < m.levels; pass++) {
    const order = m.leaves.map((_l: StreamLeaf, i: number) => i).sort((a: number, b: number) =>
      importance[b] / Math.max(1, (m.leaves as StreamLeaf[])[b].lods[levels[b]][2]) - importance[a] / Math.max(1, (m.leaves as StreamLeaf[])[a].lods[levels[a]][2]));
    let changed = false;
    for (const i of order) {
      const lod = levels[i]; if (lod === 0) { continue; }
      const leaf = m.leaves[i]; let nextLevel = lod - 1;
      while (nextLevel > 0 && leaf.lods[nextLevel][0] < 0) { nextLevel--; }
      const next = leaf.lods[nextLevel]; if (next[0] < 0) { continue; }
      const delta = next[2] - leaf.lods[lod][2];
      if (count + delta <= budget) { count += delta; levels[i] = nextLevel; changed = true; }
    }
    if (!changed) { break; }
  }
  return levels;
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
  private fullLoad: boolean = false;
  private selectionKey: string = '';
  private selection: number[] = [];
  private loadedBytes: number = 0;
  active: boolean = true;
  budget: number = 2000000;
  private progress: (message: string) => void;
  constructor(progress: (message: string) => void) { this.progress = progress; }
  private async fetch(url: string, limit: number): Promise<ArrayBuffer> {
    const request = http.createHttp(); this.request = request;
    try {
      const result = await request.request(url, { expectDataType: http.HttpDataType.ARRAY_BUFFER,
        usingCache: false, maxLimit: limit, connectTimeout: 10000, readTimeout: 30000 });
      if (result.responseCode !== 200 || !(result.result instanceof ArrayBuffer) || result.result.byteLength > limit) { throw new Error(`HTTP ${result.responseCode} 或数据超过预算`); }
      return result.result as ArrayBuffer;
    } finally { request.destroy(); if (this.request === request) { this.request = undefined; } }
  }
  async open(url: string, cacheDir: string, fullLoad: boolean = false): Promise<void> {
    if (!/^https?:\/\/[A-Za-z0-9.\-:\[\]]+(?:\/[A-Za-z0-9._-]+)*\/scene\.json$/.test(url)) { throw new Error('请输入 HTTP(S) scene.json 地址，不含账号、查询参数或片段'); }
    this.base = url.slice(0, url.lastIndexOf('/') + 1); this.fullLoad = fullLoad;
    const previous = await fs.listFile(cacheDir);
    for (const name of previous) { if (/^stream-[0-9]+$/.test(name)) { try { await fs.rmdir(`${cacheDir}/${name}`); } catch (_) {} } }
    this.directory = `${cacheDir}/stream-${Date.now()}`; await fs.mkdir(this.directory);
    const bytes = await this.fetch(url, 4194304); if (this.stopped) { return; }
    const data = new Uint8Array(bytes); let text = '';
    for (let i = 0; i < data.length; i++) { if (data[i] > 127) { throw new Error('清单应为 ASCII JSON'); } text += String.fromCharCode(data[i]); }
    const m = JSON.parse(text) as StreamManifest; validateManifest(m);
    if (fullLoad && m.chunks.reduce((n: number, c: StreamChunk) => n + c.bytes, 0) > 4 * 1073741824) { throw new Error('全量预载超过 4 GiB 本地缓存预算'); }
    m.bounds[0] *= -1; m.bounds[1] *= -1;
    for (const c of m.chunks) { c.bounds[0] *= -1; c.bounds[1] *= -1; }
    if (m.leaves) { for (const leaf of m.leaves) { leaf.bounds[0] *= -1; leaf.bounds[1] *= -1; } }
    if (m.leaves && m.levels && m.leaves.reduce((n: number, l: StreamLeaf) => n + l.lods[(m.levels as number) - 1][2], 0) > this.budget) { throw new Error('基础层超过当前预算，请提高画质后重连'); }
    this.manifest = m; this.progress(`清单就绪 · ${m.chunks.length} 块 · ${m.levels ?? 1} 级 LOD`);
  }
  retry(): void { this.published = ''; this.active = true; }
  stop(): void { this.stopped = true; this.request?.destroy(); }
  async dispose(): Promise<void> { this.stop(); if (this.directory) { try { await fs.rmdir(this.directory); } catch (_) {} } }
  tick(camera: number[], fly: boolean): void {
    if (this.stopped || !this.active || this.busy || !this.manifest) { return; }
    this.pump(camera.slice(), fly);
  }
  private async pump(camera: number[], fly: boolean): Promise<void> {
    const m = this.manifest; if (!m) { return; } this.busy = true;
    try {
      const preloading = this.fullLoad && this.cache.size < m.chunks.length;
      const key = camera.map((v: number) => v.toFixed(2)).join(',') + `${fly}:${this.budget}`;
      if (!preloading && key !== this.selectionKey) { this.selection = selectLods(m, camera, fly, this.budget); this.selectionKey = key; }
      const levels = this.selection;
      const coarseFiles: number[] = []; const desiredFiles: number[] = [];
      if (m.leaves && m.levels) {
        for (let i = 0; i < m.leaves.length; i++) { coarseFiles.push(m.leaves[i].lods[m.levels - 1][0]); if (levels.length) { desiredFiles.push(m.leaves[i].lods[levels[i]][0]); } }
      } else { for (let i = 0; i < m.chunks.length; i++) { coarseFiles.push(i); } }
      const order = Array.from(new Set(coarseFiles.concat(desiredFiles).concat(this.fullLoad ? m.chunks.map((_c: StreamChunk, i: number) => i) : [])).values()).filter((i: number) => i >= 0);
      const missing = order.find((i: number) => !this.cache.has(m.chunks[i].file));
      if (missing !== undefined) {
        const c = m.chunks[missing]; this.progress(`${this.fullLoad ? '全量预载' : '渐进加载'} ${c.file} · ${this.cache.size}/${m.chunks.length} 块`);
        const bytes = await this.fetch(this.base + c.file, 33554432); if (this.stopped) { return; }
        if (bytes.byteLength !== c.bytes) { throw new Error('分块长度与清单不符'); }
        const path = `${this.directory}/${c.file}`; const f = await fs.open(path, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
        try { let offset = 0; while (offset < bytes.byteLength) { const n = await fs.write(f.fd, bytes.slice(offset)); if (n <= 0) { throw new Error('缓存写入失败'); } offset += n; } } finally { await fs.close(f); }
        if (this.stopped) { return; } this.cache.set(c.file, path); this.loadedBytes += bytes.byteLength;
      }
      if (preloading) { this.progress(`全量预载 · 缓存 ${this.cache.size}/${m.chunks.length} · ${(this.loadedBytes / 1048576).toFixed(0)} MiB · 完成后显示场景`); return; }
      const ready = coarseFiles.filter((i: number) => i >= 0).every((i: number) => this.cache.has(m.chunks[i].file));
      const paths: string[] = []; const ranges: number[] = []; let count = 0; let refined = 0;
      if (ready && (!this.fullLoad || this.cache.size === m.chunks.length)) {
        if (m.leaves && m.levels) {
          for (let i = 0; i < m.leaves.length; i++) {
            const l = m.leaves[i]; let level = levels[i];
            while (level < m.levels - 1 && (l.lods[level][0] < 0 || !this.cache.has(m.chunks[l.lods[level][0]].file))) { level++; }
            const r = l.lods[level]; if (r[0] < 0) { continue; } const path = this.cache.get(m.chunks[r[0]].file) as string;
            let index = paths.indexOf(path); if (index < 0) { index = paths.length; paths.push(path); }
            ranges.push(index, r[1], r[2]); count += r[2]; if (level < m.levels - 1) { refined++; }
          }
        } else { for (const c of m.chunks) { paths.push(this.cache.get(c.file) as string); count += c.count; } }
        const key = paths.join('|') + ':' + ranges.join(',');
        if (key !== this.published && nativeRender.status().state !== 'loading') { nativeRender.chunks(paths, m.bounds, ranges); this.published = key; }
      }
      if (!this.fullLoad) {
        const pinned = new Set(order.map((i: number) => m.chunks[i].file));
        for (const file of Array.from(this.cache.keys())) {
          if (this.loadedBytes <= 536870912) { break; }
          const path = this.cache.get(file) as string;
          if (!pinned.has(file) && !paths.includes(path)) { await fs.unlink(path); this.cache.delete(file); this.loadedBytes -= (m.chunks.find((c: StreamChunk) => c.file === file) as StreamChunk).bytes; }
        }
      }
      this.progress(`${this.fullLoad ? '全量预载' : 'LOD 流式'} · 缓存 ${this.cache.size}/${m.chunks.length} · ${ready ? '完整基础覆盖' : '等待基础覆盖'} · ${count.toLocaleString()} 个高斯 · 细化 ${refined} 区`);
    } catch (error) { if (!this.stopped) { const failure = error as BusinessError; this.progress(`流式暂停：${failure.code ?? ''} ${failure.message ?? String(error)}；点击重试`); this.active = false; } }
    finally { this.busy = false; if (this.stopped && this.directory) { try { await fs.rmdir(this.directory); } catch (_) {} } }
  }
}
