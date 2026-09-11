# 鸿蒙开发环境与 3DGS 验证报告

验证日期：2026-09-11。C++ 原生查看器已在本机手机模拟器运行；华为 SpatialReconKit 对照路线尚未通过运行验收。完整计划仍有真机及专项验证项，不能视为全部验收完成。

## 环境与构建

- Apple Silicon / macOS 27；DevEco Studio 26.0.0.821、配套 SDK 26.0.0.105 / API 26 Release。准确路径及依赖版本见 [toolchain.md](toolchain.md)。安装盘校验及应用代码签名验证通过；未将本地 SHA 与官网 SHA 作一致性断言。
- 手机镜像安装于 `/Users/szmg/Library/Huawei/Sdk/system-image/HarmonyOS-7.0.0/phone_all_arm`，镜像版本 7.0.0.106；模拟器 `NextNews_API26`，4 GB RAM、6 GB 用户存储，HDC `127.0.0.1:5555`。
- 网络最初存在拒绝连接、TLS 握手和代理隧道 503；IDE 使用现有系统代理 `127.0.0.1:1082` 后，由用户再次下载成功。手机镜像已实际启动。额外平板镜像安装向导也显示安装完成，但平板未创建或验收。
- 空白 Stage 工程完成构建、签名、安装、启动并显示 Hello World。
- 查看器命令行构建、签名、HDC 安装与启动通过。IDE 对源工程执行“构建所有模块”也通过：`entry:assembleHap`，818 ms，退出码 0。IDE 源工程构建为未签名 HAP；签名调试副本由独立脚本生成，凭据留在本机。
- 当前调试包：`artifacts/harmonyos/NextNews-debug.hap`。应用源代码提交 `6777f54`。复现命令见 [README.md](README.md)。
- IDE 存在内部错误提示及快捷键冲突提示，未阻止本次构建；不据此断言 macOS 27 所有 IDE 功能均兼容。

## C++ / XComponent / GLES 3.0

已实际创建 EGL 上下文并渲染实例化椭圆高斯。图形字符串为 `OpenGL ES 3.0 (4.1 Metal - 91.7) / Mali-G77`，这是模拟器暴露的接口，不代表真实手机 GPU。

三类内置样例均可加载：公开 biker 80,000、本机 Tripo 输出 32,768、splat-transform 解码产物 80,000。另通过系统文件选择导入 260,000 高斯模型并旋转。模型连续切换 3 轮共 9 次均返回正确数量和就绪状态。旋转、平移、按钮缩放和复位进行了交互验证；双指缩放已实现，尚无独立双指注入验收记录。

以相同准备后的 SH0 PLY 在本机 PlayCanvas 2.18.1 参考查看器中定性比较形状、颜色和朝向；未进行像素级差分或完整遮挡测试矩阵。公开 biker 原始坐标在两边均呈倒置，Tripo smoke 样例在两边均很暗，未私自调整源坐标或颜色。比较中发现并修复了 XComponent 原生窗口缓冲尺寸不同步造成的比例失真，最终截图使用修复后的版本。

导入测试通过：

| 数据 | 结果 |
|---|---|
| 非 PLY 损坏内容 | `Not a PLY file` |
| 仅 xyz 普通点云 | `Missing float Gaussian field` |
| x 含 NaN | `Non-finite Gaussian value` |
| 截断二进制载荷 | `Truncated or unexpected PLY payload` |
| 300,001 个顶点 | `Invalid vertex count` |
| 129 MiB 文件 | 文件超过 128 MiB |
| 合法 260,000 高斯 | 正常显示 |

以上通过系统浏览器下载、文件选择器导入完成，错误时原模型仍保留。期间多次浏览器/文件选择器与 App 前后台切换后可继续使用。进入华为对照页面再返回也保留 C++ 页面。旋转模拟器左右方向操作成功，但 App 仍为竖屏，因此**不将该操作计为窗口尺寸变化或窗口重建通过**。同进程强制表面重建、长时间前后台循环、内存泄漏曲线仍待专项验证。

主机 C++ 解析、协方差、排序及异常数据测试通过 ASan/UBSan，三类样例与 260k 压力数据解析通过。使用每条命令指定的 Xcode 27 工具链绕过本机 Xcode 16 ASan 初始化挂起；未修改系统 Xcode 选择。Linter 检查文件数为零，未计为测试通过。

## 模拟器诊断快照

以下为单次快照，存在调度波动；加载不含系统文件下载和复制时间。

| 模型 | 高斯数 | 加载 ms | 排序 ms | 提交/交换 ms | 缓冲估算 MiB |
|---|---:|---:|---:|---:|---:|
| 公开样例 | 80,000 | 40 | 47.2 | 6.3 | 11.9 |
| Tripo | 32,768 | 17 | 24.1 | 2.0 | 4.9 |
| 转换样例 | 80,000 | 39 | 44.0 | 4.3 | 11.9 |
| 260k 导入 | 260,000 | 127 | 137.9 | 7.2 | 38.7 |

260k 时 `hidumper --mem` 的进程 Total PSS 为 106,548 kB，约 104.1 MiB，为单次进程内存快照。UI 的 fps 是 `1000 / (排序耗时 + 帧耗时)` 的折算，**没有测量持续交互帧率**；空闲时不连续绘制。这些值不可外推为真机性能。CPU 排序在 260k 时已明显影响交互，需要后续优化。

## 华为原生对照路线

`SpatialViewer` 编译通过，能力检查通过；运行在 `Scene.load()` 阶段失败，界面显示 `Creating scene manager failed`。系统日志包含 `InitRenderManager ctx is invalid` 和 `Failed to build object SceneManager`。将它设为独立首屏、先于 C++ 上下文创建也重现，未证明由两个渲染器争用引起。

因此目前没有 SpatialReconKit 模型成功显示的证据。需在受支持真机复测或进一步定位 SDK/模拟器 SceneManager 初始化问题。保留错误提示、场景释放和返回 C++ 页面能力。AR 放置 API 的调研记录见 [rendering-routes.md](rendering-routes.md)，未宣称实现或测试 AR 会话。

## 本机交付证据

源码位于 `apps/harmonyos`；私有仓库为 [Shuang-su/NextNews](https://github.com/Shuang-su/NextNews)。签名、SDK、安装盘、日志、测试临时文件和 HAP 均未上传 Git。

`artifacts/harmonyos/` 保存：

- `NextNews-debug.hap`、`NextNews-debug.hap.sha256`、`debug-build.log`。
- `emulator-boot.png`、`smoke-running.png`。
- 最终画面 `accepted-public.png`、`accepted-tripo.png`、`accepted-converted.png`；同名 JSON 包含设备 UI 与诊断数值。
- `qa-stress.png`、`stress-orbit.png`、`stress-memory.txt`。
- `device-import-checks.json`、`model-switches.json`、`qa-*.png`。
- `spatial-final.png`、`spatial-isolated-errors.log`、`route-return.png`。

源文件 provenance、许可及转换命令见各 rawfile 模型旁 manifest 和 `THIRD_PARTY_NOTICES.md`。模型预处理输出独立副本；普通点云不会被当作 3DGS 接受。

## SpatialReconKit 后续定位

最小基础 Scene.load 在不加载 GS 插件、模型及 C++ 上下文时仍失败。官网明确暂不支持模拟器；详情与可复现诊断见 [spatial-investigation.md](spatial-investigation.md)。当前调试包已补充阶段日志和官方支持范围提示，C++ 渲染代码未改动。

## OpenGL 交互迭代

继续以 C++/OpenGL 为主路线。已加入触点 ID 跟踪、双指同时平移/缩放、指数阻尼、世界坐标环绕中心和可收起工具。新增控制器回归测试及 C++ 相机中心断言通过，HAP 重新构建安装通过。工具面板改变窗口尺寸时的旧 EGL 缓冲问题已通过补绘修复；收起时保持模型屏幕尺度，最终截图见本机 `artifacts/harmonyos/orbit/accepted-immersive.png`。完整功能差异与验证边界见 [interaction-parity.md](interaction-parity.md)。

## 鼠标、飞行与流式追加验证

已在 XComponent 上方加入透明可聚焦输入控件，鼠标合成触摸过滤后只处理一次；系统 uinput 鼠标左键拖动确实改变相机角度。飞行模式旋转保持眼点不变，W 长按造成位置移动，双指张合与中心平移同时改变缩放和位置。键盘注入使用 `uinput -K -l 2039 3000`，早期把多个键盘命令合并调用不能作为有效验证。鼠标轴事件代码已接入，但本机滚轮注入未观察到相机变化，仍待外设复测。

流式以本机已有华发 LOD8 诊断块为源（524,474 高斯），不是整个华发资产。32 块通过离线长度、SHA 和原生解析检查；HTTP 逐块显示、视角变化下载新块、8 块常驻/16 块缓存、停止切回本地、404 与重试恢复通过。不存在完整多层 LOD 覆盖，未选块可能缺失；限制详见 streaming.md。相关实现、构建日志及测试命令已记录在工作日志。

## Native SOG / Viewer parity follow-up

The previous camera-yaw-only 180° change was incorrect for model orientation.
The current import transform is the upstream Viewer's **model Rz(180°)**,
including covariance xz/yz signs. See `stream/rz180-public.png` for the upright
public biker. Single-click uses cumulative Gaussian opacity picking, and
exclusive double-click switches orbit/flight while preserving the eye.

SOG v2 native decoding was compared to independent splat-transform SH0 PLY for
524,598 Gaussians. Maximum absolute position error: 3.05176e-5; color: 1e-6;
relative covariance: 2.93553e-6. The decoder suite also rejects bad ZIP checksums,
truncation, limits, codebooks, scales, dimensions, nested JSON and traversal.

```bash
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer \
  python3 scripts/harmonyos/test_sog.py input.sog independent-sh0.ply
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer \
  python3 scripts/harmonyos/test_core.py
source scripts/harmonyos/env.sh
node scripts/harmonyos/test_orbit.cjs
node scripts/harmonyos/test_stream.cjs
```

`apps/harmonyos/tests/math_regression.cpp` additionally checks Rz180 covariance,
translucent/empty-space picking, and exact stable radix ordering against the old
comparator. Native parser/SOG host checks use ASan/UBSan. Logs are in
`release-{controller,core,sog}-tests.log` and `math-regression.log` locally.

### Performance measurements

A diagnostic ten-second orbit uses yaw `0.3 * elapsedSeconds`, pitch 0.2,
distance 3 radii, 45° FOV, a fixed pivot and full SH0 input. It counts submitted
EGL frames, not `1000 / frameCost`. The Web script uses the same trajectory,
input and canvas dimensions and counts actual `postrender` events.

| Input / canvas | Native simulator | Upstream WebGL |
|---|---:|---:|
| Public PLY, 80,000 / 1144×671 | 59.9 fps | 60.1 fps |
| Identical SOG, 524,598 / 1144×671 | 59.9 fps | 60.0 fps |

The SOG run's native RSS was 307,332 KiB (about 300 MiB), process high-water mark
369,532 KiB. This includes application/runtime memory and is distinct from the
UI's estimated model/GPU buffers. `/proc/<pid>/status` is the source; this is not
a GPU-wide memory measurement.

A two-million-point full-scene run before independent sorting reached 17.9 fps;
it exposed sorting on the critical render path. A later two-million-point run
with independent sorting measured **39.1 fps** (391 frames / 10.00 s;
`stream/native-2m-async-benchmark`). These are comparable fixed trajectories,
not proof of identical selected LOD subsets. Sorting now runs independently,
coalesces requests and publishes only results for the current immutable scene.
Frames may briefly use the last completed order while turning, as asynchronous
sorting entails. Stationary views receive the final order. This is not a claim
of identical sorting latency or image output to PlayCanvas.

Both smaller comparisons are approximately 60 Hz limited. They do not establish
superiority, universal parity, or real-phone performance. Device: NextNews_API26,
API26, virtual Mali GLES3 on the Mac; browser: headed Chrome, ANGLE Metal/M3 Ultra,
Viewer 1.31.2 / engine 2.22.1. WebGPU, exact image differences, sustained thermal
performance, and larger identical input benchmarks remain separate checks.

### Streaming evidence

The full source has 9 LOD levels, 532 foreground bundles plus 1 environment bundle.
The simulator completed **533/533** package preload (about 3.1 GiB) and displayed
**2,000,000** selected Gaussians. Evidence: `stream/full-preload-complete.png` and
JSON. One interrupted transfer was recovered using retry, preserving cached
packages; the old error string lacked detail, so current code now shows code
and message. Preload and progressive paths share the same decoder/range checks.
A benchmark URL-input script initially inserted text instead of replacing it;
that Tripo run is not SOG evidence. Corrected input via Ctrl+A and verified the
524,598 count before the successful measurement (`stream/native-sog-524k-verified`).

Prepare a browser view of exactly the same SOG bundles:

```bash
bash scripts/harmonyos/prepare_web_reference.sh .local/new-sog-stream
bash scripts/harmonyos/serve_stream.sh .local/new-sog-stream 8768
```

Browser entry is `/` or `/?content=lod-meta.json`; native entry is `/scene.json`.
The adapter preserves original LOD ranges, but the native LOD selector is not
identical to the engine. Walk/collision, authored camera paths/settings,
annotations, XR and high-order SH remain outside the completed native subset.

### Final interaction checks

Actual mouse single-click focus changed the public model camera from reset to
`0,0,0.957,-0.001,-0.001,0.130`, preserving eye position.
`stream/mouse-single-focus-check.png` also confirms the upright model after Rz180.
Public / Tripo / converted / public switching completed with expected counts.
A 4,000,000-point selection was observed (`stream/quality-4m`); no sustained
4M benchmark or 1M device pass is claimed. Background / foreground preserved the
2M scene. Process RSS afterward was 669,532 KiB, high-water mark 1,227,416 KiB.
Emulator rotate commands reported completion but screenshots remained portrait;
landscape orientation is therefore **not verified**. A viewport-driven camera
distance change was removed: showing/hiding panels now preserves camera pose.

Final large-scene screenshots (`stream/final-stream-open` and
`stream/final-stream-collapsed`) still show a blurred environment and a nearly
edge-on foreground at the default camera. Preserving pose across panel resize
removes the implicit dolly, but does **not** resolve this image-parity issue.
The full-scene visual acceptance remains open; package completion, selected
count and FPS must not be interpreted as matching Viewer image quality.
