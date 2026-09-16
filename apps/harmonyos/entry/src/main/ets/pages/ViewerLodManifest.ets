import { StreamManifest, StreamChunk, StreamLeaf } from './StreamSession';
interface Box { min: number[]; max: number[]; }
interface Range { file: number; offset: number; count: number; }
interface Tree { bound: Box; lods?: Record<string, Range>; children?: Tree[]; }
interface Source { version: number; filenames: string[]; environment?: string; lodLevels: number; tree: Tree; }
export function componentPath(path: string): boolean {
  return typeof path === 'string' && path.length < 512 && /^[A-Za-z0-9_-]+(?:\/[A-Za-z0-9_.-]+)*\/(?:meta\.json)$/.test(path) && !path.split('/').some((s: string) => s === '.' || s === '..');
}
function sphere(box: Box): number[] {
  if (!box || !Array.isArray(box.min) || !Array.isArray(box.max) || box.min.length !== 3 || box.max.length !== 3 ||
    box.min.some((n: number, i: number) => !Number.isFinite(n) || !Number.isFinite(box.max[i]) || Math.abs(n) > 1e6 || Math.abs(box.max[i]) > 1e6 || n > box.max[i])) { throw new Error('LOD 包围盒无效'); }
  return box.min.map((n: number, i: number) => (n + box.max[i]) / 2).concat(Math.max(.001, Math.hypot(...box.min.map((n: number, i: number) => box.max[i] - n)) / 2));
}
// Runs in TaskPool. Preserve source file/offset identities; never repack points.
export function parseLodManifest(text: string): StreamManifest {
  const src = JSON.parse(text) as Source;
  if (!Array.isArray(src.filenames)) { return JSON.parse(text) as StreamManifest; }
  if ((src.version !== undefined && src.version !== 1) || src.filenames.length < 1 || src.filenames.length > 1024 || !Number.isInteger(src.lodLevels) || src.lodLevels < 1 || src.lodLevels > 16) { throw new Error('不支持的 SuperSplat LOD 清单'); }
  const bounds = sphere(src.tree?.bound), names = src.filenames.slice();
  if (src.environment) { names.push(src.environment); }
  const unique = new Set<string>();
  const chunks = names.map((name: string, i: number): StreamChunk => {
    if (!componentPath(name) || unique.has(name)) { throw new Error('无效或重复的 LOD 路径'); } unique.add(name);
    return { file: `chunk-${i.toString().padStart(4, '0')}.sog`, source: name, count: 1, bytes: 0, bounds: bounds.slice() };
  });
  const leaves: StreamLeaf[] = [], nodes: Tree[] = [src.tree]; let visited = 0;
  while (nodes.length) {
    const node = nodes.pop() as Tree;
    if (++visited > 131072) { throw new Error('LOD 节点超过预算'); }
    const b = sphere(node.bound);
    if (node.lods) {
      const lods: number[][] = [];
      for (let level = 0; level < src.lodLevels; level++) {
        const r = node.lods[level.toString()];
        if (!r) { lods.push([-1, 0, 0]); continue; }
        if (!Number.isInteger(r.file) || r.file < 0 || r.file >= src.filenames.length || !Number.isInteger(r.offset) || r.offset < 0 || !Number.isInteger(r.count) || r.count <= 0 || r.offset + r.count > 4000000) { throw new Error('LOD 范围无效'); }
        chunks[r.file].count = Math.max(chunks[r.file].count, r.offset + r.count); lods.push([r.file, r.offset, r.count]);
      }
      leaves.push({ bounds: b, aabb: node.bound.min.concat(node.bound.max), lods });
      if (leaves.length > 32768) { throw new Error('LOD 叶节点超过预算'); }
    }
    if (node.children) { if (!Array.isArray(node.children) || node.children.length > 8) { throw new Error('LOD 子节点无效'); } nodes.push(...node.children); }
  }
  return { version: 2, bounds, chunks, leaves, levels: src.lodLevels, environment: src.environment ? chunks.length - 1 : undefined };
}
interface Files { files: string[]; }
export interface SogMetadata { version: number; count: number; means: Files; quats: Files; scales: Files; sh0: Files; }
export function sogComponents(meta: SogMetadata, minimumCount: number): string[] {
  if (!meta || meta.version !== 2 || !Number.isInteger(meta.count) || meta.count < minimumCount || meta.count > 4000000) { throw new Error('SOG 数量或版本与 LOD 不符'); }
  const fields: Files[] = [meta.means, meta.quats, meta.scales, meta.sh0], names: string[] = [];
  fields.forEach((f: Files, i: number) => {
    if (!f || !Array.isArray(f.files) || f.files.length !== (i === 0 ? 2 : 1)) { throw new Error('SOG 纹理字段不完整'); }
    for (const name of f.files) {
      if (typeof name !== 'string' || !/^[A-Za-z0-9_-]+\.webp$/.test(name) || names.includes(name)) { throw new Error('SOG 纹理路径无效'); } names.push(name);
    }
  });
  return names;
}
