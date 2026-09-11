// Original controller implementation; interaction reference recorded in docs/harmonyos/interaction-parity.md.
export interface OrbitTouch { id: number; x: number; y: number; }
export class OrbitControls {
  current: number[] = [0, 0, 1, 0, 0, 0];
  target: number[] = [0, 0, 1, 0, 0, 0];
  width: number = 1;
  height: number = 1;
  fly: boolean = false;
  keys: string[] = [];
  private previous: OrbitTouch[] = [];
  reset(): void {
    this.current = [0, 0, 1, 0, 0, 0]; this.target = [0, 0, 1, 0, 0, 0]; this.fly = false; this.keys = []; this.cancel();
  }
  cancel(): void { this.previous = []; this.keys = []; }
  resize(width: number, height: number): void {
    if (!this.fly && this.height > 1 && height > 1) {
      // Preserve apparent model size when panels change the vertical-FOV viewport.
      const ratio = height / this.height;
      this.current[2] = Math.max(0.05, Math.min(20, this.current[2] * ratio));
      this.target[2] = Math.max(0.05, Math.min(20, this.target[2] * ratio));
    }
    this.width = Math.max(1, width); this.height = Math.max(1, height); this.cancel();
  }
  setFly(fly: boolean): void {
    if (fly === this.fly) { return; }
    this.target = this.current.slice();
    const y = this.current[0]; const p = this.current[1];
    const d = 3 * this.current[2] * (fly ? 1 : -1);
    this.current[3] += Math.sin(y) * Math.cos(p) * d;
    this.current[4] += Math.sin(p) * d;
    this.current[5] += Math.cos(y) * Math.cos(p) * d;
    this.target = this.current.slice(); this.fly = fly; this.cancel();
  }
  move(right: number, up: number, forward: number): void {
    const y = this.current[0]; const p = this.current[1];
    this.target[3] += Math.cos(y) * right - Math.sin(y) * Math.cos(p) * forward;
    this.target[4] += up - Math.sin(p) * forward;
    this.target[5] -= Math.sin(y) * right + Math.cos(y) * Math.cos(p) * forward;
    for (let i = 3; i < 6; i++) { this.target[i] = Math.max(-100, Math.min(100, this.target[i])); }
  }
  key(key: string, down: boolean): void {
    this.keys = this.keys.filter((k: string) => k !== key);
    if (down) { this.keys.push(key); }
  }
  zoom(factor: number): void {
    if (this.fly) { this.move(0, 0, -Math.log(Math.max(0.01, factor))); return; }

    this.target[2] = Math.max(0.05, Math.min(20, this.target[2] * factor));
  }
  pan(dx: number, dy: number): void {
    // Radius-normalized world target. 45-degree vertical FOV, distance = radius * 3 * zoom.
    const scale = 6 * this.target[2] * Math.tan(Math.PI / 8) / this.height;
    const x = -dx * scale; const y = dy * scale;
    const yaw = this.current[0]; const pitch = this.current[1];
    this.target[3] += Math.cos(yaw) * x - Math.sin(yaw) * Math.sin(pitch) * y;
    this.target[4] += Math.cos(pitch) * y;
    this.target[5] += -Math.sin(yaw) * x - Math.cos(yaw) * Math.sin(pitch) * y;
    for (let i = 3; i < 6; i++) { this.target[i] = Math.max(-100, Math.min(100, this.target[i])); }
  }
  rotate(dx: number, dy: number): void {
    const scale = Math.PI * 2 / Math.max(1, Math.min(this.width, this.height));
    this.target[0] -= dx * scale;
    this.target[1] = Math.max(-1.5, Math.min(1.5, this.target[1] + dy * scale));
    // Rebase both angles together, retaining continuous interpolation across +/- pi.
    if (Math.abs(this.target[0]) > 4 * Math.PI) {
      const turn = Math.round(this.target[0] / (2 * Math.PI)) * 2 * Math.PI;
      this.target[0] -= turn; this.current[0] -= turn;
    }
  }
  touch(points: OrbitTouch[], panMode: boolean): void {
    const next = points.slice().sort((a: OrbitTouch, b: OrbitTouch) => a.id - b.id);
    const old = this.previous;
    this.previous = next;
    if (next.length !== old.length || next.length === 0 || next.length > 2) { return; }
    for (let i = 0; i < next.length; i++) { if (next[i].id !== old[i].id) { return; } }
    if (next.length === 1) {
      const dx = next[0].x - old[0].x; const dy = next[0].y - old[0].y;
      if (panMode) { this.pan(dx, dy); } else { this.rotate(dx, dy); }
    } else {
      const dx = (next[0].x + next[1].x - old[0].x - old[1].x) / 2;
      const dy = (next[0].y + next[1].y - old[0].y - old[1].y) / 2;
      this.pan(dx, dy);
      const before = Math.hypot(old[1].x - old[0].x, old[1].y - old[0].y);
      const after = Math.hypot(next[1].x - next[0].x, next[1].y - next[0].y);
      if (before > 4 && after > 4) { this.zoom(before / after); }
    }
  }
  tick(elapsedMs: number): boolean {
    if (this.fly) {
      const speed = Math.min(64, elapsedMs) / 1000;
      this.move((Number(this.keys.includes('d')) - Number(this.keys.includes('a'))) * speed,
        (Number(this.keys.includes('e')) - Number(this.keys.includes('q'))) * speed,
        (Number(this.keys.includes('w')) - Number(this.keys.includes('s'))) * speed);
    }
    const alpha = 1 - Math.pow(0.95, Math.max(0, Math.min(64, elapsedMs)));
    let changed = false;
    for (let i = 0; i < 6; i++) {
      const delta = this.target[i] - this.current[i];
      if (delta !== 0) {
        this.current[i] = Math.abs(delta) < 0.0001 ? this.target[i] : this.current[i] + delta * alpha;
        changed = true;
      }
    }
    return changed;
  }
}
