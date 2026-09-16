import { Scene, Camera, EnvironmentBackgroundType } from '@kit.ArkGraphics3D';
import { spatialRender } from '@kit.SpatialReconKit';
import nativeRender from 'libsplat.so';
import { ViewerPose, ViewerSettings } from './ViewerSettings';
export function backgroundRGB(color: number[]): number[] {
  if (color.length !== 3 || !color.every((v: number) => Number.isFinite(v))) { throw new Error('背景颜色必须是三个有限数值'); }
  return color.map((v: number) => Math.max(0, Math.min(1, v)));
}
export interface ViewerAsset { path: string; bounds: number[]; environment?: string; }
export interface ViewerCapabilities { streaming: boolean; picking: boolean; effects: string[]; note: string; }
export interface ViewerBackend {
  readonly capabilities: ViewerCapabilities;
  load(asset: ViewerAsset): Promise<void>;
  environment(path: string, subject: string, bounds: number[]): Promise<void>;
  camera(pose: ViewerPose, bounds: number[]): void;
  background(color: number[]): void;
  effects(settings?: ViewerSettings): void;
  pick(x: number, y: number, bounds: number[]): Promise<number[]>;
  active(value: boolean): void;
  annotations(positions: ArrayBuffer, glyphs: ArrayBuffer): void;
  annotationStyle(visible: boolean, hover: number, pixels: number): void;
  dispose(): void;
}
export class OpenGLBackend implements ViewerBackend {
  effects(settings?: ViewerSettings): void {
    const fx = settings?.postEffectSettings;
    const sharp = fx?.sharpness.enabled ? Math.max(0, Math.min(1, fx.sharpness.amount)) : 0;
    const fringe = fx?.fringing.enabled ? Math.max(0, fx.fringing.intensity) : 0;
    const bloom = fx?.bloom.enabled ?? false, grade = fx?.grading.enabled ?? false, vignette = fx?.vignette.enabled ?? false;
    const mode = Math.max(0, ['none', 'linear', 'filmic', 'hejl', 'aces', 'aces2', 'neutral'].indexOf(settings?.tonemapping ?? 'none'));
    nativeRender.effects([mode, settings?.highPrecisionRendering ? 1 : 0, sharp, fringe, bloom ? 1 : 0,
      fx?.bloom.intensity ?? .1, Math.max(1, Math.min(16, fx?.bloom.blurLevel ?? 2)), grade ? 1 : 0,
      fx?.grading.brightness ?? 1, fx?.grading.contrast ?? 1, fx?.grading.saturation ?? 1,
      fx?.grading.tint[0] ?? 1, fx?.grading.tint[1] ?? 1, fx?.grading.tint[2] ?? 1,
      vignette ? 1 : 0, fx?.vignette.inner ?? .3, fx?.vignette.outer ?? .75, fx?.vignette.curvature ?? 1, fx?.vignette.intensity ?? .5,
      0, 0, sharp > 0 || fringe > 0 || bloom || grade || vignette || settings?.highPrecisionRendering ? 1 : 0]);
  }
  background(color: number[]): void { const rgb = backgroundRGB(color); nativeRender.background(rgb[0], rgb[1], rgb[2]); }
  annotations(positions: ArrayBuffer, glyphs: ArrayBuffer): void { nativeRender.annotations(positions, glyphs); }
  annotationStyle(visible: boolean, hover: number, pixels: number): void { nativeRender.annotationStyle(visible, hover, pixels); }
  readonly capabilities: ViewerCapabilities = { streaming: true, picking: true, effects: ['sharpness', 'bloom', 'grading', 'vignette', 'fringing', 'tonemapping', 'rgba16f', 'skybox-equirect'], note: '单体 PLY/SOG SH0–3 · 流式 SH0 · OpenGL ES' };
  async load(asset: ViewerAsset): Promise<void> {
    if (asset.environment) { nativeRender.chunks([asset.path, asset.environment], asset.bounds); }
    else { nativeRender.load(asset.path); }
  }
  async environment(path: string, subject: string, bounds: number[]): Promise<void> { nativeRender.chunks([subject, path], bounds); }
  camera(pose: ViewerPose, bounds: number[]): void {
    const dx = pose.position[0] - pose.target[0], dy = pose.position[1] - pose.target[1], dz = pose.position[2] - pose.target[2];
    const distance = Math.hypot(dx, dy, dz);
    nativeRender.camera(Math.atan2(dx, dz), Math.asin(dy / distance), distance / (3 * bounds[3]),
      (pose.position[0] - bounds[0]) / bounds[3], (pose.position[1] - bounds[1]) / bounds[3], (pose.position[2] - bounds[2]) / bounds[3], 1, pose.fov);
  }
  async pick(x: number, y: number, _bounds: number[]): Promise<number[]> { return nativeRender.pick(x, y); }
  active(value: boolean): void { nativeRender.setActive(value); }
  dispose(): void { this.active(false); }
}
export class HuaweiBackend implements ViewerBackend {
  effects(_settings?: ViewerSettings): void {}
  private clear: number[] = [0, 0, 0];
  background(input: number[]): void {
    const color = backgroundRGB(input);
    if (this.clear.every((v: number, i: number) => v === color[i])) { return; }
    this.clear = color.slice();
    if (this.view) { this.view.clearColor = { r: color[0], g: color[1], b: color[2], a: 1 }; }
  }
  // ArkGraphics3D exposes material sort hints, but no verified GSPlugin base /
  // transparent-world / overlay ordering. Keep common UI markers as fallback.
  annotations(_positions: ArrayBuffer, _glyphs: ArrayBuffer): void {}
  annotationStyle(_visible: boolean, _hover: number, _pixels: number): void {}
  readonly capabilities: ViewerCapabilities = { streaming: false, picking: true, effects: [],
    note: '单体 PLY / SOG · 标注使用二维叠层；深度遮挡、原生瓦片与效果等价性尚未通过' };
  private scene: Scene | undefined = undefined;
  private view: Camera | undefined = undefined;
  private model: spatialRender.GSNode | undefined = undefined;
  private generation: number = 0;
  private enabled: boolean = true;
  private changed: (scene: Scene | undefined) => void;
  constructor(changed: (scene: Scene | undefined) => void) { this.changed = changed; }
  async load(asset: ViewerAsset): Promise<void> {
    const request = ++this.generation;
    this.changed(undefined); const old = this.scene; this.scene = undefined; this.view = undefined;
    await new Promise<void>((resolve) => { setTimeout(resolve, 32); }); old?.destroy(); this.model = undefined;
    if (request !== this.generation) { return; }
    const context = Scene.getDefaultRenderContext();
    if (!context || !await context.loadPlugin(spatialRender.GSPlugin.PLUGIN_ID)) { throw new Error('此设备的 SpatialRender 插件不可用'); }
    let pending: Scene | undefined = await Scene.load();
    try {
      const model = await spatialRender.GSPlugin.loadGSNode(pending, { uri: `file://${asset.path}` }, pending.root ?? undefined);
      model.rotation = { x: 0, y: 0, z: 1, w: 0 };
      if (asset.environment) {
        const companion = await spatialRender.GSPlugin.loadGSNode(pending, { uri: `file://${asset.environment}` }, pending.root ?? undefined);
        companion.rotation = { x: 0, y: 0, z: 1, w: 0 };
      }
      const camera = await pending.getResourceFactory().createCamera({ name: 'nextnewsViewerCamera' });
      camera.fov = 85 * Math.PI / 180; camera.nearPlane = asset.bounds[3] * .001; camera.farPlane = asset.bounds[3] * 100;
      camera.clearColor = { r: this.clear[0], g: this.clear[1], b: this.clear[2], a: 1 }; camera.postProcess = null;
      pending.environment.backgroundType = EnvironmentBackgroundType.BACKGROUND_NONE;
      if (request !== this.generation) { return; }
      this.scene = pending; this.model = model; this.view = camera; camera.enabled = this.enabled;
      this.changed(pending); pending = undefined;
    } finally { pending?.destroy(); }
  }
  async environment(path: string, _subject: string, _bounds: number[]): Promise<void> {
    const scene = this.scene, request = this.generation;
    if (!scene) { throw new Error('华为主体场景尚未就绪'); }
    const companion = await spatialRender.GSPlugin.loadGSNode(scene, { uri: `file://${path}` }, scene.root ?? undefined);
    if (request !== this.generation || scene !== this.scene) { return; }
    companion.rotation = { x: 0, y: 0, z: 1, w: 0 };
  }
  camera(pose: ViewerPose, _bounds: number[]): void {
    if (!this.view) { return; }
    const dx = pose.position[0] - pose.target[0], dy = pose.position[1] - pose.target[1], dz = pose.position[2] - pose.target[2];
    const yaw = Math.atan2(dx, dz), pitch = Math.asin(dy / Math.hypot(dx, dy, dz));
    const sh = Math.sin(yaw / 2), ch = Math.cos(yaw / 2), sp = Math.sin(-pitch / 2), cp = Math.cos(-pitch / 2);
    this.view.position = { x: pose.position[0], y: pose.position[1], z: pose.position[2] };
    this.view.rotation = { x: ch * sp, y: sh * cp, z: -sh * sp, w: ch * cp }; this.view.fov = pose.fov * Math.PI / 180;
  }
  async pick(x: number, y: number, bounds: number[]): Promise<number[]> {
    if (!this.view) { return []; }
    const hits = await this.view.raycast({ x: x * 2 - 1, y: 1 - y * 2 }, {});
    if (!hits.length) { return []; }
    const p = hits[0].hitPosition;
    return [(p.x - bounds[0]) / bounds[3], (p.y - bounds[1]) / bounds[3], (p.z - bounds[2]) / bounds[3]];
  }
  active(value: boolean): void { this.enabled = value; if (this.view) { this.view.enabled = value; } }
  dispose(): void { this.generation++; this.changed(undefined); this.view = undefined; this.scene?.destroy(); this.scene = undefined; this.model = undefined; }
}
