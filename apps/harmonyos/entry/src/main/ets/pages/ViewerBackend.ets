import { Scene, Camera, EnvironmentBackgroundType } from '@kit.ArkGraphics3D';
import { spatialRender } from '@kit.SpatialReconKit';
import nativeRender from 'libsplat.so';
import { ViewerPose } from './ViewerSettings';
export interface ViewerAsset { path: string; bounds: number[]; }
export interface ViewerCapabilities { streaming: boolean; picking: boolean; postEffects: boolean; note: string; }
export interface ViewerBackend {
  readonly capabilities: ViewerCapabilities;
  load(asset: ViewerAsset): Promise<void>;
  camera(pose: ViewerPose, bounds: number[]): void;
  pick(x: number, y: number, bounds: number[]): Promise<number[]>;
  active(value: boolean): void;
  dispose(): void;
}
export class OpenGLBackend implements ViewerBackend {
  readonly capabilities: ViewerCapabilities = { streaming: true, picking: true, postEffects: false, note: 'SH0 · 原生 OpenGL ES' };
  async load(asset: ViewerAsset): Promise<void> { nativeRender.load(asset.path); }
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
  readonly capabilities: ViewerCapabilities = { streaming: false, picking: true, postEffects: false,
    note: '单体 PLY / SOG · 原生瓦片与效果等价性尚未通过；Remy 编辑/导出仍受阻' };
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
      const camera = await pending.getResourceFactory().createCamera({ name: 'nextnewsViewerCamera' });
      camera.fov = 85 * Math.PI / 180; camera.nearPlane = asset.bounds[3] * .001; camera.farPlane = asset.bounds[3] * 100;
      camera.clearColor = { r: .035, g: .045, b: .065, a: 1 }; camera.postProcess = null;
      pending.environment.backgroundType = EnvironmentBackgroundType.BACKGROUND_NONE;
      if (request !== this.generation) { return; }
      this.scene = pending; this.model = model; this.view = camera; camera.enabled = this.enabled;
      this.changed(pending); pending = undefined;
    } finally { pending?.destroy(); }
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
