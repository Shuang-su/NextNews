import { CubicSpline } from './ViewerSpline';
export interface ViewerPose { position: number[]; target: number[]; fov: number; }
export interface ViewerTrack {
  name: string; duration: number; frameRate: number; loopMode: string; interpolation: string; smoothness: number;
  keyframes: ViewerKeys;
}
export interface ViewerKeys { times: number[]; values: ViewerValues; }
export interface ViewerValues { position: number[]; target: number[]; fov: number[]; }
export interface ViewerCamera { initial: ViewerPose; }
export interface ViewerSettings { version: number; cameras: ViewerCamera[]; animTracks: ViewerTrack[]; startMode: string; }
function vector(a: number[], n: number): boolean {
  return Array.isArray(a) && a.length === n && a.every((v: number) => typeof v === 'number' && Number.isFinite(v) && Math.abs(v) < 1e8);
}
export function validatePose(p: ViewerPose): void {
  if (!p || !vector(p.position, 3) || !vector(p.target, 3) || !Number.isFinite(p.fov) || p.fov <= 1 || p.fov >= 179 ||
    Math.hypot(...p.position.map((v: number, i: number) => v - p.target[i])) < 1e-6) { throw new Error('Viewer 镜头参数无效'); }
}
export function parseViewerSettings(text: string): ViewerSettings {
  if (text.length > 4 * 1024 * 1024) { throw new Error('Viewer 配置超过 4 MiB'); }
  const settings = JSON.parse(text) as ViewerSettings;
  if (!settings || settings.version !== 2 || !Array.isArray(settings.cameras) || !settings.cameras.length ||
    !Array.isArray(settings.animTracks) || settings.animTracks.length > 64 || !['default', 'animTrack', 'annotation'].includes(settings.startMode)) {
    throw new Error('需要有效的 Viewer v2 配置');
  }
  settings.cameras.forEach((c: ViewerCamera) => validatePose(c.initial));
  settings.animTracks.forEach((t: ViewerTrack) => {
    if (!t || !Number.isFinite(t.duration) || t.duration <= 0 || !Number.isFinite(t.frameRate) || t.frameRate <= 0 ||
      !Number.isFinite(t.smoothness) || t.smoothness < 0 || t.smoothness > 1 || !['none', 'repeat', 'pingpong'].includes(t.loopMode) ||
      !['step', 'spline'].includes(t.interpolation) || !t.keyframes || !Array.isArray(t.keyframes.times) || !t.keyframes.values) { throw new Error('Viewer 动画参数无效'); }
    const k = t.keyframes; const v = k.values; const n = k.times.length;
    if (n < 2 || n > 10000 || !vector(k.times, n) || !vector(v.position, n * 3) || !vector(v.target, n * 3) || !vector(v.fov, n)) { throw new Error('Viewer 动画关键帧无效'); }
    k.times.forEach((time: number, i: number) => {
      if (time < 0 || time > t.duration * t.frameRate || (i > 0 && time <= k.times[i - 1])) { throw new Error('Viewer 动画时间必须递增'); }
      validatePose({ position: v.position.slice(i * 3, i * 3 + 3), target: v.target.slice(i * 3, i * 3 + 3), fov: v.fov[i] });
    });
  });
  return settings;
}
// Camera coordinates are already Viewer world coordinates: do NOT apply the model's Rz transform again.
export function normalizedPose(p: ViewerPose, bounds: number[]): number[] {
  validatePose(p);
  if (!vector(bounds, 4) || bounds[3] <= 0) { throw new Error('场景边界无效'); }
  const d = p.position.map((v: number, i: number) => v - p.target[i]); const distance = Math.hypot(...d);
  return [Math.atan2(d[0], d[2]), Math.asin(d[1] / distance), 1,
    ...p.position.map((v: number, i: number) => (v - bounds[i]) / bounds[3])];
}
export function defaultTrack(p: ViewerPose, inside: boolean): ViewerTrack {
  const count = inside ? 24 : 12; const times: number[] = []; const positions: number[] = []; const targets: number[] = [];
  const dx = p.position[0] - p.target[0], dy = p.position[1] - p.target[1], dz = p.position[2] - p.target[2];
  const horizontal = Math.hypot(dx, dz); const radius = Math.max(horizontal, Math.hypot(dx, dy, dz) * .3);
  const forwardX = horizontal > .001 ? dx / horizontal : 0, forwardZ = horizontal > .001 ? dz / horizontal : 1;
  const rightX = horizontal > .001 ? -forwardZ : 1, rightZ = horizontal > .001 ? forwardX : 0;
  for (let i = 0; i < count; i++) {
    const angle = i / count * Math.PI * 2; times.push(i / count * 20);
    if (inside) {
      positions.push(p.position[0] + rightX * .5 * Math.sin(angle) + forwardX * .25 * Math.sin(2 * angle), p.position[1],
        p.position[2] + rightZ * .5 * Math.sin(angle) + forwardZ * .25 * Math.sin(2 * angle));
    } else {
      positions.push(p.target[0] + radius * Math.sin(Math.atan2(dx, dz) - angle), p.target[1] + dy,
        p.target[2] + radius * Math.cos(Math.atan2(dx, dz) - angle));
    }
    targets.push(...p.target);
  }
  return { name: inside ? 'figure8' : 'rotate', duration: 20, frameRate: 1, loopMode: 'repeat', interpolation: 'spline', smoothness: 1,
    keyframes: { times, values: { position: positions, target: targets, fov: new Array<number>(count).fill(p.fov) } } };
}
export class ViewerAnimation {
  private track: ViewerTrack;
  private spline: CubicSpline;
  constructor(track: ViewerTrack) {
    this.track = track; const k = track.keyframes; const points: number[] = [];
    k.times.forEach((_time: number, i: number) => { points.push(...k.values.position.slice(i * 3, i * 3 + 3), ...k.values.target.slice(i * 3, i * 3 + 3), k.values.fov[i]); });
    const extra = track.duration === k.times[k.times.length - 1] / track.frameRate ? 1 : 0;
    this.spline = track.loopMode === 'repeat' ? CubicSpline.fromPointsLooping((track.duration + extra) * track.frameRate, k.times, points, track.smoothness) : CubicSpline.fromPoints(k.times, points, track.smoothness);
  }
  sample(seconds: number): ViewerPose {
    const t = this.track; let time = Math.max(0, seconds);
    if (t.loopMode === 'repeat') { time %= t.duration; }
    else if (t.loopMode === 'pingpong') { time %= t.duration * 2; if (time > t.duration) { time = 2 * t.duration - time; } }
    else { time = Math.min(time, t.duration); }
    const result: number[] = []; this.spline.evaluate(time * t.frameRate, result);
    return { position: result.slice(0, 3), target: result.slice(3, 6), fov: result[6] };
  }
}
