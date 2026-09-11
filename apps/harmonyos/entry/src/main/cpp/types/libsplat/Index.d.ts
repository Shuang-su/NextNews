export interface RenderStatus {
  state: string;
  message: string;
  graphics: string;
  count: number;
  bytes: number;
  loadMs: number;
  sortMs: number;
  frameMs: number;
  fps: number;
}
export const load: (path: string) => void;
export const camera: (yaw: number, pitch: number, zoom: number, panX: number, panY: number, panZ: number, fly: number) => void;
export const setActive: (active: boolean) => void;
export const status: () => RenderStatus;

export const chunks: (paths: string[], bounds: number[]) => void;
