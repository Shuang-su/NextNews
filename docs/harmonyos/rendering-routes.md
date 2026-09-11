# Two native rendering routes

## A. Portable C++ Gaussian renderer

Existing `apps/harmonyos` uses ArkUI, XComponent, EGL and OpenGL ES 3.0.
It implements covariance projection and sorted Gaussian blending; it is not a
port of the whole PlayCanvas engine. PlayCanvas splat-transform supplies offline
format conversion and its public model supports comparison. Preserve provenance
and third-party attribution when reusing code or assets.

## B. Huawei Spatial Recon Kit

The official [loading guide](https://developer.huawei.com/consumer/cn/doc/harmonyos-guides/spatial-recon-load)
describes `spatialRender.GSPlugin.loadGSNode` with ArkGraphics3D `Scene` and
`RenderContext.loadPlugin`. The guide lists PLY, GLB and MP4 containers; that does
not establish compatibility with every third-party file using those extensions.

The installed API 26 SDK contains `@hms.graphics.spatialRender.d.ts` and
`@kit.SpatialReconKit`. Its declarations mark basic Gaussian loading since API 21,
and tiled Gaussian import since 26.0.0. Required capability is
`SystemCapability.Graphics.SpatialRender`. API presence alone does not prove
support on the selected emulator or phone.

Implemented a separate `pages/SpatialViewer` ArkGraphics3D comparison page with capability
checks, asynchronous plugin/model loading, camera controls, error reporting and
resource disposal. It compiles with API 26; rendering remains unverified. Use the same prepared SH0 models to compare orientation,
scale, color, occlusion and loading behavior. Keep the existing renderer usable
when the device lacks SpatialRender. Native reconstruction C APIs are a separate
generation capability, not required for the viewer milestone.

The official kit introduction explicitly states that simulators are not supported.
Our emulator also fails a minimal ArkGraphics3D Scene.load without the GS plugin.
See [spatial-investigation.md](spatial-investigation.md) for isolation results and
physical-device reproduction steps.

Track runtime results separately for each backend. Tiled loading is an optional
future experiment, not part of the current small-model acceptance requirement.

## AR placement extension

The ARViewController reference also exposes API 26 `loadGSModel` and
`removeGSModel` for placement inside an AR session, with one Gaussian model per
session. It requires AR capability checks and session initialization involving
camera and motion-sensor permissions. This is a future AR presentation mode;
the standalone comparison viewer can use SpatialReconKit directly. AR and AGP
coordinate systems must be converted explicitly when sharing poses.

Related user-provided sources:
- [SpatialRecon C API](https://developer.huawei.com/consumer/cn/doc/harmonyos-references/capi-spatialrecon)
- [AR scene management](https://developer.huawei.com/consumer/cn/doc/harmonyos-references/arengine-api-arviewcontroller)
- [3DGS performance discussion](https://developer.huawei.com/consumer/cn/blog/topic/03223859612145383)
