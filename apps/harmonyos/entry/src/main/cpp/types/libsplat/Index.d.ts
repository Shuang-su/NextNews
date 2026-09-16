export interface RenderStatus {
  state: string;
  bounds: number[];
  message: string;
  graphics: string;
  annotationDepth: number;
  count: number;
  frames: number;
  width: number;
  height: number;
  bytes: number;
  loadMs: number;
  sortMs: number;
  gpuMs: number;
  uploadMs: number;
  uploadedRows: number;
  reusedRows: number;
  decodedFiles: number;
  subsetHits: number;
  requestRevision: number;
  displayRevision: number;
  prepareMs: number;
  refineMs: number;
  uploadedBytes: number;
  pageHits: number;
  frameMs: number;
  fps: number;
}
export const background: (r: number, g: number, b: number) => void;
export const load: (path: string) => void;
export const inspectModel: (path: string) => Promise<number[]>;
export const camera: (yaw: number, pitch: number, zoom: number, panX: number, panY: number, panZ: number, fly: number, fov?: number) => void;
export const setActive: (active: boolean) => void;
export const status: () => RenderStatus;
export const annotations: (positions: ArrayBuffer, glyphAlpha: ArrayBuffer) => void;
export const annotationStyle: (visible: boolean, hover: number, sizePixels: number) => void;

export const chunks: (paths: string[], bounds: number[], ranges?: number[]) => void;

export const pick: (x: number, y: number) => Promise<number[]>;

export const optimize: (enabled: boolean) => void;

export const selectPages: (paths: string[], bounds: number[], ranges: ArrayBuffer, revision: number, encoded?: boolean) => void;

export const dropCaches: () => void;

export const registerLods: (bounds: ArrayBuffer, boxes: ArrayBuffer, lods: ArrayBuffer, levels: number) => void;
export const selectLods: (camera: ArrayBuffer, budget: number, fov: number, aspect: number) => ArrayBuffer;

export const traceFrames: (enabled: boolean) => void;

export interface CollisionStatus { ids: string[]; bytes: number; }
export const collisionClear: () => number;
export const collisionLoadVoxel: (generation: number, id: string, metadata: string, binary: string) => Promise<number[]>;
export const collisionLoadMesh: (generation: number, id: string, path: string) => Promise<number[]>;
export const collisionSelect: (generation: number, ids: string[]) => void;
export const collisionEnter: (generation: number, x: number, y: number, z: number) => Promise<number[]>;
export const collisionStep: (generation: number, dt: number, yaw: number, right: number, forward: number, jump: boolean) => number[];
export const collisionPause: () => void;
export const collisionReset: () => number[];
export const collisionStatus: () => CollisionStatus;
export const collisionDebug: (generation: number, x: number, y: number, z: number) => Promise<number[]>;

export const intro: (enabled: boolean, waitForModel: boolean) => void;
