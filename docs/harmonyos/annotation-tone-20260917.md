# Annotation rendering and tone mapping — 2026-09-17

## Corrected reference interpretation

The earlier parity matrix incorrectly described MetaFlow's numbered hotspots as DOM content unaffected by scene effects. At the fixed MetaFlow commit `a871786dffdb195399f3e7427987d8db70296491`:

- `metaflow-viewer/src/annotation.ts` creates GPU `HotspotBase` and `HotspotOverlay` layers around the World layer. The base writes depth; the overlay ignores depth and has opacity 0.25.
- The material is an emissive `StandardMaterial`, not a DOM marker. It participates in scene tone mapping and CameraFrame effects. Tooltip title/body content is DOM and stays separate.
- `metaflow-viewer/src/annotations.ts` adjusts hotspot/hover color for CameraFrame's gamma behavior.

Therefore, moving the entire numbered hotspot after scene post-processing would diverge from the selected MetaFlow-first baseline. We retain the reference layer composition and keep ArkUI tooltip text outside scene processing. The earlier requested “isolate annotations” interpretation applies to tooltip content, not to making GPU numbered hotspots immune to effects. This source-based correction replaces the earlier matrix explanation; complete pixel parity is not claimed.

## Change

When offscreen effects are inactive, Gaussian colors already used direct tone mapping but native hotspots did not. Both hotspot passes now receive the same direct tone selection, applying linearization, the pinned tone function, and display conversion before premultiplied alpha blending. When offscreen processing is active, the hotspot shader uses no direct tone so the final compositor processes it once. Depth-writing, glyph generation, projection and 25% overlay behavior remain unchanged.

## Validation

`test_hotspots_gpu.py` builds the production hotspot class with desktop platform header/log shims and only replaces the GLSL version declaration. An actual Apple GPU framebuffer check passed default/ACES modes, normal/hover colors, visible/depth-occluded cases and 25% overlays. This is a desktop GPU regression, not phone acceptance.

```bash
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer python3 scripts/harmonyos/test_hotspots_gpu.py
```

Unsigned and signed builds use the existing SDK; the debug artifact is `artifacts/harmonyos/NextNews-metaflow-viewer.hap`. Exact same-camera Web comparisons, complete post-effect combinations, edge glyph antialiasing and Huawei depth remain outstanding.

Phone check: HUAWEI Mate 80 Pro Max / SGT-AL10, API 26. The signed build installed and launched. In the five-annotation fixture, ACES direct rendering preserved the front marker, the dimmed fully occluded marker and the partially occluded marker. The occluded marker remained clickable and its ArkUI title/body popup displayed. Evidence: `artifacts/harmonyos/annotation-tone-20260917/aces-direct.png` and `occluded-popup.png`. The early `fixture-opengl.png` was captured during opening and is not used as the settled-frame proof.

The settled `none-direct.png` provides the same-view baseline. Sampling the front marker's solid gray pixels found RGB (204,204,204) without tone mapping and (214,214,214) with ACES, consistent with the desktop shader check. This verifies that the phone used the new direct-tone uniform; it is not a full-image equivalence metric.
