import { fileIo as fs } from '@kit.CoreFileKit';
import { http } from '@kit.NetworkKit';
import { util, url } from '@kit.ArkTS';
import { cryptoFramework } from '@kit.CryptoArchitectureKit';
import nativeRender from 'libsplat.so';
import { viewerRequests } from './ViewerRequests';
interface CollisionBox { min: number[]; max: number[]; }
interface CollisionTile { id: string; ix: number; iz: number; coreBounds: CollisionBox; url: string; }
interface CollisionManifest { version: number; voxelResolution: number; tiles: CollisionTile[]; }
interface CollisionDiskEntry { path: string; size: number; touched: number; }
function boxValid(box: CollisionBox): boolean {
  return !!box && Array.isArray(box.min) && Array.isArray(box.max) && box.min.length === 3 && box.max.length === 3 &&
    box.min.every((n: number, i: number) => Number.isFinite(n) && Number.isFinite(box.max[i]) && Math.abs(n) <= 1e6 && Math.abs(box.max[i]) <= 1e6 && n < box.max[i]);
}
export function validateCollisionManifest(m: CollisionManifest): void {
  if (!m || m.version !== 1 || !Number.isFinite(m.voxelResolution) || m.voxelResolution < .005 || m.voxelResolution > 10 || !Array.isArray(m.tiles) || !m.tiles.length || m.tiles.length > 4096) { throw new Error('无效的分块碰撞清单'); }
  const ids: Set<string> = new Set();
  for (const tile of m.tiles) {
    if (!tile || typeof tile.id !== 'string' || !/^[A-Za-z0-9_-]{1,80}$/.test(tile.id) || ids.has(tile.id) || !Number.isInteger(tile.ix) || !Number.isInteger(tile.iz) || Math.abs(tile.ix) > 10000 || Math.abs(tile.iz) > 10000 || !boxValid(tile.coreBounds) || typeof tile.url !== 'string' || !tile.url.endsWith('.voxel.json')) { throw new Error('无效或重复的碰撞分块'); }
    ids.add(tile.id);
  }
}
export function collisionWorldBox(box: CollisionBox, space: string): CollisionBox {
  if (!boxValid(box)) { throw new Error('无效的碰撞边界'); }
  return space === 'metaflow-rz180' ? { min: [-box.max[0], -box.max[1], box.min[2]], max: [-box.min[0], -box.min[1], box.max[2]] } : { min: box.min.slice(), max: box.max.slice() };
}
export class ViewerCollision {
  readonly generation: number = nativeRender.collisionClear();
  private tiles: CollisionTile[] = [];
  private base: string = '';
  private coordinateSpace: string = 'auto';
  private directory: string = '';
  private stopped: boolean = false;
  private suspended: boolean = false;
  private workRevision: number = 0;
  private writing: Promise<void> = Promise.resolve();
  private cacheDir: string = '';
  private reopening: boolean = false;
  private requests: Set<http.HttpRequest> = new Set();
  private disk: CollisionDiskEntry[] = [];
  private wanted: string[] = [];
  private inflight: Set<string> = new Set();
  private failed: Set<string> = new Set();
  private paths: Map<string, string> = new Map();
  private previousEye: number[] | undefined = undefined;
  private eye: number[] = [0, 0, 0];
  private nextTick: number = 0;
  private report: (message: string) => void;
  constructor(report: (message: string) => void) { this.report = report; }
  private address(relative: string): string {
    const parsed = new url.URL(relative, this.base);
    if (!['http:', 'https:'].includes(parsed.protocol) || parsed.username || parsed.password || parsed.hash) { throw new Error('碰撞资源需要 HTTP(S) 地址'); }
    return parsed.toString();
  }
  private async path(address: string): Promise<string> {
    const digest = cryptoFramework.createMd('SHA256');
    await digest.update({ data: new util.TextEncoder().encodeInto(address) });
    const bytes = (await digest.digest()).data;
    const path = `${this.directory}/${Array.from(bytes).map((v: number) => v.toString(16).padStart(2, '0')).join('')}`; this.paths.set(address, path); return path;
  }
  private protected(path: string): boolean {
    if (this.paths.get(this.base) === path) { return true; }
    return this.tiles.some((tile: CollisionTile) => {
      if (!this.wanted.includes(tile.id) && !this.inflight.has(tile.id)) { return false; }
      const address = this.address(tile.url);
      return this.paths.get(address) === path || this.paths.get(address.replace(/\.voxel\.json(?=\?|$)/, '.voxel.bin')) === path;
    });
  }
  private current(revision: number): boolean { return !this.stopped && !this.suspended && revision === this.workRevision; }
  private ensure(revision: number): void { if (!this.current(revision)) { throw new Error('碰撞加载已取消或暂停'); } }
  private async discard(path: string): Promise<void> {
    if (fs.accessSync(path)) { await fs.unlink(path); }
    this.disk = this.disk.filter((entry: CollisionDiskEntry) => entry.path !== path);
  }
  private async save(path: string, data: ArrayBuffer, revision: number): Promise<void> {
    // Serialize eviction and atomic publication: two completed downloads must not
    // both reserve the same remaining bytes or delete the same LRU file.
    const previous = this.writing;
    let unlock: () => void = () => {};
    this.writing = new Promise<void>((resolve) => { unlock = resolve; });
    const temporary = `${path}.${this.generation}.${revision}.part`;
    try {
      await previous; this.ensure(revision);
      let total = this.disk.reduce((sum: number, entry: CollisionDiskEntry) => sum + entry.size, 0);
      for (const old of this.disk.slice().sort((a: CollisionDiskEntry, b: CollisionDiskEntry) => a.touched - b.touched)) {
        if (total + data.byteLength <= 256 * 1024 * 1024) { break; }
        if (!this.protected(old.path)) { await this.discard(old.path); total -= old.size; this.ensure(revision); }
      }
      if (total + data.byteLength > 256 * 1024 * 1024) { throw new Error('碰撞文件缓存已占满，请停止后重试'); }
      const file = await fs.open(temporary, fs.OpenMode.CREATE | fs.OpenMode.WRITE_ONLY | fs.OpenMode.TRUNC);
      try { let offset = 0; while (offset < data.byteLength) { this.ensure(revision); const count = await fs.write(file.fd, data.slice(offset)); if (count <= 0) { throw new Error('碰撞文件写入不完整'); } offset += count; } } finally { await fs.close(file); }
      this.ensure(revision); await fs.rename(temporary, path);
      this.disk = this.disk.filter((entry: CollisionDiskEntry) => entry.path !== path);
      this.disk.push({ path, size: data.byteLength, touched: Date.now() });
    } finally {
      try { if (fs.accessSync(temporary)) { await fs.unlink(temporary); } } finally { unlock(); }
    }
  }
  private async cached(address: string, limit: number, revision: number = this.workRevision): Promise<string> {
    const path = await this.path(address);
    this.ensure(revision);
    if (fs.accessSync(path)) { const size = (await fs.stat(path)).size; this.ensure(revision); if (size > 0 && size <= limit) { const entry = this.disk.find((v: CollisionDiskEntry) => v.path === path); if (entry) { entry.touched = Date.now(); } return path; } await this.discard(path); }
    await viewerRequests.acquire();
    if (!this.current(revision)) { viewerRequests.release(); throw new Error('碰撞加载已暂停'); }
    const request = http.createHttp(); this.requests.add(request);
    try {
      const response = await request.request(address, { expectDataType: http.HttpDataType.ARRAY_BUFFER, usingCache: false, maxLimit: limit, connectTimeout: 10000, readTimeout: 30000 });
      this.ensure(revision);
      if (response.responseCode !== 200 || !(response.result instanceof ArrayBuffer) || response.result.byteLength > limit || response.result.byteLength === 0) { throw new Error('碰撞下载失败或超过大小限制'); }
      const data = response.result as ArrayBuffer;
      await this.save(path, data, revision); this.ensure(revision);
      return path;
    } finally { request.destroy(); this.requests.delete(request); viewerRequests.release(); }
  }
  async open(address: string, cacheDir: string, eye: number[], coordinateSpace: string = 'auto'): Promise<void> {
    if (!['auto', 'world', 'metaflow-rz180'].includes(coordinateSpace)) { throw new Error('无效的体素坐标'); }
    this.coordinateSpace = coordinateSpace;
    const revision = this.workRevision; this.cacheDir = cacheDir; this.eye = eye.slice();
    this.base = address; this.base = this.address(address); this.directory = `${cacheDir}/viewer-collision-v1`; this.disk = [];
    if (!fs.accessSync(this.directory)) { await fs.mkdir(this.directory); }
    for (const name of await fs.listFile(this.directory)) {
      if (/^[a-f0-9]{64}$/.test(name)) { const path = `${this.directory}/${name}`, stat = await fs.stat(path); this.disk.push({ path, size: stat.size, touched: stat.mtime * 1000 }); }
    }
    const mesh = new url.URL(this.base).pathname.toLowerCase().endsWith('.glb');
    const metadata = mesh ? '' : await this.cached(this.base, 1048576, revision);
    this.ensure(revision);
    if (mesh || new url.URL(this.base).pathname.endsWith('.voxel.json')) {
      this.tiles = [{ id: 'single', ix: 0, iz: 0, url: this.base, coreBounds: { min: [-1e6, -1e6, -1e6], max: [1e6, 1e6, 1e6] } }];
    } else {
      try {
        const text = await fs.readText(metadata); this.ensure(revision); const manifest = JSON.parse(text) as CollisionManifest;
        validateCollisionManifest(manifest); this.tiles = manifest.tiles.map((tile: CollisionTile): CollisionTile => {
          return { id: tile.id, ix: coordinateSpace === 'metaflow-rz180' ? -tile.ix : tile.ix, iz: tile.iz,
            url: tile.url, coreBounds: collisionWorldBox(tile.coreBounds, coordinateSpace) };
        });
      } catch (error) { if (this.current(revision)) { await this.discard(metadata); } throw new Error(String(error)); }
      // Only the explicit player coordinateSpace applies a transform; free-form
      // manifest rotation strings remain provenance and are never applied twice.
    }
    this.report(`碰撞清单：${this.tiles.length} 块 · 正在准备附近区域`); this.tick(eye, true);
  }
  tick(eye: number[], force: boolean = false): void {
    if (this.stopped || this.suspended || !this.tiles.length || (!force && Date.now() < this.nextTick)) { return; }
    this.nextTick = Date.now() + 200; this.eye = eye.slice();
    const distance = (tile: CollisionTile): number => Math.pow(Math.max(tile.coreBounds.min[0] - eye[0], 0, eye[0] - tile.coreBounds.max[0]), 2) + Math.pow(Math.max(tile.coreBounds.min[2] - eye[2], 0, eye[2] - tile.coreBounds.max[2]), 2);
    const ordered = this.tiles.slice().sort((a: CollisionTile, b: CollisionTile) => distance(a) - distance(b));
    const center = ordered[0];
    this.wanted = ordered.filter((t: CollisionTile) => Math.abs(t.ix - center.ix) <= 1 && Math.abs(t.iz - center.iz) <= 1).map((t: CollisionTile) => t.id);
    if (this.previousEye) {
      const dx = eye[0] - this.previousEye[0], dz = eye[2] - this.previousEye[2], length = Math.hypot(dx, dz);
      if (length > .005) {
        const ahead = ordered.find((t: CollisionTile) => !this.wanted.includes(t.id) && Math.abs(t.ix - center.ix) <= 2 && Math.abs(t.iz - center.iz) <= 2 &&
          ((t.coreBounds.min[0] + t.coreBounds.max[0]) / 2 - eye[0]) * dx + ((t.coreBounds.min[2] + t.coreBounds.max[2]) / 2 - eye[2]) * dz > 0);
        if (ahead) { this.wanted.push(ahead.id); }
      }
    }
    nativeRender.collisionSelect(this.generation, this.wanted);
    const resident = nativeRender.collisionStatus().ids;
    const pending = this.wanted.filter((id: string) => !resident.includes(id) && !this.inflight.has(id) && !this.failed.has(id));
    for (const id of pending.slice(0, Math.max(0, 2 - this.inflight.size))) {
      const tile = this.tiles.find((t: CollisionTile) => t.id === id)!; this.inflight.add(id); this.load(tile);
    }
    this.previousEye = eye.slice();
  }
  private async load(tile: CollisionTile): Promise<void> {
    const revision = this.workRevision;
    let metadata = '', bin = '', nativeStarted = false;
    try {
      const address = this.address(tile.url), binary = address.replace(/\.voxel\.json(?=\?|$)/, '.voxel.bin');
      const mesh = new url.URL(address).pathname.toLowerCase().endsWith('.glb');
      metadata = await this.cached(address, mesh ? 64 * 1024 * 1024 : 1048576, revision); bin = mesh ? '' : await this.cached(binary, 64 * 1024 * 1024, revision);
      this.ensure(revision); nativeStarted = true;
      if (mesh) { await nativeRender.collisionLoadMesh(this.generation, tile.id, metadata); }
      else { await nativeRender.collisionLoadVoxel(this.generation, tile.id, metadata, bin, this.coordinateSpace === 'auto' ? -1 : this.coordinateSpace === 'metaflow-rz180' ? 1 : 0); }
      if (this.current(revision)) { const s = nativeRender.collisionStatus(); this.report(`碰撞已缓存 ${s.ids.length} / ${this.tiles.length} 块 · ${(s.bytes / 1048576).toFixed(1)} MiB`); }
    } catch (error) {
      if (this.current(revision)) {
        const message = String(error); this.failed.add(tile.id);
        // A parse failure must not poison every retry with the same cached bytes.
        if (nativeStarted && !/cache|pinned|active|Stale/.test(message)) {
          try { if (metadata) { await this.discard(metadata); } if (bin) { await this.discard(bin); } } catch (_cleanup) {}
        }
        if (this.current(revision)) { this.report(`碰撞 ${tile.id}：${message}`); }
      }
    }
    finally { this.inflight.delete(tile.id); if (!this.stopped && !this.suspended) { this.tick(this.eye, true); } }
  }
  async enter(eye: number[]): Promise<number[]> { return nativeRender.collisionEnter(this.generation, eye[0], eye[1], eye[2]); }
  step(dt: number, yaw: number, right: number, forward: number, jump: boolean): number[] { return nativeRender.collisionStep(this.generation, dt, yaw, right, forward, jump); }
  retry(): void {
    if (this.stopped || this.suspended) { return; }
    this.failed.clear();
    if (!this.tiles.length && this.base && this.cacheDir && !this.reopening) {
      this.reopening = true;
      this.open(this.base, this.cacheDir, this.eye, this.coordinateSpace).catch((error: Error) => { if (!this.stopped && !this.suspended) { this.report(String(error)); } }).finally(() => { this.reopening = false; });
    } else { this.tick(this.eye, true); }
  }
  pause(value: boolean): void { this.suspended = value; nativeRender.collisionPause(); if (value) { this.workRevision++; for (const request of this.requests) { request.destroy(); } } else { this.retry(); } }
  stop(): void { this.stopped = true; this.workRevision++; for (const request of this.requests) { request.destroy(); } nativeRender.collisionClear(); }
}
