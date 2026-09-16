import { sogComponents, SogMetadata } from './ViewerLodManifest';
import { metaFlowIndexUrl, resolveMetaFlow } from './MetaFlowScene';
import { ViewerScene, describeScene } from './ViewerScene';
import { http } from '@kit.NetworkKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import { util, url } from '@kit.ArkTS';
import { cryptoFramework } from '@kit.CryptoArchitectureKit';
import { viewerRequests } from './ViewerRequests';
import { ViewerSettings, parseViewerSettings, createSettings } from './ViewerSettings';
interface SogReceipt { names: string[]; sizes: number[]; }
export interface CatalogProject {
  modelId: string; modelTitle: string; modelType: string; modelFileUrl?: string; modelSetting?: string | Object;
  lodMeta?: string; lodSettings?: string; lodBin?: string; thumbnailUrl?: string; coverUrl?: string;
}
interface CatalogList { code: number; total: number; rows: CatalogProject[]; }
interface CatalogDetail { code: number; data: CatalogProject; }
export interface CatalogAsset extends ViewerScene { project: CatalogProject; }
export function catalogUrl(value: string): string {
  if (typeof value !== 'string' || !value.trim()) { throw new Error('项目缺少资源地址'); }
  const parsed = new url.URL(value.trim());
  if (parsed.protocol !== 'https:' || parsed.username || parsed.password || parsed.hash) { throw new Error('线上资源需要 HTTPS 地址'); }
  return parsed.toString();
}
export function validateProject(p: CatalogProject): void {
  if (p && (p.modelTitle === null || p.modelTitle === undefined || p.modelTitle === '')) { p.modelTitle = '未命名项目'; }
  if (!p || typeof p.modelId !== 'string' || !/^[0-9]{1,24}$/.test(p.modelId) || typeof p.modelTitle !== 'string' || !p.modelTitle.length || p.modelTitle.length > 500 || !['default', 'lod'].includes(p.modelType)) { throw new Error('项目字段无效'); }
}
export class ViewerCatalog {
  private stopped: boolean = false;
  private cancellations: Set<() => void> = new Set();
  private requests: Set<http.HttpRequest> = new Set();
  private details: Map<string, CatalogAsset> = new Map();
  cancel(): void { this.stopped = true; this.cancellations.forEach((cancel: () => void) => cancel()); this.cancellations.clear(); this.requests.forEach((r: http.HttpRequest) => r.destroy()); this.requests.clear(); }
  private async fetch(address: string, limit: number): Promise<ArrayBuffer> {
    await viewerRequests.acquire();
    if (this.stopped) { viewerRequests.release(); throw new Error('线上项目请求已取消'); }
    const request = http.createHttp(); this.requests.add(request);
    try {
      const response = await request.request(catalogUrl(address), { expectDataType: http.HttpDataType.ARRAY_BUFFER, maxLimit: limit, usingCache: false, connectTimeout: 10000, readTimeout: 30000 });
      if (this.stopped || response.responseCode !== 200 || !(response.result instanceof ArrayBuffer) || response.result.byteLength > limit) { throw new Error(`资源加载失败 HTTP ${response.responseCode}`); }
      return response.result as ArrayBuffer;
    } finally { request.destroy(); this.requests.delete(request); viewerRequests.release(); }
  }
  private async download(address: string, limit: number, progress: (percent: number) => void): Promise<ArrayBuffer> {
    await viewerRequests.acquire();
    if (this.stopped) { viewerRequests.release(); throw new Error('线上项目请求已取消'); }
    const request = http.createHttp(); this.requests.add(request);
    let cancel: () => void = () => {};
    try {
      return await new Promise<ArrayBuffer>((resolve, reject) => {
        const chunks: ArrayBuffer[] = []; let bytes = 0; let ended = false; let code = 0; let settled = false;
        const fail = (message: string): void => {
          if (settled) { return; } settled = true; chunks.length = 0; reject(new Error(message));
        };
        cancel = (): void => { fail('线上项目请求已取消'); };
        this.cancellations.add(cancel);
        const finish = (): void => {
          // requestInStream may acknowledge HTTP before or after dataEnd.
          if (settled || !ended || !code) { return; }
          if (this.stopped || code !== 200) { fail(`资源加载失败 HTTP ${code}`); return; }
          settled = true; const result = new Uint8Array(bytes); let offset = 0;
          chunks.forEach((chunk: ArrayBuffer) => { result.set(new Uint8Array(chunk), offset); offset += chunk.byteLength; });
          chunks.length = 0; progress(100); resolve(result.buffer);
        };
        request.on('dataReceive', (chunk: ArrayBuffer) => {
          if (settled) { return; }
          bytes += chunk.byteLength;
          if (bytes > limit) { fail('模型下载超过大小限制'); request.destroy(); return; }
          chunks.push(chunk);
        });
        request.on('dataReceiveProgress', (info: http.DataReceiveProgressInfo) => {
          if (settled || this.stopped) { return; }
          progress(Number.isFinite(info.totalSize) && info.totalSize > 0 && info.receiveSize >= 0 && info.receiveSize <= info.totalSize ? info.receiveSize / info.totalSize * 100 : -1);
        });
        request.on('dataEnd', () => { ended = true; finish(); });
        progress(-1);
        request.requestInStream(catalogUrl(address), { maxLimit: limit, usingCache: false, connectTimeout: 10000, readTimeout: 30000 })
          .then((status: number) => { code = status; if (code !== 200) { fail(`资源加载失败 HTTP ${code}`); } else { finish(); } })
          .catch((error: Error) => { fail(error.message || '模型下载失败'); });
      });
    } finally { this.cancellations.delete(cancel); request.destroy(); this.requests.delete(request); viewerRequests.release(); }
  }
  private async text(address: string): Promise<string> {
    return new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(new Uint8Array(await this.fetch(address, 4194304)));
  }
  async list(page: number, search: string): Promise<CatalogList> {
    if (!Number.isInteger(page) || page < 1 || page > 10000 || search.length > 200) { throw new Error('分页参数无效'); }
    const result = JSON.parse(await this.text(`https://sztvbmsapi.sztv.com.cn/api/gaussian/model/list?pageNum=${page}&pageSize=20&modelTitle=${encodeURIComponent(search.trim())}`)) as CatalogList;
    if (result.code !== 200 || !Number.isInteger(result.total) || result.total < 0 || !Array.isArray(result.rows) || result.rows.length > 20) { throw new Error('项目列表响应无效'); }
    result.rows.forEach((p: CatalogProject) => validateProject(p)); return result;
  }
  async player(address: string): Promise<CatalogAsset> {
    address = catalogUrl(address);
    const page = new url.URL(address);
    const config = resolveMetaFlow(address, page.searchParams.get('content') ? '{"resources":[]}' : await this.text(metaFlowIndexUrl(address)));
    const settings = config.settings ? parseViewerSettings(await this.text(config.settings)) : createSettings();
    settings.revealEffect = config.reveal ? 'radial' : 'none'; settings.experienceType = config.experienceType;
    if (config.skybox) { settings.background.skyboxUrl = config.skybox; }
    if (config.voxelManifest) { settings.voxelManifestUrl = config.voxelManifest; }
    if (config.voxel) { settings.voxelUrl = config.voxel; }
    if (config.collision) { settings.collisionUrl = config.collision; }
    if (config.highPrecision !== undefined) { settings.highPrecisionRendering = config.highPrecision; }
    if (config.nofx) {
      settings.highPrecisionRendering = false;
      settings.postEffectSettings.sharpness.enabled = false; settings.postEffectSettings.bloom.enabled = false;
      settings.postEffectSettings.grading.enabled = false; settings.postEffectSettings.vignette.enabled = false; settings.postEffectSettings.fringing.enabled = false;
    }
    const scene = describeScene(config.content, config.settings || address, settings, config.poster, config.collision || config.voxelManifest || config.voxel);
    let type = 'default';
    if (new url.URL(config.content).pathname.endsWith('.json')) {
      const source = JSON.parse(await this.text(config.content)) as SogMetadata;
      if (source.version === 2 && source.means) { sogComponents(source, 1); }
      else { type = 'lod'; }
    }
    const project: CatalogProject = { modelId: '0', modelTitle: config.title, modelType: type };
    return { project, content: scene.content, configuration: scene.configuration, poster: scene.poster,
      settings: scene.settings, collision: scene.collision, player: config };
  }
  async detail(id: string): Promise<CatalogAsset> {
    if (!/^[0-9]{1,24}$/.test(id)) { throw new Error('项目编号无效'); }
    const cached = this.details.get(id); if (cached) { return cached; }
    const response = JSON.parse(await this.text(`https://sztvbmsapi.sztv.com.cn/api/gaussian/model/detail/${id}`)) as CatalogDetail;
    if (response.code !== 200) { throw new Error('项目详情加载失败'); } const p = response.data; validateProject(p);
    if (p.modelId !== id) { throw new Error('项目编号不匹配'); }
    const lod = p.modelType === 'lod';
    const content = catalogUrl((lod ? p.lodMeta : p.modelFileUrl) ?? '');
    let settings = createSettings({ position: [16.650447845458984, 22.538938522338867, -32.462284088134766], target: [-2.944159287248759, 10.141348153415594, .27791836255628866], fov: 75 });
    settings.tonemapping = 'none'; // MetaFlow business-player fallback when modelSetting is absent.
    const configuration = lod && p.lodSettings ? catalogUrl(p.lodSettings) : `https://sztvbmsapi.sztv.com.cn/api/gaussian/model/detail/${id}`;
    if (lod && p.lodSettings) { settings = parseViewerSettings(await this.text(configuration)); }
    else if (p.modelSetting) { settings = parseViewerSettings(typeof p.modelSetting === 'string' ? p.modelSetting : JSON.stringify(p.modelSetting)); }
    const scene = describeScene(content, configuration, settings, p.coverUrl || p.thumbnailUrl || '', lod ? p.lodBin || '' : '');
    const result: CatalogAsset = { project: p, content: scene.content, configuration: scene.configuration,
      poster: scene.poster, settings: scene.settings, collision: scene.collision }; this.details.set(id, result); return result;
  }
  async model(asset: CatalogAsset, directory: string, progress?: (percent: number) => void): Promise<string> {
    return this.resource(asset.content, directory, progress);
  }
  async resource(content: string, directory: string, progress?: (percent: number) => void): Promise<string> {
    if (new url.URL(content).pathname.endsWith('.json')) { return this.looseSog(content, directory, progress); }
    const extension = new url.URL(content).pathname.endsWith('.sog') ? '.sog' : new url.URL(content).pathname.endsWith('.ply') ? '.ply' : '';
    if (!extension) { throw new Error('单体模型暂时仅支持 SOG / 标准 3DGS PLY'); }
    const hash = cryptoFramework.createMd('SHA256'); await hash.update({ data: new util.TextEncoder().encodeInto(content) });
    const key = Array.from((await hash.digest()).data).map((v: number) => v.toString(16).padStart(2, '0')).join('');
    const path = `${directory}/cdn-${key}${extension}`;
    try { if ((await fs.stat(path)).size > 0) { progress?.(100); return path; } } catch (_) {}
    const bytes = progress ? await this.download(content, 134217728, progress) : await this.fetch(content, 134217728);
    const file = await fs.open(`${path}.part`, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
    try { let offset = 0; while (offset < bytes.byteLength) { const n = await fs.write(file.fd, bytes.slice(offset)); if (n <= 0) { throw new Error('模型缓存写入失败'); } offset += n; } }
    finally { await fs.close(file); }
    if (this.stopped) { throw new Error('项目加载已取消'); } await fs.rename(`${path}.part`, path); return path;
  }
  private async writeResource(path: string, data: ArrayBuffer): Promise<void> {
    if (this.stopped) { throw new Error('项目加载已取消'); }
    const file = await fs.open(`${path}.part`, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
    try { let offset = 0; while (offset < data.byteLength) { const n = await fs.write(file.fd, data.slice(offset)); if (n <= 0) { throw new Error('资源缓存写入失败'); } offset += n; } }
    finally { await fs.close(file); }
    if (this.stopped) { throw new Error('项目加载已取消'); }
    await fs.rename(`${path}.part`, path);
  }
  private async looseSog(address: string, directory: string, progress?: (percent: number) => void): Promise<string> {
    progress?.(-1);
    const metadata = await this.fetch(address, 1048576);
    const text = new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(new Uint8Array(metadata));
    const names = sogComponents(JSON.parse(text) as SogMetadata, 1);
    const digest = cryptoFramework.createMd('SHA256'); await digest.update({ data: new util.TextEncoder().encodeInto(address + text) });
    const key = Array.from((await digest.digest()).data).map((v: number) => v.toString(16).padStart(2, '0')).join('');
    const folder = `${directory}/cdn-${key}.parts`, path = `${folder}/meta.json`;
    try {
      const receipt = JSON.parse(await fs.readText(`${folder}/ready.json`)) as SogReceipt;
      if (receipt.names.length !== names.length || receipt.sizes.length !== names.length || (await fs.stat(path)).size !== metadata.byteLength) { throw new Error('缓存元数据不完整'); }
      for (let i = 0; i < names.length; i++) { if (receipt.names[i] !== names[i] || !Number.isInteger(receipt.sizes[i]) || receipt.sizes[i] < 1 || receipt.sizes[i] > 33554432 || (await fs.stat(`${folder}/${names[i]}`)).size !== receipt.sizes[i]) { throw new Error('缓存纹理不完整'); } }
      if (this.stopped) { throw new Error('项目加载已取消'); } progress?.(100); return path;
    } catch (_) {}
    if (this.stopped) { throw new Error('项目加载已取消'); }
    try { await fs.mkdir(folder); } catch (_) { if (!await fs.access(folder)) { throw new Error('无法创建 SOG 缓存'); } }
    try { await fs.unlink(`${folder}/ready.json`); } catch (_) {}
    const sizes: number[] = []; let total = metadata.byteLength;
    for (const name of names) {
      const bytes = await this.fetch(new url.URL(name, address).toString(), 33554432);
      total += bytes.byteLength; if (total > 134217728) { throw new Error('单体 SOG 超过 128 MiB'); }
      await this.writeResource(`${folder}/${name}`, bytes); sizes.push(bytes.byteLength);
    }
    await this.writeResource(path, metadata);
    const receipt: SogReceipt = { names, sizes };
    await this.writeResource(`${folder}/ready.json`, new util.TextEncoder().encodeInto(JSON.stringify(receipt)).buffer as ArrayBuffer);
    progress?.(100); return path;
  }

}
