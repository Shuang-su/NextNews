// Native presentation of MetaFlow ui.ts/index.scss loading semantics.
@Component
export struct ViewerLoading {
  @Prop @Watch('stageChanged') stage: string = '';
  @Prop failed: boolean = false;
  @Prop progress: number = -1;
  onHidden: () => void = () => {};
  @State elapsed: string = '0.0';
  private canvas: CanvasRenderingContext2D = new CanvasRenderingContext2D(new RenderingContextSettings(true));
  private timer: number = -1;
  private started: number = 0;
  private barWidth: number = 300;
  private ready: boolean = false;
  private stageChanged(): void { this.started = Date.now(); this.elapsed = '0.0'; }
  aboutToAppear(): void {
    this.stageChanged();
    this.timer = setInterval(() => {
      if (this.failed) { this.paint(0); return; }
      const seconds = (Date.now() - this.started) / 1000;
      this.elapsed = seconds.toFixed(1); this.paint(seconds);
    }, 33);
  }
  aboutToDisappear(): void { if (this.timer !== -1) { clearInterval(this.timer); this.timer = -1; } this.onHidden(); }
  private paint(seconds: number): void {
    if (!this.ready) { return; }
    const c = this.canvas, w = this.barWidth;
    c.clearRect(0, 0, w, 10);
    if (this.failed) { c.fillStyle = '#E6FF6C6C'; c.fillRect(0, 0, w, 10); return; }
    if (this.progress >= 0) {
      c.fillStyle = '#FFFFFF'; c.fillRect(0, 0, w, 10);
      c.fillStyle = '#42D2F6'; c.fillRect(0, 0, w * Math.min(100, this.progress) / 100, 10); return;
    }
    const x = ((seconds % 1.2) / 1.2 * 3 - 1) * w;
    const gradient = c.createLinearGradient(x - w / 2, 0, x + w / 2, 0);
    gradient.addColorStop(0, '#0042D2F6'); gradient.addColorStop(.5, '#42D2F6'); gradient.addColorStop(1, '#0042D2F6');
    c.fillStyle = gradient; c.fillRect(0, 0, w, 10);
  }
  build() {
    Column({ space: 8 }) {
      if (this.failed || this.progress >= 0) { Text(this.failed ? '加载失败' : `${Math.trunc(this.progress)}%`).fontSize(18).fontColor('#FFFFFF') }
      Canvas(this.canvas).width('100%').height(10).borderRadius(4).clip(true)
        .onAreaChange((_old: Area, area: Area) => { this.barWidth = Number(area.width); })
        .onReady(() => { this.ready = true; this.paint(0); })
      Text(this.failed || this.progress >= 0 ? this.stage : `${this.stage} (${this.elapsed}s)`)
        .fontSize(12).fontColor('#E0FFFFFF').shadow({ radius: 4, color: '#AA000000', offsetX: 0, offsetY: 0 }).textAlign(TextAlign.Center).maxLines(3)
    }.padding(16).width('100%').constraintSize({ maxWidth: 380 }).hitTestBehavior(HitTestMode.None).id('MetaFlow加载动画')
  }
}
