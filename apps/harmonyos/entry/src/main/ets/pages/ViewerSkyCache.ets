import { fileIo as fs } from '@kit.CoreFileKit';
const leases: Map<string, number> = new Map();
let pruning: Promise<void> = Promise.resolve();
interface SkyEntry { path: string; bytes: number; used: number; }
export function retainSkyImage(path: string): void { leases.set(path, (leases.get(path) ?? 0) + 1); }
export function releaseSkyImage(path: string): void {
  const count = (leases.get(path) ?? 0) - 1;
  if (count > 0) { leases.set(path, count); } else { leases.delete(path); }
}
// Only derived panorama cache files are eligible; never model or original files.
export function pruneSkyImages(directory: string, limit: number = 256 * 1024 * 1024): Promise<void> {
  pruning = pruning.catch(() => {}).then(async () => {
    const entries: SkyEntry[] = []; let bytes = 0;
    for (const name of await fs.listFile(directory)) {
      if (!/^sky-[0-9a-f]{64}\.image$/.test(name)) { continue; }
      const path = `${directory}/${name}`;
      try { const stat = await fs.stat(path); entries.push({ path, bytes: stat.size, used: Number(stat.mtime) }); bytes += stat.size; } catch (_) {}
    }
    entries.sort((a: SkyEntry, b: SkyEntry) => a.used - b.used);
    for (const entry of entries) {
      if (bytes <= limit) { break; }
      if (leases.has(entry.path)) { continue; }
      try { await fs.unlink(entry.path); bytes -= entry.bytes; } catch (_) {}
    }
  });
  return pruning;
}
