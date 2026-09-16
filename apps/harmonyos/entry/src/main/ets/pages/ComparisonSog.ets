import { common } from '@kit.AbilityKit';
import { http } from '@kit.NetworkKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import { cryptoFramework } from '@kit.CryptoArchitectureKit';
import { StreamManifest, validateManifest } from './StreamSession';
interface ComparisonProvenance { input_count: number; output_count: number; sh_degree: number; output_sha256: string; source_sog_sha256: string; }
export interface ComparisonAsset { path: string; bounds: number[]; count: number; hash: string; }
async function get(url: string, limit: number): Promise<ArrayBuffer> {
  const request = http.createHttp();
  try {
    const r = await request.request(url, { expectDataType: http.HttpDataType.ARRAY_BUFFER, maxLimit: limit });
    if (r.responseCode !== 200 || !(r.result instanceof ArrayBuffer) || r.result.byteLength > limit) { throw new Error('同源样例下载失败'); }
    return r.result;
  } finally { request.destroy(); }
}
export async function comparisonSog(context: common.UIAbilityContext, ply: boolean = false): Promise<ComparisonAsset> {
  const base = 'http://127.0.0.1:8768/';
  const bytes = new Uint8Array(await get(base + 'scene.json', 4194304)); let text = '';
  for (const v of bytes) { text += String.fromCharCode(v); }
  const manifest = JSON.parse(text) as StreamManifest; validateManifest(manifest);
  const chunk = manifest.chunks.find((c) => c.file === 'chunk-0008.sog');
  if (!chunk || !chunk.sha256) { throw new Error('缺少有 SHA-256 来源记录的同源 SOG'); }
  let file = chunk.file, hash = chunk.sha256, size = chunk.bytes;
  if (ply) {
    const bytes = new Uint8Array(await get(base + 'comparison-sh0.manifest.json', 1048576)); let text = '';
    for (const v of bytes) { text += String.fromCharCode(v); }
    const provenance = JSON.parse(text) as ComparisonProvenance;
    if (provenance.source_sog_sha256 !== chunk.sha256 || provenance.output_count !== chunk.count || provenance.input_count !== chunk.count || provenance.sh_degree !== 0 || !/^[a-f0-9]{64}$/.test(provenance.output_sha256)) { throw new Error('同源 PLY 计数或来源不匹配'); }
    file = 'comparison-sh0.ply'; hash = provenance.output_sha256; size = 0;
  }
  const path = `${context.cacheDir}/comparison-${hash}.${ply ? 'ply' : 'sog'}`;
  let exists = false; try { exists = (await fs.stat(path)).size === size && !ply; } catch (_) {}
  if (!exists) {
    const data = await get(base + file, 33554432);
    const md = cryptoFramework.createMd('SHA256'); await md.update({ data: new Uint8Array(data) });
    const actualHash = Array.from((await md.digest()).data).map((v: number) => v.toString(16).padStart(2, '0')).join('');
    if (actualHash !== hash) { throw new Error('同源 SOG 校验失败'); }
    const f = await fs.open(path + '.part', fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
    try { let offset = 0; while (offset < data.byteLength) { const n = await fs.write(f.fd, data.slice(offset)); if (n <= 0) { throw new Error('写入失败'); } offset += n; } } finally { await fs.close(f); }
    await fs.rename(path + '.part', path);
  }
  console.info(`ComparisonSog file=${file} count=${chunk.count} sha256=${hash} diskHit=${exists}`);
  return { path: path, bounds: [-chunk.bounds[0], -chunk.bounds[1], chunk.bounds[2], chunk.bounds[3]], count: chunk.count, hash: hash };
}
