# MetaFlow loading and range reveal — 2026-09-16

This supersedes the initial random stagger in `intro-20260916.md`.
Read-only reference: local `Metaflow-xr` at `a871786`, specifically
`metaflow-viewer/src/ui.ts`, `index.scss`, `viewer.ts` and
`gsplat-reveal-radial.ts`. That project was not modified.

## Implemented

- Bottom loading panel, max 380 vp with 16 vp side padding and 120 vp bottom
  inset, following the MetaFlow layout.
- Unknown progress uses a cyan `#42d2f6` gradient sweeping on a 1.2-second cycle,
  with the current stage and elapsed seconds. No fabricated percentage.
- The native component supports determinate percentage rendering, but the current
  model pipeline supplies unknown progress until ready; byte/upload percentages
  are not yet wired. Errors stop movement and show red failure state.
- Stage changes reset elapsed time. Component removal stops its timer; the panel
  is absent while the application is inactive. Pointer input passes through.
- Model catalog closes on selection so it cannot conceal the loading panel.
- Stream progress remains visible for missing target detail and full preload;
  unrelated prefetch does not keep it spinning. Failure/cancellation stop it.
- White scene backgrounds receive the reference text shadow.
- OpenGL reveal now has two radial waves: an accelerating dot wave, then a lift
  wave delayed by one second. Speed .36, acceleration 2.1 follow MetaFlow's
  ordinary-scene profile. During the first wave covariance becomes spherical;
  the lift wave restores original covariance. RMS radius uses covariance trace,
  equivalent to upstream `gsplatGetSizeFromScale`.
- Minimum timeline duration is five seconds, extended to cover the scene radius
  and lift transition. A stalled frame advances at most 1/30 s, matching the
  reference clamp. Pause/replay/cancel remain supported; LOD commits do not reset.

## Actual validation

- Native timeline regression passes ASan/UBSan, including long-frame clamp,
  paused time, repeated LOD commit and scene-dependent duration.
- Stream regression passes loading state, four-request scheduling, cached camera
  turns, prefetch exclusion, full preload, failures/cancel and cache-pressure cases.
- Viewer runtime and pointer-input regressions pass.
- Standard and signed builds pass. Installed/started on Mate 80 Pro Max API 26,
  SGT-AL10, 1320×2848. The earlier recorded OS build is 7.0.0.105.
- Actual device captures under `artifacts/harmonyos/metaflow-opening-20260916`:
  `loading-cyan.jpeg` shows the sweep during the CDN ancestor scene load;
  `loading-final.jpeg` shows stage/elapsed during a manifest request;
  `stream-later.png` and the associated UI query confirm removal after the
  2M Huafa selection settles. These captures precede the final text-shadow-only
  change; they are not performance measurements.

## Still different / not accepted

Poster blur/sharpen and determinate byte/upload progress are not wired. Loading
state polls every 500 ms; this is not yet a frame-exact Web `firstFrame` handshake.
The reveal currently starts at model/manifest center, not the reference camera
focus. Character/mega-voxel profiles, per-particle oscillation, streamed subject
versus environment gating, Huawei equivalence and pixel parity remain pending.
Do not label the complete MetaFlow opening experience finished.

No 20-trial performance acceptance was run in this batch. Existing performance
failures remain open. Artifacts/signing credentials are local only.

Final installed HAP: `artifacts/harmonyos/NextNews-metaflow-opening-preview.hap`.
SHA-256: `c36f6e441ca47cfc788ba2da77134baf3e147199577be7ea45a308c5df580c31`.
