import { OrbitControls } from './OrbitControls';
import { ViewerProfile } from './ViewerProfile';
import { ViewerPose, ViewerSettings, normalizedPose } from './ViewerSettings';
export interface ViewerMarker { index: number; x: number; y: number; depth: number; title: string; text: string; }
export interface ViewerSnapshot { pose: ViewerPose; fly: boolean; selected: number; time: number; playing: boolean; }
// The world-space camera, navigation and annotation state are independent of the renderer.
export class ViewerRuntime {
  controls: OrbitControls = new OrbitControls();
  private sceneBounds: number[] = [0, 0, 0, 1];
  get bounds(): number[] { return this.sceneBounds; }
  set bounds(value: number[]) { this.sceneBounds = value; this.controls.sceneRadius = value[3]; }
  profile: ViewerProfile | undefined = undefined;
  selected: number = -1;
  gaming: boolean = false;
  private from: ViewerPose | undefined = undefined;
  private destination: ViewerPose | undefined = undefined;
  private transitionTime: number = 0;
  pose(): ViewerPose {
    const c = this.controls.current, b = this.bounds, d = c[2] * 3 * b[3];
    const back = [Math.sin(c[0]) * Math.cos(c[1]), Math.sin(c[1]), Math.cos(c[0]) * Math.cos(c[1])];
    const origin = c.slice(3, 6).map((v: number, i: number) => b[i] + v * b[3]);
    const eye = this.controls.fly ? origin : origin.map((v: number, i: number) => v + back[i] * d);
    return { position: eye, target: eye.map((v: number, i: number) => v - back[i] * d), fov: this.controls.fov };
  }
  apply(pose: ViewerPose, fly: boolean = this.controls.fly): void {
    const c = normalizedPose(pose, this.bounds);
    if (!fly) { for (let i = 0; i < 3; i++) { c[i + 3] = (pose.target[i] - this.bounds[i]) / this.bounds[3]; } }
    this.controls.current = c; this.controls.target = c.slice(); this.controls.fly = fly; this.controls.fov = pose.fov;
  }
  configure(settings: ViewerSettings): void { this.profile = new ViewerProfile(settings, this.bounds); this.selected = -1; this.interrupt(); }
  initial(): void {
    if (!this.profile) { return; }
    if (this.profile.settings.startMode === 'annotation' && this.profile.settings.annotations.length) { this.selected = 0; }
    const pose = this.selected >= 0 ? this.profile.settings.annotations[this.selected].camera.initial : this.profile.initial();
    const inside = Math.hypot(...pose.position.map((v: number, i: number) => v - this.bounds[i])) < this.bounds[3];
    this.apply(pose, this.selected < 0 && inside);
  }
  interrupt(): void { this.from = undefined; this.destination = undefined; }
  transition(pose: ViewerPose): void { this.from = this.pose(); this.destination = pose; this.transitionTime = 0; this.controls.cancel(); }
  annotation(index: number): void {
    if (!this.profile || !this.profile.settings.annotations.length) { return; }
    this.selected = (index + this.profile.settings.annotations.length) % this.profile.settings.annotations.length;
    this.transition(this.profile.settings.annotations[this.selected].camera.initial);
  }
  navigate(point: number[], doubleTap: boolean): void {
    if (this.controls.fly && this.gaming) { return; }
    const world = point.map((v: number, i: number) => this.bounds[i] + v * this.bounds[3]);
    const pose = this.pose(), delta = world.map((v: number, i: number) => v - pose.position[i]);
    const distance = Math.hypot(...delta); if (distance < .0001) { return; }
    if (this.controls.fly && !doubleTap) {
      const travel = Math.max(0, distance - Math.min(1, distance * .2));
      const destination = pose.position.map((v: number, i: number) => v + delta[i] * travel / distance);
      this.transition({ position: destination, target: world, fov: pose.fov });
    } else {
      this.controls.setFly(false); this.transition({ position: pose.position, target: world, fov: pose.fov });
    }
  }
  tick(ms: number): boolean {
    if (!this.from || !this.destination) { return this.controls.tick(ms); }
    this.transitionTime += Math.max(0, Math.min(ms, 64));
    const t = Math.min(1, this.transitionTime / 500), a = 1 - Math.pow(1 - t, 3), end = this.destination, start = this.from;
    // Interpolate orientation and focus distance separately. Opposite cameras can
    // otherwise interpolate eye and target to the same point halfway through.
    const s = normalizedPose(start, this.bounds), e = normalizedPose(end, this.bounds);
    const yaw = s[0] + Math.atan2(Math.sin(e[0] - s[0]), Math.cos(e[0] - s[0])) * a;
    const pitch = s[1] + (e[1] - s[1]) * a, distance = (s[2] + (e[2] - s[2]) * a) * 3 * this.bounds[3];
    const position = start.position.map((v: number, i: number) => v + (end.position[i] - v) * a);
    const back = [Math.sin(yaw) * Math.cos(pitch), Math.sin(pitch), Math.cos(yaw) * Math.cos(pitch)];
    this.apply({ position, target: position.map((v: number, i: number) => v - back[i] * distance), fov: start.fov + (end.fov - start.fov) * a });
    if (t === 1) { this.interrupt(); }
    return true;
  }
  anchor(index: number): ViewerMarker | undefined {
    return this.project(index, this.pose().position, this.projectionBasis());
  }
  private projectionBasis(): number[] {
    const c = this.controls.current;
    return [Math.sin(c[0]), Math.cos(c[0]), Math.sin(c[1]), Math.cos(c[1]), this.controls.height / (2 * Math.tan(this.controls.fov * Math.PI / 360))];
  }
  private project(index: number, eye: number[], basis: number[]): ViewerMarker | undefined {
    const a = this.profile?.settings.annotations[index]; if (!a) { return undefined; }
    const sy = basis[0], cy = basis[1], sp = basis[2], cp = basis[3], focal = basis[4];
    const dx = a.position[0] - eye[0], dy = a.position[1] - eye[1], dz = a.position[2] - eye[2];
    const depth = -(dx * sy * cp + dy * sp + dz * cy * cp);
    if (depth <= .000001) { return undefined; }
    const x = this.controls.width / 2 + focal * (dx * cy - dz * sy) / depth;
    const y = this.controls.height / 2 - focal * (-dx * sy * sp + dy * cp - dz * cy * sp) / depth;
    return { index, x, y, depth, title: a.title, text: a.text };
  }
  markers(): ViewerMarker[] {
    const result: ViewerMarker[] = [], count = this.profile?.settings.annotations.length ?? 0;
    const eye = this.pose().position, basis = this.projectionBasis();
    for (let i = 0; i < count; i++) {
      const m = this.project(i, eye, basis); if (m && m.x >= -22 && m.y >= -22 && m.x <= this.controls.width + 22 && m.y <= this.controls.height + 22) { result.push(m); }
    }
    return result;
  }
  snapshot(playing: boolean): ViewerSnapshot { return { pose: this.pose(), fly: this.controls.fly, selected: this.selected, time: this.profile?.time ?? 0, playing }; }
}
