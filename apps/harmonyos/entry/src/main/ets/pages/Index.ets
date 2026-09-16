import { HotspotPosition, ViewerHotspot } from './ViewerHotspot';
import { ViewerNavCursor } from './ViewerNavCursor';
import { ViewerCatalog, CatalogProject, CatalogAsset } from './ViewerCatalog';
import { ViewerCollision } from './ViewerCollision';
import { ViewerProjection } from './ViewerProjection';
import { ViewerTimeline } from './ViewerTimeline';
import { AnnotationPopup, annotationPopup, annotationGlyphs } from './ViewerAnnotations';
import { Scene } from '@kit.ArkGraphics3D';
import { util } from '@kit.ArkTS';
import { ViewerRuntime, ViewerMarker } from './ViewerRuntime';
import { ViewerBackend, OpenGLBackend, HuaweiBackend } from './ViewerBackend';
import { PointerInput, tapPosition } from './PointerInput';
import { comparisonSog } from './ComparisonSog';
import { KeyCode } from '@kit.InputKit';
import { common } from '@kit.AbilityKit';
import { fileIo as fs, picker } from '@kit.CoreFileKit';
import { BusinessError } from '@kit.BasicServicesKit';
import nativeRender from 'libsplat.so';
import { ViewerProfile } from './ViewerProfile';
import { ViewerPose, ViewerAnnotation, parseViewerSettings, createSettings, framePose } from './ViewerSettings';
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
  private catalog: ViewerCatalog = new ViewerCatalog();
  private catalogRevision: number = 0;
  private catalogLoading: boolean = false;
  private streamAsset: CatalogAsset | undefined = undefined;
  @State catalogVisible: boolean = false;
  @State catalogRows: CatalogProject[] = [];
  @State catalogPage: number = 1;
  @State catalogTotal: number = 0;
  @State catalogSearch: string = '';
  @State catalogMessage: string = '';
  @State catalogBusy: boolean = false;
  private async catalogList(page: number = 1): Promise<void> {
    const revision = ++this.catalogRevision; this.catalogBusy = true; this.catalogVisible = true; this.catalogMessage = '正在获取线上项目';
    try {
      const result = await this.catalog.list(page, this.catalogSearch);
      if (revision !== this.catalogRevision || !this.visible) { return; }
      this.catalogRows = result.rows; this.catalogTotal = result.total; this.catalogPage = page; this.catalogMessage = `共 ${result.total} 个项目 · 第 ${page} 页`;
    } catch (error) { if (revision === this.catalogRevision) { this.catalogMessage = String(error); } }
    finally { if (revision === this.catalogRevision) { this.catalogBusy = false; } }
  }
  private cancelCatalog(): void {
    this.catalog.cancel(); this.catalog = new ViewerCatalog(); this.catalogRevision++; this.generation++; this.catalogBusy = false; this.busy = false;
    if (this.catalogLoading) { this.stopStream(); } this.catalogLoading = false; this.catalogMessage = '已取消加载';
  }
  private async openCatalog(id: string): Promise<void> {
    if (this.busy) { return; }
    const revision = ++this.generation; this.busy = true; this.catalogLoading = true; this.localError = ''; this.catalogMessage = '正在读取项目配置';
    this.stopStream(); this.clearProfile();
    try {
      const asset = await this.catalog.detail(id);
      if (revision !== this.generation || !this.visible) { return; }
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      if (asset.project.modelType === 'lod') {
        if (this.backendName !== 'OpenGL') { throw new Error('线上整场流式请切换 OpenGL；华为瓦片尚未通过验证'); }
        this.sourcePath = ''; this.streamUrl = asset.content;
        await this.startStream(asset);
        if (revision !== this.generation || !this.visible || !this.stream?.manifest) { return; }
      } else {
        this.catalogMessage = '正在下载单体 SOG';
        const path = await this.catalog.model(asset, context.cacheDir);
        if (revision !== this.generation || !this.visible) { return; }
        const bounds = await nativeRender.inspectModel(path);
        if (revision !== this.generation || !this.visible) { return; }
        this.viewer.bounds = bounds; this.sourcePath = path; this.title = asset.project.modelTitle;
        this.awaitingModelCamera = this.backendName === 'OpenGL'; await this.backend.load({ path, bounds });
        this.applyCatalogSettings(asset);
      }
      this.catalogMessage = '项目已提交，正在显示'; this.modelsVisible = false;
    } catch (error) { if (revision === this.generation) { this.localError = String(error); this.catalogMessage = String(error); } }
    finally { if (revision === this.generation) { this.busy = false; this.catalogLoading = false; } }
  }
  private applyCatalogSettings(asset: CatalogAsset): void {
    this.viewer.configure(asset.settings); this.viewer.initial(); this.flyMode = this.controls.fly;
    this.duration = this.viewer.profile!.animation.duration; this.timeline = 0;
    this.profilePlaying = asset.settings.startMode === 'animTrack'; this.animationMode = this.profilePlaying;
    if (asset.collision) { this.collisionUrl = asset.collision; this.loadCollision(); }
    this.updateCamera();
  }
  @State collisionDebug: boolean = false;
  @State collisionDebugMessage: string = '';
  private debugCanvas: CanvasRenderingContext2D = new CanvasRenderingContext2D(new RenderingContextSettings(true));
  private debugReady: boolean = false;
  private debugPending: boolean = false;
  private debugRevision: number = 0;
  private debugAt: number = 0;
  private debugDrawAt: number = 0;
  private debugWire: number[] = [];
  private clearDebug(): void { this.debugRevision++; this.debugDrawAt = 0; this.debugWire = []; this.collisionDebugMessage = ''; this.drawDebug(); }
  private drawDebug(): void {
    if (!this.debugReady || !this.collisionDebug) { return; }
    const now = Date.now(); if (now - this.debugDrawAt < 100) { return; } this.debugDrawAt = now;
    const ctx = this.debugCanvas, w = this.controls.width, h = this.controls.height;
    ctx.clearRect(0, 0, w, h);
    if (!this.collisionDebug || !this.active) { return; }
    const projection = new ViewerProjection(this.viewer.pose(), w, h);
    const colors = ['#66F3DB', '#DA9BFF', '#FFCF68'];
    ctx.lineWidth = .8; ctx.globalAlpha = .7;
    for (let kind = 0; kind < 3; kind++) {
      ctx.beginPath(); ctx.strokeStyle = colors[kind];
      for (let i = 1; i + 6 < this.debugWire.length; i += 7) {
        if (this.debugWire[i] !== kind) { continue; }
        const line = projection.segment(this.debugWire.slice(i + 1, i + 4), this.debugWire.slice(i + 4, i + 7));
        if (line.length === 4) { ctx.moveTo(line[0], line[1]); ctx.lineTo(line[2], line[3]); }
      }
      ctx.stroke();
    }
  }
  private async refreshDebug(): Promise<void> {
    const session = this.collision;
    if (!this.collisionDebug || !session || this.debugPending || !this.active) { return; }
    this.debugPending = true; const revision = this.debugRevision, pose = this.viewer.pose();
    const direction = pose.target.map((v: number, i: number) => v - pose.position[i]), length = Math.hypot(...direction);
    const eye = pose.position.map((v: number, i: number) => v + direction[i] / length * 5);
    try {
      const wire = await nativeRender.collisionDebug(session.generation, eye[0], eye[1], eye[2]);
      if (revision !== this.debugRevision || this.collision !== session || !this.collisionDebug || !this.active) { return; }
      this.debugWire = wire;
      this.collisionDebugMessage = `碰撞透视：青色体素 · 紫色 GLB · 黄色已加载区域\n镜头前方 10 米范围 · ${Math.floor((wire.length - 1) / 7)} 条边${wire[0] ? '（显示量已限流）' : ''}`;
      this.drawDebug();
    } catch (error) { if (revision === this.debugRevision) { this.collisionDebugMessage = ((error as BusinessError).message || String(error)); } }
    finally { this.debugPending = false; }
  }
  @State walkMode: boolean = false;
  @State walkPending: boolean = false;
  @State collisionMessage: string = '';
  @State collisionPanel: boolean = false;
  @State collisionUrl: string = 'http://127.0.0.1:8780/voxel-tiles.json';
  private collision: ViewerCollision | undefined = undefined;
  private walkKeys: Set<string> = new Set();
  private jumpHeld: boolean = false;
  private jumpOwner: number = -1;
  private jumpQueued: boolean = false;
  private walkLogged: number = 0;
  private walkDestination: number[] | undefined = undefined;
  private walkRun: number = 1;
  private walkProgress: number = 0;
  private walkBlockedTime: number = 0;
  private async loadCollision(): Promise<void> {
    if (this.stream && !this.stream.manifest) { this.localError = '请先完成流式场景加载，再接入对应碰撞数据'; return; }
    this.stopCollision();
    const session = new ViewerCollision((message: string) => { if (this.collision === session) { this.collisionMessage = message; } });
    this.collision = session; this.collisionMessage = '正在读取碰撞数据';
    try { await session.open(this.collisionUrl, (this.getUIContext().getHostContext() as common.UIAbilityContext).cacheDir, this.viewer.pose().position); }
    catch (error) { if (this.collision === session) { this.collisionMessage = ((error as BusinessError).message || String(error)); } }
  }
  private leaveWalk(): void { this.walkMode = false; this.walkPending = false; this.walkDestination = undefined; this.walkKeys.clear(); this.jumpHeld = false; this.jumpQueued = false; this.jumpOwner = -1; nativeRender.collisionPause(); }
  private stopCollision(): void { this.leaveWalk(); this.clearDebug(); const old = this.collision; this.collision = undefined; old?.stop(); this.collisionMessage = ''; }
  private async enterWalk(): Promise<void> {
    if (!this.collision || this.walkPending) { this.localError = '请先在模型菜单加载碰撞数据'; return; }
    const session = this.collision; this.walkPending = true; this.stopAnimation(); this.viewer.interrupt(); this.controls.cancel(); this.resetJoystick();
    try {
      const eye = await session.enter(this.viewer.pose().position);
      if (!this.walkPending || this.collision !== session || !this.active) { return; }
      const pose = this.viewer.pose(), delta = pose.target.map((v: number, i: number) => v - pose.position[i]);
      this.viewer.apply({ position: eye, target: eye.map((v: number, i: number) => v + delta[i]), fov: 90 }, true);
      this.walkMode = true; this.flyMode = true; this.localError = ''; this.modelsVisible = false; this.settingsVisible = false; this.updateCamera();
    } catch (error) { this.localError = `步行出生点：${((error as BusinessError).message || String(error))}`; } finally { this.walkPending = false; }
  }
  private walkFrame(ms: number): void {
    const session = this.collision; if (!session) { this.leaveWalk(); return; }
    const before = this.controls.current.slice();
    const dt = Math.max(0, Math.min(ms / 1000, .064));
    let right = this.joyX + Number(this.walkKeys.has('d')) - Number(this.walkKeys.has('a'));
    let forward = -this.joyY + Number(this.walkKeys.has('w')) - Number(this.walkKeys.has('s'));
    let multiplier = this.walkKeys.has('shift') ? 2 : this.walkKeys.has('ctrl') ? .5 : 1;
    if (right || forward) { this.walkDestination = undefined; }
    if (this.walkDestination) {
      const eye = this.viewer.pose().position, dx = this.walkDestination[0] - eye[0], dz = this.walkDestination[2] - eye[2], distance = Math.hypot(dx, dz);
      this.walkBlockedTime = this.walkProgress - distance < .6 * dt ? this.walkBlockedTime + dt : 0; this.walkProgress = distance;
      if (distance < .5 || this.walkBlockedTime >= .2) { this.walkDestination = undefined; this.clearNav(); }
      else {
        const yaw = Math.atan2(-dx, -dz), diff = Math.atan2(Math.sin(yaw - this.controls.current[0]), Math.cos(yaw - this.controls.current[0]));
        this.controls.target[0] = this.controls.current[0] + Math.max(-192 * Math.PI / 180 * dt, Math.min(192 * Math.PI / 180 * dt, diff * 5 * dt));
        forward = Math.max(0, Math.cos(diff)); multiplier = this.walkRun;
      }
    }
    const length = Math.max(1, Math.hypot(right, forward));
    // The body is owned by native physics. UI touch/pinch may only rotate the camera.
    for (let i = 2; i < 6; i++) { this.controls.target[i] = this.controls.current[i]; }
    this.controls.tick(ms);
    try {
      const result = session.step(dt, this.controls.current[0], right / length * 4 * dt * multiplier, forward / length * 4 * dt * multiplier, this.jumpHeld || this.jumpQueued); this.jumpQueued = false;
      if (result.length !== 5) { return; }
      for (let i = 0; i < 3; i++) { this.controls.current[i + 3] = (result[i] - this.viewer.bounds[i]) / this.viewer.bounds[3]; this.controls.target[i + 3] = this.controls.current[i + 3]; }
      if (Date.now() - this.walkLogged >= 500) { this.walkLogged = Date.now(); console.info(`ViewerWalk x=${result[0]} y=${result[1]} z=${result[2]} grounded=${result[3]} blocked=${result[4]}`); }
      if (result[4]) { this.collisionMessage = '边界碰撞数据未就绪，移动暂停；可继续转动或切换飞行'; }
      if (before.some((value: number, i: number) => Math.abs(value - this.controls.current[i]) > 1e-7)) { this.updateCamera(); }
    } catch (error) { this.localError = ((error as BusinessError).message || String(error)); this.leaveWalk(); }
  }
  private navCursor: ViewerNavCursor = new ViewerNavCursor();
  private navCanvas: CanvasRenderingContext2D = new CanvasRenderingContext2D(new RenderingContextSettings(true));
  private navReady: boolean = false;
  private clearNav(): void { this.navCursor.clear(); this.drawNav(); }
  private drawNav(): void {
    if (!this.navReady) { return; }
    const canvas = this.navCanvas; canvas.clearRect(0, 0, this.controls.width, this.controls.height);
    if (!this.active || (this.flyMode && this.gaming)) { return; }
    const rings = this.navCursor.rings(this.viewer.pose(), this.controls.width, this.controls.height);
    if (!rings.length) { return; }
    canvas.beginPath();
    for (const ring of rings) {
      canvas.moveTo(ring[0], ring[1]);
      for (let i = 0; i < 12; i++) {
        const a = ((i + 11) % 12) * 2, b = i * 2, c = ((i + 1) % 12) * 2, d = ((i + 2) % 12) * 2;
        canvas.bezierCurveTo(ring[b] + (ring[c] - ring[a]) / 6, ring[b + 1] + (ring[c + 1] - ring[a + 1]) / 6,
          ring[c] - (ring[d] - ring[b]) / 6, ring[c + 1] - (ring[d + 1] - ring[b + 1]) / 6, ring[c], ring[c + 1]);
      }
      canvas.closePath();
    }
    canvas.fillStyle = 'rgba(255,255,255,0.6)'; canvas.fill('evenodd');
  }
  private comparisonBounds: number[] | undefined = undefined;
  private viewer: ViewerRuntime = new ViewerRuntime();
  private gl: OpenGLBackend = new OpenGLBackend();
  private huawei: HuaweiBackend = new HuaweiBackend((scene: Scene | undefined) => { this.spatialScene = scene; });
  private backend: ViewerBackend = this.gl;
  private sourcePath: string = '';
  private awaitingModelCamera: boolean = false;
  @State spatialScene: Scene | undefined = undefined;
  @State backendName: string = 'OpenGL';
  @State markers: HotspotPosition[] = [];
  @State annotationIndex: number = -1;
  @State annotationTitle: string = '';
  @State annotationText: string = '';
  @State annotationCount: number = 0;
  @State @Watch('syncAnnotationStyle') showAnnotations: boolean = true;
  @State @Watch('syncAnnotationStyle') gaming: boolean = false;
  @State annotationDepth: number = 0;
  @State @Watch('syncAnnotations') gpuAnnotations: boolean = false;
  @State hoveredAnnotation: number = -1;
  @State popupVisible: boolean = false;
  @State popup: AnnotationPopup = { x: 8, y: 8, width: 200, arrow: 16, flipped: false };
  private popupHeight: number = 100;
  private glyphs: ArrayBuffer | undefined = undefined;
  private annotatedProfile: ViewerProfile | undefined = undefined;
  private annotatedBackend: string = '';
  private annotatedGpu: boolean = false;
  private syncAnnotationStyle(): void {
    if (!this.visible) { return; }
    const visible = this.gpuAnnotations && this.toolsVisible && this.showAnnotations && !(this.flyMode && this.gaming && !this.animationMode);
    this.backend.annotationStyle(visible, this.hoveredAnnotation, this.getUIContext().vp2px(25));
  }
  private syncAnnotations(): void {
    if (this.annotatedProfile !== this.viewer.profile || this.annotatedBackend !== this.backendName || this.annotatedGpu !== this.gpuAnnotations) {
      this.annotatedProfile = this.viewer.profile; this.annotatedBackend = this.backendName; this.annotatedGpu = this.gpuAnnotations;
      try {
        const annotations: ViewerAnnotation[] = this.gpuAnnotations ? this.viewer.profile?.settings.annotations ?? [] : [];
        const positions = new Float32Array(annotations.length * 3);
        annotations.forEach((a: ViewerAnnotation, i: number) => { positions.set(a.position, i * 3); });
        if (annotations.length && !this.glyphs) { this.glyphs = annotationGlyphs(); }
        this.backend.annotations(positions.buffer as ArrayBuffer, this.glyphs ?? new ArrayBuffer(0));
      } catch (error) { console.warn(`Annotation atlas: ${((error as BusinessError).message || String(error))}`); }
    }
    this.syncAnnotationStyle();
  }
  private positionPopup(): void {
    const marker = this.viewer.anchor(this.viewer.selected); this.popupVisible = marker !== undefined;
    if (marker) { this.popup = annotationPopup(marker.x, marker.y, this.controls.width, this.controls.height, this.popupHeight); }
  }
  private closeAnnotation(): void { this.viewer.selected = -1; this.hoveredAnnotation = -1; this.refreshAnnotations(); }
  @State timeline: number = 0;
  @State duration: number = 20;
  @State profileReady: boolean = false;
  private timelineUpdated: number = 0;
  @State profilePlaying: boolean = false;
  @State @Watch('syncAnnotationStyle') animationMode: boolean = false;
  private scrubResume: boolean = false;
  private stopAnimation(): void { this.animationMode = false; this.profilePlaying = false; this.scrubResume = false; }
  private scrubAnimation(phase: string): void {
    if (phase === 'begin') {
      if (!this.animationMode) { return; }
      this.scrubResume = this.profilePlaying; this.profilePlaying = false; this.viewer.interrupt(); this.resetJoystick(); this.controls.cancel();
    } else {
      this.profilePlaying = phase === 'end' && this.animationMode && this.active && this.scrubResume; this.scrubResume = false;
    }
  }
  private seekAnimation(value: number): void {
    if (!this.animationMode || this.walkMode || !this.viewer.profile) { return; }
    this.timeline = value; this.applyProfilePose(this.viewer.profile.seek(value));
  }
  @StorageLink('nextnewsForeground') @Watch('foregroundChanged') foreground: boolean = true;
  private applyProfilePose(pose: ViewerPose): void {
    this.viewer.apply(pose, this.controls.fly); this.updateCamera();
  }
  private refreshAnnotations(): void {
    this.profileReady = this.viewer.profile !== undefined;
    const previous = this.markers;
    this.markers = this.viewer.markers().map((marker: ViewerMarker): HotspotPosition => {
      const anchor = previous.find((item: HotspotPosition) => item.index === marker.index);
      if (anchor) { anchor.update(marker); return anchor; } return new HotspotPosition(marker);
    }); this.annotationIndex = this.viewer.selected;
    this.annotationCount = this.viewer.profile?.settings.annotations.length ?? 0;
    const annotation = this.viewer.profile?.settings.annotations[this.annotationIndex];
    this.annotationTitle = annotation?.title ?? ''; this.annotationText = annotation?.text ?? '';
    this.positionPopup(); this.syncAnnotations();
  }
  private activateAnnotation(index: number): void { this.clearNav(); this.leaveWalk();
    this.stopAnimation(); this.autoOrbit = false; this.resetJoystick(); this.pickGeneration++;
    this.controls.setFly(false); this.flyMode = false; this.viewer.annotation(index); this.refreshAnnotations();
  }
  private play(): void { this.clearNav(); this.leaveWalk();
    if (!this.viewer.profile) { this.viewer.configure(createSettings(this.viewer.pose())); }
    this.duration = this.viewer.profile!.animation.duration;
    if (this.viewer.profile!.ended) { this.viewer.profile!.time = 0; }
    this.viewer.interrupt(); this.controls.cancel(); this.pointers.cancel(); this.resetJoystick(); this.autoOrbit = false;
    this.profilePlaying = !this.profilePlaying; this.animationMode = true; this.profileReady = true;
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
      this.leaveWalk(); this.stopAnimation(); this.viewer.configure(parseViewerSettings(text)); this.viewer.initial();
      const collisionUrl = this.viewer.profile!.settings.collisionUrl ?? this.viewer.profile!.settings.voxelManifestUrl ?? this.viewer.profile!.settings.voxelUrl;
      if (collisionUrl) { this.collisionUrl = collisionUrl; this.loadCollision(); } this.flyMode = this.controls.fly;
      this.duration = this.viewer.profile!.animation.duration; this.timeline = 0;
      this.profilePlaying = this.viewer.profile!.settings.startMode === 'animTrack'; this.animationMode = this.profilePlaying; this.modelsVisible = false; this.updateCamera();
    } catch (error) { this.localError = ((error as BusinessError).message || String(error)); }
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
      this.localError = ((error as BusinessError).message || String(error)); this.viewer.apply(snapshot.pose, snapshot.fly); this.updateCamera();
    } finally { this.busy = false; }
  }

  private async profileCamera(play: boolean): Promise<void> {
    this.leaveWalk(); this.stopAnimation(); this.controls.cancel(); this.resetJoystick();
    if (this.title !== '转换样例' && this.title !== '流式场景' && this.title !== '同源 SOG' && !this.viewer.profile) { this.viewer.configure(createSettings(this.viewer.pose())); }
    try {
      if (!this.viewer.profile) { this.viewer.profile = await ViewerProfile.load(this.getUIContext().getHostContext() as common.UIAbilityContext, this.viewer.bounds); }
      this.autoOrbit = false; this.benchmarkStart = 0; this.profilePlaying = play; this.animationMode = play;
      this.duration = this.viewer.profile.animation.duration;
      if (play) { this.applyProfilePose(this.viewer.profile.animation.sample(this.viewer.profile.time)); }
      else { this.viewer.initial(); this.flyMode = this.controls.fly; this.updateCamera(); }
    } catch (error) { this.localError = ((error as BusinessError).message || String(error)); }
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
  private mouseDistance: number = 0;
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
  @State @Watch('syncAnnotationStyle') toolsVisible: boolean = true;
  @State @Watch('menuChanged') settingsVisible: boolean = false;
  @State @Watch('menuChanged') modelsVisible: boolean = false;
  @State autoOrbit: boolean = false;
  @State flySpeed: number = 1;
  @State gpuOptimized: boolean = true;
  @State hint: string = '';
  @State joyX: number = 0;
  @State joyY: number = 0;
  @State @Watch('menuChanged') helpVisible: boolean = false;
  @State @Watch('menuChanged') diagnosticsVisible: boolean = false;
  private menuScroller: Scroller = new Scroller();
  private menuChanged(): void {
    setTimeout(() => { if (this.visible && (this.settingsVisible || this.modelsVisible || this.helpVisible || this.diagnosticsVisible)) {
      this.menuScroller.scrollTo({ xOffset: 0, yOffset: 0, animation: false });
    } }, 0);
  }
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
    this.scrubResume = false; this.clearDebug();
    this.active = this.foreground; this.motionTime = Date.now(); this.pickGeneration++; this.resetJoystick(); this.controls.cancel(); this.pointers.cancel();
    this.walkKeys.clear(); this.jumpHeld = false; this.jumpQueued = false; this.jumpOwner = -1; this.collision?.pause(!this.foreground);
    this.backend.active(this.foreground); if (this.stream) { this.stream.active = this.foreground; }
    if (this.foreground && this.backendName === 'Huawei' && this.sourcePath && !this.busy) { this.restoreHuawei(); }
  }
  private async restoreHuawei(): Promise<void> {
    const snapshot = this.viewer.snapshot(this.profilePlaying); this.busy = true;
    try {
      await this.huawei.load({ path: this.sourcePath, bounds: this.viewer.bounds });
      if (this.visible && this.backendName === 'Huawei') { this.viewer.apply(snapshot.pose, snapshot.fly); this.updateCamera(); }
    } catch (error) { this.localError = ((error as BusinessError).message || String(error)); } finally { this.busy = false; }
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
      if (this.active && !this.busy) { this.collision?.tick(this.viewer.pose().position); }
      if (this.active && !this.busy && now - this.debugAt >= 500) { this.debugAt = now; this.refreshDebug(); }
      if (this.active && !this.busy && this.walkMode) { this.walkFrame(dt);
      } else if (this.active && this.profilePlaying && this.viewer.profile) {
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
      if (this.backendName !== 'OpenGL') {
        this.loading = false; this.message = this.backend.capabilities.note;
        this.graphics = 'SpatialReconKit / ArkGraphics3D';
        this.details = '华为渲染帧率和显存需使用系统工具采集。当前接口未提供等价的 GPU/排序统计。';
        return;
      }
      const state = nativeRender.status();
      this.annotationDepth = state.annotationDepth;
      if (state.state === 'ready' && !this.stream && state.count > 0) { this.viewer.bounds = state.bounds; if (this.awaitingModelCamera) { this.awaitingModelCamera = false; this.updateCamera(); } }
      this.loading = state.state === 'loading';
      if (this.stream) { console.info(`StreamFrame state=${state.state} frames=${state.frames} count=${state.count} loadMs=${state.loadMs.toFixed(0)} frameMs=${state.frameMs.toFixed(1)} uploadMs=${state.uploadMs.toFixed(1)} uploadedRows=${state.uploadedRows} reusedRows=${state.reusedRows} decodedFiles=${state.decodedFiles} subsetHits=${state.subsetHits} request=${state.requestRevision} display=${state.displayRevision} prepareMs=${state.prepareMs.toFixed(1)} refineMs=${state.refineMs.toFixed(1)} uploadedBytes=${state.uploadedBytes} pageHits=${state.pageHits}`); }
      if (!this.busy && this.localError.length === 0) {
        this.message = state.state === 'ready' ? '官方导航 · 环绕聚焦 / 飞行前往 · 双击聚焦' : state.message;
      }
      this.graphics = state.graphics;
      this.details = `${state.count.toLocaleString()} 个高斯 · SH0\n加载 ${state.loadMs.toFixed(0)} ms · 排序 ${state.sortMs.toFixed(1)} ms\n单帧 ${state.frameMs.toFixed(1)} ms · 渲染耗时折算 ${state.fps.toFixed(1)} fps\n最近上传批次 ${state.uploadMs.toFixed(1)} ms · 上传/复用 ${state.uploadedRows}/${state.reusedRows} 行\n高斯 GPU ${state.gpuMs < 0 ? '计时不可用' : state.gpuMs.toFixed(2) + ' ms'}\n模型缓冲估算 ${(state.bytes / 1048576).toFixed(1)} MiB\n相机 ${this.controls.current.map((v: number) => v.toFixed(3)).join(",")} · ${this.flyMode ? "fly" : "orbit"}`;
    }, 500);
  }
  aboutToDisappear(): void {
    this.visible = false; this.catalog.cancel(); this.catalogRevision++; this.stopStream(); this.stopCollision();
    this.generation++;
    if (this.timer !== -1) { clearInterval(this.timer); this.timer = -1; }
    if (this.motionTimer !== -1) { clearInterval(this.motionTimer); this.motionTimer = -1; }
    this.controls.cancel(); this.pointers.cancel(); this.resetJoystick();
    this.backend.dispose();
  }
  onPageShow(): void { this.collision?.pause(false); this.active = true; if (this.stream) { this.stream.active = true; } this.motionTime = Date.now(); this.backend.active(true); }
  onPageHide(): void { this.clearNav(); this.scrubResume = false; this.clearDebug(); this.collision?.pause(true); this.walkKeys.clear(); this.jumpHeld = false; this.jumpQueued = false; this.jumpOwner = -1; this.pickGeneration++; this.benchmarkStart = 0; this.active = false; this.resetJoystick(); if (this.stream) { this.stream.active = false; } this.controls.cancel(); this.pointers.cancel(); this.backend.active(false); }
  private updateCamera(): void {
    this.backend.camera(this.viewer.pose(), this.viewer.bounds); this.refreshAnnotations(); this.drawDebug(); this.drawNav();
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
    } catch (error) { this.benchmarkResult = `测量未通过：${((error as BusinessError).message || String(error))}`; console.info(`StreamTrial failure=${((error as BusinessError).message || String(error))}`); }
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
  private setMode(fly: boolean): void { this.clearNav(); this.leaveWalk(); this.viewer.interrupt(); this.stopAnimation(); this.autoOrbit = false; this.resetJoystick(); this.pickGeneration++; this.flyMode = fly; this.controls.setFly(fly); this.updateCamera(); }
  private toggleMode(): void { this.setMode(!this.flyMode); }
  private async focusAt(x: number, y: number, doubleTap: boolean = false): Promise<void> {
    if (this.flyMode && this.gaming) { return; }
    const request = ++this.pickGeneration;
    try { const point = await this.backend.pick(x / this.controls.width, y / this.controls.height, this.viewer.bounds);
      if (request === this.pickGeneration && this.active) { if (point.length !== 3) { this.clearNav(); return; }
        this.navCursor.set(point.map((v: number, i: number) => this.viewer.bounds[i] + v * this.viewer.bounds[3]), this.viewer.pose(), this.walkMode ? 'walk' : doubleTap || !this.flyMode ? 'orbit' : 'fly'); this.drawNav();
        if (this.walkMode) {
          this.walkDestination = point.map((v: number, i: number) => this.viewer.bounds[i] + v * this.viewer.bounds[3]); this.walkRun = doubleTap ? 2 : 1;
          const eye = this.viewer.pose().position; this.walkProgress = Math.hypot(this.walkDestination[0] - eye[0], this.walkDestination[2] - eye[2]); this.walkBlockedTime = 0; return;
        }
        this.viewer.navigate(point, doubleTap); this.flyMode = this.controls.fly; this.updateCamera(); }
    } catch (error) { console.warn(`Focus: ${((error as BusinessError).message || String(error))}`); }
  }
  private frame(): void { this.clearNav(); this.leaveWalk(); this.viewer.interrupt(); this.stopAnimation(); this.controls.setFly(false); this.flyMode = false; this.viewer.transition(framePose(this.viewer.bounds, this.controls.fov)); }
  private reset(): void { this.clearNav();
    if (this.walkMode) { const eye = nativeRender.collisionReset(); for (let i = 0; i < 3; i++) { this.controls.current[i + 3] = (eye[i] - this.viewer.bounds[i]) / this.viewer.bounds[3]; this.controls.target[i + 3] = this.controls.current[i + 3]; } this.walkDestination = undefined; this.updateCamera(); return; }
    this.stopAnimation(); this.autoOrbit = false; this.resetJoystick(); this.pickGeneration++; this.viewer.interrupt();
    this.benchmarkStart = 0; this.benchmarkResult = ''; this.controls.cancel();
    if (this.viewer.profile) { this.viewer.profile.time = 0; this.viewer.selected = -1; this.viewer.initial(); }
    else { this.controls.reset(); this.controls.fov = 85; this.viewer.apply(framePose(this.viewer.bounds), false); }
    this.flyMode = this.controls.fly; this.timeline = 0; this.updateCamera();
  }
  private clearProfile(): void { this.clearNav(); this.stopCollision(); this.stopAnimation(); this.viewer.profile = undefined; this.profileReady = false; this.viewer.selected = -1; this.annotationCount = 0; this.markers = []; this.annotationIndex = -1; this.timeline = 0; this.popupVisible = false; this.syncAnnotations(); }
  private touch(event: TouchEvent): void {
    if (event.source === SourceType.Mouse) { return; }
    if (event.type === TouchType.Down) { this.clearNav(); this.pickGeneration++; }
    if (event.type === TouchType.Down && this.viewer.selected >= 0) { this.closeAnnotation(); }
    this.viewer.interrupt();
    this.stopAnimation();
    this.autoOrbit = false;
    const points = this.pointers.viewport(this.touchPhase(event.type), this.touchPoints(event.touches), this.touchPoints(event.changedTouches));
    this.controls.touch(points, this.panMode);
  }
  private gestureTap(event: GestureEvent, doubleTap: boolean): void {
    if (this.pointers.blockTap || this.pointers.stickId >= 0) { return; }
    // HarmonyOS can deliver a sparse fingerList after multi-touch release.
    const point = tapPosition(event.fingerList);
    if (point.length !== 2 || point[0] < 0 || point[1] < 0 || point[0] > this.controls.width || point[1] > this.controls.height) { return; }
    this.focusAt(point[0], point[1], doubleTap);
  }
  private touchPhase(type: TouchType): string {
    return type === TouchType.Down ? 'down' : type === TouchType.Up ? 'up' : type === TouchType.Cancel ? 'cancel' : 'move';
  }
  private touchPoints(points: TouchObject[]): OrbitTouch[] {
    return points.map((p: TouchObject): OrbitTouch => { return { id: p.id, x: p.x, y: p.y }; });
  }
  private resetJoystick(): void { this.pointers.cancelStick(); this.joyX = 0; this.joyY = 0; }

  private mouse(event: MouseEvent): void {
    if (event.action === MouseAction.Press || this.mouseButton !== MouseButton.None) { this.viewer.interrupt(); this.stopAnimation(); }
    if (event.action === MouseAction.Press) {
      this.clearNav(); this.pickGeneration++;
      if (this.viewer.selected >= 0) { this.closeAnnotation(); }
      this.autoOrbit = false; if (this.pointers.stickId < 0) { this.pointers.blockTap = false; }
      this.getUIContext().getFocusController().requestFocus('viewer-surface');
      this.mouseDistance = 0; this.mouseButton = event.button; this.mouseX = event.x; this.mouseY = event.y;
    } else if (event.action === MouseAction.Release) {
      console.info(`ViewerMouse release yaw=${this.controls.target[0]} pitch=${this.controls.target[1]}`);
      this.mouseButton = MouseButton.None;
    } else if (event.action === MouseAction.Move && this.mouseButton !== MouseButton.None) {
      const dx = event.x - this.mouseX; const dy = event.y - this.mouseY;
      this.mouseDistance += Math.abs(dx) + Math.abs(dy); if (this.mouseDistance >= 15) { this.pointers.blockTap = true; }
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
      case KeyCode.KEYCODE_3: key = '3'; break;
      case KeyCode.KEYCODE_SHIFT_LEFT: key = 'shift'; break;
      case KeyCode.KEYCODE_CTRL_LEFT: key = 'ctrl'; break;
      case KeyCode.KEYCODE_ESCAPE: key = 'escape'; break;
    }
    if (this.walkMode && ['w', 'a', 's', 'd', 'shift', 'ctrl', ' '].includes(key)) {
      if (key === ' ') { this.jumpHeld = event.type === KeyType.Down; if (this.jumpHeld) { this.jumpQueued = true; } } else if (event.type === KeyType.Down) { this.walkKeys.add(key); } else { this.walkKeys.delete(key); }
      return true;
    }
    if (event.type === KeyType.Down && key === '3') { this.enterWalk(); return true; }
    console.info(`ViewerKey ${event.keyCode} type=${event.type} key=${key}`);
    if (event.type === KeyType.Down && key === 'escape') { this.viewer.interrupt(); this.profilePlaying = false; this.gaming = false; this.viewer.gaming = false; this.toolsVisible = true; this.helpVisible = this.settingsVisible = this.modelsVisible = false; this.resetJoystick(); return true; }
    if (event.type === KeyType.Down && key === 'h') { this.helpVisible = !this.helpVisible; return true; }
    if (event.type === KeyType.Down && key === 'f') { this.frame(); return true; }
    if (event.type === KeyType.Down && key === ' ') { this.play(); return true; }
    if (event.type === KeyType.Down && key === 'g') { this.gaming = !this.gaming; this.viewer.gaming = this.gaming; this.resetJoystick(); return true; }
    if (event.type === KeyType.Down && (key === '1' || key === '2')) { this.setMode(key === '2'); return true; }
    if (key === 'r' && event.type === KeyType.Down) { this.reset(); return true; }
    if ('wasdqe'.includes(key) && key.length === 1) {
      this.viewer.interrupt(); if (event.type === KeyType.Down) { this.stopAnimation(); if (!this.flyMode) { this.toggleMode(); } } this.gaming = true; this.viewer.gaming = true;
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
  private async startStream(asset?: CatalogAsset): Promise<void> {
    const selected = asset ?? (this.streamAsset?.content === this.streamUrl ? this.streamAsset : undefined);
    this.streamAsset = selected;
    if (this.backendName !== 'OpenGL') { this.localError = '华为原生瓦片请使用独立实验入口'; return; }
    this.stopStream(); this.clearProfile(); this.localError = ''; this.title = '流式场景'; this.reset();
    const session = new StreamSession((message: string) => { if (this.stream === session) { this.streamMessage = message; } });
    this.stream = session; session.budget = this.quality; session.paged = this.paged; session.encoded = this.encoded;
    try {
      const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
      await session.open(this.streamUrl, context.cacheDir, this.fullLoad);
      if (this.stream === session) {
        this.viewer.bounds = session.manifest!.bounds.slice();
        if (selected) { this.title = selected.project.modelTitle; this.applyCatalogSettings(selected); }
        else if (this.streamUrl === 'http://127.0.0.1:8768/scene.json') { await this.profileCamera(false); }
        session.tick(this.controls.current, this.controls.fly, this.controls.fov, this.controls.width / Math.max(1, this.controls.height));
      }
    } catch (error) { if (this.stream === session) { this.streamMessage = `流式失败：${((error as BusinessError).message || String(error))}`; } }
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
  private async annotationFixture(): Promise<void> {
    this.gpuAnnotations = true;
    await this.sample('annotation-fixture.ply', '标注遮挡夹具');
    if (this.localError || this.title !== '标注遮挡夹具') { return; }
    const request = this.generation;
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    const data = await context.resourceManager.getRawFileContent('annotation-fixture.json');
    if (!this.visible || request !== this.generation) { return; }
    this.viewer.configure(parseViewerSettings(new util.TextDecoder('utf-8', { fatal: true }).decodeWithStream(data)));
    this.viewer.initial(); this.flyMode = this.controls.fly; this.diagnosticsVisible = false; this.updateCamera();
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
    } catch (error) { this.localError = ((error as BusinessError).message || String(error)); } finally { this.busy = false; }
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
    this.stopAnimation(); this.viewer.interrupt();
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
        .onBlur(() => { this.walkKeys.clear(); this.jumpHeld = false; this.jumpQueued = false; this.jumpOwner = -1; this.controls.cancel(); this.pointers.cancel(); this.resetJoystick(); this.mouseButton = MouseButton.None; })
        .parallelGesture(GestureGroup(GestureMode.Exclusive,
          TapGesture({ count: 2 }).onAction((event: GestureEvent) => { this.gestureTap(event, true); }),
          TapGesture({ count: 1 }).onAction((event: GestureEvent) => { this.gestureTap(event, false); })
        ))
        .onKeyEvent((event: KeyEvent) => this.handleKey(event))
        .onAxisEvent((event: AxisEvent) => { this.stopAnimation(); this.controls.zoom(Math.exp(Math.max(-1, Math.min(1, event.getVerticalAxisValue() * 0.002)))); });

      if (this.collisionDebug) {
        Canvas(this.debugCanvas).width('100%').height('100%').hitTestBehavior(HitTestMode.None)
          .onReady(() => { this.debugReady = true; this.drawDebug(); })
          .onDisAppear(() => { this.debugReady = false; })
      }
      Canvas(this.navCanvas).width('100%').height('100%').hitTestBehavior(HitTestMode.None).id('navigation-target')
        .onReady(() => { this.navReady = true; this.drawNav(); }).onDisAppear(() => { this.navReady = false; })
      if (this.toolsVisible && this.showAnnotations && !(this.flyMode && this.gaming && !this.animationMode)) {
        ForEach(this.markers, (marker: HotspotPosition) => {
          ViewerHotspot({ marker, hovered: this.hoveredAnnotation === marker.index,
            draw: this.backendName !== 'OpenGL' || !this.gpuAnnotations || this.annotationDepth === 0,
            activate: () => { this.activateAnnotation(marker.index); },
            hover: (value: boolean) => { this.hoveredAnnotation = value ? marker.index : this.hoveredAnnotation === marker.index ? -1 : this.hoveredAnnotation; this.syncAnnotationStyle(); } })
        }, (marker: HotspotPosition) => marker.index.toString())
      }
      Column({ space: 6 }) {
        Row() {
          Column({ space: 3 }) {
            Text('NEXTNEWS').fontSize(10).fontColor('#8CA7CB').letterSpacing(2).width('100%')
            Text(this.toolsVisible ? this.title : '').fontSize(14).fontWeight(FontWeight.Medium).fontColor('#EEF4FF').width('100%')
          }.layoutWeight(1)
          Button('模型列表').fontSize(13).height(40).backgroundColor('#CC243247').fontColor('#FFFFFF').id('模型列表').onClick(() => { this.toolsVisible = true; this.modelsVisible = true; this.settingsVisible = this.helpVisible = this.diagnosticsVisible = false; this.catalogList(); })
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
        if (this.collisionDebug && this.collisionDebugMessage) {
          Text(this.collisionDebugMessage).fontSize(10).fontColor('#66F3DB').width('100%').backgroundColor('#99000000')
        }
      }.padding(16).width('100%').backgroundColor('#00000000')
      if (this.toolsVisible) {
        Column({ space: 8 }) {
          if (this.hint) { Text(this.hint).fontSize(12).fontColor('#EEF4FF').padding(6).backgroundColor('#DD1C283B').borderRadius(6) }
          if (this.settingsVisible || this.modelsVisible || this.helpVisible || this.diagnosticsVisible) {
            Scroll(this.menuScroller) {
              Column({ space: 12 }) {
                if (this.modelsVisible) {
                  Text(this.catalogVisible ? '线上项目' : '模型库').fontColor('#EEF4FF').fontSize(16).width('100%')
                  if (this.catalogVisible) { Button('返回本地工具').fontSize(12).onClick(() => { this.catalogVisible = false; }) }
                  Button('线上项目 · 数科 3DGS').width('100%').fontSize(14).enabled(!this.busy).onClick(() => { this.catalogList(); })
                  if (this.catalogVisible) {
                    TextInput({ text: this.catalogSearch, placeholder: '搜索项目名称' }).fontSize(13).fontColor('#EEF4FF').placeholderColor('#BBD0E4').backgroundColor('#28354A').onChange((value: string) => { this.catalogSearch = value; }).onSubmit(() => { this.catalogList(); })
                    Row({ space: 8 }) {
                      Button('搜索').fontSize(12).enabled(!this.catalogBusy && !this.busy).onClick(() => { this.catalogList(); })
                      Button('取消加载').fontSize(12).onClick(() => { this.cancelCatalog(); })
                    }
                    Text(this.catalogMessage).fontSize(12).fontColor('#BBD0E4')
                    ForEach(this.catalogRows, (project: CatalogProject) => {
                      Button({ type: ButtonType.Normal }) {
                        Row({ space: 10 }) {
                          if (project.thumbnailUrl || project.coverUrl) { Image((project.thumbnailUrl || project.coverUrl || '').trim()).width(60).height(44).objectFit(ImageFit.Cover).borderRadius(6) }
                          Column({ space: 3 }) {
                            Text(project.modelTitle).fontSize(13).fontColor('#EEF4FF').maxLines(2).width('100%')
                            Text(project.modelType === 'lod' ? '渐进流式' : '单体模型').fontSize(11).fontColor('#BBD0E4').width('100%')
                          }.layoutWeight(1)
                        }.padding(8)
                      }.width('100%').backgroundColor('#243247').borderRadius(8).enabled(!this.busy).onClick(() => { this.openCatalog(project.modelId); })
                    }, (project: CatalogProject) => project.modelId)
                    Row({ space: 8 }) {
                      Button('上一页').fontSize(12).enabled(!this.catalogBusy && !this.busy && this.catalogPage > 1).onClick(() => { this.catalogList(this.catalogPage - 1); })
                      Button('下一页').fontSize(12).enabled(!this.catalogBusy && !this.busy && this.catalogPage * 20 < this.catalogTotal).onClick(() => { this.catalogList(this.catalogPage + 1); })
                    }
                  }
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
                  Button('碰撞 / 体素（实验）').fontSize(12).onClick(() => { this.collisionPanel = !this.collisionPanel; })
                  if (this.collisionPanel) {
                    TextInput({ text: this.collisionUrl, placeholder: 'collisionUrl / voxelUrl / voxelManifestUrl' }).id('collision-address').fontSize(12).onChange((value: string) => { this.collisionUrl = value; })
                    Row({ space: 8 }) { Button('加载碰撞').fontSize(12).onClick(() => { this.loadCollision(); }); Button('重试碰撞').fontSize(12).onClick(() => { this.collision?.retry(); }); Button('停止碰撞').fontSize(12).onClick(() => { this.stopCollision(); }) }
                    Text(this.collisionMessage).fontSize(12).fontColor('#BBD0E4')
                    Button(this.collisionDebug ? '关闭碰撞显示' : '显示碰撞边线').fontSize(12).onClick(() => { this.collisionDebug = !this.collisionDebug; this.clearDebug(); this.refreshDebug(); })
                  }
                  Button('导入 viewer-settings.json').fontSize(12).onClick(() => { this.importSettings(); })
                  Button('同源 SOG 对照').enabled(!this.busy).onClick(() => { this.loadComparison(); })
                  Button('打开本地 PLY / SOG').fontSize(13).width('100%').enabled(!this.busy).onClick(() => { this.importModel(); })
                  Button('流式').fontSize(12).onClick(() => { this.streamPanel = !this.streamPanel; })
                  if (this.streamPanel) {
                    TextInput({ text: this.streamUrl, placeholder: 'HTTP(S) lod-meta.json / scene.json' }).selectAll(true).fontSize(12)
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
                  Button(this.gpuAnnotations ? '深度标注：开（实验）' : '深度标注：关').fontSize(12).enabled(this.backendName === 'OpenGL').onClick(() => { this.gpuAnnotations = !this.gpuAnnotations; })
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
                  Text('环绕：单击聚焦；飞行：单击前往目标\n双击聚焦并进入环绕；游戏控制不触发点击导航\n左键或单指旋转 · 右键 / 中键平移\n双指平移和缩放 · 滚轮缩放\n1 环绕 · 2 飞行 · G 游戏控制 · WASD 移动 · Q/E 升降\nR 复位 · F 完整取景 · H 帮助 · 空格播放 / 暂停\n游戏控制支持左摇杆移动、右侧转头。3 步行（需碰撞数据），步行空格跳跃；数据缺失时暂停跨界。')
                    .fontSize(12).fontColor('#B7C9DE').width('100%').lineHeight(22)
                }
                if (this.diagnosticsVisible) {
                  Button('标注遮挡夹具').fontSize(12).enabled(!this.busy).onClick(() => { this.annotationFixture().catch((e: Error) => { this.localError = e.message; }); })
                  Text(this.backendName === 'OpenGL' ? `标注深度：${this.annotationDepth > 0 ? this.annotationDepth + ' bit' : '二维回退 / 尚未就绪'}` : '标注深度：华为二维回退（未验证等价遮挡）').fontSize(11).fontColor('#99B1C4')
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
            }.constraintSize({ maxHeight: this.modelsVisible && this.catalogVisible ? Math.max(200, this.controls.height - 160) : 330 }).width('100%').backgroundColor('#EB222222').borderRadius(12)
          }
          if (this.profileReady && !this.modelsVisible && !this.settingsVisible && !this.helpVisible && !this.diagnosticsVisible) {
            if (this.showAnnotations && this.annotationCount > 0 && !(this.flyMode && this.gaming && !this.animationMode)) {
              Row({ space: 8 }) {
                ViewerTool({ icon: $r('app.media.viewer_prev'), label: '上一个标注', action: () => { this.activateAnnotation(this.annotationIndex < 0 ? this.annotationCount - 1 : this.annotationIndex - 1); } })
                Text(this.annotationIndex < 0 ? `标注 · ${this.annotationCount}` : `${this.annotationIndex + 1} / ${this.annotationCount} · ${this.annotationTitle}`)
                  .fontColor('#FFFFFF').fontSize(12).maxLines(1).textOverflow({ overflow: TextOverflow.Ellipsis }).layoutWeight(1)
                ViewerTool({ icon: $r('app.media.viewer_next'), label: '下一个标注', action: () => { this.activateAnnotation(this.annotationIndex + 1); } })
              }.padding({ left: 6, right: 6 }).width('100%').backgroundColor('#B0222222').borderRadius(12)
            }
            if (this.animationMode) { Row({ space: 8 }) {
              Text(`${Math.floor(this.timeline / 60)}:${Math.floor(this.timeline % 60).toString().padStart(2, '0')}`).fontSize(10).fontColor('#FFFFFF').width(32)
              ViewerTimeline({ active: this.foreground, value: this.timeline, duration: this.duration, seek: (value: number) => { this.seekAnimation(value); }, drag: (phase: string) => { this.scrubAnimation(phase); } }).layoutWeight(1)
            }.width('100%') }
          }
          Row() {
            ViewerTool({ icon: this.profilePlaying ? $r('app.media.viewer_pause') : $r('app.media.viewer_play'), label: this.profilePlaying ? '暂停动画' : '播放动画', selected: this.profilePlaying, action: () => { this.play(); } })
            ViewerTool({ icon: $r('app.media.viewer_orbit'), label: '环绕', selected: !this.flyMode && !this.animationMode, action: () => { this.setMode(false); } })
            ViewerTool({ icon: $r('app.media.viewer_fly'), label: '飞行', selected: this.flyMode && !this.walkMode && !this.animationMode, action: () => { this.setMode(true); } })
            ViewerTool({ icon: $r('app.media.viewer_walk'), label: '步行', selected: this.walkMode, action: () => { this.enterWalk(); } })
            ViewerTool({ icon: $r('app.media.viewer_help'), label: '帮助', selected: this.helpVisible, action: () => { this.helpVisible = !this.helpVisible; this.settingsVisible = false; this.modelsVisible = false; this.diagnosticsVisible = false; } })
            ViewerTool({ icon: $r('app.media.viewer_settings'), label: '设置', selected: this.settingsVisible, action: () => { this.settingsVisible = !this.settingsVisible; this.modelsVisible = false; this.helpVisible = false; this.diagnosticsVisible = false; } })
            ViewerTool({ icon: $r('app.media.viewer_folder'), label: '模型', selected: this.modelsVisible, action: () => { this.modelsVisible = !this.modelsVisible; this.settingsVisible = false; this.helpVisible = false; this.diagnosticsVisible = false; } })
            ViewerTool({ icon: $r('app.media.viewer_expand'), label: '沉浸显示', action: () => { this.toolsVisible = false; this.resetJoystick(); } })
          }.justifyContent(FlexAlign.SpaceBetween).padding(6).width('100%').backgroundColor('#B0222222').borderRadius(28)
        }.padding(16).width('100%').position({ x: 0, y: '100%' }).translate({ y: '-100%' })
        if (!this.settingsVisible && !this.modelsVisible && !this.helpVisible && !this.diagnosticsVisible) {
          Column({ space: 8 }) {
            ViewerTool({ icon: $r('app.media.viewer_plus'), label: '放大', selected: false, action: () => { this.stopAnimation(); this.controls.zoom(1 / 1.2); }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
            ViewerTool({ icon: $r('app.media.viewer_minus'), label: '缩小', selected: false, action: () => { this.stopAnimation(); this.controls.zoom(1.2); }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
          }.padding(16).position({ x: '100%', y: '50%' }).translate({ x: '-100%', y: '-50%' })
          if (this.flyMode && this.gaming && !this.animationMode) {
            Stack() {
              Circle().width(96).height(96).fill('#9928354A').stroke('#7794BD').strokeWidth(1)
              Circle().width(34).height(34).id('飞行摇杆手柄').fill('#B0CFFF').translate({ x: this.joyX * 30, y: this.joyY * 30 })
            }.width(96).height(96).position({ x: 20, y: '100%' }).translate({ y: -206 })
              .onDisAppear(() => { this.resetJoystick(); })
              .accessibilityText('飞行摇杆').id('飞行摇杆').onTouch((event: TouchEvent) => { this.joystick(event); })
            Column({ space: 8 }) {
              if (this.walkMode) {
                Button('跳跃').width(64).height(64).borderRadius(32).backgroundColor('#A0222222').onTouch((event: TouchEvent) => { event.stopPropagation();
                  if (event.type === TouchType.Cancel) { this.jumpOwner = -1; this.jumpHeld = false; }
                  else if (event.type === TouchType.Down && this.jumpOwner < 0) { const point = event.changedTouches.find((p: TouchObject) => p.x >= 0 && p.y >= 0 && p.x <= 64 && p.y <= 64); if (point) { this.jumpOwner = point.id; this.jumpHeld = true; this.jumpQueued = true; } }
                  else if (event.type === TouchType.Up && event.changedTouches.some((p: TouchObject) => p.id === this.jumpOwner)) { this.jumpOwner = -1; this.jumpHeld = false; } })
              } else {
              ViewerTool({ icon: $r('app.media.viewer_up'), label: '上升', selected: false, action: () => { this.controls.move(0, .12 * this.flySpeed, 0); }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
              ViewerTool({ icon: $r('app.media.viewer_down'), label: '下降', selected: false, action: () => { this.controls.move(0, -.12 * this.flySpeed, 0); }, onHoverChange: (label: string) => { this.hint = label; }, onKey: (event: KeyEvent) => this.handleKey(event) })
              }
            }.position({ x: '100%', y: '100%' }).translate({ x: -62, y: -206 })
          }
        }
      }
      if (this.toolsVisible && this.showAnnotations && this.popupVisible && !(this.flyMode && this.gaming && !this.animationMode) && !this.modelsVisible && !this.settingsVisible && !this.helpVisible && !this.diagnosticsVisible) {
        Stack({ alignContent: Alignment.TopStart }) {
          Column({ space: 4 }) {
            Row() {
              Text(this.annotationTitle).fontSize(14).fontWeight(FontWeight.Bold).fontColor('#FFFFFF').layoutWeight(1).maxLines(3).textOverflow({ overflow: TextOverflow.Ellipsis })
              Button('×').width(32).height(44).fontSize(20).fontColor('#FFFFFF').backgroundColor('#00000000').accessibilityText('关闭标注').id('关闭标注').onClick(() => { this.closeAnnotation(); })
            }.width('100%')
            Scroll() { Text(this.annotationText).fontSize(14).lineHeight(20).fontColor('#FFFFFF').width('100%') }
              .constraintSize({ maxHeight: Math.max(40, Math.min(240, this.controls.height - 100)) })
          }.padding(8).width(this.popup.width).backgroundColor('#CC000000').borderRadius(4)
            .onAreaChange((_old: Area, area: Area) => { if (Math.abs(this.popupHeight - Number(area.height)) > .5) { this.popupHeight = Number(area.height); this.positionPopup(); } })
          Polygon().points(this.popup.flipped ? [[0, 0], [8, 8], [0, 16]] : [[8, 0], [0, 8], [8, 16]])
            .width(8).height(16).fill('#CC000000').position({ x: this.popup.flipped ? this.popup.width : -8, y: this.popup.arrow - 8 })
        }.width(this.popup.width).position({ x: this.popup.x, y: this.popup.y }).id('annotation-popup')
          .onTouch((event: TouchEvent) => { event.stopPropagation(); }).onClick(() => {})
      }
    }.width('100%').height('100%').backgroundColor('#091322').clip(true)
  }
}
