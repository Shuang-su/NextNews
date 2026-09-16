import { LengthMetricsUnit } from '@kit.ArkUI';
export interface AnnotationPopup { x: number; y: number; width: number; arrow: number; flipped: boolean; }
// Pinned annotation.ts: 25px anchor offset, right-first/left-flip, 8px margin.
export function annotationPopup(x: number, y: number, width: number, height: number, cardHeight: number): AnnotationPopup {
  const w = Math.min(200, Math.max(1, width - 16)), h = Math.min(cardHeight, Math.max(1, height - 16));
  let left = x + 25; const flipped = left + w > width - 8;
  if (flipped) { left = x - 25 - w; }
  left = Math.max(8, Math.min(left, width - w - 8));
  const top = Math.max(8, Math.min(y - h / 2, height - h - 8));
  return { x: left, y: top, width: w, arrow: Math.max(16, Math.min(y - top, h - 16)), flipped };
}
// Only ten digits are rasterized once. The native shader composes labels up to
// 1000 using a 10 KiB R8 atlas, avoiding a full texture per annotation.
export function annotationGlyphs(): ArrayBuffer {
  const canvas = new OffscreenCanvas(320, 32, LengthMetricsUnit.PX), context = canvas.getContext('2d', new RenderingContextSettings(true));
  if (!context) { throw new Error('标注字形画布不可用'); }
  context.clearRect(0, 0, 320, 32); context.fillStyle = '#FFFFFF'; context.font = 'bold 32px sans-serif';
  context.textAlign = 'center'; context.textBaseline = 'middle';
  for (let i = 0; i < 10; i++) { context.fillText(i.toString(), i * 32 + 16, 16); }
  const rgba = context.getImageData(0, 0, 320, 32).data, alpha = new Uint8Array(320 * 32);
  if (rgba.length !== alpha.length * 4) { throw new Error('标注字形尺寸不匹配'); }
  for (let i = 0; i < alpha.length; i++) { alpha[i] = rgba[i * 4 + 3]; }
  return alpha.buffer;
}
