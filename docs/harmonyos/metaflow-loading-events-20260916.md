# MetaFlow loading / reveal batch, 2026-09-16

This is a partial implementation of the approved MetaFlow-first plan, continuing
`3434e53` on draft PR #5. It is not complete Viewer parity or a performance pass.
Primary reference: local MetaFlow `a871786dffdb195399f3e7427987d8db70296491`,
`metaflow-viewer/src/{viewer.ts,ui.ts,index.scss,gsplat-reveal-radial.ts}`.
Official SuperSplat v1.31.2 is the secondary reference; these reveal effects are
MetaFlow-specific.

## Implemented

- Native EGL presentation emits a thread-safe ArkTS notification carrying the
  opening request identity. A renderer surface generation rejects events queued
  before window destruction. Subscription replacement/VM cleanup abort delivery.
  The 500 ms diagnostic timer no longer dismisses the OpenGL loading panel.
- Foreground recovery can acknowledge an already presented frame; a pending
  reveal survives a rejected release during surface destruction. LOD updates
  never rearm loading. Configured animation waits until the first-open panel exits.
- CDN details supply cover/thumbnail posters. Single SOG downloads use actual
  HTTP byte progress when total size is known; unknown totals and GPU preparation
  stay indeterminate. Known progress drives the MetaFlow blur formula. Download
  cancellation, size limits and late events are handled independently of progress.
- A common scene descriptor preserves resource/configuration provenance and
  resolves sound, skybox and collision URLs in a copied settings object, relative
  to the settings URL (inline business settings use their API detail URL).
- PLY/SOG retain their world AABB. Stream manifests preserve the source root AABB
  and apply the existing model Rz(180°) once. Reveal radius is the farthest corner
  from the frozen camera focus, minimum one world unit. Old manifests without an
  AABB retain a conservative sphere-derived fallback, not exact-box parity.
- Ordinary / character / mega-voxel motion and dot-size profiles match the fixed
  reference constants; GPU center oscillation and two-wave expansion share the
  same paused, clamped-delta timeline. Explicit `experienceType`,
  `voxelManifestUrl`, `revealEffect: none` control profile/disable behavior.
- Streaming diagnostic text moved from the normal view into the diagnostics menu.

## Checks and evidence

Unsigned project build and signed local-copy build passed with the paired tools.
Host checks: opening handoff/event isolation, config-relative resolution without
mutation, both HTTP/dataEnd orderings, known/unknown progress, cancellation,
oversize/HTTP/network failures, all 49 recorded CDN detail mappings, eight real
LOD manifests, stream selection and file-cache repair. Native intro math and
parser tests passed under Xcode 27 ASan/UBSan. The old one-point orbit test expected
a radius-relative distance below the already-existing 0.01 world minimum; its
expectation was corrected, and transformed AABB coverage assertions were added.

Phone: HUAWEI Mate 80 Pro Max, SGT-AL10, API 26, 1320×2848. No simulator result is
substituted for phone evidence. Ignored artifacts:

- `artifacts/harmonyos/native-event-20260916`: first event implementation,
  six alternating turns without reopening loading. Observed first-frame → reveal
  release 17 ms in one trace; this is **not** a P95 benchmark.
- `artifacts/harmonyos/metaflow-opening-batch-20260916`: reveal profiles/AABB preview,
  six more turns and a single-model necklace reveal sequence. Files named
  `cold-*.jpeg` are capture names only: cache state was not proven, so these are
  **not** classified as cold-network measurements.
- `artifacts/harmonyos/metaflow-opening-final-20260916`: final surface-generation
  build, repeated opening/turn checks and foreground recovery.
- `artifacts/harmonyos/NextNews-metaflow-opening.hap`: latest locally signed HAP,
  SHA-256 `9b28897ff0921bb88822bf0aeae509ceac46fbddc90a29246f65ff1e175da300`.
  Final trace: first presentation 22:06:25.290, reveal release 22:06:25.311;
  no repeated release during the six turns or foreground recovery.

## Remaining scope (not accepted)

- Complete external MetaFlow player configuration merge, subject/environment
  shared bounds/timeline, late environment readiness and high-detail gating.
- Poster/progress visual comparison under throttled known/unknown transfers;
  dedicated phone cancellation/reopen and window recreation stress matrix.
- Complete toolbar/input/controller/annotation parity and collision boundary
  acceptance; all post-processing, skybox/audio playback, higher-order SH.
- Granular renderer capability API, Huawei presentation events, native tiled
  evidence and effects equivalence. Huawei still uses its earlier readiness path.
- Full four-viewer comparison and 20-run performance gates. Prior failed 2M/4M/8M
  results remain applicable until new measurements; no detail/resolution reduction
  has been used to declare a pass.

The source for three reveal profiles exists, but only the ordinary profile has
this batch's phone image sequence. Character/mega profile visual parity is still
an explicit validation gap. Raw resources, reference repositories, toolchain and
other platform files were not modified.
