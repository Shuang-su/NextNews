import { ViewerPose } from './ViewerSettings';
// Shared world-space diagnostic projection. Clip before dividing by depth;
// otherwise segments crossing the camera plane generate enormous coordinates.
export class ViewerProjection {
  private eye: number[];
  private right: number[];
  private up: number[];
  private forward: number[];
  private focal: number;
  private width: number;
  private height: number;
  constructor(pose: ViewerPose, width: number, height: number) {
    this.width = width; this.height = height;
    this.eye = pose.position;
    const d = pose.target.map((v: number, i: number) => v - pose.position[i]);
    const length = Math.hypot(...d), yaw = Math.atan2(-d[0], -d[2]), pitch = Math.asin(-d[1] / length);
    this.right = [Math.cos(yaw), 0, -Math.sin(yaw)];
    this.up = [-Math.sin(yaw) * Math.sin(pitch), Math.cos(pitch), -Math.cos(yaw) * Math.sin(pitch)];
    this.forward = d.map((v: number) => v / length);
    this.focal = height / (2 * Math.tan(pose.fov * Math.PI / 360));
  }
  private camera(p: number[]): number[] {
    const d = p.map((v: number, i: number) => v - this.eye[i]);
    return [this.right, this.up, this.forward].map((axis: number[]) => axis.reduce((sum: number, v: number, i: number) => sum + v * d[i], 0));
  }
  segment(a: number[], b: number[]): number[] {
    let p = this.camera(a), q = this.camera(b);
    if (p[2] < .01 && q[2] < .01) { return []; }
    if (p[2] < .01 || q[2] < .01) {
      const t = (.01 - p[2]) / (q[2] - p[2]);
      const c = p.map((v: number, i: number) => v + (q[i] - v) * t);
      if (p[2] < .01) { p = c; } else { q = c; }
    }
    const x = this.width / 2 + this.focal * p[0] / p[2], y = this.height / 2 - this.focal * p[1] / p[2];
    const dx = this.focal * (q[0] / q[2] - p[0] / p[2]), dy = -this.focal * (q[1] / q[2] - p[1] / p[2]);
    let lo = 0, hi = 1;
    const slopes = [-dx, dx, -dy, dy], offsets = [x, this.width - x, y, this.height - y];
    for (let i = 0; i < 4; i++) {
      if (Math.abs(slopes[i]) < 1e-12) { if (offsets[i] < 0) { return []; } continue; }
      const t = offsets[i] / slopes[i];
      if (slopes[i] < 0) { lo = Math.max(lo, t); } else { hi = Math.min(hi, t); }
      if (lo > hi) { return []; }
    }
    return [x + dx * lo, y + dy * lo, x + dx * hi, y + dy * hi];
  }
}
