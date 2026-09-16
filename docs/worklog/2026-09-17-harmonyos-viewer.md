# HarmonyOS Viewer — 2026-09-17

Continued MetaFlow-first parity on PR #5. Added single-model PLY SH1–3 parsing and native direction-dependent color, an SH0 baseline switch, cancellation-aware texture staging and coefficient memory accounting. Source polynomials remain pinned to MetaFlow's PlayCanvas 2.21.3.

[Implementation and real-phone evidence](../harmonyos/sh-20260917.md) distinguish parser/build passes from the inspected SH1 color experiment. Full SOG/streaming SH, Huawei tiled verification, visual parity and performance targets remain open. Other-platform changes are preserved.

Added [compressed single-SOG SH and bounded legacy merge preservation](../harmonyos/sog-sh-20260917.md). Bundled/unbundled SH fixtures, cache repair, reordered/mixed merge tests passed; actual phone compressed direction/SH0 screenshots inspected. Stable GPU page SH and the remaining acceptance matrix are still open.

Corrected gamepad monitor registration order from the official local guide and implemented per-device aggregation/removal with native and ArkTS regression tests. Physical-controller acceptance remains blocked.

## Encoded stable-page SH

Implemented compressed per-source SH books/centroids and protected GPU mapping pages, sharing the 4 MiB upload budget. Shared CPU SH books are charged once per cache. Three-source SH1/2/3 storage fixture rendered on Mate 80 Pro Max / API 26; SH0 switch produced gray baseline. Same-selection re-submit recorded three page hits and zero upload bytes. These are small-fixture checks, not large-scene performance acceptance. Details and remaining gaps: [stable-page SH report](../harmonyos/sh-pages-20260917.md).

## SH higher-band validation and explicit float-page limitation

Added a reproducible desktop GPU harness against independent associated Legendre SH evaluation: 2,910 production-shader samples passed, max error 1.51479e-7. Added nonzero highest-band fixture generation and inspected SH2/SH3 directional changes on the phone. Twenty same-selection retries on 768 Gaussians all reused three pages with zero upload; this is not large-scene performance acceptance. Removed silent source-SH dropping in unsupported float stable pages: those loads now report their limitation. Full details: [SH page report](../harmonyos/sh-pages-20260917.md).

The phone negative-path check exposed an indefinite “preparing” overlay after native decode failure. Added scene-owned native error status and UI propagation, including stale-job rejection and a Chinese recovery hint for unsupported float-page SH. This is a loading error fix in addition to the SH numerical tests.

## Annotation reference correction and direct tone mapping

Re-read fixed MetaFlow annotation code: numbered hotspots are GPU base/overlay layers and participate in scene processing; only title/body tooltip content is DOM. Corrected the earlier matrix claim. Added direct tone mapping to both native hotspot passes when offscreen composition is inactive, preventing Gaussian/marker tone mismatch. Production hotspot class passed desktop GPU normal/hover, default/ACES, visible/occluded overlay checks. Signed build installed on Mate 80 Pro Max / API 26; front/behind/partial fixture markers and occluded popup visually checked. Full same-camera Web pixel comparison remains open. See [report](../harmonyos/annotation-tone-20260917.md).
