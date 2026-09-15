import { Scene } from '@kit.ArkGraphics3D';
import { util } from '@kit.ArkTS';
import { ViewerRuntime, ViewerMarker } from './ViewerRuntime';
import { ViewerBackend, OpenGLBackend, HuaweiBackend } from './ViewerBackend';
import { PointerInput } from './PointerInput';
import { comparisonSog } from './ComparisonSog';
import { KeyCode } from '@kit.InputKit';
import { common } from '@kit.AbilityKit';
import { fileIo as fs, picker } from '@kit.CoreFileKit';
import { BusinessError } from '@kit.BasicServicesKit';
import nativeRender from 'libsplat.so';
import { ViewerProfile } from './ViewerProfile';
import { ViewerPose, parseViewerSettings, createSettings, framePose } from './ViewerSettings';
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
    }.width(44).height(44).borderRadius(22).backgroundColor(this.selected ? '#35FFFFFF' : '#00000000')
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
  private comparisonBounds: number[] | undefined = undefined;
  private viewer: ViewerRuntime = new ViewerRuntime();
  private gl: OpenGLBackend = new OpenGLBackend();
  private huawei: HuaweiBackend = new HuaweiBackend((scene: Scene | undefined) => { this.spatialScene = scene; });
  private backend: ViewerBackend = this.gl;
  private sourcePath: string = '';
  private awaitingModelCamera: boolean = false;
  @State spatialScene: Scene | undefined = undefined;
  @State backendName: string = 'OpenGL';
  @State markers: ViewerMarker[] = [];
  @State annotationIndex: number = -1;
  @State annotationTitle: string = '';
  @State annotationText: string = '';
  @State annotationCount: number = 0;
  @State showAnnotations: boolean = true;
  @State gaming: boolean = false;
  @State timeline: number = 0;
  @State duration: number = 20;
  @State profileReady: boolean = false;
  private timelineUpdated: number = 0;
  @State profilePlaying: boolean = false;
  @StorageLink('nextnewsForeground') @Watch('foregroundChanged') foreground: boolean = true;
  private applyProfilePose(pose: ViewerPose): void {
    this.viewer.apply(pose, this.controls.fly); this.updateCamera();
  }
  private refreshAnnotations(): void {
    this.profileReady = this.viewer.profile !== undefined;
    this.markers = this.viewer.markers(); this.annotationIndex = this.viewer.selected;
    this.annotationCount = this.viewer.profile?.settings.annotations.length ?? 0;
    const annotation = this.viewer.profile?.settings.annotations[this.annotationIndex];
    this.annotationTitle = annotation?.title ?? ''; this.annotationText = annotation?.text ?? '';
  }
  private activateAnnotation(index: number): void {
    this.profilePlaying = false; this.autoOrbit = false; this.resetJoystick(); this.pickGeneration++;
    this.controls.setFly(false); this.flyMode = false; this.viewer.annotation(index); this.refreshAnnotations();
  }
  private play(): void {
    if (!this.viewer.profile) { this.viewer.configure(createSettings(this.viewer.pose())); }
    this.duration = this.viewer.profile!.animation.duration;
    if (this.viewer.profile!.ended) { this.viewer.profile!.time = 0; }
    this.viewer.interrupt(); this.autoOrbit = false; this.profilePlaying = !this.profilePlaying; this.profileReady = true;
  }
  private async importSettings(): Promise<void> {
    const request = ++this.pickGeneration;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      const selected = await new picker.DocumentViewPicker(context).select({ maxSelectNumber: 1, fileSuffixFilters: ['Viewer 设置|.json'] });
      if (!selected.length) { return; }
      const file = await fs.open(selected[0], fs.OpenMode.READ_ONLY);
      let text = '';
      try {
        const size = (await fs.stat(file.fd)).size; if (size > 4 * 1024 * 1024) { throw new Error('Viewer 配置超过 4 MiB'); }
        const data = new ArrayBuffer(size); let offset = 0;
        while (offset < size) { const part = new ArrayBuffer(size - offset); const n = await fs.read(file.fd, part); if (n <= 0) { throw new Error('配置读取不完整'); } new Uint8Array(data).set(new Uint8Array(part, 0, n), offset); offset += n; }
        text = new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(new Uint8Array(data));
      } finally { await fs.close(file); }
      if (request !== this.pickGeneration || !this.visible) { return; }
      this.viewer.configure(parseViewerSettings(text)); this.viewer.initial(); this.flyMode = this.controls.fly;
      this.duration = this.viewer.profile!.animation.duration; this.timeline = 0;
      this.profilePlaying = this.viewer.profile!.settings.startMode === 'animTrack'; this.modelsVisible = false; this.updateCamera();
    } catch (error) { this.localError = String(error); }
  }
  private async switchBackend(): Promise<void> {
    if (this.busy) { return; }
    if (this.stream) { this.localError = '华为原生整场流式尚未通过；请用同源单体模型切换对照，原生瓦片实验入口保留在模型菜单。'; return; }
    if (!this.sourcePath) { return; }
    const next = this.backendName === 'OpenGL' ? this.huawei : this.gl, previous = this.backend;
    const snapshot = this.viewer.snapshot(this.profilePlaying); this.busy = true; this.localError = '';
    this.profilePlaying = false; this.viewer.interrupt(); this.resetJoystick(); this.pickGeneration++;
    try {
      previous.active(false);
      this.backend = next; this.backendName = next === this.gl ? 'OpenGL' : 'Huawei';
      this.awaitingModelCamera = next === this.gl; await next.load({ path: this.sourcePath, bounds: this.viewer.bounds });
      this.viewer.apply(snapshot.pose, snapshot.fly); this.flyMode = snapshot.fly; next.active(this.active); this.updateCamera();
      previous.dispose(); this.profilePlaying = snapshot.playing;
    } catch (error) {
      next.dispose(); this.backend = previous; this.backendName = previous === this.gl ? 'OpenGL' : 'Huawei'; previous.active(this.active);
      this.localError = String(error); this.viewer.apply(snapshot.pose, snapshot.fly); this.updateCamera();
    } finally { this.busy = false; }
  }

  private async profileCamera(play: boolean): Promise<void> {
    if (this.title !== '转换样例' && this.title !== '流式场景' && this.title !== '同源 SOG' && !this.viewer.profile) { this.viewer.configure(createSettings(this.viewer.pose())); }
    try {
      if (!this.viewer.profile) { this.viewer.profile = await ViewerProfile.load(this.getUIContext().getHostContext() as common.UIAbilityContext, this.viewer.bounds); }
      this.autoOrbit = false; this.benchmarkStart = 0; this.profilePlaying = play;
      this.duration = this.viewer.profile.animation.duration;
      if (play) { this.applyProfilePose(this.viewer.profile.animation.sample(this.viewer.profile.time)); }
      else { this.viewer.initial(); this.flyMode = this.controls.fly; this.updateCamera(); }
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
  @State paged: boolean = false;
  @State encoded: boolean = false;
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
  private pointers: PointerInput = new PointerInput();
  private controls: OrbitControls = this.viewer.controls;
  private motionTimer: number = -1;
  private motionTime: number = 0;
  private streamTime: number = 0;
  @State streamMeasuring: boolean = false;
  @State benchmarkResult: string = '';
  private benchmarkStart: number = 0;
  private benchmarkFrames: number = 0;
  private benchmarkCount: number = 0;
  private active: boolean = true;

  private foregroundChanged(): void {
    this.active = this.foreground; this.motionTime = Date.now(); this.pickGeneration++; this.resetJoystick(); this.controls.cancel(); this.pointers.cancel();
    this.backend.active(this.foreground); if (this.stream) { this.stream.active = this.foreground; }
    if (this.foreground && this.backendName === 'Huawei' && this.sourcePath && !this.busy) { this.restoreHuawei(); }
  }
  private async restoreHuawei(): Promise<void> {
    const snapshot = this.viewer.snapshot(this.profilePlaying); this.busy = true;
    try {
      await this.huawei.load({ path: this.sourcePath, bounds: this.viewer.bounds });
      if (this.visible && this.backendName === 'Huawei') { this.viewer.apply(snapshot.pose, snapshot.fly); this.updateCamera(); }
    } catch (error) { this.localError = String(error); } finally { this.busy = false; }
  }
  aboutToAppear(): void {
    this.visible = true;
    this.motionTime = Date.now();
    this.motionTimer = setInterval(() => {
      const now = Date.now(); const dt = now - this.motionTime; this.motionTime = now;
      if (this.active && now - this.streamTime >= 50) {
        this.streamTime = now;
        this.stream?.tick(this.controls.current, this.controls.fly, this.controls.fov, this.controls.width / Math.max(1, this.controls.height));
      }
      if (this.active && this.profilePlaying && this.viewer.profile) {
        this.applyProfilePose(this.viewer.profile.advance(dt / 1000));
        if (now - this.timelineUpdated >= 100) { this.timelineUpdated = now; this.timeline = this.viewer.profile.animation.cursor(this.viewer.profile.time); }
        if (this.viewer.profile.ended) { this.profilePlaying = false; }
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
        if (this.viewer.tick(dt)) { this.updateCamera(); }
      }
    }, 16);
    this.timer = setInterval(() => {
      if (this.backendName !== 'OpenGL') { this.loading = false; this.message = this.backend.capabilities.note; return; }
      const state = nativeRender.status();
      if (state.state === 'ready' && !this.stream && state.count > 0) { this.viewer.bounds = state.bounds; if (this.awaitingModelCamera) { this.awaitingModelCamera = false; this.updateCamera(); } }
      this.loading = state.state === 'loading';
      if (this.stream) { console.info(`StreamFrame state=${state.state} frames=${state.frames} count=${state.count} loadMs=${state.loadMs.toFixed(0)} frameMs=${state.frameMs.toFixed(1)} uploadMs=${state.uploadMs.toFixed(1)} uploadedRows=${state.uploadedRows} reusedRows=${state.reusedRows} decodedFiles=${state.decodedFiles} subsetHits=${state.subsetHits} request=${state.requestRevision} display=${state.displayRevision} prepareMs=${state.prepareMs.toFixed(1)} refineMs=${state.refineMs.toFixed(1)} uploadedBytes=${state.uploadedBytes} pageHits=${state.pageHits}`); }
      if (!this.busy && this.localError.length === 0) {
        this.message = state.state === 'ready' ? '官方导航 · 环绕聚焦 / 飞行前往 · 双击聚焦' : state.message;
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
    this.controls.cancel(); this.pointers.cancel(); this.resetJoystick();
    this.backend.dispose();
  }
  onPageShow(): void { this.active = true; if (this.stream) { this.stream.active = true; } this.motionTime = Date.now(); this.backend.active(true); }
  onPageHide(): void { this.pickGeneration++; this.benchmarkStart = 0; this.active = false; this.resetJoystick(); if (this.stream) { this.stream.active = false; } this.controls.cancel(); this.pointers.cancel(); this.backend.active(false); }
  private updateCamera(): void {
    this.backend.camera(this.viewer.pose(), this.viewer.bounds); this.refreshAnnotations();
  }

  private benchmark(): void {
    this.stopStream(); this.reset(); const s = nativeRender.status();
    if (s.state !== 'ready' || !s.count) { return; }
    this.benchmarkFrames = s.frames; this.benchmarkCount = s.count; this.benchmarkStart = Date.now(); this.benchmarkResult = '测量中：固定环绕轨迹，10 秒';
  }
  private async measureStream(fileOnly: boolean = false): Promise<void> {
    const stream = this.stream;
    if (!stream || !stream.paged || this.streamMeasuring) { return; }
    this.settingsVisible = false; this.diagnosticsVisible = false;
    this.streamMeasuring = true; this.autoOrbit = false; this.profilePlaying = false;
    const prefetch = stream.prefetchEnabled; stream.prefetchEnabled = false;
    this.resetJoystick();
    const original = this.controls.current.slice();
    try {
      // Two warm-up turns, then 20 identical alternating poses. No LOD reduction.
      for (let trial = -2; trial < 20 && this.active && this.stream === stream; trial++) {
        const pose = original.slice(); pose[0] += trial % 2 === 0 ? -.65 : .65;
        this.controls.current = pose; this.controls.target = pose.slice(); this.updateCamera();
        if (fileOnly && trial >= 0) { nativeRender.dropCaches(); stream.retry(); }
        const start = Date.now(), networkStart = stream.networkBytes;
        stream.tick(pose, this.controls.fly, this.controls.fov, this.controls.width / Math.max(1, this.controls.height));
        let complete = false;
        while (Date.now() - start < 45000 && this.active && this.stream === stream) {
          const status = nativeRender.status();
          if (stream.selectionSubmitted && stream.coverage === 1 && status.state === 'ready' && status.displayRevision === stream.submittedRevision && status.count === stream.requestedCount) {
            console.info(`StreamTrial trial=${trial} cache=${fileOnly ? 'file' : 'gpu'} budget=${this.quality} revision=${stream.submittedRevision} target=${stream.targetRevision} elapsedMs=${Date.now() - start} refineMs=${status.refineMs.toFixed(2)} uploadedBytes=${status.uploadedBytes} networkBytes=${stream.networkBytes - networkStart} count=${status.count} coverage=${stream.coverage}`);
            this.benchmarkResult = `流式测量 ${Math.max(0, trial + 1)}/20 · ${Date.now() - start} ms`; complete = true; break;
          }
          if (status.state === 'error') { throw new Error(status.message); }
          await new Promise<void>((resolve) => { setTimeout(resolve, 20); });
        }
        if (!complete) { throw new Error('目标细节未在 45 秒内完整显示'); }
        await new Promise<void>((resolve) => { setTimeout(resolve, 300); });
      }
    } catch (error) { this.benchmarkResult = `测量未通过：${String(error)}`; console.info(`StreamTrial failure=${String(error)}`); }
    finally {
      stream.prefetchEnabled = prefetch;
      if (this.active && this.stream === stream) { this.controls.current = original; this.controls.target = original.slice(); this.updateCamera(); }
      this.streamMeasuring = false;
    }
  }
  private async measureContinuous(): Promise<void> {
    const stream = this.stream;
    if (!stream || !stream.paged || this.streamMeasuring) { return; }
    this.streamMeasuring = true; this.autoOrbit = false; this.profilePlaying = false;
    this.settingsVisible = false; this.diagnosticsVisible = false; this.resetJoystick();
    const original = this.controls.current.slice(), start = Date.now(); nativeRender.traceFrames(true);
    console.info(`StreamContinuous start budget=${this.quality}`);
    try {
      while (Date.now() - start < 20000 && this.active && this.stream === stream) {
        const t = (Date.now() - start) / 1000, pose = original.slice();
        pose[0] += .65 * Math.sin(t * 1.2); pose[1] += .08 * Math.sin(t * .7);
        this.controls.current = pose; this.controls.target = pose.slice(); this.updateCamera();
        stream.tick(pose, this.controls.fly, this.controls.fov, this.controls.width / Math.max(1, this.controls.height));
        await new Promise<void>((resolve) => { setTimeout(resolve, 16); });
      }
      console.info(`StreamContinuous complete elapsedMs=${Date.now() - start} budget=${this.quality}`);
      this.benchmarkResult = '连续转向完成 · 逐帧记录已写入日志';
    } finally { nativeRender.traceFrames(false); this.streamMeasuring = false; }
  }
  private toggleMode(): void { this.viewer.interrupt(); this.profilePlaying = false; this.autoOrbit = false; this.resetJoystick(); this.pickGeneration++; this.flyMode = !this.flyMode; this.controls.setFly(this.flyMode); this.updateCamera(); }
  private async focusAt(x: number, y: number, doubleTap: boolean = false): Promise<void> {
    const request = ++this.pickGeneration;
    try { const point = await this.backend.pick(x / this.controls.width, y / this.controls.height, this.viewer.bounds);
      if (request === this.pickGeneration && this.active) { if (point.length !== 3) { return; } this.viewer.navigate(point, doubleTap); this.flyMode = this.controls.fly; this.updateCamera(); }
    } catch (error) { console.warn(`Focus: ${String(error)}`); }
  }
  private frame(): void { this.viewer.interrupt(); this.profilePlaying = false; this.controls.setFly(false); this.flyMode = false; this.viewer.transition(framePose(this.viewer.bounds, this.controls.fov)); }
  private reset(): void {
    this.profilePlaying = false; this.autoOrbit = false; this.resetJoystick(); this.pickGeneration++; this.viewer.interrupt();
    this.benchmarkStart = 0; this.benchmarkResult = ''; this.controls.cancel();
    if (this.viewer.profile) { this.viewer.profile.time = 0; this.viewer.selected = -1; this.viewer.initial(); }
    else { this.controls.reset(); this.controls.fov = 85; this.viewer.apply(framePose(this.viewer.bounds), false); }
    this.flyMode = this.controls.fly; this.timeline = 0; this.updateCamera();
  }
  private clearProfile(): void { this.viewer.profile = undefined; this.profileReady = false; this.viewer.selected = -1; this.annotationCount = 0; this.markers = []; this.annotationIndex = -1; this.timeline = 0; }
  private touch(event: TouchEvent): void {
    this.viewer.interrupt();
    this.profilePlaying = false;
    if (event.source === SourceType.Mouse) { return; }
    this.autoOrbit = false;
    const points = this.pointers.viewport(this.touchPhase(event.type), this.touchPoints(event.touches), this.touchPoints(event.changedTouches));
    this.controls.touch(points, this.panMode);
  }
  private touchPhase(type: TouchType): string {
    return type === TouchType.Down ? 'down' : type === TouchType.Up ? 'up' : type === TouchType.Cancel ? 'cancel' : 'move';
  }
  private touchPoints(points: TouchObject[]): OrbitTouch[] {
    return points.map((p: TouchObject): OrbitTouch => { return { id: p.id, x: p.x, y: p.y }; });
  }
  private resetJoystick(): void { this.pointers.cancelStick(); this.joyX = 0; this.joyY = 0; }

  private mouse(event: MouseEvent): void {
    if (event.action === MouseAction.Press || this.mouseButton !== MouseButton.None) { this.viewer.interrupt(); this.profilePlaying = false; }
    if (event.action === MouseAction.Press) {
      this.autoOrbit = false; if (this.pointers.stickId < 0) { this.pointers.blockTap = false; }
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
      case KeyCode.KEYCODE_F: key = 'f'; break;
      case KeyCode.KEYCODE_H: key = 'h'; break;
      case KeyCode.KEYCODE_SPACE: key = ' '; break;
      case KeyCode.KEYCODE_G: key = 'g'; break;
      case KeyCode.KEYCODE_1: key = '1'; break;
      case KeyCode.KEYCODE_2: key = '2'; break;
      case KeyCode.KEYCODE_ESCAPE: key = 'escape'; break;
    }
    if (event.type === KeyType.Down && key !== ' ') { this.profilePlaying = false; }
    console.info(`ViewerKey ${event.keyCode} type=${event.type} key=${key}`);
    if (event.type === KeyType.Down && key === 'escape') { this.viewer.interrupt(); this.profilePlaying = false; this.gaming = false; this.viewer.gaming = false; this.toolsVisible = true; this.helpVisible = this.settingsVisible = this.modelsVisible = false; this.resetJoystick(); return true; }
    if (event.type === KeyType.Down && key === 'h') { this.helpVisible = !this.helpVisible; return true; }
    if (event.type === KeyType.Down && key === 'f') { this.frame(); return true; }
    if (event.type === KeyType.Down && key === ' ') { this.play(); return true; }
    if (event.type === KeyType.Down && key === 'g') { this.gaming = !this.gaming; this.viewer.gaming = this.gaming; this.resetJoystick(); return true; }
    if (event.type === KeyType.Down && (key === '1' || key === '2')) { if (this.flyMode !== (key === '2')) { this.toggleMode(); } return true; }
    if (key === 'r' && event.type === KeyType.Down) { this.reset(); return true; }
    if ('wasdqe'.includes(key) && key.length === 1) {
      this.viewer.interrupt(); if (event.type === KeyType.Down && !this.flyMode) { this.toggleMode(); } this.gaming = true; this.viewer.gaming = true;
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
    if (this.backendName !== 'OpenGL') { this.localError = '华为原生瓦片请使用独立实验入口'; return; }
    this.stopStream(); this.clearProfile(); this.localError = ''; this.title = '流式场景'; this.reset();
    const session = new StreamSession((message: string) => { if (this.stream === session) { this.streamMessage = message; } });
    this.stream = session; session.budget = this.quality; session.paged = this.paged; session.encoded = this.encoded;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      await session.open(this.streamUrl, context.cacheDir, this.fullLoad);
      if (this.stream === session) {
        this.viewer.bounds = session.manifest!.bounds.slice();
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
        this.title = label; this.sourcePath = path; this.clearProfile();
        const source = SAMPLE_BOUNDS.find((s) => s.file === name)!;
        this.viewer.bounds = [-source.center[0], -source.center[1], source.center[2], source.radius];
        this.reset(); this.loading = true; this.awaitingModelCamera = this.backendName === 'OpenGL'; await this.backend.load({ path, bounds: this.viewer.bounds }); this.updateCamera();
      }
    } catch (error) {
      this.localError = (error as BusinessError).message;
      this.message = this.localError;
    } finally { this.busy = false; }
  }
  private async loadComparison(): Promise<void> {
    if (this.busy) { return; } this.busy = true; this.stopStream(); this.localError = '';
    const request = ++this.generation;
    try {
      const asset = await comparisonSog(this.getUIContext().getHostContext() as common.UIAbilityContext);
      if (!this.visible || request !== this.generation) { return; }
      this.title = '同源 SOG'; this.sourcePath = asset.path; this.clearProfile(); this.comparisonBounds = asset.bounds; this.viewer.bounds = asset.bounds;
      if (this.backendName === 'OpenGL') { nativeRender.chunks([asset.path], asset.bounds, [0, 0, asset.count]); } else { await this.backend.load({ path: asset.path, bounds: asset.bounds }); }
      await this.profileCamera(false);
    } catch (error) { this.localError = String(error); } finally { this.busy = false; }
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
        const bounds = await nativeRender.inspectModel(destination);
        if (!this.visible || request !== this.generation) { return; }
        this.title = '本地模型'; this.sourcePath = destination; this.viewer.bounds = bounds; this.clearProfile(); this.reset(); this.loading = true; this.awaitingModelCamera = this.backendName === 'OpenGL'; await this.backend.load({ path: destination, bounds }); this.updateCamera();
      }
    } catch (error) {
      this.localError = (error as BusinessError).message;
      this.message = this.localError;
    } finally { this.busy = false; }
  }
  private joystick(event: TouchEvent): void {
    event.stopPropagation();
    this.profilePlaying = false; this.viewer.interrupt();
    this.pointers.joystick(this.touchPhase(event.type), this.touchPoints(event.touches), this.touchPoints(event.changedTouches));
    this.joyX = this.pointers.x; this.joyY = this.pointers.y;
    if (event.type !== TouchType.Move) { console.info(`ViewerStick phase=${this.touchPhase(event.type)} owner=${this.pointers.stickId} x=${this.joyX.toFixed(3)} y=${this.joyY.toFixed(3)}`); }
  }

  build() {
    Stack({ alignContent: Alignment.TopStart }) {
      if (this.backendName === 'Huawei') {
        if (this.spatialScene) { Component3D({ scene: this.spatialScene, modelType: ModelType.SURFACE }).width('100%').height('100%') }
      } else {
      XComponent({ id: 'nextnews-splat', type: XComponentType.SURFACE, libraryname: 'splat' })
        .width('100%').height('100%')
        .onLoad(() => { nativeRender.setActive(true); if (nativeRender.status().count === 0) { this.sample('tripo.ply', 'Tripo Splat'); } else { this.updateCamera(); } });
      }
      Button('', { type: ButtonType.Normal }).width('100%').height('100%').backgroundColor('#00000000')
        .stateEffect(false).focusOnTouch(true).accessibilityText('3D 模型视窗')
        .onAreaChange((_old: Area, area: Area) => { this.controls.resize(Number(area.width), Number(area.height)); this.updateCamera(); })
        .id('viewer-surface').focusable(true).defaultFocus(true)
        .onTouch((event: TouchEvent) => { this.touch(event); })
        .onMouse((event: MouseEvent) => { this.mouse(event); })
        .onHover((hover: boolean) => { if (!hover) { this.mouseButton = MouseButton.None; } })
        .onBlur(() => { this.controls.cancel(); this.pointers.cancel(); this.resetJoystick(); this.mouseButton = MouseButton.None; })
        .parallelGesture(GestureGroup(GestureMode.Exclusive,
          TapGesture({ count: 2 }).onAction((event: GestureEvent) => { if (!this.pointers.blockTap && this.pointers.stickId < 0 && event.fingerList.length) { this.focusAt(event.fingerList[0].localX, event.fingerList[0].localY, true); } }),
          TapGesture({ count: 1 }).onAction((event: GestureEvent) => {
            if (!this.pointers.blockTap && this.pointers.stickId < 0 && event.fingerList.length > 0) { this.focusAt(event.fingerList[0].localX, event.fingerList[0].localY); }
          })
        ))
        .onKeyEvent((event: KeyEvent) => this.handleKey(event))
        .onAxisEvent((event: AxisEvent) => { this.controls.zoom(Math.exp(Math.max(-1, Math.min(1, event.getVerticalAxisValue() * 0.002)))); });

      if (this.toolsVisible && this.showAnnotations && !(this.flyMode && this.gaming)) {
        ForEach(this.markers, (marker: ViewerMarker) => {
          Button(`${marker.index + 1}`, { type: ButtonType.Circle }).width(44).height(44).fontSize(14)
            .fontColor('#FFFFFF').backgroundColor(this.annotationIndex === marker.index ? '#e98443' : '#80000000')
            .border({ width: 2, color: '#FFFFFF' }).position({ x: marker.x - 22, y: marker.y - 22 })
            .accessibilityText(`标注 ${marker.index + 1} ${marker.title}`).id(`annotation-${marker.index}`)
            .onClick(() => { this.activateAnnotation(marker.index); })
        }, (marker: ViewerMarker) => marker.index.toString())
      }
      Column({ space: 6 }) {
        Row() {
          Column({ space: 3 }) {
            Text('NEXTNEWS').fontSize(10).fontColor('#8CA7CB').letterSpacing(2).width('100%')
            Text(this.toolsVisible ? this.title : '').fontSize(14).fontWeight(FontWeight.Medium).fontColor('#EEF4FF').width('100%')
          }.layoutWeight(1)
          ViewerTool({ icon: this.toolsVisible ? $r('app.media.viewer_expand') : $r('app.media.viewer_collapse'), label: this.toolsVisible ? '隐藏工具' : '显示工具', selected: !this.toolsVisible, action: () => { this.toolsVisible = !this.toolsVisible; }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
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
      }.padding(16).width('100%').backgroundColor('#00000000')
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
                  Button('SuperSplat Web 对照').fontSize(12).onClick(() => this.getUIContext().getRouter().pushUrl({ url: 'pages/WebComparison' }))
                  Button(this.backendName === 'OpenGL' ? '切换华为原生' : '切换 OpenGL').fontSize(13).width('100%').enabled(!this.busy).onClick(() => { this.switchBackend(); })
                  Text(this.backend.capabilities.note).fontSize(11).fontColor('#BBBBBB')
                  Button('华为瓦片实验').fontSize(12).onClick(() => { this.getUIContext().getRouter().pushUrl({ url: 'pages/SpatialViewer' }); })
                  Button('导入 viewer-settings.json').fontSize(12).onClick(() => { this.importSettings(); })
                  Button('同源 SOG 对照').enabled(!this.busy).onClick(() => { this.loadComparison(); })
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
                  Text('设置').fontSize(16).fontColor('#FFFFFF').width('100%')
                  Button(this.gaming ? '游戏控制：开' : '游戏控制：关').fontSize(12).onClick(() => { this.gaming = !this.gaming; this.viewer.gaming = this.gaming; this.resetJoystick(); })
                  Button(this.showAnnotations ? '标注：显示' : '标注：隐藏').fontSize(12).onClick(() => { this.showAnnotations = !this.showAnnotations; })
                  Row({ space: 12 }) {
                    Button('完整取景').fontSize(12).onClick(() => { this.frame(); this.settingsVisible = false; })
                    Button('复位').fontSize(12).onClick(() => { this.reset(); this.settingsVisible = false; })
                  }
                  Button(this.encoded ? '纹理：SOG 编码（实验）' : '纹理：浮点').fontSize(12).enabled(this.paged && this.quality <= 4000000).onClick(() => {
                    this.encoded = !this.encoded; if (this.stream) { this.stream.encoded = this.encoded; this.stream.retry(); }
                  })
                  Button(this.paged ? '流式：稳定页（实验）' : '流式：兼容').fontSize(12).enabled(this.quality <= 4000000).onClick(() => {
                    this.paged = !this.paged;
                    if (this.stream) { this.stream.paged = this.paged; this.stream.retry(); }
                  })
                  Row({ space: 8 }) {
                    Text('高斯预算').fontSize(12).fontColor('#AFC6D8').layoutWeight(1)
                    Button(this.quality === 1000000 ? '100 万' : this.quality === 2000000 ? '200 万' : this.quality === 4000000 ? '400 万' : '800 万（实验）').fontSize(12).onClick(() => {
                      this.quality = this.quality === 1000000 ? 2000000 : this.quality === 2000000 ? 4000000 : this.quality === 4000000 && this.paged && this.encoded ? 8000000 : 1000000;
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
                  Text('环绕：单击聚焦；飞行：单击前往目标\n双击聚焦并进入环绕；游戏控制不触发点击导航\n左键或单指旋转 · 右键 / 中键平移\n双指平移和缩放 · 滚轮缩放\n1 环绕 · 2 飞行 · G 游戏控制 · WASD 移动 · Q/E 升降\nR 复位 · F 完整取景 · H 帮助 · 空格播放 / 暂停\n游戏控制支持左摇杆移动、右侧转头。步行与碰撞正在后续批次接入。')
                    .fontSize(12).fontColor('#B7C9DE').width('100%').lineHeight(22)
                }
                if (this.diagnosticsVisible) {
                  Button(this.gpuOptimized ? 'GPU 裁剪：开' : 'GPU 裁剪：关').fontSize(12).onClick(() => { this.gpuOptimized = !this.gpuOptimized; nativeRender.optimize(this.gpuOptimized); })
                  Button('连续转向 20 秒').fontSize(12).enabled(this.paged && !this.streamMeasuring).onClick(() => { this.measureContinuous(); })
                  Button('流式测量 20 次').fontSize(12).enabled(this.paged && !this.streamMeasuring).onClick(() => { this.measureStream(); })
                  Button('文件缓存测量 20 次').fontSize(12).enabled(this.paged && !this.streamMeasuring).onClick(() => { this.measureStream(true); })
                  Button('测量 10 秒').fontSize(12).enabled(this.benchmarkStart === 0).onClick(() => { this.benchmark(); })
                  if (this.benchmarkResult) { Text(this.benchmarkResult).fontSize(11).fontColor('#EBC478') }
                  Text(this.details).fontSize(11).fontColor('#99B1C4').width('100%')
                  Text(this.graphics).fontSize(10).fontColor('#6F899F').width('100%')
                }
              }.padding(16).width('100%')
            }.constraintSize({ maxHeight: 330 }).width('100%').backgroundColor('#EB222222').borderRadius(12)
          }
          if (this.profileReady && !this.modelsVisible && !this.settingsVisible && !this.helpVisible && !this.diagnosticsVisible) {
            if (this.showAnnotations && this.annotationCount > 0 && !(this.flyMode && this.gaming)) {
              if (this.annotationIndex >= 0 && this.markers.some((m: ViewerMarker) => m.index === this.annotationIndex)) {
                Column({ space: 6 }) {
                  Row() { Text(this.annotationTitle).fontSize(16).fontWeight(FontWeight.Medium).fontColor('#FFFFFF').layoutWeight(1)
                    Button('×').width(40).height(40).backgroundColor('#00000000').accessibilityText('关闭标注').onClick(() => { this.viewer.selected = -1; this.refreshAnnotations(); }) }
                  Scroll() { Text(this.annotationText).fontSize(13).lineHeight(21).fontColor('#DDDDDD').width('100%') }.constraintSize({ maxHeight: 150 })
                }.width('100%').padding(12).backgroundColor('#E6222222').borderRadius(12)
              }
              Row({ space: 8 }) {
                ViewerTool({ icon: $r('app.media.viewer_prev'), label: '上一个标注', action: () => { this.activateAnnotation(this.annotationIndex < 0 ? this.annotationCount - 1 : this.annotationIndex - 1); } })
                Text(this.annotationIndex < 0 ? `标注 · ${this.annotationCount}` : `${this.annotationIndex + 1} / ${this.annotationCount} · ${this.annotationTitle}`)
                  .fontColor('#FFFFFF').fontSize(12).maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis }).layoutWeight(1)
                ViewerTool({ icon: $r('app.media.viewer_next'), label: '下一个标注', action: () => { this.activateAnnotation(this.annotationIndex + 1); } })
              }.padding({ left: 6, right: 6 }).width('100%').backgroundColor('#B0222222').borderRadius(12)
            }
            Row({ space: 8 }) {
              Text(`${Math.floor(this.timeline / 60)}:${Math.floor(this.timeline % 60).toString().padStart(2, '0')}`).fontSize(10).fontColor('#FFFFFF').width(32)
              Slider({ value: this.timeline, min: 0, max: this.duration, step: .01 }).layoutWeight(1).trackColor('#666666').selectedColor('#FFFFFF')
                .id('动画时间轴').onChange((value: number) => { this.profilePlaying = false; this.viewer.interrupt(); this.timeline = value; if (this.viewer.profile) { this.applyProfilePose(this.viewer.profile.seek(value)); } })
            }.width('100%')
          }
          Row() {
            ViewerTool({ icon: this.profilePlaying ? $r('app.media.viewer_pause') : $r('app.media.viewer_play'), label: this.profilePlaying ? '暂停动画' : '播放动画', selected: this.profilePlaying, action: () => { this.play(); } })
            ViewerTool({ icon: $r('app.media.viewer_orbit'), label: '环绕', selected: !this.flyMode, action: () => { if (this.flyMode) { this.toggleMode(); } } })
            ViewerTool({ icon: $r('app.media.viewer_fly'), label: '飞行', selected: this.flyMode, action: () => { if (!this.flyMode) { this.toggleMode(); } } })
            ViewerTool({ icon: $r('app.media.viewer_help'), label: '帮助', selected: this.helpVisible, action: () => { this.helpVisible = !this.helpVisible; this.settingsVisible = false; this.modelsVisible = false; this.diagnosticsVisible = false; } })
            ViewerTool({ icon: $r('app.media.viewer_settings'), label: '设置', selected: this.settingsVisible, action: () => { this.settingsVisible = !this.settingsVisible; this.modelsVisible = false; this.helpVisible = false; this.diagnosticsVisible = false; } })
            ViewerTool({ icon: $r('app.media.viewer_folder'), label: '模型', selected: this.modelsVisible, action: () => { this.modelsVisible = !this.modelsVisible; this.settingsVisible = false; this.helpVisible = false; this.diagnosticsVisible = false; } })
            ViewerTool({ icon: $r('app.media.viewer_expand'), label: '沉浸显示', action: () => { this.toolsVisible = false; this.resetJoystick(); } })
          }.justifyContent(FlexAlign.SpaceBetween).padding(6).width('100%').backgroundColor('#B0222222').borderRadius(28)
        }.padding(16).width('100%').position({ x: 0, y: '100%' }).translate({ y: '-100%' })
        if (!this.settingsVisible && !this.modelsVisible && !this.helpVisible && !this.diagnosticsVisible) {
          Column({ space: 8 }) {
            ViewerTool({ icon: $r('app.media.viewer_plus'), label: '放大', selected: false, action: () => { this.controls.zoom(1 / 1.2); }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
            ViewerTool({ icon: $r('app.media.viewer_minus'), label: '缩小', selected: false, action: () => { this.controls.zoom(1.2); }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
          }.padding(16).position({ x: '100%', y: '50%' }).translate({ x: '-100%', y: '-50%' })
          if (this.flyMode && this.gaming) {
            Stack() {
              Circle().width(96).height(96).fill('#9928354A').stroke('#7794BD').strokeWidth(1)
              Circle().width(34).height(34).id('飞行摇杆手柄').fill('#B0CFFF').translate({ x: this.joyX * 30, y: this.joyY * 30 })
            }.width(96).height(96).position({ x: 20, y: '100%' }).translate({ y: -206 })
              .onDisAppear(() => { this.resetJoystick(); })
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
