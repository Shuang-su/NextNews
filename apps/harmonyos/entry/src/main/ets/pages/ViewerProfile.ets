import { common } from '@kit.AbilityKit';
import { util } from '@kit.ArkTS';
import { ViewerSettings, ViewerPose, ViewerAnimation, parseViewerSettings, defaultTrack } from './ViewerSettings';
export class ViewerProfile {
  settings: ViewerSettings;
  animation: ViewerAnimation;
  time: number = 0;
  constructor(settings: ViewerSettings) {
    this.settings = settings;
    // The bundled profile belongs to Huafa; use its original foreground root AABB after Rz(180).
    const p = settings.cameras[0].initial.position;
    const min = [-21.87618, -87.61899, -56.45915], max = [422.6212, 28.71702, 383.1812];
    const inside = p.every((v: number, i: number) => v >= min[i] && v <= max[i]);
    const track = settings.startMode === 'animTrack' && settings.animTracks.length ? settings.animTracks[0] : defaultTrack(settings.cameras[0].initial, inside);
    this.animation = new ViewerAnimation(track);
  }
  initial(): ViewerPose { return this.settings.cameras[0].initial; }
  advance(seconds: number): ViewerPose { this.time += Math.max(0, seconds); return this.animation.sample(this.time); }
  static async load(context: common.UIAbilityContext): Promise<ViewerProfile> {
    const bytes = await context.resourceManager.getRawFileContent('viewer-settings.json');
    return new ViewerProfile(parseViewerSettings(new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(bytes)));
  }
}
