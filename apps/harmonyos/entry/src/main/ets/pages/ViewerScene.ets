import { MetaFlowConfig } from './MetaFlowScene';
import { url } from '@kit.ArkTS';
import { ViewerSettings } from './ViewerSettings';

// A scene owns the provenance of its URLs. Resolution creates a copy so cached
// configuration and user-provided JSON retain their original relative addresses.
export interface ViewerScene {
  player?: MetaFlowConfig;
  content: string;
  configuration: string;
  poster: string;
  settings: ViewerSettings;
  collision: string;
}
export function sceneResource(value: string | undefined, base: string): string {
  if (!value) { return ''; }
  if (typeof value !== 'string' || value.length > 4096) { throw new Error('场景资源地址无效'); }
  const resolved = new url.URL(value.trim(), base);
  if (resolved.protocol !== 'https:' || resolved.username || resolved.password || resolved.hash) {
    throw new Error('场景资源需要无凭据的 HTTPS 地址');
  }
  return resolved.toString();
}
export function describeScene(content: string, configuration: string, settings: ViewerSettings,
  poster: string = '', collision: string = ''): ViewerScene {
  const resolved = JSON.parse(JSON.stringify(settings)) as ViewerSettings;
  if (resolved.soundUrl) { resolved.soundUrl = sceneResource(resolved.soundUrl, configuration); }
  if (resolved.background.skyboxUrl) { resolved.background.skyboxUrl = sceneResource(resolved.background.skyboxUrl, configuration); }
  if (resolved.collisionUrl) { resolved.collisionUrl = sceneResource(resolved.collisionUrl, configuration); }
  if (resolved.voxelUrl) { resolved.voxelUrl = sceneResource(resolved.voxelUrl, configuration); }
  if (resolved.voxelManifestUrl) { resolved.voxelManifestUrl = sceneResource(resolved.voxelManifestUrl, configuration); }
  return { content, configuration, poster: sceneResource(poster, configuration), settings: resolved,
    collision: collision ? sceneResource(collision, configuration) : resolved.collisionUrl ?? resolved.voxelManifestUrl ?? resolved.voxelUrl ?? '' };
}
