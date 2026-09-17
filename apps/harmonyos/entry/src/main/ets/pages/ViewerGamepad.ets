// MetaFlow a871786 / GamepadDevice: raw standard axes, 4 world units/s,
// 18 degrees/s, first-person rotation scaled by vertical FOV / 120.
export interface GamepadFrame { right: number; forward: number; yaw: number; pitch: number; active: boolean; }
export function gamepadFrame(axes: number[], seconds: number, firstPerson: boolean, fov: number): GamepadFrame {
  const zero: GamepadFrame = { right: 0, forward: 0, yaw: 0, pitch: 0, active: false };
  if (axes.length !== 4 || !axes.every((v: number) => Number.isFinite(v)) || !Number.isFinite(seconds) || !Number.isFinite(fov)) { return zero; }
  const dt = Math.max(0, Math.min(.064, seconds));
  // Native validates each device axis; preserve the reference sum across devices.
  const values = axes;
  const rotate = 18 * (firstPerson ? fov / 120 : 1) * dt * Math.PI / 180;
  return { right: values[0] * 4 * dt, forward: -values[1] * 4 * dt, yaw: -values[2] * rotate, pitch: values[3] * rotate, active: values.some((v: number) => v !== 0) };
}
