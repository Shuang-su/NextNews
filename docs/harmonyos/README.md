# HarmonyOS native 3DGS development

Latest implementation batches: [scene/audio/environment](metaflow-scene-20260916.md), [native post effects](metaflow-postfx-20260916.md), [panorama skybox](metaflow-skybox-20260916.md), and [gamepad/cache isolation](gamepad-20260916.md).
The current [parity matrix](viewer-parity-v1312.md) supersedes historical implementation/status descriptions below; full Viewer and performance acceptance remains incomplete.

Previous installed Viewer: [first-open-only loading and reveal handoff](firstframe-20260916.md), including the background and displacement candidates below.
[OpenGL depth annotations](depth-enabled-20260916.md) are now enabled and phone-verified for the recorded cases; Huawei remains a 2D fallback.
Included preceding changes: [configured background color](background-20260916.md), including [world-space wheel/pinch displacement](displacement-input-20260916.md).
The [v1.31.2 parity matrix](viewer-parity-v1312.md) distinguishes preview checks from unfinished acceptance.

## Architecture

The application uses ArkTS/ArkUI (Stage model), a surface XComponent, a C++17
native library and OpenGL ES 3.0. Models and streamed selections are decoded, merged, packed and initially sorted on an independent loader thread. A render worker owns its EGL context;
a separate worker sorts camera updates. Streaming textures upload to a staging texture in at most 4 MiB batches per frame; the previous resident scene remains drawable until the replacement is ready. Rendering uses instanced Gaussian ellipses,
projected anisotropic covariance and back-to-front premultiplied alpha blending.
The first version uses SH0 color, a 45° vertical field of view and orbit controls.
The main renderer uses native OpenGL ES. The separate **SuperSplat Web 对照**
page uses ArkWeb to run a pinned upstream viewer on the same phone.

`libsplat.so` exports `load(path)`, `camera(yaw,pitch,zoom,targetX,targetY,targetZ,fly)`,
`pick(x,y)`, `chunks(paths,bounds,ranges)`, `setActive(boolean)`, and `status()`. Model parsing is asynchronous relative to
ArkUI. The latest pending model supersedes prior loading; surface destruction
cancels work, joins the worker and releases the EGL resources. CPU scene data
is retained so a recreated surface can render it again.

## Local build

```bash
bash scripts/harmonyos/build.sh
```

For a signed debug HAP after starting the emulator and logging in:

```bash
source scripts/harmonyos/env.sh
devecocli emulator start NextNews_API26
devecocli auth login
bash scripts/harmonyos/debug-build.sh
```

The debug script synchronizes source into ignored `.local/signed-harmonyos`,
retains local signing configuration there and writes
`artifacts/harmonyos/NextNews-debug.hap`. Signing credentials never enter the
tracked build profile. With exactly one device connected, install and launch:

```bash
/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc install artifacts/harmonyos/NextNews-debug.hap
/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc shell aa start -b com.nextnews.splatviewer -a EntryAbility
```

The environment wrapper is project-scoped and does not edit shell profiles or
the machine-wide Xcode selection. DevEco CLI is pinned to `1.3.0-stable` in
`.tools/deveco`. Install it on another machine using:

```bash
npm install --prefix .tools/deveco --cache .tools/npm-cache --save-exact @deveco/deveco-cli@1.3.0-stable
```

Use Huawei's official Mac ARM DevEco Studio and its paired API 26 SDK. Open
`apps/harmonyos` in Studio. Signing is a local developer operation; never commit
certificate material or account-specific signing configuration.

## Model preparation

The app also decodes bundled SOG v2 (ZIP + lossless WebP) natively, with SH0 colors and full Gaussian covariance. No Mac PLY conversion is required for these SOG files.

The app accepts vertex-only binary little-endian 3DGS PLY with float32 fields:
`x/y/z`, `f_dc_0..2`, `opacity` (logit), `scale_0..2` (log scale), and
`rot_0..3` (wxyz quaternion). Normal vectors and other scalar properties may be
present. High-order SH, ordinary RGB point clouds, list properties, malformed
payloads and non-finite values are rejected. Limits: 4,000,000 Gaussians and
128 MiB. This is an interchange subset, not a general-purpose PLY importer.

Use the already-installed local splat-transform to decompress a copy:

```bash
node /Users/szmg/Documents/splat-transform/bin/cli.mjs input.sog -H 0 intermediate.ply
python3 scripts/harmonyos/prepare_model.py intermediate.ply output.ply --limit 80000
```

The preparation script normalizes wxyz quaternions, selects deterministic evenly
spaced source indices, and writes a SHA-256/provenance manifest. Exact alpha 0/1
exports use negative/positive infinity as their logit; **only this specific
opacity endpoint representation** is converted to finite logits -20/+20 and
counted in the manifest. NaN, infinite positions/scales and invalid quaternions
remain errors. Sources are never modified, and existing outputs are not replaced.

Built-in validation models:

| Model | Source | Prepared Gaussians |
|---|---|---:|
| Public | PlayCanvas engine biker.compressed.ply, pinned commit in public-source.json | 80,000 |
| Tripo | Local TripoSplat MPS smoke output | 32,768 |
| Converted | Local Huafa LOD8 SOG chunk, decoded by splat-transform 2.1.1 (bebac61) | 80,000 |

Additional `.local/samples/stress-260k.ply` contains 260,000 Gaussians. Local Tripo
and Huafa models are included only for this local development exercise; publication
is outside this task. Public example attribution is in THIRD_PARTY_NOTICES.md.

## Controls and diagnostics

Drag to orbit; use two fingers to pan and pinch simultaneously, or switch the single-finger drag mode for panning. Use +/− to zoom; reset
returns to the bounding-box center. Models receive the Viewer import transform Rz(180°); this is not a camera yaw rotation. Choose a built-in sample or use the system
file picker to copy a prepared PLY into the app sandbox.

The UI reports loading and sorting times, frame submission/swap time, buffer
memory estimates and a frame-cost-derived FPS. These are diagnostic estimates,
not measured device-wide RSS or a sustained interactive FPS benchmark. Idle
frames are not continuously redrawn. Simulator results do not predict phone
performance.

## Native core tests

```bash
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer python3 scripts/harmonyos/test_core.py
```

Tests use ASan and UBSan. On this macOS 27 host, the default Xcode 16 ASan runtime
deadlocks during dyld initialization; the per-command Xcode 27 override resolves
that host-tool issue. Tests exercise analytic covariance rotation, SH0/opacity,
sorting, cancellation, damaged payloads, ordinary point clouds, missing and
duplicate fields, NaN/Inf, invalid scale/quaternion and resource limits. Device
build, EGL rendering and gesture/lifecycle acceptance are separate checks.

## Official references

- https://developer.huawei.com/consumer/cn/download/deveco-studio
- https://developer.huawei.com/consumer/cn/doc/doccenter-capabilities/api/opengles
- https://developer.huawei.com/consumer/en/doc/harmonyos-references/js-apis-file-picker
- https://github.com/playcanvas/splat-transform

See verification.md for the actual completed checks and outstanding work.
See toolchain.md for versions and rendering-routes.md for the SpatialReconKit
comparison page and the separate future AR placement path.

See [interaction-parity.md](interaction-parity.md) for the SuperSplat Viewer orbit
comparison and [work log](../worklog/2026-09-11-harmonyos.md) for deployment and troubleshooting history.

## Flight and streaming

The mode button switches between orbit and free flight. Left drag looks around;
WASD move and Q/E change elevation while the viewport has focus. On-screen
forward/back buttons are also available. Flight has no collision or walking physics.
See [streaming.md](streaming.md) for the HTTP chunk manifest, offline converter,
local server commands and the limits of this first streaming implementation.

## Overlay controls and GPU diagnostics

The bottom icon toolbar offers orbit, fly, reset, auto-orbit, model library,
settings and help. Models/settings are overlays, so opening them preserves the
render viewport and camera. Right-side buttons zoom; fly mode adds a touch
joystick and elevation buttons. Settings include flight speed and splat budget.
F toggles immersive application UI, H opens help, and Space toggles auto-orbit.

The diagnostics panel offers a GPU optimization A/B switch and a fixed ten-second
trajectory. Optional disjoint timer queries report real GPU time when supported;
the current emulator reports unavailable. See verification.md for measured
results and interaction-parity.md for features still absent from the native app.

After a fresh application launch, run the toolbar device regression:

```bash
source scripts/harmonyos/env.sh
python3 scripts/harmonyos/verify_toolbar.py
```

It checks immersive/help shortcuts and double-click mode changes, and saves
fresh screenshots/layouts under an ignored timestamped artifacts directory.

First physical-device results (Mate 80 Pro Max, API 26) are recorded in
[device-20260914.md](device-20260914.md), separately from simulator measurements.

Huawei's SpatialReconKit / ArkGraphics3D comparison is now available in the model
library under **华为原生渲染**. The [real-device comparison](spatial-device-20260914.md)
records the three PLY inputs, orientation, scene remount fix and system FPS evidence.
To reproduce the sequential device capture (restarts only this debug app):

```bash
source scripts/harmonyos/env.sh
python3 scripts/harmonyos/compare_renderers.py
```

This page currently supports the bundled PLY samples, orbit/pan/pinch/reset and a
fixed trajectory; the richer flight/focus/SOG streaming viewer remains the C++ route.

Streaming responsiveness regression and the native tiled adapter status are documented in [streaming-pipeline-20260914.md](streaming-pipeline-20260914.md).

Streaming CPU caches now use bounded LRU retention for decoded chunks and
selected ranges. Retained GPU textures skip exactly matching rows (up to
4 MiB of changed rows per frame). This does not yet eliminate whole-scene
CPU packing. See the streaming pipeline report for memory costs and phone
evidence; `test_stream_cache.py` exercises row identities and LRU eviction.

The experimental **流式：稳定页** switch retains source GPU pages and sends a
versioned selection of addresses. See [stable-pages-20260914.md](stable-pages-20260914.md)
for implementation, measured gates and current limitations. `benchmark_pages.py`
records 20 full-coverage camera turns; `--file-only` drops reusable CPU/GPU source
identities while retaining the previous drawable scene and cached files. The
legacy path remains the default pending complete acceptance.

The **纹理：SOG 编码（实验）** switch adds 32-byte GPU Gaussians, two-file native
decode prefetch and row-coalesced uploads. It also unlocks the experimental 8M
budget. The **SuperSplat Web 对照** and **同源 PLY** controls provide separate
upstream/Huawei checks; see [encoded-pages-20260914.md](encoded-pages-20260914.md)
for reproducible commands, measurements and limits of the comparison.

Multitouch controls use independent pointer ownership for the flight stick and
viewport. Run `node scripts/harmonyos/test_pointer_input.cjs` for the regression.

## Common Viewer preview (2026-09-15)

The common Viewer preview supersedes the historical toolbar/key descriptions above:
F frames the scene, Space plays/pauses the configured animation, 1/2 select orbit/fly,
G changes gaming controls, and double-click focuses into orbit. The shared single-model
OpenGL/Huawei page includes viewer-settings migration, annotations and a timeline.
See [scope, device evidence and incomplete parity matrix](viewer-common-20260915.md).
`verify_viewer_common.py` is the current smoke test; historical `verify_toolbar.py`
expects superseded custom double-click/keyboard behavior.

## Shared walk / voxel preview (2026-09-15)

Key 3 enters experimental walk after collision resources are loaded; Space jumps in
walk. The native module uses the official fixed step and shared world coordinates
for OpenGL and Huawei. It supports single and tiled v1.0/v1.1 voxels, byte caches
and unknown-region blocking. See [scope, commands and phone evidence](collision-preview-20260915.md).
`verify_walk.py --backend OpenGL|Huawei` captures the actual 32-tile scene or the
Huawei single sample respectively; these are separate functional tests.

The follow-up adds static GLB collision and cancellation/cache-retry handling.
See [GLB scope and phone evidence](glb-collision-20260916.md) and the
[complete remaining parity matrix](viewer-parity-v1312.md). No current preview
claims full Viewer parity or passing the 2M/4M/8M performance gates.
