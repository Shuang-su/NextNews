import { KeyCode } from '@kit.InputKit';
import { common } from '@kit.AbilityKit';
import { fileIo as fs, picker } from '@kit.CoreFileKit';
import { BusinessError } from '@kit.BasicServicesKit';
import nativeRender from 'libsplat.so';
import { ViewerProfile } from './ViewerProfile';
import { ViewerPose, normalizedPose } from './ViewerSettings';
import { SAMPLE_BOUNDS } from './ModelBounds';
import { StreamSession } from './StreamSession';
import { OrbitControls, OrbitTouch } from './OrbitControls';

@Component
struct ViewerTool {
  @Prop icon: Resource = $r('app.media.viewer_help');
  @Prop label: string = '';
  @Prop selected: boolean = false;
  action: () => void = () => {};
  onHoverChange: (label: string) => void = () => {};
  onKey: (event: KeyEvent) => boolean = () => false;
  build() {
    Button({ type: ButtonType.Normal }) {
      Image(this.icon).width(22).height(22).objectFit(ImageFit.Contain)
    }.width(42).height(42).borderRadius(12).backgroundColor(this.selected ? '#3969C6' : '#DD1C283B')
      .accessibilityText(this.label).id(this.label).onClick(() => {
        this.action(); setTimeout(() => { this.getUIContext().getFocusController().requestFocus('viewer-surface'); }, 0);
      })
      .onKeyEvent((event: KeyEvent) => this.onKey(event))
      .onHover((hover: boolean) => { this.onHoverChange(hover ? this.label : ''); })
  }
}

@Entry
@Component
struct Index {
  private profile: ViewerProfile | undefined = undefined;
  @State profilePlaying: boolean = false;
  private applyProfilePose(pose: ViewerPose): void {
    const sample = SAMPLE_BOUNDS[2];
    const bounds = this.stream?.manifest?.bounds ?? [-sample.center[0], -sample.center[1], sample.center[2], sample.radius];
    this.controls.current = normalizedPose(pose, bounds); this.controls.target = this.controls.current.slice();
    this.controls.fly = true; this.flyMode = true; this.controls.fov = pose.fov; this.updateCamera();
  }
  private async profileCamera(play: boolean): Promise<void> {
    if (this.title !== '转换样例' && this.title !== '流式场景') { this.localError = '请先加载华发转换样例或流式场景'; return; }
    try {
      if (!play || !this.profile) { this.profile = await ViewerProfile.load(this.getUIContext().getHostContext() as common.UIAbilityContext); }
      this.autoOrbit = false; this.benchmarkStart = 0; this.profilePlaying = play;
      this.applyProfilePose(play ? this.profile.animation.sample(this.profile.time) : this.profile.initial());
    } catch (error) { this.localError = String(error); }
  }
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
  @State settingsVisible: boolean = false;
  @State modelsVisible: boolean = false;
  @State autoOrbit: boolean = false;
  @State flySpeed: number = 1;
  @State gpuOptimized: boolean = true;
  @State hint: string = '';
  @State joyX: number = 0;
  @State joyY: number = 0;
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
      if (this.active && this.profilePlaying && this.profile) {
        this.applyProfilePose(this.profile.advance(dt / 1000));
      } else if (this.active && this.benchmarkStart > 0) {
        const elapsed = now - this.benchmarkStart;
        if (elapsed >= 10000) {
          const s = nativeRender.status();
          this.benchmarkResult = `${this.benchmarkCount.toLocaleString()} 个 · ${s.width}×${s.height} · ${(s.frames - this.benchmarkFrames)} 帧 / ${(elapsed / 1000).toFixed(2)} 秒 = ${((s.frames - this.benchmarkFrames) * 1000 / elapsed).toFixed(1)} fps`;
          console.info(`ViewerBenchmark ${this.benchmarkResult}`); this.benchmarkStart = 0;
        } else {
          this.controls.current = [elapsed * .0003, .2, 1, 0, 0, 0]; this.controls.target = this.controls.current.slice(); this.updateCamera();
        }
      } else if (this.active) {
        if (this.autoOrbit && !this.flyMode) { this.controls.target[0] += Math.min(dt, 64) * .0002; }
        if (this.flyMode && (this.joyX !== 0 || this.joyY !== 0)) {
          const speed = Math.min(dt, 64) * .0006 * this.flySpeed;
          this.controls.move(this.joyX * speed, 0, -this.joyY * speed);
        }
        if (this.controls.tick(dt)) { this.updateCamera(); }
      }
    }, 16);
    this.timer = setInterval(() => {
      this.stream?.tick(this.controls.current, this.controls.fly, this.controls.fov, this.controls.width / Math.max(1, this.controls.height));
      const state = nativeRender.status();
      this.loading = state.state === 'loading';
      if (this.stream) { console.info(`StreamFrame state=${state.state} frames=${state.frames} count=${state.count} loadMs=${state.loadMs.toFixed(0)} frameMs=${state.frameMs.toFixed(1)} uploadMs=${state.uploadMs.toFixed(1)} uploadedRows=${state.uploadedRows} reusedRows=${state.reusedRows} decodedFiles=${state.decodedFiles} subsetHits=${state.subsetHits}`); }
      if (!this.busy && this.localError.length === 0) {
        this.message = state.state === 'ready' ? '单击聚焦 · 双击环绕 / 飞行 · 拖动旋转' : state.message;
      }
      this.graphics = state.graphics;
      this.details = `${state.count.toLocaleString()} 个高斯 · SH0\n加载 ${state.loadMs.toFixed(0)} ms · 排序 ${state.sortMs.toFixed(1)} ms\n单帧 ${state.frameMs.toFixed(1)} ms · 渲染耗时折算 ${state.fps.toFixed(1)} fps\n最近上传批次 ${state.uploadMs.toFixed(1)} ms · 上传/复用 ${state.uploadedRows}/${state.reusedRows} 行\nGPU ${state.gpuMs < 0 ? '计时不可用' : state.gpuMs.toFixed(2) + ' ms'}\n模型缓冲估算 ${(state.bytes / 1048576).toFixed(1)} MiB\n相机 ${this.controls.current.map((v: number) => v.toFixed(3)).join(",")} · ${this.flyMode ? "fly" : "orbit"}`;
    }, 500);
  }
  aboutToDisappear(): void {
    this.visible = false; this.stopStream();
    this.generation++;
    if (this.timer !== -1) { clearInterval(this.timer); this.timer = -1; }
    if (this.motionTimer !== -1) { clearInterval(this.motionTimer); this.motionTimer = -1; }
    this.controls.cancel(); this.joyX = 0; this.joyY = 0;
    nativeRender.setActive(false);
  }
  onPageShow(): void { this.active = true; if (this.stream) { this.stream.active = true; } this.motionTime = Date.now(); nativeRender.setActive(true); }
  onPageHide(): void { this.pickGeneration++; this.benchmarkStart = 0; this.active = false; this.joyX = 0; this.joyY = 0; if (this.stream) { this.stream.active = false; } this.controls.cancel(); nativeRender.setActive(false); }
  private updateCamera(): void {
    const c = this.controls.current;
    nativeRender.camera(c[0], c[1], c[2], c[3], c[4], c[5], this.controls.fly ? 1 : 0, this.controls.fov);
  }
  private benchmark(): void {
    this.stopStream(); this.reset(); const s = nativeRender.status();
    if (s.state !== 'ready' || !s.count) { return; }
    this.benchmarkFrames = s.frames; this.benchmarkCount = s.count; this.benchmarkStart = Date.now(); this.benchmarkResult = '测量中：固定环绕轨迹，10 秒';
  }
  private toggleMode(): void { this.autoOrbit = false; this.joyX = 0; this.joyY = 0; this.pickGeneration++; this.flyMode = !this.flyMode; this.controls.setFly(this.flyMode); this.updateCamera(); }
  private async focusAt(x: number, y: number): Promise<void> {
    const request = ++this.pickGeneration;
    try { const point = await nativeRender.pick(x / this.controls.width, y / this.controls.height);
      if (request === this.pickGeneration && this.active) { this.controls.focus(point); this.updateCamera(); }
    } catch (error) { console.warn(`Focus: ${String(error)}`); }
  }
  private reset(): void { this.profilePlaying = false; this.profile = undefined; this.controls.fov = 45; this.autoOrbit = false; this.joyX = 0; this.joyY = 0; this.pickGeneration++; this.benchmarkStart = 0; this.benchmarkResult = '';  this.flyMode = false; this.controls.reset(); if (this.title === '流式场景') { this.controls.current[1] = .35; this.controls.target[1] = .35; } this.updateCamera(); }
  private touch(event: TouchEvent): void {
    this.profilePlaying = false;
    if (event.source === SourceType.Mouse) { return; }
    if (event.type === TouchType.Cancel) { this.controls.cancel(); return; }
    this.autoOrbit = false;
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
    this.profilePlaying = false;
    if (event.action === MouseAction.Press) {
      this.autoOrbit = false;
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
    if (event.type === KeyType.Down) { this.profilePlaying = false; }
    let key = event.keyText.toLowerCase();
    switch (event.keyCode) {
      case KeyCode.KEYCODE_W: key = 'w'; break;
      case KeyCode.KEYCODE_A: key = 'a'; break;
      case KeyCode.KEYCODE_S: key = 's'; break;
      case KeyCode.KEYCODE_D: key = 'd'; break;
      case KeyCode.KEYCODE_Q: key = 'q'; break;
      case KeyCode.KEYCODE_E: key = 'e'; break;
      case KeyCode.KEYCODE_R: key = 'r'; break;
      case KeyCode.KEYCODE_F: key = 'f'; break;
      case KeyCode.KEYCODE_H: key = 'h'; break;
      case KeyCode.KEYCODE_SPACE: key = ' '; break;
    }
    console.info(`ViewerKey ${event.keyCode} type=${event.type} key=${key}`);
    if (event.type === KeyType.Down && key === 'h') { this.helpVisible = !this.helpVisible; return true; }
    if (event.type === KeyType.Down && key === 'f') { this.toolsVisible = !this.toolsVisible; return true; }
    if (event.type === KeyType.Down && key === ' ') { this.autoOrbit = !this.autoOrbit && !this.flyMode; return true; }
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
    this.stopStream(); this.localError = ''; this.title = '流式场景'; this.reset();
    const session = new StreamSession((message: string) => { if (this.stream === session) { this.streamMessage = message; } });
    this.stream = session; session.budget = this.quality;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      await session.open(this.streamUrl, context.cacheDir, this.fullLoad);
      if (this.stream === session) {
        if (this.streamUrl === 'http://127.0.0.1:8768/scene.json') { await this.profileCamera(false); }
        session.tick(this.controls.current, this.controls.fly, this.controls.fov, this.controls.width / Math.max(1, this.controls.height));
      }
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
        this.title = label; this.reset(); this.loading = true; nativeRender.load(path);
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
        this.title = '本地模型'; this.reset(); this.loading = true; nativeRender.load(destination);
      }
    } catch (error) {
      this.localError = (error as BusinessError).message;
      this.message = this.localError;
    } finally { this.busy = false; }
  }
  private joystick(event: TouchEvent): void {
    if (event.type === TouchType.Up || event.type === TouchType.Cancel || event.touches.length === 0) {
      this.joyX = 0; this.joyY = 0; return;
    }
    const x = (event.touches[0].x - 48) / 30; const y = (event.touches[0].y - 48) / 30;
    const length = Math.max(1, Math.hypot(x, y)); this.joyX = x / length; this.joyY = y / length;
  }
  build() {
    Stack({ alignContent: Alignment.TopStart }) {
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

      Column({ space: 6 }) {
        Row() {
          Column({ space: 3 }) {
            Text('NEXTNEWS / 3DGS').fontSize(10).fontColor('#8CA7CB').letterSpacing(2).width('100%')
            Text(this.title).fontSize(18).fontWeight(FontWeight.Medium).fontColor('#EEF4FF').width('100%')
          }.layoutWeight(1)
          ViewerTool({ icon: this.toolsVisible ? $r('app.media.viewer_expand') : $r('app.media.viewer_collapse'), label: this.toolsVisible ? '沉浸显示' : '显示工具', selected: !this.toolsVisible, action: () => { this.toolsVisible = !this.toolsVisible; }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
        }.width('100%')
        if (this.loading || this.busy || this.localError) {
          Row({ space: 8 }) {
            if (this.loading || this.busy) { LoadingProgress().width(18).height(18).color('#90B7FF') }
            Text(this.localError || this.message).fontSize(12).fontColor('#E7C78C').layoutWeight(1)
          }.padding(8).backgroundColor('#D9162233').borderRadius(8)
        }
        if (this.streamMessage && this.toolsVisible) {
          Text(this.streamMessage).fontSize(10).fontColor('#B7C9DE').maxLines(2).width('100%')
        }
      }.padding(16).width('100%').backgroundColor('#B3091322')
      if (this.toolsVisible) {
        Column({ space: 8 }) {
          if (this.hint) { Text(this.hint).fontSize(12).fontColor('#EEF4FF').padding(6).backgroundColor('#DD1C283B').borderRadius(6) }
          if (this.settingsVisible || this.modelsVisible || this.helpVisible || this.diagnosticsVisible) {
            Scroll() {
              Column({ space: 12 }) {
                if (this.modelsVisible) {
                  Text('模型库').fontColor('#EEF4FF').fontSize(16).width('100%')
                  Row({ space: 6 }) {
                    Button('公开样例').fontSize(12).enabled(!this.busy).onClick(() => { this.sample('public.ply', '公开样例'); })
                    Button('Tripo').fontSize(12).enabled(!this.busy).onClick(() => { this.sample('tripo.ply', 'Tripo Splat'); })
                    Button('转换样例').fontSize(12).enabled(!this.busy).onClick(() => { this.sample('converted.ply', '转换样例'); })
                  }
                  Row({ space: 6 }) {
                    Button('配置默认镜头').fontSize(12).onClick(() => { this.profileCamera(false); })
                    Button(this.profilePlaying ? '暂停配置动画' : '播放配置动画').fontSize(12).onClick(() => { if (this.profilePlaying) { this.profilePlaying = false; } else { this.profileCamera(true); } })
                  }
                  Button('华为原生渲染').fontSize(13).width('100%').onClick(() => { this.getUIContext().getRouter().pushUrl({ url: 'pages/SpatialViewer' }).catch((error: BusinessError) => { this.localError = error.message; }); })
                  Button('打开本地 PLY / SOG').fontSize(13).width('100%').enabled(!this.busy).onClick(() => { this.importModel(); })
                  Button('流式').fontSize(12).onClick(() => { this.streamPanel = !this.streamPanel; })
                  if (this.streamPanel) {
                    TextInput({ text: this.streamUrl, placeholder: 'HTTP(S) scene.json' }).selectAll(true).fontSize(12)
                      .fontColor('#EEF4FF').backgroundColor('#28354A').onChange((value: string) => { this.streamUrl = value; })
                    Button(this.fullLoad ? '加载：全量预载' : '加载：渐进 LOD').fontSize(12).onClick(() => { this.fullLoad = !this.fullLoad; })
                    Row({ space: 8 }) {
                      Button('连接流式').fontSize(12).onClick(() => { this.startStream(); })
                      Button('重试').fontSize(12).onClick(() => { if (this.stream?.manifest) { this.stream.retry(); } else { this.startStream(); } })
                      Button('停止').fontSize(12).onClick(() => { this.stopStream(); })
                    }
                  }
                }
                if (this.settingsVisible) {
                  Text('视图设置').fontSize(16).fontColor('#EEF4FF').width('100%')
                  Row({ space: 8 }) {
                    Text('高斯预算').fontSize(12).fontColor('#AFC6D8').layoutWeight(1)
                    Button(this.quality === 1000000 ? '100 万' : this.quality === 2000000 ? '200 万' : '400 万').fontSize(12).onClick(() => {
                      this.quality = this.quality === 1000000 ? 2000000 : this.quality === 2000000 ? 4000000 : 1000000;
                      if (this.stream) { this.stream.budget = this.quality; }
                    })
                  }.width('100%')
                  Row({ space: 8 }) {
                    Text('单指 / 左键').fontSize(12).fontColor('#AFC6D8').layoutWeight(1)
                    Button(this.panMode ? '平移' : '旋转').fontSize(12).onClick(() => { this.panMode = !this.panMode; })
                  }.width('100%')
                  Text('飞行速度 ×' + this.flySpeed.toFixed(1)).fontSize(12).fontColor('#AFC6D8').width('100%')
                  Slider({ value: this.flySpeed, min: 0.2, max: 5, step: 0.2 }).onChange((value: number) => { this.flySpeed = value; this.controls.speed = value; })
                  Button(this.diagnosticsVisible ? '关闭性能信息' : '性能信息').fontSize(12).onClick(() => { this.diagnosticsVisible = !this.diagnosticsVisible; this.settingsVisible = false; })
                }
                if (this.helpVisible) {
                  Text('浏览操作').fontSize(16).fontColor('#EEF4FF').width('100%')
                  Text('单击聚焦 · 双击切换环绕 / 飞行\n左键或单指旋转 · 右键 / 中键平移\n双指同时平移与缩放 · 滚轮缩放\n飞行：WASD 移动，Q/E 升降；触屏使用摇杆\nR 复位 · F 沉浸显示 · H 帮助 · 空格自动环绕\n播放图标开启自动环绕；拖动会停止自动环绕。')
                    .fontSize(12).fontColor('#B7C9DE').width('100%').lineHeight(22)
                }
                if (this.diagnosticsVisible) {
                  Button(this.gpuOptimized ? 'GPU 裁剪：开' : 'GPU 裁剪：关').fontSize(12).onClick(() => { this.gpuOptimized = !this.gpuOptimized; nativeRender.optimize(this.gpuOptimized); })
                  Button('测量 10 秒').fontSize(12).enabled(this.benchmarkStart === 0).onClick(() => { this.benchmark(); })
                  if (this.benchmarkResult) { Text(this.benchmarkResult).fontSize(11).fontColor('#EBC478') }
                  Text(this.details).fontSize(11).fontColor('#99B1C4').width('100%')
                  Text(this.graphics).fontSize(10).fontColor('#6F899F').width('100%')
                }
              }.padding(16).width('100%')
            }.constraintSize({ maxHeight: 330 }).width('100%').backgroundColor('#F0162233').borderRadius(18)
          }
          Row({ space: 5 }) {
            ViewerTool({ icon: $r('app.media.viewer_orbit'), label: '环绕', selected: !this.flyMode, action: () => { if (this.flyMode) { this.toggleMode(); } }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
            ViewerTool({ icon: $r('app.media.viewer_fly'), label: '飞行', selected: this.flyMode, action: () => { if (!this.flyMode) { this.toggleMode(); } }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
            ViewerTool({ icon: $r('app.media.viewer_reset'), label: '复位', selected: false, action: () => { this.reset(); }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
            ViewerTool({ icon: this.autoOrbit ? $r('app.media.viewer_pause') : $r('app.media.viewer_play'), label: '自动环绕', selected: this.autoOrbit, action: () => {
              if (this.flyMode) { this.toggleMode(); } this.autoOrbit = !this.autoOrbit;
            }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
            ViewerTool({ icon: $r('app.media.viewer_folder'), label: '模型', selected: this.modelsVisible, action: () => { this.modelsVisible = !this.modelsVisible; this.settingsVisible = false; this.helpVisible = false; this.diagnosticsVisible = false; }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
            ViewerTool({ icon: $r('app.media.viewer_settings'), label: '设置', selected: this.settingsVisible, action: () => { this.settingsVisible = !this.settingsVisible; this.modelsVisible = false; this.helpVisible = false; this.diagnosticsVisible = false; }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
            ViewerTool({ icon: $r('app.media.viewer_help'), label: '帮助', selected: this.helpVisible, action: () => { this.helpVisible = !this.helpVisible; this.settingsVisible = false; this.modelsVisible = false; this.diagnosticsVisible = false; }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
          }.padding(8).backgroundColor('#CC101A29').borderRadius(18)
          Text(this.flyMode ? '飞行 · 拖动转头' : '环绕 · 单击聚焦').fontSize(10).fontColor('#AFC6D8')
        }.padding(16).width('100%').position({ x: 0, y: '100%' }).translate({ y: '-100%' })
        if (!this.settingsVisible && !this.modelsVisible && !this.helpVisible && !this.diagnosticsVisible) {
          Column({ space: 8 }) {
            ViewerTool({ icon: $r('app.media.viewer_plus'), label: '放大', selected: false, action: () => { this.controls.zoom(1 / 1.2); }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
            ViewerTool({ icon: $r('app.media.viewer_minus'), label: '缩小', selected: false, action: () => { this.controls.zoom(1.2); }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
          }.padding(16).position({ x: '100%', y: '50%' }).translate({ x: '-100%', y: '-50%' })
          if (this.flyMode) {
            Stack() {
              Circle().width(96).height(96).fill('#9928354A').stroke('#7794BD').strokeWidth(1)
              Circle().width(34).height(34).fill('#B0CFFF').translate({ x: this.joyX * 30, y: this.joyY * 30 })
            }.width(96).height(96).position({ x: 20, y: '100%' }).translate({ y: -206 })
              .accessibilityText('飞行摇杆').id('飞行摇杆').onTouch((event: TouchEvent) => { this.joystick(event); })
            Column({ space: 8 }) {
              ViewerTool({ icon: $r('app.media.viewer_up'), label: '上升', selected: false, action: () => { this.controls.move(0, .12 * this.flySpeed, 0); }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
              ViewerTool({ icon: $r('app.media.viewer_down'), label: '下降', selected: false, action: () => { this.controls.move(0, -.12 * this.flySpeed, 0); }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
            }.position({ x: '100%', y: '100%' }).translate({ x: -62, y: -206 })
          }
        }
      }
    }.width('100%').height('100%').backgroundColor('#091322')
  }
}
