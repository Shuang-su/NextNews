import { KeyCode } from '@kit.InputKit';
import { common } from '@kit.AbilityKit';
import { fileIo as fs, picker } from '@kit.CoreFileKit';
import { BusinessError } from '@kit.BasicServicesKit';
import nativeRender from 'libsplat.so';
import { StreamSession } from './StreamSession';
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
  @State flyMode: boolean = false;
  private pickGeneration: number = 0;
  private mouseButton: MouseButton = MouseButton.None;
  private mouseX: number = 0;
  private mouseY: number = 0;
  @State streamUrl: string = 'http://127.0.0.1:8768/scene.json';
  @State streamMessage: string = '';
  @State quality: number = 2000000;
  @State fullLoad: boolean = false;
  @State streamPanel: boolean = false;
  private stream: StreamSession | undefined = undefined;
  private timer: number = -1;
  private generation: number = 0;
  private visible: boolean = true;
  @State toolsVisible: boolean = true;
  @State helpVisible: boolean = false;
  @State diagnosticsVisible: boolean = false;
  private controls: OrbitControls = new OrbitControls();
  private motionTimer: number = -1;
  private motionTime: number = 0;
  @State benchmarkResult: string = '';
  private benchmarkStart: number = 0;
  private benchmarkFrames: number = 0;
  private benchmarkCount: number = 0;
  private active: boolean = true;

  aboutToAppear(): void {
    this.visible = true;
    this.motionTime = Date.now();
    this.motionTimer = setInterval(() => {
      const now = Date.now(); const dt = now - this.motionTime; this.motionTime = now;
      if (this.active && this.benchmarkStart > 0) {
        const elapsed = now - this.benchmarkStart;
        if (elapsed >= 10000) {
          const s = nativeRender.status();
          this.benchmarkResult = `${this.benchmarkCount.toLocaleString()} 个 · ${s.width}×${s.height} · ${(s.frames - this.benchmarkFrames)} 帧 / ${(elapsed / 1000).toFixed(2)} 秒 = ${((s.frames - this.benchmarkFrames) * 1000 / elapsed).toFixed(1)} fps`;
          console.info(`ViewerBenchmark ${this.benchmarkResult}`); this.benchmarkStart = 0;
        } else {
          this.controls.current = [elapsed * .0003, .2, 1, 0, 0, 0]; this.controls.target = this.controls.current.slice(); this.updateCamera();
        }
      } else if (this.active && this.controls.tick(dt)) { this.updateCamera(); }
    }, 16);
    this.timer = setInterval(() => {
      this.stream?.tick(this.controls.current, this.controls.fly);
      const state = nativeRender.status();
      this.loading = state.state === 'loading';
      if (!this.busy && this.localError.length === 0) {
        this.message = state.state === 'ready' ? '单击聚焦 · 双击环绕 / 飞行 · 拖动旋转' : state.message;
      }
      this.graphics = state.graphics;
      this.details = `${state.count.toLocaleString()} 个高斯 · SH0\n加载 ${state.loadMs.toFixed(0)} ms · 排序 ${state.sortMs.toFixed(1)} ms\n单帧 ${state.frameMs.toFixed(1)} ms · 渲染耗时折算 ${state.fps.toFixed(1)} fps\n模型缓冲估算 ${(state.bytes / 1048576).toFixed(1)} MiB\n相机 ${this.controls.current.map((v: number) => v.toFixed(3)).join(",")} · ${this.flyMode ? "fly" : "orbit"}`;
    }, 500);
  }
  aboutToDisappear(): void {
    this.visible = false; this.stopStream();
    this.generation++;
    if (this.timer !== -1) { clearInterval(this.timer); this.timer = -1; }
    if (this.motionTimer !== -1) { clearInterval(this.motionTimer); this.motionTimer = -1; }
    this.controls.cancel();
    nativeRender.setActive(false);
  }
  onPageShow(): void { this.active = true; if (this.stream) { this.stream.active = true; } this.motionTime = Date.now(); nativeRender.setActive(true); }
  onPageHide(): void { this.pickGeneration++; this.benchmarkStart = 0; this.active = false; if (this.stream) { this.stream.active = false; } this.controls.cancel(); nativeRender.setActive(false); }
  private updateCamera(): void {
    const c = this.controls.current;
    nativeRender.camera(c[0], c[1], c[2], c[3], c[4], c[5], this.controls.fly ? 1 : 0);
  }
  private benchmark(): void {
    this.stopStream(); this.reset(); const s = nativeRender.status();
    if (s.state !== 'ready' || !s.count) { return; }
    this.benchmarkFrames = s.frames; this.benchmarkCount = s.count; this.benchmarkStart = Date.now(); this.benchmarkResult = '测量中：固定环绕轨迹，10 秒';
  }
  private toggleMode(): void { this.pickGeneration++; this.flyMode = !this.flyMode; this.controls.setFly(this.flyMode); this.updateCamera(); }
  private async focusAt(x: number, y: number): Promise<void> {
    const request = ++this.pickGeneration;
    try { const point = await nativeRender.pick(x / this.controls.width, y / this.controls.height);
      if (request === this.pickGeneration && this.active) { this.controls.focus(point); this.updateCamera(); }
    } catch (error) { console.warn(`Focus: ${String(error)}`); }
  }
  private reset(): void { this.pickGeneration++; this.benchmarkStart = 0; this.benchmarkResult = '';  this.flyMode = false; this.controls.reset(); this.updateCamera(); }
  private touch(event: TouchEvent): void {
    if (event.source === SourceType.Mouse) { return; }
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
  private mouse(event: MouseEvent): void {
    if (event.action === MouseAction.Press) {
      this.getUIContext().getFocusController().requestFocus('viewer-surface');
      this.mouseButton = event.button; this.mouseX = event.x; this.mouseY = event.y;
    } else if (event.action === MouseAction.Release) {
      console.info(`ViewerMouse release yaw=${this.controls.target[0]} pitch=${this.controls.target[1]}`);
      this.mouseButton = MouseButton.None;
    } else if (event.action === MouseAction.Move && this.mouseButton !== MouseButton.None) {
      const dx = event.x - this.mouseX; const dy = event.y - this.mouseY;
      this.mouseX = event.x; this.mouseY = event.y;
      if (this.mouseButton === MouseButton.Right || this.mouseButton === MouseButton.Middle || this.panMode) {
        this.controls.pan(dx, dy);
      } else { this.controls.rotate(dx, dy); }
    }
  }
  private handleKey(event: KeyEvent): boolean {
    let key = event.keyText.toLowerCase();
    switch (event.keyCode) {
      case KeyCode.KEYCODE_W: key = 'w'; break;
      case KeyCode.KEYCODE_A: key = 'a'; break;
      case KeyCode.KEYCODE_S: key = 's'; break;
      case KeyCode.KEYCODE_D: key = 'd'; break;
      case KeyCode.KEYCODE_Q: key = 'q'; break;
      case KeyCode.KEYCODE_E: key = 'e'; break;
      case KeyCode.KEYCODE_R: key = 'r'; break;
    }
    console.info(`ViewerKey ${event.keyCode} type=${event.type} key=${key}`);
    if (key === 'r' && event.type === KeyType.Down) { this.reset(); return true; }
    if ('wasdqe'.includes(key) && key.length === 1) {
      this.controls.key(key, event.type === KeyType.Down); return true;
    }
    return false;
  }
  private stopStream(): void {
    const old = this.stream; this.stream = undefined; old?.stop();
    if (old) { nativeRender.chunks([], [0, 0, 0, 1]); }
    if (old) { setTimeout(() => { old.dispose(); }, 1000); }
    this.streamMessage = '';
  }
  private async startStream(): Promise<void> {
    this.stopStream(); this.localError = ''; this.reset(); this.title = '流式场景';
    const session = new StreamSession((message: string) => { if (this.stream === session) { this.streamMessage = message; } });
    this.stream = session; session.budget = this.quality;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      await session.open(this.streamUrl, context.cacheDir, this.fullLoad);
      if (this.stream === session) { session.tick(this.controls.current, this.controls.fly); }
    } catch (error) { if (this.stream === session) { this.streamMessage = `流式失败：${String(error)}`; } }
  }
  private async sample(name: string, label: string): Promise<void> {
    if (this.busy) { return; }
    this.stopStream();
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
    if (this.busy) { return; }
    this.stopStream();
    this.localError = '';
    const request = ++this.generation;
    this.busy = true;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const documentPicker = new picker.DocumentViewPicker(context);
      const uris = await documentPicker.select({ maxSelectNumber: 1, fileSuffixFilters: ['3DGS|.ply,.sog'] });
      if (uris.length === 0) { return; }
      const source = await fs.open(uris[0], fs.OpenMode.READ_ONLY);
      const destination = `${context.cacheDir}/imported${uris[0].toLowerCase().endsWith('.sog') ? '.sog' : '.ply'}`;
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
      if (this.streamMessage) { Text(this.streamMessage).fontSize(11).fontColor('#EBC478').width('100%'); }
      Text(this.message).fontSize(13).fontColor('#AFC6D8').width('100%');
      Stack() {
      XComponent({ id: 'nextnews-splat', type: XComponentType.SURFACE, libraryname: 'splat' })
        .width('100%').height('100%')
        .onLoad(() => { nativeRender.setActive(true); if (nativeRender.status().count === 0) { this.sample('tripo.ply', 'Tripo Splat'); } });
      Button('', { type: ButtonType.Normal }).width('100%').height('100%').backgroundColor('#00000000')
        .stateEffect(false).focusOnTouch(true).accessibilityText('3D 模型视窗')
        .onAreaChange((_old: Area, area: Area) => { this.controls.resize(Number(area.width), Number(area.height)); this.updateCamera(); })
        .id('viewer-surface').focusable(true).defaultFocus(true)
        .onTouch((event: TouchEvent) => { this.touch(event); })
        .onMouse((event: MouseEvent) => { this.mouse(event); })
        .onHover((hover: boolean) => { if (!hover) { this.mouseButton = MouseButton.None; } })
        .onBlur(() => { this.controls.cancel(); this.mouseButton = MouseButton.None; })
        .parallelGesture(GestureGroup(GestureMode.Exclusive,
          TapGesture({ count: 2 }).onAction(() => { this.toggleMode(); }),
          TapGesture({ count: 1 }).onAction((event: GestureEvent) => {
            if (event.fingerList.length > 0) { this.focusAt(event.fingerList[0].localX, event.fingerList[0].localY); }
          })
        ))
        .onKeyEvent((event: KeyEvent) => this.handleKey(event))
        .onAxisEvent((event: AxisEvent) => { this.controls.zoom(Math.exp(Math.max(-1, Math.min(1, event.getVerticalAxisValue() * 0.002)))); });
      }.width('100%').layoutWeight(1)
      if (this.toolsVisible) {
      Row({ space: 6 }) {
        Button(this.quality === 1000000 ? '画质：100 万' : this.quality === 2000000 ? '画质：200 万' : '画质：400 万').fontSize(12).onClick(() => {
          this.quality = this.quality === 1000000 ? 2000000 : this.quality === 2000000 ? 4000000 : 1000000;
          if (this.stream) { this.stream.budget = this.quality; }
        });
        Button(this.flyMode ? '模式：飞行' : '模式：环绕').fontSize(12).onClick(() => {
          this.toggleMode();
        });
        if (this.flyMode) {
          Button('前进').fontSize(12).onClick(() => { this.controls.move(0, 0, 0.25); });
          Button('后退').fontSize(12).onClick(() => { this.controls.move(0, 0, -0.25); });
        }
      }
      Row({ space: 6 }) {
        Button(this.panMode ? '拖动：平移' : '拖动：旋转').onClick(() => { this.panMode = !this.panMode; });
        Button('−').onClick(() => { this.controls.zoom(1.2); });
        Button('+').onClick(() => { this.controls.zoom(1 / 1.2); });
        Button('复位').onClick(() => { this.reset(); });
      }
      Row({ space: 8 }) {
        Button('公开样例').enabled(!this.busy).onClick(() => { this.sample('public.ply', '公开样例'); });
        Button('Tripo').enabled(!this.busy).onClick(() => { this.sample('tripo.ply', 'Tripo Splat'); });
        Button('转换样例').enabled(!this.busy).onClick(() => { this.sample('converted.ply', '转换样例'); });
      }
      Button('打开本地 PLY / SOG').width('100%').enabled(!this.busy).onClick(() => { this.importModel(); });
      Row({ space: 8 }) {
        Button('流式').fontSize(12).onClick(() => { this.streamPanel = !this.streamPanel; });
        Button('操作说明').fontSize(12).onClick(() => { this.helpVisible = !this.helpVisible; });
        Button('性能信息').fontSize(12).onClick(() => { this.diagnosticsVisible = !this.diagnosticsVisible; });
      }
      if (this.streamPanel) {
        TextInput({ text: this.streamUrl, placeholder: 'HTTP(S) scene.json' }).selectAll(true).fontColor('#EDF6FC').backgroundColor('#233044').onChange((value: string) => { this.streamUrl = value; });
        Row({ space: 6 }) {
          Button(this.fullLoad ? '加载：全量预载' : '加载：渐进 LOD').fontSize(12).onClick(() => { this.fullLoad = !this.fullLoad; });
          Button('连接流式').fontSize(12).onClick(() => { this.startStream(); });
          Button('重试').fontSize(12).onClick(() => { if (this.stream?.manifest) { this.stream.retry(); } else { this.startStream(); } });
          Button('停止').fontSize(12).onClick(() => { this.stopStream(); });
        }
      }
      if (this.helpVisible) {
        Text('单击：聚焦；双击：环绕 / 飞行\n左键 / 单指拖动：环绕或飞行转头\n右键拖动：平移；滚轮：缩放 / 前进\n飞行：WASD 移动，Q/E 升降，R 复位\n双指拖动：平移；张合：缩放，可同时进行\n拖动模式按钮：单指平移辅助\n复位：回到初始视角；收起工具：扩大画面')
          .fontSize(12).fontColor('#AFC6D8').width('100%');
      }
      if (this.diagnosticsVisible) {
        Button('测量 10 秒').fontSize(12).enabled(this.benchmarkStart === 0).onClick(() => { this.benchmark(); });
        if (this.benchmarkResult) { Text(this.benchmarkResult).fontSize(11).fontColor('#EBC478'); }
        Text(this.details).fontSize(11).fontColor('#99B1C4').width('100%');
        Text(this.graphics).fontSize(10).fontColor('#6F899F').maxLines(2).width('100%');
      }
      }
    }.padding({ left: 16, right: 16, top: 18, bottom: 18 }).width('100%').height('100%').backgroundColor('#091322');
  }
}
