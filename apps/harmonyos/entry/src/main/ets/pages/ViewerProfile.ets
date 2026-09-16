import { MetaFlowConfig } from './MetaFlowScene';
import { common } from '@kit.AbilityKit';
import { util } from '@kit.ArkTS';
import { ViewerSettings, ViewerPose, ViewerAnimation, parseViewerSettings, defaultTrack, framePose } from './ViewerSettings';
export class ViewerProfile {
  settings: ViewerSettings;
  animation: ViewerAnimation;
  time: number = 0;
  initialMode: string = '';
  private defaultPose: ViewerPose;
  constructor(settings: ViewerSettings, bounds: number[] = [0, 0, 0, 1], player?: MetaFlowConfig) {
    this.settings = settings;
    this.defaultPose = settings.cameras.length ? settings.cameras[0].initial : framePose(bounds);
    const inside = Math.hypot(...this.defaultPose.position.map((v: number, i: number) => v - bounds[i])) < bounds[3];
    let track = settings.startMode === 'animTrack' && settings.animTracks.length ? settings.animTracks[0] : defaultTrack(this.defaultPose, inside);
    if (player) {
      const explicit = settings.startMode === 'animTrack' && settings.animTracks.length > 0;
      const figure8 = !explicit && ((player.syntheticAnimation === 'figure8' && settings.cameras.length > 0) || (inside && settings.cameras.length > 0));
      const hasAnimation = explicit || figure8 || !inside;
      if (!explicit) { track = defaultTrack(this.defaultPose, figure8); }
      const preferred = player.mode === 'orbit' ? 'orbit' : player.mode === 'fly' || player.mode === 'walk' ? 'fly' : inside ? 'fly' : 'orbit';
      this.initialMode = player.mode === 'anim' ? (hasAnimation ? 'anim' : preferred) : explicit || figure8 ? 'anim' : preferred;
      if (player.noanim && this.initialMode === 'anim') { this.initialMode = preferred; }
    }
    this.animation = new ViewerAnimation(track);
  }
  initial(): ViewerPose { return this.defaultPose; }
  get ended(): boolean { return this.animation.loopMode === 'none' && this.time >= this.animation.duration; }
  seek(seconds: number): ViewerPose {
    this.time = Math.max(0, Math.min(this.animation.duration, seconds)); return this.animation.sample(this.time);
  }
  advance(seconds: number): ViewerPose { this.time += Math.max(0, seconds); return this.animation.sample(this.time); }
  static async load(context: common.UIAbilityContext, bounds: number[] = [0, 0, 0, 1]): Promise<ViewerProfile> {
    const bytes = await context.resourceManager.getRawFileContent('viewer-settings.json');
    return new ViewerProfile(parseViewerSettings(new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(bytes)), bounds);
  }
}
