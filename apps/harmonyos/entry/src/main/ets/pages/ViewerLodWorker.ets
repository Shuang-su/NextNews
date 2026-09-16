import { parseLodManifest } from './ViewerLodManifest';
import { StreamManifest } from './StreamSession';
@Concurrent
export function parseLodWorker(text: string): StreamManifest { return parseLodManifest(text); }
