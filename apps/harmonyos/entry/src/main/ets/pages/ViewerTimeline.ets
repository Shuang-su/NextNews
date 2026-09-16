import { ViewerScrub, ScrubPoint } from './ViewerScrub';
import { KeyCode } from '@kit.InputKit';
@Component
export struct ViewerTimeline {
  @Prop value: number = 0;
  @Prop duration: number = 20;
  @Prop @Watch('activeChanged') active: boolean = true;
  @State trackWidth: number = 300;
  private scrub: ViewerScrub = new ViewerScrub();
  seek: (value: number) => void = () => {};
  drag: (phase: string) => void = () => {};
  private cancel(): void { if (this.scrub.owner !== -1) { this.scrub.cancel(); this.drag('cancel'); } }
  private activeChanged(): void { if (!this.active) { this.cancel(); } }
  private points(p: TouchObject[]): ScrubPoint[] { return p.map((v: TouchObject): ScrubPoint => { return { id: v.id, x: v.x, y: v.y }; }); }
  private touch(event: TouchEvent): void {
    event.stopPropagation(); if (event.source === SourceType.Mouse) { return; }
    if (event.type === TouchType.Cancel) { this.cancel(); return; }
    const changes = this.points(event.changedTouches);
    if (event.type === TouchType.Down) {
      const t = this.scrub.begin(changes, this.trackWidth);
      if (t !== undefined) { this.drag('begin'); this.seek(t * this.duration); }
    } else if (event.type === TouchType.Move) {
      const t = this.scrub.move(this.points(event.touches), this.trackWidth); if (t !== undefined) { this.seek(t * this.duration); }
    } else if (event.type === TouchType.Up) {
      const t = this.scrub.move(changes, this.trackWidth);
      if (this.scrub.release(changes)) { if (t !== undefined) { this.seek(t * this.duration); } this.drag('end'); }
    }
  }
  aboutToDisappear(): void { this.cancel(); }
  build() {
    Stack({ alignContent: Alignment.Start }) {
      Row().height(2).width('100%').backgroundColor('#777777')
      Row().height(2).width(`${Math.max(0, Math.min(100, this.value / this.duration * 100))}%`).backgroundColor('#FFFFFF')
      Circle().width(12).height(12).fill('#FFFFFF').position({ x: Math.max(0, Math.min(this.trackWidth - 12, this.value / this.duration * (this.trackWidth - 12))), y: 16 })
    }.height(44).width('100%').id('动画时间轴').accessibilityText(`动画时间 ${this.value.toFixed(1)} 秒，共 ${this.duration.toFixed(1)} 秒`)
      .focusable(true).onAreaChange((_old: Area, area: Area) => { this.trackWidth = Number(area.width); })
      .onTouch((event: TouchEvent) => { this.touch(event); })
      .onBlur(() => { this.cancel(); })
      .onMouse((event: MouseEvent) => {
        if (event.action === MouseAction.Press && event.button === MouseButton.Left && this.scrub.owner === -1) {
          this.scrub.owner = -2; this.drag('begin'); this.seek(this.scrub.value(event.x, this.trackWidth) * this.duration);
        } else if (this.scrub.owner === -2 && event.action === MouseAction.Move) { this.seek(this.scrub.value(event.x, this.trackWidth) * this.duration); }
        else if (this.scrub.owner === -2 && event.action === MouseAction.Release) { this.scrub.cancel(); this.drag('end'); }
      })
      .onHover((hover: boolean) => { if (!hover && this.scrub.owner === -2) { this.cancel(); } })
      .onKeyEvent((event: KeyEvent): boolean => {
        if (event.type !== KeyType.Down) { return false; }
        const delta = event.keyCode === KeyCode.KEYCODE_DPAD_LEFT ? -1 : event.keyCode === KeyCode.KEYCODE_DPAD_RIGHT ? 1 : 0;
        if (!delta) { return false; }
        this.drag('begin'); this.seek(Math.max(0, Math.min(this.duration, this.value + delta))); this.drag('end'); return true;
      })
  }
}
