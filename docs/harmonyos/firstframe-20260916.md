# First-open-only loading and visible reveal handoff — 2026-09-16

Corrects the previous loading preview: MetaFlow removes `loadingWrap` once
`loaded` is set by `firstFrame`; camera-driven LOD changes never recreate it.
Reference remains local MetaFlow `a871786`, ui.ts and viewer.ts.

## Changes

- Loading UI has an explicit per-scene opening state. File/network/LOD activity
  only updates stage text while that opening is visible. Once dismissed, changes
  to loading/busy/coverage, budget, later errors or scene refinement cannot show
  the loading panel again. Selecting a new scene arms a new opening.
- OpenGL reports an opening request and presented request. Presentation is
  acknowledged only after a valid nonempty selection's successful EGL swap.
  Old upload generations and old presented requests cannot complete a new opening.
- The particle timeline remains held until the ArkUI loading component disappears.
  Its callback releases that same request's playback. Backgrounding cannot consume
  the handoff; resuming retries an outstanding release without replaying an
  already-started opening.
- Cancellation exits loading. Post-opening errors remain ordinary error/status
  text rather than reopening the initial loading panel.
- Reveal origin now uses the Viewer world-space camera target. Its bounds remain
  fixed during the reveal, including subsequent LOD commits. Coverage radius uses
  the scene sphere plus focus offset, a conservative bound; it is not yet the
  upstream exact AABB-corner radius.

Huawei still uses the existing scene-load completion fallback, not an equivalent
EGL presented-frame signal. Do not claim backend-equivalent first-frame evidence.

## Verification

- Standard and signed HAP builds pass.
- ASan/UBSan native intro tests cover held-first-frame, release, pause, long-frame
  clamping, LOD commit and replay/cancellation.
- `node scripts/harmonyos/test_opening_handoff.cjs` exercises the actual page
  release method: overlay exit, inactive/resume, stale request rejection, one-shot
  release, camera target propagation, explicit replay and backend isolation.
- Existing viewer runtime, pointer ownership and stream regressions pass.
- Final HAP installed on HUAWEI Mate 80 Pro Max SGT-AL10, API 26, 1320×2848.
  Previously recorded OS build: 7.0.0.105.
- Final-build phone test observed initial loading of CDN 前海冰雪世界, then checked
  six alternating camera turns: all six had no `MetaFlow加载动画` UI node.
  Screenshots and query dumps: `artifacts/harmonyos/firstframe-final-20260916/`.
- Device log: request 3 FirstFrame at 21:13:36.962, VisiblePlayback at
  21:13:37.209; no extra opening event during the six turns. This is functional
  ordering evidence, not a 20-run performance benchmark.
- Selected 祖先像, sent Home and reopened the app. Its reveal resumed and the
  loading panel did not stick; actual screenshot inspected. Request 4 FirstFrame
  at 21:14:34.712 precedes VisiblePlayback at 21:14:35.105.

Final artifact: `artifacts/harmonyos/NextNews-firstframe-preview.hap`.
SHA-256: `476925c44d0f1a2989602e571cf941a7b8f4cb2d55147c23463653cf577c97a2`.

## Outstanding

Poster blur/sharpen, byte/upload percentages, specialized character/mega-voxel
profiles, oscillation and exact AABB reveal radius remain pending. ArkUI observes
native presentation through its existing 500 ms status poll, so it may retire
loading later than the Web event path. No new performance acceptance claim.
