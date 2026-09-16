import { comparisonSog, ComparisonAsset } from './ComparisonSog';
import { common } from '@kit.AbilityKit';
import { SpatialStream } from './SpatialStream';
import { ViewerProfile } from './ViewerProfile';
import { ViewerPose } from './ViewerSettings';
import { Scene, Camera, EnvironmentBackgroundType } from '@kit.ArkGraphics3D';
import { spatialRender } from '@kit.SpatialReconKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { SAMPLE_BOUNDS, SampleBounds } from './ModelBounds';

@Entry
@Component
struct SpatialViewer {
  private gsNode: spatialRender.GSNode | undefined = undefined;
  private tiled: SpatialStream | undefined = undefined;
  @State tiledUrl: string = 'http://127.0.0.1:8768/huafa.scene.json';
  private profile: ViewerProfile | undefined = undefined;
  private profileTimer: number = -1;
  @State profilePlaying: boolean = false;
  private stopProfile(): void { if (this.profileTimer !== -1) { clearInterval(this.profileTimer); this.profileTimer = -1; } this.profilePlaying = false; }
  private profilePose(p: ViewerPose): void {
    if (!this.camera) { return; }
    const d = p.position.map((v: number, i: number) => v - p.target[i]); const len = Math.hypot(...d);
    const yaw = Math.atan2(d[0], d[2]), pitch = Math.asin(d[1] / len);
    const hs = Math.sin(yaw / 2), hc = Math.cos(yaw / 2), ps = Math.sin(-pitch / 2), pc = Math.cos(-pitch / 2);
    this.yaw = yaw; this.pitch = pitch; this.zoom = len / (this.bounds.radius * 3);
    this.targetOffset = p.target.map((v: number, i: number) => v - this.bounds.center[i]); this.panX = 0; this.panY = 0;
    this.camera.position = { x: p.position[0], y: p.position[1], z: p.position[2] };
    this.camera.rotation = { x: hc * ps, y: hs * pc, z: -hs * ps, w: hc * pc };
    this.camera.fov = p.fov * Math.PI / 180;
  }
  private async profileCamera(play: boolean): Promise<void> {
    if (this.selectedIndex !== 2 && this.selectedIndex !== 3 && this.selectedIndex !== 4 && this.selectedIndex !== 5 || !this.camera) { this.message = '请先加载华发转换产物'; return; }
    try {
      this.stopProfile(); this.stopBenchmark();
      if (!play || !this.profile) { this.profile = await ViewerProfile.load(this.getUIContext().getHostContext() as common.UIAbilityContext, [...this.bounds.center, this.bounds.radius]); }
      this.profilePose(play ? this.profile.animation.sample(this.profile.time) : this.profile.initial()); this.profilePlaying = play;
      let previous = Date.now();
      if (play) { this.profileTimer = setInterval(() => {
        const now = Date.now(); if (this.foreground && this.profile) { this.profilePose(this.profile.advance((now - previous) / 1000)); } previous = now;
      }, 16); }
      if (this.selectedIndex !== 3) { this.message = 'viewer-settings.json · 75° · 默认镜头 / 配置动画'; }
    } catch (error) { this.message = String(error); }
  }
  @State scene: Scene | undefined = undefined;
  @State message: string = '检查 SpatialRender 能力';
  @State busy: boolean = false;
  @State panMode: boolean = false;
  @StorageLink('nextnewsForeground') @Watch('foregroundChanged') foreground: boolean = true;
  @State benchmarkResult: string = '';
  private benchmarkTimer: number = -1;
  private camera: Camera | undefined = undefined;
  private generation: number = 0;
  private selectedIndex: number = 1;
  private visible: boolean = true;
  private bounds: SampleBounds = SAMPLE_BOUNDS[0];
  private yaw: number = 0;
  private pitch: number = 0;
  private zoom: number = 1;
  private targetOffset: number[] = [0, 0, 0];
  private panX: number = 0;
  private panY: number = 0;
  private lastX: number = 0;
  private lastY: number = 0;
  private pinchStart: number = 1;

  aboutToAppear(): void { this.visible = true; this.load(1); }
  aboutToDisappear(): void {
    this.visible = false; this.generation++; this.tiled?.stop(); this.stopBenchmark(); this.stopProfile();
    this.camera = undefined;
    this.scene?.destroy(); this.scene = undefined; this.gsNode = undefined;
    if (!this.busy) { this.tiled?.dispose(); }
  }
  onPageHide(): void { this.stopBenchmark(); if (this.camera) { this.camera.enabled = false; } }
  onPageShow(): void { if (this.camera) { this.camera.enabled = true; } }
  private foregroundChanged(): void {
    if (!this.foreground) {
      this.stopBenchmark();
      if (this.camera) { this.camera.enabled = false; }
    } else if (this.visible && this.scene) {
      // GS resources may be lost when the system recreates the rendering surface.
      this.load(this.selectedIndex, true);
    }
  }
  private stopBenchmark(): void {
    if (this.benchmarkTimer !== -1) { clearInterval(this.benchmarkTimer); this.benchmarkTimer = -1; this.benchmarkResult = '测量已停止'; }
  }
  private benchmark(): void {
    if (!this.camera || this.busy) { return; }
    this.reset(); const start = Date.now();
    this.benchmarkResult = '固定环绕轨迹 · 测量 10 秒';
    console.info(`SpatialBenchmark start file=${this.bounds.file}`);
    this.benchmarkTimer = setInterval(() => {
      const elapsed = Date.now() - start;
      if (elapsed >= 10000) {
        this.stopBenchmark(); this.benchmarkResult = '轨迹完成 · 帧率由系统采集';
        console.info(`SpatialBenchmark complete elapsedMs=${elapsed}`); return;
      }
      this.yaw = elapsed * .0003; this.pitch = .2; this.zoom = 1;
      this.updateCamera();
    }, 16);
  }
  private reset(): void {
    this.stopProfile(); if (this.camera) { this.camera.fov = Math.PI / 4; }
    this.stopBenchmark(); this.benchmarkResult = '';
    this.targetOffset = [0, 0, 0]; this.yaw = 0; this.pitch = 0; this.zoom = 1; this.panX = 0; this.panY = 0;
    this.updateCamera();
  }
  private updateCamera(): void {
    if (!this.camera) { return; }
    const sy = Math.sin(this.yaw); const cy = Math.cos(this.yaw);
    const sp = Math.sin(this.pitch); const cp = Math.cos(this.pitch);
    const r = this.bounds.radius; const d = r * 3 * this.zoom;
    this.camera.position = {
      x: this.bounds.center[0] + this.targetOffset[0] + sy * cp * d + (cy * this.panX - sy * sp * this.panY) * r,
      y: this.bounds.center[1] + this.targetOffset[1] + sp * d + cp * this.panY * r,
      z: this.bounds.center[2] + this.targetOffset[2] + cy * cp * d + (-sy * this.panX - cy * sp * this.panY) * r
    };
    const hs = Math.sin(this.yaw / 2); const hc = Math.cos(this.yaw / 2);
    const ps = Math.sin(-this.pitch / 2); const pc = Math.cos(-this.pitch / 2);
    this.camera.rotation = { x: hc * ps, y: hs * pc, z: -hs * ps, w: hc * pc };
  }
  private async load(index: number, preserveView: boolean = false): Promise<void> {
    if (this.busy) { return; }
    if (!canIUse('SystemCapability.Graphics.SpatialRender')) {
      this.message = '此设备未声明 SpatialRender 能力，请返回 C++ 查看器或使用受支持真机。'; return;
    }
    const oldStream = this.tiled; oldStream?.stop(); this.tiled = undefined; this.stopProfile(); this.stopBenchmark(); this.benchmarkResult = ''; this.busy = true; this.message = '正在加载华为原生 3DGS 插件与模型';
    const view = [this.yaw, this.pitch, this.zoom, this.panX, this.panY, ...this.targetOffset, this.camera?.fov ?? Math.PI / 4];
    const request = ++this.generation; const start = Date.now();
    let pending: Scene | undefined = undefined;
    let stage: string = '初始化渲染上下文';
    try {
      // Component3D's Scene option is construction-time state: detach before replacing it.
      const oldScene = this.scene; this.scene = undefined; this.camera = undefined;
      await new Promise<void>((resolve) => { setTimeout(resolve, 32); });
      oldScene?.destroy(); this.gsNode = undefined; await oldStream?.dispose();
      if (!this.visible || request !== this.generation) { return; }
      const context = Scene.getDefaultRenderContext();
      console.info(`SpatialViewer contextAvailable=${context !== null}`);
      if (!context || !await context.loadPlugin(spatialRender.GSPlugin.PLUGIN_ID)) {
        throw new Error('SpatialRender 插件不可用');
      }
      console.info(`SpatialViewer pluginLoaded elapsedMs=${Date.now() - start}`);
      stage = '创建 ArkGraphics3D 场景';
      pending = await Scene.load();
      console.info(`SpatialViewer sceneCreated elapsedMs=${Date.now() - start}`);
      stage = '加载 3DGS 模型';
      let asset: ComparisonAsset | undefined = undefined;
      if (index === 4 || index === 5) { asset = await comparisonSog(this.getUIContext().getHostContext() as common.UIAbilityContext, index === 5); }
      if (index !== 3) {
        const model = await spatialRender.GSPlugin.loadGSNode(pending,
          { uri: asset ? `file://${asset.path}` : `OhosRawFile://models/${SAMPLE_BOUNDS[index].file}` }, pending.root ?? undefined);
        model.rotation = { x: 0, y: 0, z: 1, w: 0 }; this.gsNode = model;
      }
      stage = '创建相机';
      const camera = await pending.getResourceFactory().createCamera({ name: 'comparisonCamera' });
      if (!this.visible || request !== this.generation) { return; }
      const source = SAMPLE_BOUNDS[index >= 3 ? 2 : index];
      this.bounds = { file: source.file, center: [-source.center[0], -source.center[1], source.center[2]], radius: source.radius };
      if (asset) { this.bounds = { file: index === 5 ? 'comparison-sh0.ply' : 'chunk-0008.sog', center: asset.bounds.slice(0, 3), radius: asset.bounds[3] }; }
      camera.fov = Math.PI / 4; camera.nearPlane = this.bounds.radius * 0.001;
      camera.farPlane = this.bounds.radius * 100; camera.enabled = this.foreground;
      pending.environment.backgroundType = EnvironmentBackgroundType.BACKGROUND_NONE;
      camera.postProcess = null;
      camera.clearColor = { r: .035, g: .045, b: .065, a: 1 };
      this.camera = camera; this.reset(); this.selectedIndex = index;
      if (preserveView) {
        this.yaw = view[0]; this.pitch = view[1]; this.zoom = view[2];
        this.panX = view[3]; this.panY = view[4]; this.targetOffset = view.slice(5, 8); camera.fov = view[8]; this.updateCamera();
      }
      if (index === 3) {
        stage = '加载原生分块清单';
        const adapter = new SpatialStream((message: string) => { if (request === this.generation) { this.message = message; } });
        this.tiled = adapter;
        await adapter.open(this.tiledUrl, this.getUIContext().getHostContext() as common.UIAbilityContext, pending, camera);
        await this.profileCamera(false);
      }
      if (index === 4 || index === 5) { await this.profileCamera(false); }
      this.scene = pending; pending = undefined;
      if (index !== 3) { this.message = `${this.bounds.file} · 加载接口 ${Date.now() - start} ms · SH0 输入\nSpatialReconKit / ArkGraphics3D · Rz 180°`; }
    } catch (error) {
      if (this.visible && request === this.generation) {
        const detail = (error as BusinessError)?.message ?? String(error);
        this.message = `${stage}失败：${detail || '引擎未提供错误详情'}。可返回 C++ 查看器。`;
        console.error(`SpatialViewer ${stage}: code=${(error as BusinessError)?.code ?? -1} ${detail}`);
      }
    } finally {
      if (pending) { pending.destroy(); this.gsNode = undefined; }
      if (!this.visible) { this.tiled?.dispose(); }
      if (request === this.generation) { this.busy = false; }
    }
  }
  build() {
    Stack({ alignContent: Alignment.TopStart }) {
      if (this.scene) {
        Component3D({ scene: this.scene, modelType: ModelType.SURFACE })
          .width('100%').height('100%')
          .gesture(GestureGroup(GestureMode.Exclusive,
            PinchGesture({ fingers: 2 }).onActionStart(() => { this.stopProfile(); this.stopBenchmark(); this.pinchStart = this.zoom; })
              .onActionUpdate((event: GestureEvent) => {
                this.zoom = Math.max(0.05, Math.min(20, this.pinchStart / Math.max(event.scale, 0.01)));
                this.updateCamera();
              }),
            PanGesture({ fingers: 1 }).onActionStart(() => { this.stopProfile(); this.stopBenchmark(); this.lastX = 0; this.lastY = 0; })
              .onActionUpdate((event: GestureEvent) => {
                const dx = event.offsetX - this.lastX; const dy = event.offsetY - this.lastY;
                this.lastX = event.offsetX; this.lastY = event.offsetY;
                if (this.panMode) { this.panX -= dx * 0.003; this.panY += dy * 0.003; }
                else { this.yaw += dx * 0.008; this.pitch = Math.max(-1.5, Math.min(1.5, this.pitch + dy * 0.008)); }
                this.updateCamera();
              })
          ));
      } else {
        Blank().width('100%').height('100%');
      }
      Column({ space: 8 }) {
        Row({ space: 10 }) {
          Button('返回').onClick(() => this.getUIContext().getRouter().back());
        Text('Spatial Recon 原生对照').fontSize(20).fontColor('#EDF6FC');
      }.width('100%');
      Text(this.message).fontSize(13).fontColor('#AFC6D8').width('100%');

      }.padding(12).width('100%').backgroundColor('#BB07131F');
      Column({ space: 8 }) {
      Row({ space: 8 }) {
        Button('Tripo').enabled(!this.busy).onClick(() => this.load(0));
        Button('公开样例').enabled(!this.busy).onClick(() => this.load(1));
        Button('转换产物').enabled(!this.busy).onClick(() => this.load(2));
      }
      Row({ space: 8 }) {
        Button(this.panMode ? '拖动：平移' : '拖动：旋转').onClick(() => { this.panMode = !this.panMode; });
        Button('复位').onClick(() => this.reset());
        Button('测量 10 秒').enabled(!this.busy).onClick(() => this.benchmark());
      }
        Row({ space: 6 }) {
          Button('配置默认镜头').fontSize(12).onClick(() => this.profileCamera(false));
          Button(this.profilePlaying ? '暂停配置动画' : '播放配置动画').fontSize(12).onClick(() => { if (this.profilePlaying) { this.stopProfile(); } else { this.profileCamera(true); } });
        }
        TextInput({ text: this.tiledUrl }).fontSize(12).fontColor('#EDF6FC').backgroundColor('#28354A').onChange((url: string) => { this.tiledUrl = url; });
        Row({ space: 6 }) {
          Button('同源 SOG · loadGSNode').fontSize(12).enabled(!this.busy).onClick(() => this.load(4));
          Button('同源 PLY').fontSize(12).enabled(!this.busy).onClick(() => this.load(5));
        }
        Button('连接原生流式').enabled(!this.busy).onClick(() => this.load(3));
        Text(this.benchmarkResult).fontSize(12).fontColor('#EBC478');
      }.padding(12).width('100%').backgroundColor('#BB07131F').position({ x: 0, y: '100%' }).translate({ y: '-100%' });
    }.width('100%').height('100%').backgroundColor('#07131F');
  }
}
