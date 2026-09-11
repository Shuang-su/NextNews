# Verification status — 2026-09-11

This is a development checkpoint, not completed runtime acceptance.

- Installed official Mac ARM DevEco Studio 26.0.0.821 at `/Applications/DevEco-Studio.app`; disk image verification and application signature verification passed. Paired SDK: 26.0.0.105 / API 26 Release.
- Command Line Tools 26.0.0.821 and DevEco CLI 1.3.0-stable are installed in the ignored project `.tools` directory. Bundled Node 24.14.1, OHPM 26.0.0.630 and Hvigor 6.26.4 are used without changing global shell settings.
- Empty Stage application and C++/ArkTS 3DGS application built successfully. Current HAP is unsigned; see `entry/build/default/outputs/default/entry-default-unsigned.hap` under the application.
- Native parser, covariance and sorting tests passed with ASan/UBSan using a per-command Xcode 27 override. Three prepared model types and a 260,000-Gaussian host stress case passed. These results do not establish device rendering or performance.
- IDE launched and opened the project. Emulator download failed through CLI (connection refused) and IDE (SSL handshake terminated). Proxy diagnosis is ongoing.
- Signing, HDC connection, installation, EGL rendering, visual comparison, gestures, lifecycle and device metrics remain pending.
- Linter reported zero files checked, so its empty findings are not counted as validation.

## Follow-up: two renderers and network diagnosis

- Added `pages/SpatialViewer`: capability check, asynchronous SpatialReconKit plugin/model load, ArkGraphics3D camera, three shared SH0 samples, orbit/pan/pinch/reset, error display and scene disposal. Both renderer routes compile using the installed Studio SDK. Neither has runtime acceptance yet.
- Configured IDE HTTP proxy to the already configured local system proxy `127.0.0.1:1082`. IDE Check Connection to `https://developer.huawei.com` returned success. Retrying the image through IDE returned `HTTP/1.1 503 Service Unavailable` from the proxy tunnel. The simulator CLI still reported connection refused. This narrows the failure to the download connection; it does not prove the image server itself is down.
- Repository created and verified private: `https://github.com/Shuang-su/NextNews`. SDKs, installers, build artifacts and signing material are excluded.

- Subsequent user-triggered IDE retry began downloading the 2.31 GB phone image from `update.dbankcdn.com`; this supersedes the earlier network-blocked state. Installation and startup are still pending.
- DevEco CLI OAuth login completed. Credentials remain local.

Build/test/download logs remain in ignored `artifacts/harmonyos`. Full simulator agreements remain local; user authorized acceptance.
