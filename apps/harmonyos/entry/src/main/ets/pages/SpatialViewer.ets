import { Scene, Camera } from '@kit.ArkGraphics3D';
import { spatialRender } from '@kit.SpatialReconKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { SAMPLE_BOUNDS, SampleBounds } from './ModelBounds';

@Entry
@Component
struct SpatialViewer {
  @State scene: Scene | undefined = undefined;
  @State message: string = '检查 SpatialRender 能力';
  @State busy: boolean = false;
  @State panMode: boolean = false;
  private camera: Camera | undefined = undefined;
  private generation: number = 0;
  private visible: boolean = true;
  private bounds: SampleBounds = SAMPLE_BOUNDS[0];
  private yaw: number = 0;
  private pitch: number = 0;
  private zoom: number = 1;
  private panX: number = 0;
  private panY: number = 0;
  private lastX: number = 0;
  private lastY: number = 0;
  private pinchStart: number = 1;

  aboutToAppear(): void { this.visible = true; this.load(0); }
  aboutToDisappear(): void {
    this.visible = false; this.generation++;
    this.camera = undefined;
    this.scene?.destroy(); this.scene = undefined;
  }
  onPageHide(): void { if (this.camera) { this.camera.enabled = false; } }
  onPageShow(): void { if (this.camera) { this.camera.enabled = true; } }
  private reset(): void {
    this.yaw = 0; this.pitch = 0; this.zoom = 1; this.panX = 0; this.panY = 0;
    this.updateCamera();
  }
  private updateCamera(): void {
    if (!this.camera) { return; }
    const sy = Math.sin(this.yaw); const cy = Math.cos(this.yaw);
    const sp = Math.sin(this.pitch); const cp = Math.cos(this.pitch);
    const r = this.bounds.radius; const d = r * 3 * this.zoom;
    this.camera.position = {
      x: this.bounds.center[0] + sy * cp * d + (cy * this.panX - sy * sp * this.panY) * r,
      y: this.bounds.center[1] + sp * d + cp * this.panY * r,
      z: this.bounds.center[2] + cy * cp * d + (-sy * this.panX - cy * sp * this.panY) * r
    };
    const hs = Math.sin(this.yaw / 2); const hc = Math.cos(this.yaw / 2);
    const ps = Math.sin(-this.pitch / 2); const pc = Math.cos(-this.pitch / 2);
    this.camera.rotation = { x: hc * ps, y: hs * pc, z: -hs * ps, w: hc * pc };
  }
  private async load(index: number): Promise<void> {
    if (this.busy) { return; }
    if (!canIUse('SystemCapability.Graphics.SpatialRender')) {
      this.message = '此设备未声明 SpatialRender 能力，请返回 C++ 查看器或使用受支持真机。'; return;
    }
    this.busy = true; this.message = '正在加载华为原生 3DGS 插件与模型';
    const request = ++this.generation; const start = Date.now();
    let pending: Scene | undefined = undefined;
    let stage: string = '初始化渲染上下文';
    try {
      const context = Scene.getDefaultRenderContext();
      if (!context || !await context.loadPlugin(spatialRender.GSPlugin.PLUGIN_ID)) {
        throw new Error('SpatialRender 插件不可用');
      }
      stage = '创建 ArkGraphics3D 场景';
      pending = await Scene.load();
      stage = '加载 3DGS 模型';
      await spatialRender.GSPlugin.loadGSNode(pending,
        { uri: `OhosRawFile://models/${SAMPLE_BOUNDS[index].file}` }, pending.root ?? undefined);
      const camera = await pending.getResourceFactory().createCamera({ name: 'comparisonCamera' });
      if (!this.visible || request !== this.generation) { return; }
      this.bounds = SAMPLE_BOUNDS[index];
      camera.fov = Math.PI / 4; camera.nearPlane = this.bounds.radius * 0.001;
      camera.farPlane = this.bounds.radius * 100; camera.enabled = true;
      const previous = this.scene;
      this.camera = camera; this.reset(); this.scene = pending; pending = undefined;
      previous?.destroy();
      this.message = `${this.bounds.file} · 原生加载 ${Date.now() - start} ms · SH0 输入\n图像、颜色和设备兼容性仍需实测确认`;
    } catch (error) {
      if (this.visible && request === this.generation) {
        const detail = (error as BusinessError)?.message ?? String(error);
        this.message = `${stage}失败：${detail || '引擎未提供错误详情'}。可返回 C++ 查看器。`;
        console.error(`SpatialViewer ${stage}: ${detail}`);
      }
    } finally {
      pending?.destroy();
      if (request === this.generation) { this.busy = false; }
    }
  }
  build() {
    Column({ space: 10 }) {
      Row({ space: 10 }) {
        Button('返回').onClick(() => this.getUIContext().getRouter().back());
        Text('Spatial Recon 原生对照').fontSize(20).fontColor('#EDF6FC');
      }.width('100%');
      Text(this.message).fontSize(13).fontColor('#AFC6D8').width('100%');
      if (this.scene) {
        Component3D({ scene: this.scene, modelType: ModelType.SURFACE })
          .width('100%').layoutWeight(1)
          .gesture(GestureGroup(GestureMode.Exclusive,
            PinchGesture({ fingers: 2 }).onActionStart(() => { this.pinchStart = this.zoom; })
              .onActionUpdate((event: GestureEvent) => {
                this.zoom = Math.max(0.05, Math.min(20, this.pinchStart / Math.max(event.scale, 0.01)));
                this.updateCamera();
              }),
            PanGesture({ fingers: 1 }).onActionStart(() => { this.lastX = 0; this.lastY = 0; })
              .onActionUpdate((event: GestureEvent) => {
                const dx = event.offsetX - this.lastX; const dy = event.offsetY - this.lastY;
                this.lastX = event.offsetX; this.lastY = event.offsetY;
                if (this.panMode) { this.panX -= dx * 0.003; this.panY += dy * 0.003; }
                else { this.yaw += dx * 0.008; this.pitch = Math.max(-1.5, Math.min(1.5, this.pitch + dy * 0.008)); }
                this.updateCamera();
              })
          ));
      } else {
        Blank().layoutWeight(1);
      }
      Row({ space: 8 }) {
        Button('Tripo').enabled(!this.busy).onClick(() => this.load(0));
        Button('公开样例').enabled(!this.busy).onClick(() => this.load(1));
        Button('转换产物').enabled(!this.busy).onClick(() => this.load(2));
      }
      Row({ space: 8 }) {
        Button(this.panMode ? '拖动：平移' : '拖动：旋转').onClick(() => { this.panMode = !this.panMode; });
        Button('复位').onClick(() => this.reset());
      }
    }.padding(16).width('100%').height('100%').backgroundColor('#07131F');
  }
}
