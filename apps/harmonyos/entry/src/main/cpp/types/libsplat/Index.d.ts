export interface RenderStatus {
  state: string;
  message: string;
  graphics: string;
  count: number;
  frames: number;
  width: number;
  height: number;
  bytes: number;
  loadMs: number;
  sortMs: number;
  gpuMs: number;
  uploadMs: number;
  frameMs: number;
  fps: number;
}
export const load: (path: string) => void;
export const camera: (yaw: number, pitch: number, zoom: number, panX: number, panY: number, panZ: number, fly: number, fov?: number) => void;
export const setActive: (active: boolean) => void;
export const status: () => RenderStatus;

export const chunks: (paths: string[], bounds: number[], ranges?: number[]) => void;

export const pick: (x: number, y: number) => Promise<number[]>;

export const optimize: (enabled: boolean) => void;
