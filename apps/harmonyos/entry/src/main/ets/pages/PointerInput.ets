import { OrbitTouch } from './OrbitControls';
// Pointer ownership lasts from down to up/cancel. An existing finger elsewhere
// can never become the joystick owner when the touches array is reordered.
export class PointerInput {
  stickId: number = -1;
  x: number = 0;
  y: number = 0;
  blockTap: boolean = false;
  private lookIds: Set<number> = new Set();
  cancelStick(): void { this.stickId = -1; this.x = 0; this.y = 0; }
  cancel(): void { this.cancelStick(); this.lookIds.clear(); this.blockTap = true; }
  viewport(phase: string, touches: OrbitTouch[], changed: OrbitTouch[]): OrbitTouch[] {
    if (phase === 'cancel') { this.lookIds.clear(); this.blockTap = true; return []; }
    if (phase === 'down') {
      if (this.lookIds.size === 0 && this.stickId < 0) { this.blockTap = false; }
      for (const p of changed) { if (p.id !== this.stickId) { this.lookIds.add(p.id); } }
    }
    if (touches.length > 1 || this.stickId >= 0) { this.blockTap = true; }
    if (phase === 'up') { for (const p of changed) { this.lookIds.delete(p.id); } }
    return touches.filter((p: OrbitTouch) => this.lookIds.has(p.id) && p.id !== this.stickId);
  }
  joystick(phase: string, touches: OrbitTouch[], changed: OrbitTouch[]): void {
    this.blockTap = true;
    if (phase === 'cancel') { this.cancelStick(); return; }
    if (phase === 'down' && this.stickId < 0) {
      const owner = changed.find((p: OrbitTouch) => p.x >= 0 && p.y >= 0 && p.x <= 96 && p.y <= 96);
      if (owner) { this.stickId = owner.id; this.lookIds.delete(owner.id); }
    }
    if (phase === 'up' && changed.some((p: OrbitTouch) => p.id === this.stickId)) { this.cancelStick(); return; }
    if (this.stickId < 0) { return; }
    const owner = touches.find((p: OrbitTouch) => p.id === this.stickId);
    if (!owner) { this.cancelStick(); return; }
    const x = (owner.x - 48) / 30, y = (owner.y - 48) / 30;
    const length = Math.max(1, Math.hypot(x, y)); this.x = x / length; this.y = y / length;
  }
}
