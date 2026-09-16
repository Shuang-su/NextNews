// One network limit for splat refinement and collision resources.
class ViewerRequestPool {
  private active: number = 0;
  private waiting: Array<() => void> = [];
  acquire(): Promise<void> {
    return new Promise<void>((resolve) => {
      if (this.active < 4) { this.active++; resolve(); } else { this.waiting.push(resolve); }
    });
  }
  release(): void {
    const next = this.waiting.shift();
    if (next) { next(); } else { this.active--; }
  }
}
export const viewerRequests: ViewerRequestPool = new ViewerRequestPool();
