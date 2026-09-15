export interface ScrubPoint { id: number; x: number; y: number; }
// Ownership lasts until this exact pointer is released. A second hand cannot
// replace the playhead pointer or end its drag by lifting another finger.
export class ViewerScrub {
  owner: number = -1;
  begin(points: ScrubPoint[], width: number): number | undefined {
    if (this.owner !== -1) { return undefined; }
    const point = points.find((p: ScrubPoint) => p.x >= 0 && p.x <= width && p.y >= 0 && p.y <= 44);
    if (!point) { return undefined; }
    this.owner = point.id; return this.value(point.x, width);
  }
  move(points: ScrubPoint[], width: number): number | undefined {
    const point = points.find((p: ScrubPoint) => p.id === this.owner);
    return point ? this.value(point.x, width) : undefined;
  }
  release(points: ScrubPoint[]): boolean {
    if (this.owner === -1 || !points.some((p: ScrubPoint) => p.id === this.owner)) { return false; }
    this.cancel(); return true;
  }
  cancel(): void { this.owner = -1; }
  value(x: number, width: number): number { return Math.max(0, Math.min(1, x / Math.max(1, width))); }
}
