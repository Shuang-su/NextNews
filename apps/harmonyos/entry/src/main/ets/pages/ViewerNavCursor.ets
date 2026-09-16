import { ViewerPose } from './ViewerSettings';
// SuperSplat v1.31.2 nav-cursor: world target, 48px diameter, 0.85 inner
// radius, 12 samples, cubic smoothing and 60% white. Normal currently uses
// its camera-facing fallback; collision/depth neighborhood fitting is separate.
export class ViewerNavCursor {
  position: number[] | undefined = undefined;
  private normal: number[] = [0, 1, 0];
  mode: string = 'orbit';
  clear(): void { this.position = undefined; }
  set(position: number[], pose: ViewerPose, mode: string): void {
    this.position = position.slice(); this.mode = mode;
    const d = pose.position.map((v: number, i: number) => v - position[i]), length = Math.hypot(...d);
    this.normal = length > 1e-6 ? d.map((v: number) => v / length) : [0, 1, 0];
  }
  rings(pose: ViewerPose, width: number, height: number): number[][] {
    const p = this.position; if (!p || width <= 0 || height <= 0) { return []; }
    const distance = Math.hypot(...pose.position.map((v: number, i: number) => v - p[i]));
    if (this.mode !== 'orbit' && distance < 2) { return []; }
    const d = pose.target.map((v: number, i: number) => v - pose.position[i]), length = Math.hypot(...d);
    const yaw = Math.atan2(-d[0], -d[2]), pitch = Math.asin(-d[1] / length);
    const right = [Math.cos(yaw), 0, -Math.sin(yaw)], up = [-Math.sin(yaw) * Math.sin(pitch), Math.cos(pitch), -Math.cos(yaw) * Math.sin(pitch)];
    const forward = d.map((v: number) => v / length), focal = height / (2 * Math.tan(pose.fov * Math.PI / 360));
    const n = this.normal;
    const tangent = Math.abs(n[1]) < .99 ? [-n[2], 0, n[0]] : [0, n[2], -n[1]];
    const tlen = Math.hypot(...tangent); for (let i = 0; i < 3; i++) { tangent[i] /= tlen; }
    const bitangent = [n[1] * tangent[2] - n[2] * tangent[1], n[2] * tangent[0] - n[0] * tangent[2], n[0] * tangent[1] - n[1] * tangent[0]];
    const radius = 48 * distance * Math.tan(pose.fov * Math.PI / 360) / height;
    const rings: number[][] = [];
    for (const ratio of [1, .85]) {
      const ring: number[] = [];
      for (let i = 0; i < 12; i++) {
        const a = 2 * Math.PI * i / 12;
        const v = p.map((value: number, k: number) => value + radius * ratio * (Math.cos(a) * tangent[k] + Math.sin(a) * bitangent[k]) - pose.position[k]);
        const depth = forward.reduce((sum: number, f: number, k: number) => sum + f * v[k], 0);
        if (depth <= .001) { return []; }
        ring.push(width / 2 + focal * right.reduce((sum: number, r: number, k: number) => sum + r * v[k], 0) / depth,
          height / 2 - focal * up.reduce((sum: number, u: number, k: number) => sum + u * v[k], 0) / depth);
      }
      rings.push(ring);
    }
    return rings;
  }
}
