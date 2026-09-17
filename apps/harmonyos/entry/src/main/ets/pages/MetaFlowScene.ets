import { url } from '@kit.ArkTS';
import { sceneResource } from './ViewerScene';
export interface MetaFlowFiles {
  model?: string; settings?: string; thumbnail?: string; environment?: string; collision?: string;
  voxel?: string; voxelManifest?: string; recommended?: string | number; lod?: MetaFlowLod[];
}
interface MetaFlowLod { level: string | number; file: string; }
export interface MetaFlowViewer {
  defaultCameraMode?: string; syntheticAnimation?: string; animationFirstExitMode?: string; voxelCoordinateSpace?: string;
}
export interface MetaFlowResource {
  id: string; title?: string; route?: string; aliases?: string[]; experienceType?: string;
  files: MetaFlowFiles; viewer?: MetaFlowViewer; recommended?: string | number; fileSize?: MetaFlowSize;
}
interface MetaFlowSize { recommended?: string | number; }
interface MetaFlowIndex { resources: MetaFlowResource[]; }
export interface MetaFlowConfig {
  content: string; settings: string; poster: string; environment: string; collision: string; voxel: string; voxelManifest: string;
  title: string; experienceType: string; mode: string; syntheticAnimation: string; animationFirstExitMode: string;
  voxelCoordinateSpace: string; reveal: boolean; noanim: boolean; nofx: boolean; noui: boolean; fullLoad: boolean;
  highPrecision?: boolean; budget?: number; skybox: string;
}
function route(value: string): string { return value.length > 1 && value.endsWith('/') ? value.slice(0, -1) : value; }
export function metaFlowIndexUrl(address: string): string { return new url.URL('/data/index.json', address).toString(); }
export function resolveMetaFlow(address: string, indexText: string = '{"resources":[]}'): MetaFlowConfig {
  const page = new url.URL(sceneResource(address, address));
  const query = page.searchParams;
  let resource: MetaFlowResource | undefined = undefined;
  if (!query.get('content')) {
    if (indexText.length > 16 * 1024 * 1024) { throw new Error('MetaFlow 索引过大'); }
    const index = JSON.parse(indexText) as MetaFlowIndex;
    if (!index || !Array.isArray(index.resources) || index.resources.length > 10000) { throw new Error('MetaFlow 项目索引无效'); }
    const path = route(decodeURIComponent(page.pathname));
    resource = index.resources.find((r: MetaFlowResource) => r && (route(r.route ?? '') === path ||
      (Array.isArray(r.aliases) && r.aliases.some((alias: string) => typeof alias === 'string' && route(alias) === path))));
    if (!resource) { throw new Error('MetaFlow 地址未匹配到项目'); }
  }
  const files = resource?.files;
  let model = files?.model ?? '';
  if (files?.lod?.length) {
    const value = resource?.fileSize?.recommended ?? resource?.recommended ?? files.recommended;
    const level = typeof value === 'number' ? value : typeof value === 'string' ? Number(value.toLowerCase().replace(/^lod\s*/, '')) : NaN;
    model = files.lod.find((entry: MetaFlowLod) => Number(entry.level) === level && typeof entry.file === 'string')?.file ??
      files.lod.find((entry: MetaFlowLod) => typeof entry.file === 'string')?.file ?? model;
  }
  const dataBase = metaFlowIndexUrl(address);
  const pick = (name: string, fallback: string = ''): string => {
    const explicit = query.get(name); return explicit ? sceneResource(explicit, address) : sceneResource(fallback, dataBase);
  };
  const viewer = resource?.viewer;
  const mode = viewer?.defaultCameraMode ?? '';
  if (mode && !['orbit', 'fly', 'walk', 'anim'].includes(mode)) { throw new Error('MetaFlow 默认模式无效'); }
  const space = viewer?.voxelCoordinateSpace ?? 'world';
  if (!['world', 'metaflow-rz180'].includes(space)) { throw new Error('MetaFlow 体素坐标无效'); }
  const budgetText = query.get('budget'), budget = budgetText ? Number(budgetText) * 1000000 : undefined;
  if (budget !== undefined && (!Number.isFinite(budget) || budget <= 0 || budget > 8000000)) { throw new Error('预算需在 0～800 万之间'); }
  const result: MetaFlowConfig = {
    content: pick('content', model), settings: pick('settings', files?.settings ?? ''), poster: pick('poster', files?.thumbnail ?? ''),
    environment: pick('environment', files?.environment ?? ''), collision: pick('collision', files?.collision ?? ''),
    voxel: pick('voxel', files?.voxel ?? ''), voxelManifest: pick('voxelManifest', files?.voxelManifest ?? ''), skybox: pick('skybox'),
    title: resource?.title ?? 'MetaFlow 场景', experienceType: resource?.experienceType ?? 'scene', mode,
    syntheticAnimation: viewer?.syntheticAnimation ?? '', animationFirstExitMode: viewer?.animationFirstExitMode ?? 'default',
    voxelCoordinateSpace: space, reveal: !query.has('noreveal'), noanim: query.has('noanim'), nofx: query.has('nofx'),
    noui: query.has('noui'), fullLoad: query.has('fullload'), highPrecision: query.has('hpr') ? true : undefined, budget
  };
  if (!result.content) { throw new Error('MetaFlow 项目缺少主体资源'); }
  return result;
}
