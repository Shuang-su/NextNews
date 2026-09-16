import { parseLodWorker } from './ViewerLodWorker';
import { util, taskpool } from '@kit.ArkTS';
import { componentPath, sogComponents, SogMetadata } from './ViewerLodManifest';
import { viewerRequests } from './ViewerRequests';
import { cryptoFramework } from '@kit.CryptoArchitectureKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { http } from '@kit.NetworkKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import nativeRender from 'libsplat.so';
interface DiskComponent { name: string; bytes: number; }
interface DiskReceipt { source: string; files: DiskComponent[]; }
export interface StreamChunk { source?: string; sha256?: string; file: string; count: number; bytes: number; bounds: number[]; }
export interface StreamLeaf { bounds: number[]; aabb?: number[]; lods: number[][]; }
export interface StreamManifest { environment?: number; version: number; bounds: number[]; chunks: StreamChunk[]; leaves?: StreamLeaf[]; levels?: number; }
function validBounds(b: number[]): boolean {
  return Array.isArray(b) && b.length === 4 && b.every((v: number) => Number.isFinite(v) && Math.abs(v) <= 1e6) && b[3] > 0;
}
export function validateManifest(m: StreamManifest): void {
  if (!m || (m.version !== 1 && m.version !== 2) || !validBounds(m.bounds) || !Array.isArray(m.chunks) || !m.chunks.length || m.chunks.length > 1024) { throw new Error('无效的流式清单'); }
  const names: string[] = [];
  for (const c of m.chunks) {
    if (!c || (c.sha256 !== undefined && !/^[a-f0-9]{64}$/.test(c.sha256)) || typeof c.file !== 'string' || !/^chunk-[0-9]{4}\.(ply|sog)$/.test(c.file) || names.includes(c.file) ||
      !Number.isInteger(c.count) || c.count < 1 || c.count > 4000000 || !Number.isInteger(c.bytes) || (c.source ? (!componentPath(c.source) || c.bytes < 0 || c.bytes > 134217728) : (c.bytes < 1 || c.bytes > 33554432)) || !validBounds(c.bounds)) { throw new Error('分块字段或资源预算无效'); }
    names.push(c.file);
  }
  if (m.version === 1 && m.chunks.reduce((n: number, c: StreamChunk) => n + c.count, 0) > 4000000) { throw new Error('平面清单超过 400 万预算，请使用分层清单'); }
  if (m.version === 2) {
    if (!m.leaves || !Array.isArray(m.leaves) || !m.leaves.length || m.leaves.length > 32768 || !m.levels || !Number.isInteger(m.levels) || m.levels < 1 || m.levels > 16) { throw new Error('无效的 LOD 层级'); }
    let coarse = 0;
    for (const leaf of m.leaves) {
      if ((leaf.aabb !== undefined && (!Array.isArray(leaf.aabb) || leaf.aabb.length !== 6 || !leaf.aabb.every((v: number) => Number.isFinite(v) && Math.abs(v) <= 1e6) || [0, 1, 2].some((k: number) => (leaf.aabb as number[])[k] > (leaf.aabb as number[])[k + 3]))) || !validBounds(leaf.bounds) || !Array.isArray(leaf.lods) || leaf.lods.length !== m.levels) { throw new Error('无效的 LOD 节点'); }
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
export function selectLods(m: StreamManifest, camera: number[], fly: boolean, budget: number, fov: number = 45, aspect: number = 1): number[] {
  if (!m.leaves || !m.levels) { return []; }
  const levels = m.leaves.map(() => (m.levels as number) - 1);
  let count = m.leaves.reduce((n: number, l: StreamLeaf) => n + l.lods[(m.levels as number) - 1][2], 0);
  const back = [Math.sin(camera[0]) * Math.cos(camera[1]), Math.sin(camera[1]), Math.cos(camera[0]) * Math.cos(camera[1])];
  const eye = [0, 1, 2].map((k: number) => m.bounds[k] + (camera[k + 3] + (fly ? 0 : back[k] * 3 * camera[2])) * m.bounds[3]);
  const ty = Math.tan(Math.max(1, Math.min(178, fov)) * Math.PI / 360);
  const fovScale = Math.min(ty, ty * Math.max(.1, aspect)) / Math.tan(Math.PI / 6);
  // Same distance inputs as PlayCanvas: closest AABB point, min-axis FOV,
  // continuous 1..5 rear penalty. Offscreen leaves keep a meaningful LOD.
  const distance = m.leaves.map((l: StreamLeaf) => {
    const a = l.aabb ?? [l.bounds[0] - l.bounds[3], l.bounds[1] - l.bounds[3], l.bounds[2] - l.bounds[3],
      l.bounds[0] + l.bounds[3], l.bounds[1] + l.bounds[3], l.bounds[2] + l.bounds[3]];
    const v = [0, 1, 2].map((k: number) => Math.max(a[k], Math.min(a[k + 3], eye[k])) - eye[k]);
    const length = Math.hypot(v[0], v[1], v[2]);
    const behind = length > .01 ? Math.max(0, (v[0] * back[0] + v[1] * back[1] + v[2] * back[2]) / length) : 0;
    return Math.max(.0001, length * (1 + behind * 4) * fovScale);
  });
  const nearest = m.leaves.map((_l: StreamLeaf, i: number) => i).sort((a: number, b: number) => distance[a] - distance[b]);
  // Match default placement thresholds, then enforce a fixed user budget.
  count = 0;
  for (let i = 0; i < levels.length; i++) {
    levels[i] = Math.min(m.levels - 1, distance[i] < 5 ? 0 : Math.floor(1 + Math.log(distance[i] / 5) / Math.log(3)));
    count += m.leaves[i].lods[levels[i]][2];
  }
  for (let pass = 0; count > budget && pass < m.levels; pass++) {
    for (let k = nearest.length - 1; k >= 0 && count > budget; k--) {
      const i = nearest[k], level = levels[i]; if (level >= m.levels - 1) { continue; }
      count += m.leaves[i].lods[level + 1][2] - m.leaves[i].lods[level][2]; levels[i]++;
    }
  }
  for (let pass = 0; pass < m.levels; pass++) {
    let changed = false;
    for (const i of nearest) {
      const level = levels[i]; if (level === 0) { continue; }
      const next = m.leaves[i].lods[level - 1];
      const delta = next[2] - m.leaves[i].lods[level][2];
      if (next[0] >= 0 && count + delta <= budget) { count += delta; levels[i]--; changed = true; }
    }
    if (!changed) { break; }
  }
  return levels;
}
export class StreamSession {
  manifest: StreamManifest | undefined = undefined;
  private requests: Set<http.HttpRequest> = new Set();
  private directory: string = '';
  private persistent: boolean = false;
  private async digest(bytes: ArrayBuffer): Promise<string> {
    const md = cryptoFramework.createMd('SHA256'); await md.update({ data: new Uint8Array(bytes) });
    const result = await md.digest(); return Array.from(result.data).map((v: number) => v.toString(16).padStart(2, '0')).join('');
  }
  private base: string = '';
  private cache: Map<string, string> = new Map();
  private stopped: boolean = false;
  private busy: boolean = false;
  private published: string = '';
  private fullLoad: boolean = false;
  private selectionKey: string = '';
  private selection: number[] = [];
  private loadedBytes: number = 0;
  private reservedBytes: number = 0;
  networkBytes: number = 0;
  prefetchEnabled: boolean = true;
  private previousFiles: Set<string> = new Set();
  private targetFiles: Set<string> = new Set();
  active: boolean = true;
  budget: number = 2000000;
  paged: boolean = false;
  encoded: boolean = false;
  private revision: number = 0;
  targetRevision: number = 0;
  private submittedTarget: number = -1;
  targetAt: number = 0;
  coverage: number = 0;
  requestedCount: number = 0;
  get selectionSubmitted(): boolean { return !this.publishDirty; }
  get submittedRevision(): number { return this.revision; }
  private publishDirty: boolean = true;
  private progress: (message: string) => void;
  constructor(progress: (message: string) => void) { this.progress = progress; }
  private async fetch(url: string, limit: number): Promise<ArrayBuffer> {
    await viewerRequests.acquire();
    if (this.stopped) { viewerRequests.release(); throw new Error('流式加载已取消'); }
    const request = http.createHttp(); this.requests.add(request);
    try {
      const result = await request.request(url, { expectDataType: http.HttpDataType.ARRAY_BUFFER,
        usingCache: false, maxLimit: limit, connectTimeout: 10000, readTimeout: 30000 });
      if (result.responseCode !== 200 || !(result.result instanceof ArrayBuffer) || result.result.byteLength > limit) { throw new Error(`HTTP ${result.responseCode} 或数据超过预算`); }
      this.networkBytes += result.result.byteLength;
      return result.result as ArrayBuffer;
    } finally { request.destroy(); this.requests.delete(request); viewerRequests.release(); }
  }
  async open(url: string, cacheDir: string, fullLoad: boolean = false): Promise<void> {
    url = url.trim();
    if (!/^https?:\/\/[A-Za-z0-9.\-:\[\]]+(?:\/[A-Za-z0-9._-]+)*\/[A-Za-z0-9._-]+\.json$/.test(url)) { throw new Error('请输入 HTTP(S) 流式 JSON 地址，不含账号、查询参数或片段'); }
    this.base = url.slice(0, url.lastIndexOf('/') + 1); this.fullLoad = fullLoad;
    const previous = await fs.listFile(cacheDir);
    for (const name of previous) { if (/^stream-[0-9]+$/.test(name)) { try { await fs.rmdir(`${cacheDir}/${name}`); } catch (_) {} } }

    const bytes = await this.fetch(url, 33554432); if (this.stopped) { return; }
    const text = new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(new Uint8Array(bytes));
    const m = await taskpool.execute(parseLodWorker, text) as StreamManifest;
    if (this.stopped) { return; }
    if (m.environment !== undefined && (!Number.isInteger(m.environment) || !Array.isArray(m.chunks) || m.environment < 0 || m.environment >= m.chunks.length || !m.chunks[m.environment].source || !Array.isArray(m.leaves) || !Number.isInteger(m.levels) || (m.levels as number) < 1 || (m.levels as number) > 16)) { throw new Error('环境分块无效'); }
    if (m.environment !== undefined) {
      const c = m.chunks[m.environment];
      const data = await this.fetch(this.base + c.source, 1048576);
      const meta = JSON.parse(new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(new Uint8Array(data))) as SogMetadata;
      sogComponents(meta, 1); c.count = meta.count;
      m.leaves!.push({ bounds: m.bounds.slice(), lods: new Array<number[]>(m.levels as number).fill([m.environment, 0, c.count]) });
    }
    if (this.stopped) { return; }
    validateManifest(m);
    this.persistent = m.chunks.every((c: StreamChunk) => c.sha256 !== undefined || c.source !== undefined);
    this.directory = this.persistent ? `${cacheDir}/stream-v4-${await this.digest(new util.TextEncoder().encodeInto(url + text).buffer as ArrayBuffer)}` : `${cacheDir}/stream-${Date.now()}`;
    try { await fs.mkdir(this.directory); } catch (_) { if (!await fs.access(this.directory)) { throw new Error('无法创建流式缓存'); } }
    if (this.persistent) {
      for (const c of m.chunks) {
        if (c.source) {
          try {
            const path = `${this.directory}/${c.file}.parts`;
            const receipt = JSON.parse(await fs.readText(`${path}/ready.json`)) as DiskReceipt;
            if (receipt.source !== this.base + c.source || !Array.isArray(receipt.files) || receipt.files.length !== 6) { continue; }
            let total = 0; const seen = new Set<string>();
            for (const f of receipt.files) {
              if (!f || typeof f.name !== 'string' || !/^(meta\.json|[A-Za-z0-9_-]+\.webp)$/.test(f.name) || seen.has(f.name) || !Number.isInteger(f.bytes) || f.bytes <= 0 || f.bytes > 33554432 || (await fs.stat(`${path}/${f.name}`)).size !== f.bytes) { throw new Error('缓存分块不完整'); }
              seen.add(f.name); total += f.bytes;
            }
            if (!seen.has('meta.json') || total > 134217728) { continue; }
            this.cache.set(c.file, `${path}/meta.json`); c.bytes = total; this.loadedBytes += total;
          } catch (_) {}
          continue;
        }
        const path = `${this.directory}/${c.file}`;
        try { if ((await fs.stat(path)).size === c.bytes) { this.cache.set(c.file, path); this.loadedBytes += c.bytes; } } catch (_) {}
      }
      console.info(`StreamCache diskHits=${this.cache.size} diskBytes=${this.loadedBytes}`);
    }
    if (fullLoad && m.chunks.reduce((n: number, c: StreamChunk) => n + c.bytes, 0) > 4 * 1073741824) { throw new Error('全量预载超过 4 GiB 本地缓存预算'); }
    m.bounds[0] *= -1; m.bounds[1] *= -1;
    for (const c of m.chunks) { c.bounds[0] *= -1; c.bounds[1] *= -1; }
    if (m.leaves) { for (const leaf of m.leaves) { leaf.bounds[0] *= -1; leaf.bounds[1] *= -1;
      if (leaf.aabb) { const a = leaf.aabb; leaf.aabb = [-a[3], -a[4], a[2], -a[0], -a[1], a[5]]; } } }
    if (m.leaves && m.levels && m.leaves.reduce((n: number, l: StreamLeaf) => n + l.lods[(m.levels as number) - 1][2], 0) > this.budget) { throw new Error('基础层超过当前预算，请提高画质后重连'); }
    if (this.stopped) { return; }
    if (m.leaves && m.levels) {
      const boxes: number[] = []; const ranges: number[] = [];
      for (const leaf of m.leaves) {
        const b = leaf.bounds;
        boxes.push(...(leaf.aabb ?? [b[0] - b[3], b[1] - b[3], b[2] - b[3], b[0] + b[3], b[1] + b[3], b[2] + b[3]]));
        for (const range of leaf.lods) { ranges.push(...range); }
      }
      nativeRender.registerLods(new Float64Array(m.bounds).buffer, new Float64Array(boxes).buffer, new Int32Array(ranges).buffer, m.levels);
    }
    this.manifest = m; this.progress(`清单就绪 · ${m.chunks.length} 块 · ${m.levels ?? 1} 级 LOD`);
  }
  private async unbundled(c: StreamChunk): Promise<string> {
    const directory = `${this.directory}/${c.file}.parts`;
    try { await fs.rmdir(directory); } catch (_) {}
    await fs.mkdir(directory);
    const address = this.base + c.source;
    const bytes = await this.fetch(address, 1048576);
    const meta = JSON.parse(new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(new Uint8Array(bytes))) as SogMetadata;
    const names = sogComponents(meta, c.count); let total = bytes.byteLength;
    const files: DiskComponent[] = [{ name: 'meta.json', bytes: bytes.byteLength }];
    const base = address.slice(0, address.lastIndexOf('/') + 1);
    for (const name of names) {
      const data = await this.fetch(base + name, 33554432); total += data.byteLength;
      if (total > 134217728 || this.stopped) { throw new Error('SOG 分块超过预算或已取消'); }
      await this.writeFile(`${directory}/${name}`, data); files.push({ name, bytes: data.byteLength });
    }
    // Metadata is the commit marker: never expose partially downloaded textures.
    await this.writeFile(`${directory}/meta.json`, bytes);
    if (this.loadedBytes + this.reservedBytes + total > 4 * 1073741824) { throw new Error('流式缓存达到 4 GiB，请关闭全量预载'); }
    const receipt: DiskReceipt = { source: address, files };
    this.reservedBytes += total;
    try { await this.writeFile(`${directory}/ready.json`, new util.TextEncoder().encodeInto(JSON.stringify(receipt)).buffer as ArrayBuffer); c.bytes = total; this.loadedBytes += total; }
    finally { this.reservedBytes -= total; }
    console.info(`StreamFile file=${c.file} readyAt=${Date.now()} bytes=${total} source=unbundled`);
    return `${directory}/meta.json`;
  }
  private async writeFile(path: string, bytes: ArrayBuffer): Promise<void> {
    const f = await fs.open(`${path}.part`, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
    try { let offset = 0; while (offset < bytes.byteLength) { const n = await fs.write(f.fd, bytes.slice(offset)); if (n <= 0) { throw new Error('缓存写入失败'); } offset += n; } }
    finally { await fs.close(f); }
    if (this.stopped) { throw new Error('流式加载已取消'); }
    await fs.rename(`${path}.part`, path);
  }
  retry(): void { this.publishDirty = true; this.published = ''; this.active = true; }
  stop(): void { this.stopped = true; for (const request of this.requests) { request.destroy(); } this.requests.clear(); }
  async dispose(): Promise<void> { this.stop(); if (this.directory && !this.persistent) { try { await fs.rmdir(this.directory); } catch (_) {} } }
  tick(camera: number[], fly: boolean, fov: number = 45, aspect: number = 1): void {
    if (this.stopped || !this.active || !this.manifest) { return; }
    try {
      this.updateSelection(camera, fly, fov, aspect);
      this.publishCached();
      if (!this.busy) { this.pump(camera.slice(), fly, fov, aspect); }
    } catch (error) { this.active = false; this.progress(`流式提交失败：${String(error)}`); console.error(`StreamSubmit ${String(error)}`); }
  }
  private updateSelection(camera: number[], fly: boolean, fov: number, aspect: number): void {
    const m = this.manifest; if (!m) { return; }
    const key = camera.map((v: number) => v.toFixed(2)).join(',') + `${fly}:${this.budget}:${fov}:${aspect}`;
    if (key !== this.selectionKey) { this.targetAt = Date.now(); this.selection = this.paged && m.leaves ? Array.from(new Uint32Array(nativeRender.selectLods(new Float64Array([...camera, fly ? 1 : 0]).buffer, this.budget, fov, aspect))) : selectLods(m, camera, fly, this.budget, fov, aspect); this.selectionKey = key; this.targetRevision++; console.info(`StreamTarget target=${this.targetRevision} cameraAt=${this.targetAt} selectedAt=${Date.now()} budget=${this.budget} fov=${fov} aspect=${aspect}`); this.publishDirty = true; }
    if (m.leaves && this.publishDirty) {
      const files = new Set<string>();
      for (let i = 0; i < m.leaves.length; i++) { const file = m.leaves[i].lods[this.selection[i]][0]; if (file >= 0) { files.add(m.chunks[file].file); } }
      if (files.size !== this.targetFiles.size || Array.from(files).some((file: string) => !this.targetFiles.has(file))) {
        this.previousFiles = this.targetFiles; this.targetFiles = files;
      }
    }
  }
  // Called on camera ticks even while downloads are pending. Cached detail must
  // not wait for unrelated network requests or use an old camera selection.
  private publishCached(): string[] {
    const m = this.manifest; if (!m || this.stopped) { return []; }
    if (!this.publishDirty) { return []; }
    const coarseFiles = m.leaves && m.levels ? m.leaves.map((l: StreamLeaf) => l.lods[(m.levels as number) - 1][0]) : m.chunks.map((_c: StreamChunk, i: number) => i);
    const ready = coarseFiles.filter((i: number) => i >= 0).every((i: number) => this.cache.has(m.chunks[i].file));
    const levels = this.selection;
    const paths: string[] = []; const ranges: number[] = []; let count = 0; let refined = 0; let matched = 0; let desiredCount = 0;
    if (ready) {
      if (m.leaves && m.levels) {
        for (let i = 0; i < m.leaves.length; i++) {
          const l = m.leaves[i]; let level = levels[i]; desiredCount += l.lods[level][2];
          while (level < m.levels - 1 && l.lods[level][0] >= 0 && !this.cache.has(m.chunks[l.lods[level][0]].file)) { level++; }
          if (level === levels[i]) { matched++; }
          const r = l.lods[level]; if (r[0] < 0) { continue; } const path = this.cache.get(m.chunks[r[0]].file) as string;
          let index = paths.indexOf(path); if (index < 0) { index = paths.length; paths.push(path); }
          ranges.push(index, r[1], r[2]); count += r[2]; if (level < m.levels - 1) { refined++; }
        }
      } else { for (const c of m.chunks) { paths.push(this.cache.get(c.file) as string); count += c.count; } }
      const key = paths.join('|') + ':' + ranges.join(',');
      this.coverage = m.leaves ? matched / m.leaves.length : 1; this.requestedCount = desiredCount || count;
      if (key !== this.published && (nativeRender.status().state !== 'loading' || (this.paged && this.submittedTarget !== this.targetRevision))) {
        if (this.paged) { nativeRender.selectPages(paths, m.bounds, new Uint32Array(ranges).buffer, ++this.revision, this.encoded);
          console.info(`StreamSelection revision=${this.revision} target=${this.targetRevision} cameraAt=${this.targetAt} submittedAt=${Date.now()} coverage=${this.coverage.toFixed(6)} desiredCount=${this.requestedCount} count=${count}`);
        } else { nativeRender.chunks(paths, m.bounds, ranges); }
        this.published = key; this.submittedTarget = this.targetRevision; this.publishDirty = false;
      } else if (key === this.published) { this.publishDirty = false; }
    }
    this.progress(`${this.fullLoad ? '全量预载' : 'LOD 流式'} · 缓存 ${this.cache.size}/${m.chunks.length} · ${ready ? '完整基础覆盖' : '等待基础覆盖'} · ${count.toLocaleString()} 个高斯 · 细化 ${refined} 区`);
    return paths;
  }
  private async pump(camera: number[], fly: boolean, fov: number, aspect: number): Promise<void> {
    const m = this.manifest; if (!m) { return; } this.busy = true;
    try {
      this.updateSelection(camera, fly, fov, aspect);
      const levels = this.selection;
      const coarseFiles: number[] = []; const desiredFiles: number[] = []; const prefetchFiles: number[] = [];
      if (m.leaves && m.levels) {
        for (let i = 0; i < m.leaves.length; i++) { coarseFiles.push(m.leaves[i].lods[m.levels - 1][0]); if (levels.length) { desiredFiles.push(m.leaves[i].lods[levels[i]][0]); if (levels[i] > 0) { prefetchFiles.push(m.leaves[i].lods[levels[i] - 1][0]); } } }
      } else { for (let i = 0; i < m.chunks.length; i++) { coarseFiles.push(i); } }
      const order = Array.from(new Set(coarseFiles.concat(desiredFiles).concat(this.prefetchEnabled ? prefetchFiles : []).concat(this.fullLoad ? m.chunks.map((_c: StreamChunk, i: number) => i) : [])).values()).filter((i: number) => i >= 0);
      const required = new Set(coarseFiles.concat(desiredFiles));
      let headroom = Math.max(0, 536870912 - this.loadedBytes);
      const missing: number[] = [];
      for (const i of order) {
        if (this.cache.has(m.chunks[i].file)) { continue; }
        if (!this.fullLoad && !required.has(i)) { if (m.chunks[i].bytes > headroom) { continue; } headroom -= m.chunks[i].bytes; }
        missing.push(i); if (missing.length === 4) { break; }
      }
      // A bounded batch overlaps network latency without decoding on the UI thread.
      // Wait for every request to settle before cleanup, even when one fails.
      let failure: string = '';
      await Promise.all(missing.map(async (index: number): Promise<void> => {
        const c = m.chunks[index];
        try {
        if (c.source) {
          const path = await this.unbundled(c); if (this.stopped) { return; }
          this.cache.set(c.file, path); this.publishDirty = true;
        } else {
        const bytes = await this.fetch(this.base + c.file, 33554432); if (this.stopped) { return; }
        if (bytes.byteLength !== c.bytes) { throw new Error('分块长度与清单不符'); }
        if (c.sha256 && await this.digest(bytes) !== c.sha256) { throw new Error('分块 SHA-256 校验失败'); }
        const path = `${this.directory}/${c.file}`; const temp = `${path}.part`; const f = await fs.open(temp, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
        try { let offset = 0; while (offset < bytes.byteLength) { const n = await fs.write(f.fd, bytes.slice(offset)); if (n <= 0) { throw new Error('缓存写入失败'); } offset += n; } } finally { await fs.close(f); }
        await fs.rename(temp, path);
        if (this.stopped) { return; } console.info(`StreamFile file=${c.file} readyAt=${Date.now()} bytes=${bytes.byteLength}`); this.cache.set(c.file, path); this.publishDirty = true; this.loadedBytes += bytes.byteLength;
        }
        } catch (error) { failure = String(error); }
      }));
      if (failure) { throw new Error(failure); }
      if (this.stopped) { return; }
      const paths = this.publishCached();
      if (!this.fullLoad) {
        // Evict prefetch before the current/previous target. Alternating warm
        // views must not redownload a file whose GPU pages are still resident.
        const pinned = new Set(coarseFiles.concat(desiredFiles).filter((i: number) => i >= 0).map((i: number) => m.chunks[i].file));
        this.targetFiles.forEach((file: string) => { pinned.add(file); });
        this.previousFiles.forEach((file: string) => { pinned.add(file); });
        for (const file of Array.from(this.cache.keys())) {
          if (this.loadedBytes <= 536870912) { break; }
          const path = this.cache.get(file) as string;
          if (!pinned.has(file) && !paths.includes(path)) { if (path.endsWith('/meta.json')) { await fs.rmdir(path.slice(0, -10)); } else { await fs.unlink(path); } this.cache.delete(file); this.loadedBytes -= (m.chunks.find((c: StreamChunk) => c.file === file) as StreamChunk).bytes; }
        }
      }
    } catch (error) { if (!this.stopped) { const failure = error as BusinessError; this.progress(`流式暂停：${failure.code ?? ''} ${failure.message ?? String(error)}；点击重试`); this.active = false; } }
    finally { this.busy = false; if (this.stopped && this.directory && !this.persistent) { try { await fs.rmdir(this.directory); } catch (_) {} } }
  }
}
