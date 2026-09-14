import { common } from '@kit.AbilityKit';
import { http } from '@kit.NetworkKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import { Scene, Camera } from '@kit.ArkGraphics3D';
import { spatialRender } from '@kit.SpatialReconKit';

// The engine owns LOD selection. This adapter only supplies the requested SOG files.
export class SpatialStream {
  private node: spatialRender.TiledGSNode | undefined = undefined;
  private root: string = '';
  private base: string = '';
  private stopped: boolean = false;
  private requests: Set<http.HttpRequest> = new Set();
  private queue: spatialRender.GSTile[] = [];
  private queued: Set<string> = new Set();
  private ready: Set<string> = new Set();
  private running: number = 0;
  private bytes: number = 0;
  private watchdog: number = -1;
  private requested: number = 0;
  private progress: (message: string) => void;
  constructor(progress: (message: string) => void) { this.progress = progress; }
  private async fetch(url: string, limit: number): Promise<ArrayBuffer> {
    const request = http.createHttp(); this.requests.add(request);
    try {
      const r = await request.request(url, { expectDataType: http.HttpDataType.ARRAY_BUFFER,
        usingCache: false, maxLimit: limit, connectTimeout: 10000, readTimeout: 30000 });
      if (r.responseCode !== 200 || !(r.result instanceof ArrayBuffer) || r.result.byteLength > limit) { throw new Error(`HTTP ${r.responseCode} 或文件过大`); }
      return r.result as ArrayBuffer;
    } finally { this.requests.delete(request); request.destroy(); }
  }
  private async save(name: string, bytes: ArrayBuffer): Promise<void> {
    const slash = name.lastIndexOf('/');
    if (slash >= 0) { await fs.mkdir(`${this.root}/${name.slice(0, slash)}`, true); }
    const path = `${this.root}/${name}`; const temp = `${path}.part`;
    const file = await fs.open(temp, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
    try { let offset = 0; while (offset < bytes.byteLength) {
      const n = await fs.write(file.fd, bytes.slice(offset)); if (n <= 0) { throw new Error('写入瓦片失败'); } offset += n;
    } } finally { await fs.close(file); }
    await fs.rename(temp, path);
  }
  async open(url: string, context: common.UIAbilityContext, scene: Scene, camera: Camera): Promise<spatialRender.TiledGSNode> {
    if (!/^https?:\/\/[A-Za-z0-9.\-:\[\]]+(?:\/[A-Za-z0-9._-]+)*\.json$/.test(url)) { throw new Error('请输入 HTTP(S) 场景 JSON 地址'); }
    this.base = url.slice(0, url.lastIndexOf('/') + 1);
    this.root = `${context.cacheDir}/spatial-stream-${Date.now()}`; await fs.mkdir(this.root);
    this.progress('下载原生流式清单');
    await this.save('scene.json', await this.fetch(url, 4194304));
    if (this.stopped) { throw new Error('加载已取消'); }
    this.progress('SpatialRender 正在解析清单');
    console.info('SpatialStream loadTiledGSNode begin');
    const node = await spatialRender.GSPlugin.loadTiledGSNode(scene, { uri: `file://${this.root}/scene.json` }, scene.root ?? undefined);
    if (this.stopped) { node.destroy(); throw new Error('加载已取消'); }
    this.node = node;
    node.position = { x: 0, y: 0, z: 0 }; node.scale = { x: 1, y: 1, z: 1 }; node.visible = true;
    node.rotation = { x: 0, y: 0, z: 1, w: 0 };
    node.setTileRequestCallback((tiles: spatialRender.GSTile[]) => {
      if (this.stopped) { return; }
      this.requested += tiles.length;
      for (const tile of tiles) {
        if (!/^(?:[A-Za-z0-9_-]+\/)*[A-Za-z0-9_.-]+\.sog$/.test(tile.uri) || tile.uri.includes('..')) {
          this.progress('引擎返回不支持的瓦片路径'); continue;
        }
        if (this.ready.has(tile.uri)) { node.notifyTileReady(tile); }
        else if (!this.queued.has(tile.uri)) { this.queued.add(tile.uri); this.queue.push(tile); }
      }
      this.pump();
    });
    node.setCamera(camera);
    this.progress('清单接口已返回，等待首个瓦片请求');
    this.watchdog = setTimeout(() => {
      if (!this.stopped && this.requested === 0) { this.progress('尚无瓦片请求：需要核对清单格式与引擎场景，尚未完成流式显示'); }
    }, 10000);
    console.info('SpatialStream node ready');
    return node;
  }
  private pump(): void {
    while (!this.stopped && this.running < 2 && this.queue.length > 0) {
      const tile = this.queue.shift() as spatialRender.GSTile; this.running++;
      this.loadTile(tile);
    }
  }
  private async loadTile(tile: spatialRender.GSTile): Promise<void> {
    try {
      const bytes = await this.fetch(this.base + tile.uri, 33554432);
      if (this.stopped) { return; }
      if (this.bytes + bytes.byteLength > 4 * 1073741824) { throw new Error('原生流式缓存超过 4 GiB'); }
      this.bytes += bytes.byteLength;
      await this.save(tile.uri, bytes);
      if (this.stopped) { return; }
      this.ready.add(tile.uri); this.node?.notifyTileReady(tile);
      this.progress(`原生流式 · 就绪 ${this.ready.size} · 待加载 ${this.queue.length} · ${(this.bytes / 1048576).toFixed(0)} MiB`);
      console.info(`SpatialStream tile ready ${tile.uri}`);
    } catch (error) { if (!this.stopped) { this.progress(`瓦片失败 ${tile.uri}: ${String(error)}`); } }
    finally { this.queued.delete(tile.uri); this.running--; this.pump(); }
  }
  async dispose(): Promise<void> {
    this.stop();
    while (this.running > 0 || this.requests.size > 0) { await new Promise<void>((resolve) => { setTimeout(resolve, 50); }); }
    if (this.root) { try { await fs.rmdir(this.root); } catch (_) {} }
  }
  stop(): void {
    if (this.stopped) { return; }
    if (this.watchdog !== -1) { clearTimeout(this.watchdog); this.watchdog = -1; }
    this.stopped = true; this.node?.setTileRequestCallback(null); this.node = undefined; this.queue = [];
    for (const request of this.requests) { request.destroy(); } this.requests.clear();
  }
}
