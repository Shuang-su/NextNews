// Bounds of the committed SH0 samples; regenerate when samples change.
export interface SampleBounds { file: string; center: number[]; radius: number; }
export const SAMPLE_BOUNDS: SampleBounds[] = [
  { file: 'tripo.ply', center: [-0.008107542991638184, -0.005169227719306946, 0.011494562029838562], radius: 0.8732711095444725 },
  { file: 'public.ply', center: [-0.09686808288097382, -1.5900772213935852, 0.04090338945388794], radius: 1.765534449442812 },
  { file: 'converted.ply', center: [-199.667462348938, 36.28969097137451, 163.55656623840332], radius: 315.46975433958914 }
];
