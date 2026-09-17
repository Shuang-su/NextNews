// Settings/animation semantics follow SuperSplat Viewer 1.31.2 (MIT).
import { CubicSpline } from './ViewerSpline';
export interface ViewerPose { position: number[]; target: number[]; fov: number; }
export interface ViewerTrack {
  name: string; duration: number; frameRate: number; loopMode: string; interpolation: string; smoothness: number;
  keyframes: ViewerKeys;
}
export interface ViewerKeys { times: number[]; values: ViewerValues; }
export interface ViewerValues { position: number[]; target: number[]; fov: number[]; }
export interface ViewerCamera { initial: ViewerPose; }
export interface ViewerAnnotation { position: number[]; title: string; text: string; camera: ViewerCamera; extras?: Object; }
export interface ViewerBackground { color: number[]; skyboxUrl?: string; }
export interface Sharpness { enabled: boolean; amount: number; }
export interface Bloom { enabled: boolean; intensity: number; blurLevel: number; }
export interface Grading { enabled: boolean; brightness: number; contrast: number; saturation: number; tint: number[]; }
export interface Vignette { enabled: boolean; intensity: number; inner: number; outer: number; curvature: number; }
export interface Fringing { enabled: boolean; intensity: number; }
export interface ViewerEffects { sharpness: Sharpness; bloom: Bloom; grading: Grading; vignette: Vignette; fringing: Fringing; }
export interface ViewerSettings {
  revealEffect?: string; experienceType?: string;
  version: number; cameras: ViewerCamera[]; animTracks: ViewerTrack[]; startMode: string;
  annotations: ViewerAnnotation[]; background: ViewerBackground; tonemapping: string;
  highPrecisionRendering: boolean; postEffectSettings: ViewerEffects; soundUrl?: string; collisionUrl?: string; voxelUrl?: string; voxelManifestUrl?: string;
}
interface LegacyCamera { position?: number[]; target?: number[]; fov?: number; startAnim?: string; }
interface LegacySettings { camera: LegacyCamera; background?: ViewerBackground; animTracks?: ViewerTrack[]; }
export function defaultEffects(): ViewerEffects {
  return { sharpness: { enabled: false, amount: 0 }, bloom: { enabled: false, intensity: .1, blurLevel: 2 },
    grading: { enabled: false, brightness: 1, contrast: 1, saturation: 1, tint: [1, 1, 1] },
    vignette: { enabled: false, intensity: .5, inner: .3, outer: .75, curvature: 1 }, fringing: { enabled: false, intensity: .5 } };
}
export function createSettings(pose?: ViewerPose): ViewerSettings {
  return { version: 2, cameras: pose ? [{ initial: pose }] : [], animTracks: [], annotations: [], startMode: 'default',
    background: { color: [0, 0, 0] }, tonemapping: 'linear', highPrecisionRendering: false, postEffectSettings: defaultEffects() };
}
function vector(a: number[], n: number): boolean {
  return Array.isArray(a) && a.length === n && a.every((v: number) => typeof v === 'number' && Number.isFinite(v) && Math.abs(v) < 1e8);
}
export function validatePose(p: ViewerPose): void {
  if (!p || !vector(p.position, 3) || !vector(p.target, 3) || !Number.isFinite(p.fov) || p.fov <= 1 || p.fov >= 179 ||
    Math.hypot(...p.position.map((v: number, i: number) => v - p.target[i])) < 1e-6) { throw new Error('Viewer 镜头参数无效'); }
}
function finite(values: number[]): void {
  if (!values.every((v: number) => typeof v === 'number' && Number.isFinite(v) && Math.abs(v) < 1e8)) { throw new Error('Viewer 效果参数无效'); }
}
// Accept authored MetaFlow JSONC without altering comment-like text inside strings.
export function settingsJson(text: string): string {
  let output = '', quoted = false, escaped = false, line = false, block = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i], next = text[i + 1];
    if (line) { if (c === '\n' || c === '\r') { line = false; output += c; } continue; }
    if (block) { if (c === '*' && next === '/') { block = false; i++; } else if (c === '\n' || c === '\r') { output += c; } continue; }
    if (quoted) { output += c; if (escaped) { escaped = false; } else if (c === '\\') { escaped = true; } else if (c === '"') { quoted = false; } continue; }
    if (c === '"') { quoted = true; output += c; }
    else if (c === '/' && next === '/') { line = true; output += ' '; i++; }
    else if (c === '/' && next === '*') { block = true; output += ' '; i++; }
    else { output += c; }
  }
  if (block) { throw new Error('Viewer 配置注释未结束'); }
  let clean = ''; quoted = false; escaped = false;
  for (let i = 0; i < output.length; i++) {
    const c = output[i];
    if (quoted) { clean += c; if (escaped) { escaped = false; } else if (c === '\\') { escaped = true; } else if (c === '"') { quoted = false; } continue; }
    if (c === '"') { quoted = true; }
    if (c === ',') { let j = i + 1; while (j < output.length && /\s/.test(output[j])) { j++; } if (output[j] === '}' || output[j] === ']') { continue; } }
    clean += c;
  }
  return clean;
}
export function parseViewerSettings(text: string): ViewerSettings {
  if (text.length > 4 * 1024 * 1024) { throw new Error('Viewer 配置超过 4 MiB'); }
  text = settingsJson(text);
  let settings = JSON.parse(text) as ViewerSettings;
  if (!settings || typeof settings !== 'object') { throw new Error('Viewer 配置无效'); }
  if (settings.version === undefined) {
    const legacy = JSON.parse(text) as LegacySettings;
    if (!legacy.camera) { throw new Error('旧版 Viewer 配置缺少 camera'); }
    const tracks = legacy.animTracks ?? [];
    tracks.forEach((t: ViewerTrack) => {
      if (!t.frameRate) { t.frameRate = 30; t.keyframes.times = t.keyframes.times.map((time: number) => time * 30); }
      if (t.smoothness === undefined) { t.smoothness = 0; }
      t.keyframes.values.fov = new Array<number>(t.keyframes.times.length).fill(legacy.camera.fov || 60);
    });
    settings = { version: 2, cameras: legacy.camera.position && legacy.camera.target ? [{ initial: {
      position: legacy.camera.position, target: legacy.camera.target, fov: legacy.camera.fov || 75 } }] : [],
      animTracks: tracks, startMode: legacy.camera.startAnim === 'animTrack' ? 'animTrack' : 'default', annotations: [],
      background: legacy.background ?? { color: [0, 0, 0] }, tonemapping: 'none', highPrecisionRendering: false,
      postEffectSettings: defaultEffects() };
    // Exact legacy migration defaults differ from the defaults used by newly authored v2 files.
    settings.postEffectSettings.bloom.intensity = 1; settings.postEffectSettings.grading.brightness = 0;
  }
  if (settings.version !== 2 || !Array.isArray(settings.cameras) || settings.cameras.length > 64 ||
    !Array.isArray(settings.animTracks) || settings.animTracks.length > 64 || !['default', 'animTrack', 'annotation'].includes(settings.startMode)) {
    throw new Error('需要有效的 Viewer v1 / v2 配置');
  }
  if ((settings.revealEffect !== undefined && !['radial', 'none'].includes(settings.revealEffect)) ||
    (settings.experienceType !== undefined && !['character', 'scene'].includes(settings.experienceType))) { throw new Error('MetaFlow 揭示配置无效'); }
  settings.annotations = settings.annotations ?? []; settings.background = settings.background ?? { color: [0, 0, 0] };
  settings.tonemapping = settings.tonemapping ?? 'linear'; settings.highPrecisionRendering = settings.highPrecisionRendering ?? false;
  settings.postEffectSettings = settings.postEffectSettings ?? defaultEffects();
  if (!['none', 'linear', 'filmic', 'hejl', 'aces', 'aces2', 'neutral'].includes(settings.tonemapping) ||
    typeof settings.highPrecisionRendering !== 'boolean' || !vector(settings.background.color, 3) ||
    !Array.isArray(settings.annotations) || settings.annotations.length > 1000) { throw new Error('Viewer 显示配置无效'); }
  const fx = settings.postEffectSettings;
  if (!fx.sharpness || !fx.bloom || !fx.grading || !fx.vignette || !fx.fringing ||
    ![fx.sharpness.enabled, fx.bloom.enabled, fx.grading.enabled, fx.vignette.enabled, fx.fringing.enabled].every((v: boolean) => typeof v === 'boolean') ||
    !vector(fx.grading.tint, 3)) { throw new Error('Viewer 效果配置无效'); }
  finite([fx.sharpness.amount, fx.bloom.intensity, fx.bloom.blurLevel, fx.grading.brightness, fx.grading.contrast,
    fx.grading.saturation, fx.vignette.intensity, fx.vignette.inner, fx.vignette.outer, fx.vignette.curvature, fx.fringing.intensity]);
  if (fx.vignette.curvature <= 0 || fx.vignette.outer <= fx.vignette.inner) { throw new Error('暗角曲率必须为正，外缘必须大于内缘'); }
  if ((settings.soundUrl !== undefined && typeof settings.soundUrl !== 'string') ||
    (settings.background.skyboxUrl !== undefined && typeof settings.background.skyboxUrl !== 'string')) { throw new Error('Viewer 资源地址无效'); }
  for (const address of [settings.collisionUrl, settings.voxelUrl, settings.voxelManifestUrl]) { if (address !== undefined && (typeof address !== 'string' || address.length > 4096)) { throw new Error('Viewer 碰撞地址无效'); } }
  settings.cameras.forEach((c: ViewerCamera) => { if (!c) { throw new Error('Viewer 镜头缺失'); } validatePose(c.initial); });
  settings.annotations.forEach((a: ViewerAnnotation) => {
    if (!a || !vector(a.position, 3) || typeof a.title !== 'string' || typeof a.text !== 'string' ||
      a.title.length > 4096 || a.text.length > 65536 || !a.camera) { throw new Error('Viewer 标注无效'); }
    validatePose(a.camera.initial);
  });
  settings.animTracks.forEach((t: ViewerTrack) => {
    if (!t || !Number.isFinite(t.duration) || t.duration <= 0 || !Number.isFinite(t.frameRate) || t.frameRate <= 0 ||
      !Number.isFinite(t.smoothness) || t.smoothness < 0 || t.smoothness > 1 || !['none', 'repeat', 'pingpong'].includes(t.loopMode) ||
      !['step', 'spline'].includes(t.interpolation) || !t.keyframes || !Array.isArray(t.keyframes.times) || !t.keyframes.values) { throw new Error('Viewer 动画参数无效'); }
    const k = t.keyframes; const v = k.values; const n = k.times.length;
    if (n < 1 || n > 10000 || !vector(k.times, n) || !vector(v.position, n * 3) || !vector(v.target, n * 3) || !vector(v.fov, n)) { throw new Error('Viewer 动画关键帧无效'); }
    k.times.forEach((time: number, i: number) => {
      if (time < 0 || time > t.duration * t.frameRate || (i > 0 && time <= k.times[i - 1])) { throw new Error('Viewer 动画时间必须递增'); }
      validatePose({ position: v.position.slice(i * 3, i * 3 + 3), target: v.target.slice(i * 3, i * 3 + 3), fov: v.fov[i] });
    });
  });
  return settings;
}
export function framePose(bounds: number[], fov: number = 85): ViewerPose {
  const d = bounds[3] / Math.sin(fov * Math.PI / 360), unit = d / 3;
  return { position: [bounds[0] + 2 * unit, bounds[1] + unit, bounds[2] + 2 * unit], target: bounds.slice(0, 3), fov };
}
// Camera and annotations are already Viewer world coordinates: never apply model Rz twice.
export function normalizedPose(p: ViewerPose, bounds: number[]): number[] {
  validatePose(p);
  if (!vector(bounds, 4) || bounds[3] <= 0) { throw new Error('场景边界无效'); }
  const d = p.position.map((v: number, i: number) => v - p.target[i]); const distance = Math.hypot(...d);
  return [Math.atan2(d[0], d[2]), Math.asin(d[1] / distance), distance / (3 * bounds[3]),
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
  private spline: CubicSpline | undefined = undefined;
  constructor(track: ViewerTrack) {
    this.track = track; const k = track.keyframes; const points: number[] = [];
    k.times.forEach((_time: number, i: number) => { points.push(...k.values.position.slice(i * 3, i * 3 + 3), ...k.values.target.slice(i * 3, i * 3 + 3), k.values.fov[i]); });
    if (k.times.length > 1 && track.interpolation === 'spline') {
      const extra = track.duration === k.times[k.times.length - 1] / track.frameRate ? 1 : 0;
      this.spline = track.loopMode === 'repeat' ?
        CubicSpline.fromPointsLooping((track.duration + extra) * track.frameRate, k.times, points, track.smoothness) : CubicSpline.fromPoints(k.times, points, track.smoothness);
    }
  }
  get duration(): number { return this.track.duration; }
  get loopMode(): string { return this.track.loopMode; }
  cursor(seconds: number): number {
    const t = this.track; let time = Math.max(0, seconds);
    if (t.loopMode === 'repeat') { time %= t.duration; }
    else if (t.loopMode === 'pingpong') { time %= t.duration * 2; if (time > t.duration) { time = 2 * t.duration - time; } }
    else { time = Math.min(time, t.duration); }
    return time;
  }

  sample(seconds: number): ViewerPose {
    const time = this.cursor(seconds) * this.track.frameRate, k = this.track.keyframes;
    if (!this.spline) {
      let index = 0; while (index + 1 < k.times.length && k.times[index + 1] <= time) { index++; }
      return { position: k.values.position.slice(index * 3, index * 3 + 3), target: k.values.target.slice(index * 3, index * 3 + 3), fov: k.values.fov[index] };
    }
    const result: number[] = []; this.spline.evaluate(time, result);
    return { position: result.slice(0, 3), target: result.slice(3, 6), fov: result[6] };
  }
}
