import { common } from '@kit.AbilityKit';
import { util } from '@kit.ArkTS';
import { ViewerSettings, ViewerPose, ViewerAnimation, parseViewerSettings, defaultTrack, framePose } from './ViewerSettings';
export class ViewerProfile {
  settings: ViewerSettings;
  animation: ViewerAnimation;
  time: number = 0;
  private defaultPose: ViewerPose;
  constructor(settings: ViewerSettings, bounds: number[] = [0, 0, 0, 1]) {
    this.settings = settings;
    this.defaultPose = settings.cameras.length ? settings.cameras[0].initial : framePose(bounds);
    const inside = Math.hypot(...this.defaultPose.position.map((v: number, i: number) => v - bounds[i])) < bounds[3];
    const track = settings.startMode === 'animTrack' && settings.animTracks.length ? settings.animTracks[0] : defaultTrack(this.defaultPose, inside);
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
