import { common } from '@kit.AbilityKit';
import { fileIo as fs, picker } from '@kit.CoreFileKit';
import { BusinessError } from '@kit.BasicServicesKit';
import nativeRender from 'libsplat.so';
import { OrbitControls, OrbitTouch } from './OrbitControls';

@Entry
@Component
struct Index {
  @State message: string = '准备原生渲染窗口';
  @State details: string = '';
  @State graphics: string = '';
  @State busy: boolean = false;
  @State loading: boolean = false;
  @State localError: string = '';
  @State title: string = 'NextNews · 3DGS';
  @State panMode: boolean = false;
  private timer: number = -1;
  private generation: number = 0;
  private visible: boolean = true;
  @State toolsVisible: boolean = true;
  @State helpVisible: boolean = false;
  @State diagnosticsVisible: boolean = false;
  private controls: OrbitControls = new OrbitControls();
  private motionTimer: number = -1;
  private motionTime: number = 0;
  private active: boolean = true;

  aboutToAppear(): void {
    this.visible = true;
    this.motionTime = Date.now();
    this.motionTimer = setInterval(() => {
      const now = Date.now(); const dt = now - this.motionTime; this.motionTime = now;
      if (this.active && this.controls.tick(dt)) { this.updateCamera(); }
    }, 32);
    this.timer = setInterval(() => {
      const state = nativeRender.status();
      this.loading = state.state === 'loading';
      if (!this.busy && this.localError.length === 0) {
        this.message = state.state === 'ready' ? '单指旋转 · 双指平移 / 缩放' : state.message;
      }
      this.graphics = state.graphics;
      this.details = `${state.count.toLocaleString()} 个高斯 · SH0\n加载 ${state.loadMs.toFixed(0)} ms · 排序 ${state.sortMs.toFixed(1)} ms\n单帧 ${state.frameMs.toFixed(1)} ms · 渲染耗时折算 ${state.fps.toFixed(1)} fps\n模型缓冲估算 ${(state.bytes / 1048576).toFixed(1)} MiB`;
    }, 500);
  }
  aboutToDisappear(): void {
    this.visible = false;
    this.generation++;
    if (this.timer !== -1) { clearInterval(this.timer); this.timer = -1; }
    if (this.motionTimer !== -1) { clearInterval(this.motionTimer); this.motionTimer = -1; }
    this.controls.cancel();
    nativeRender.setActive(false);
  }
  onPageShow(): void { this.active = true; this.motionTime = Date.now(); nativeRender.setActive(true); }
  onPageHide(): void { this.active = false; this.controls.cancel(); nativeRender.setActive(false); }
  private updateCamera(): void {
    const c = this.controls.current;
    nativeRender.camera(c[0], c[1], c[2], c[3], c[4], c[5]);
  }
  private reset(): void { this.controls.reset(); this.updateCamera(); }
  private touch(event: TouchEvent): void {
    if (event.type === TouchType.Cancel) { this.controls.cancel(); return; }
    const points: OrbitTouch[] = [];
    for (const touch of event.touches) {
      if (event.type === TouchType.Up && event.changedTouches.some((ended: TouchObject) => ended.id === touch.id)) {
        continue;
      }
      points.push({ id: touch.id, x: touch.x, y: touch.y });
    }
    this.controls.touch(points, this.panMode);
  }
  private async sample(name: string, label: string): Promise<void> {
    if (this.busy || this.loading) { return; }
    this.localError = '';
    const request = ++this.generation;
    this.busy = true;
    this.message = '正在准备模型';
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const bytes = await context.resourceManager.getRawFileContent(`models/${name}`);
      const path = `${context.cacheDir}/${name}`;
      const file = await fs.open(path, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
      try {
        let offset = 0;
        while (offset < bytes.byteLength) {
          const chunk = bytes.buffer.slice(bytes.byteOffset + offset, bytes.byteOffset + bytes.byteLength);
          const written = await fs.write(file.fd, chunk);
          if (written <= 0) { throw new Error('模型文件写入失败'); }
          offset += written;
        }
      } finally { await fs.close(file); }
      if (this.visible && request === this.generation) {
        this.reset(); this.title = label; this.loading = true; nativeRender.load(path);
      }
    } catch (error) {
      this.localError = (error as BusinessError).message;
      this.message = this.localError;
    } finally { this.busy = false; }
  }
  private async importModel(): Promise<void> {
    if (this.busy || this.loading) { return; }
    this.localError = '';
    const request = ++this.generation;
    this.busy = true;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const documentPicker = new picker.DocumentViewPicker(context);
      const uris = await documentPicker.select({ maxSelectNumber: 1, fileSuffixFilters: ['3DGS PLY|.ply'] });
      if (uris.length === 0) { return; }
      const source = await fs.open(uris[0], fs.OpenMode.READ_ONLY);
      const destination = `${context.cacheDir}/imported.ply`;
      try {
        const stat = await fs.stat(source.fd);
        if (stat.size > 128 * 1024 * 1024) { throw new Error('文件超过 128 MiB，请先在 Mac 上降采样'); }
        await fs.copyFile(source.fd, destination);
      } finally { await fs.close(source); }
      if (this.visible && request === this.generation) {
        this.reset(); this.title = '本地模型'; this.loading = true; nativeRender.load(destination);
      }
    } catch (error) {
      this.localError = (error as BusinessError).message;
      this.message = this.localError;
    } finally { this.busy = false; }
  }
  build() {
    Column({ space: 10 }) {
      Row() {
        Text(this.title).fontSize(23).fontWeight(FontWeight.Bold).fontColor('#EDF6FC').layoutWeight(1);
        Button(this.toolsVisible ? '收起工具' : '显示工具').fontSize(12)
          .onClick(() => { this.toolsVisible = !this.toolsVisible; });
      }.width('100%');
      Text(this.message).fontSize(13).fontColor('#AFC6D8').width('100%');
      XComponent({ id: 'nextnews-splat', type: XComponentType.SURFACE, libraryname: 'splat' })
        .width('100%').layoutWeight(1)
        .onLoad(() => { nativeRender.setActive(true); if (nativeRender.status().count === 0) { this.sample('tripo.ply', 'Tripo Splat'); } })
        .onAreaChange((_old: Area, area: Area) => { this.controls.resize(Number(area.width), Number(area.height)); this.updateCamera(); })
        .onTouch((event: TouchEvent) => { this.touch(event); });
      if (this.toolsVisible) {
      Row({ space: 6 }) {
        Button(this.panMode ? '拖动：平移' : '拖动：旋转').onClick(() => { this.panMode = !this.panMode; });
        Button('−').onClick(() => { this.controls.zoom(1.2); });
        Button('+').onClick(() => { this.controls.zoom(1 / 1.2); });
        Button('复位').onClick(() => { this.reset(); });
      }
      Row({ space: 8 }) {
        Button('公开样例').enabled(!this.busy && !this.loading).onClick(() => { this.sample('public.ply', '公开样例'); });
        Button('Tripo').enabled(!this.busy && !this.loading).onClick(() => { this.sample('tripo.ply', 'Tripo Splat'); });
        Button('转换样例').enabled(!this.busy && !this.loading).onClick(() => { this.sample('converted.ply', '转换样例'); });
      }
      Button('打开本地 PLY').width('100%').enabled(!this.busy && !this.loading).onClick(() => { this.importModel(); });
      Row({ space: 8 }) {
        Button('操作说明').fontSize(12).onClick(() => { this.helpVisible = !this.helpVisible; });
        Button('性能信息').fontSize(12).onClick(() => { this.diagnosticsVisible = !this.diagnosticsVisible; });
      }
      if (this.helpVisible) {
        Text('单指拖动：环绕旋转\n双指拖动：平移；张合：缩放，可同时进行\n拖动模式按钮：单指平移辅助\n复位：回到初始视角；收起工具：扩大画面')
          .fontSize(12).fontColor('#AFC6D8').width('100%');
      }
      if (this.diagnosticsVisible) {
        Text(this.details).fontSize(11).fontColor('#99B1C4').width('100%');
        Text(this.graphics).fontSize(10).fontColor('#6F899F').maxLines(2).width('100%');
      }
      }
    }.padding({ left: 16, right: 16, top: 18, bottom: 18 }).width('100%').height('100%').backgroundColor('#091322');
  }
}
