# HarmonyOS Viewer — 2026-09-17

Continued MetaFlow-first parity on PR #5. Added single-model PLY SH1–3 parsing and native direction-dependent color, an SH0 baseline switch, cancellation-aware texture staging and coefficient memory accounting. Source polynomials remain pinned to MetaFlow's PlayCanvas 2.21.3.

[Implementation and real-phone evidence](../harmonyos/sh-20260917.md) distinguish parser/build passes from the inspected SH1 color experiment. Full SOG/streaming SH, Huawei tiled verification, visual parity and performance targets remain open. Other-platform changes are preserved.

Added [compressed single-SOG SH and bounded legacy merge preservation](../harmonyos/sog-sh-20260917.md). Bundled/unbundled SH fixtures, cache repair, reordered/mixed merge tests passed; actual phone compressed direction/SH0 screenshots inspected. Stable GPU page SH and the remaining acceptance matrix are still open.

Corrected gamepad monitor registration order from the official local guide and implemented per-device aggregation/removal with native and ArkTS regression tests. Physical-controller acceptance remains blocked.

## Encoded stable-page SH

Implemented compressed per-source SH books/centroids and protected GPU mapping pages, sharing the 4 MiB upload budget. Shared CPU SH books are charged once per cache. Three-source SH1/2/3 storage fixture rendered on Mate 80 Pro Max / API 26; SH0 switch produced gray baseline. Same-selection re-submit recorded three page hits and zero upload bytes. These are small-fixture checks, not large-scene performance acceptance. Details and remaining gaps: [stable-page SH report](../harmonyos/sh-pages-20260917.md).
