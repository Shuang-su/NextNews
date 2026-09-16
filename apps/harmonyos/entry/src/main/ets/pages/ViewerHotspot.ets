import { ViewerMarker } from './ViewerRuntime';
@Observed
export class HotspotPosition {
  index: number; x: number; y: number; title: string;
  constructor(marker: ViewerMarker) { this.index = marker.index; this.x = marker.x; this.y = marker.y; this.title = marker.title; }
  update(marker: ViewerMarker): void { this.x = marker.x; this.y = marker.y; this.title = marker.title; }
}
@Component
export struct ViewerHotspot {
  @ObjectLink marker: HotspotPosition;
  @Prop hovered: boolean = false;
  @Prop draw: boolean = true;
  activate: () => void = () => {};
  hover: (value: boolean) => void = () => {};
  build() {
    Button({ type: ButtonType.Circle }) {
      if (this.draw) {
        Text(`${this.marker.index + 1}`).fontSize(12).fontWeight(FontWeight.Bold).fontColor(this.hovered ? '#FF6600' : '#CCCCCC')
          .width(25).height(25).textAlign(TextAlign.Center).backgroundColor('#000000').borderRadius(13)
          .border({ width: 1.2, color: this.hovered ? '#FF6600' : '#CCCCCC' })
      }
    }.width(44).height(44).backgroundColor('#00000000').stateEffect(false).position({ x: this.marker.x - 22, y: this.marker.y - 22 })
      .accessibilityText(`标注 ${this.marker.index + 1} ${this.marker.title}`).id(`annotation-${this.marker.index}`)
      .onTouch((event: TouchEvent) => { event.stopPropagation(); })
      .onHover((value: boolean) => { this.hover(value); }).onClick(() => { this.activate(); })
  }
}
