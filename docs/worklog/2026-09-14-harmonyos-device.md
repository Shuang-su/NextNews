# 2026-09-14：首台实机接入

- HDC 识别 HUAWEI Mate 80 Pro Max，API 26 Release，与现有目标兼容。
- 使用项目现有 DevEco CLI/SDK 和忽略目录中的签名副本，生成实机调试签名、构建 HAP、安装及启动成功。未修改全局工具配置；证书、设备标识和账号日志不进入仓库。
- 通过 USB HDC 转发本机 8768 流式服务；应用默认 URL 无需改动。
- 公开样例在实机可见且正立。Maleoon 935 / OpenGL ES 3.2，1320×2623，8 万高斯首轮 10 秒得到 780 次交换提交。该数据不等于持续真机性能结论。

详细方法和结果见 ../harmonyos/device-20260914.md。
- 流式 SOG 选集达到 200 万高斯，58/533 个包缓存；固定轨迹 335 次交换 / 10.01 秒（33.5 次/秒）。测试停止继续下载并冻结常驻选集，不代表边下载边浏览的性能。
- 此时进程 RSS 605948 KiB，峰值 851832 KiB。设备计时扩展连续报告约0.01ms，保留原始读数但不据此宣称 GPU 耗时或占用率。
- 前景建筑可见，环境模糊在实机仍存在，继续保持为未验收项。
- 实机注入拖动与双击飞行切换成功；摇杆拖动的150速度参数被CLI拒绝，改用300后成功。此项不能代替用户手动触控手感与双指回归。
- Home后重新启动EntryAbility通过，公开场景保留；结束时手机停留在公开样例，USB流式服务保持可用。

## SpatialReconKit / ArkGraphics3D 真机实现与对比

- 重新阅读用户提供的官方 3DGS 加载指南，并对照已安装 API 26 SDK。恢复模型库中的「华为原生渲染」入口，使用同一批 PLY，加入 Rz(180°)、统一全视口、相机和固定十秒轨迹。
- 真机通过插件、Scene、GSNode 以及三类输入的可见画面；没有沿用模拟器失败来推断手机不支持。
- 修复 Component3D 更新 Scene 后保留旧画面的问题：卸载旧组件后销毁并重挂新场景。初轮旧画面下的帧率数据作废。
- 修复 Home 后返回只剩背景的问题：前台重载所选模型并恢复原相机参数；新截图确认恢复成功。
- 同一 1320×2623 视口、输入和轨迹，稳定表面记录均约 60 Hz。点击后的短时 120 Hz 与刷新率策略有关，不作为引擎性能胜负结论。SmartPerf 全进程 PSS、完整 FPS 数组与图像对比见 [专项报告](../harmonyos/spatial-device-20260914.md)。
- 新增 `scripts/harmonyos/compare_renderers.py`，使用实际渲染表面 ID 采集并校验新帧，避免误测桌面 `code_artSurface` 或沿用页面自己的交换计数。CLI 测试脚本也修正了模型库切换后需重新打开性能面板的步骤。
- 自动刷新签名时出现 TLS ECONNRESET；复用当日有效的本机真机签名完成部署，没有绕过 TLS 校验、升级工具链或提交签名。
- 华为路径目前验证三份小 PLY，尚无大规模／全量 SOG 的等量基准；背景及颜色处理仍有差异。保留 OpenGL 的飞行、拾取及 SOG 流式能力。

### 用户配置与完整场景修正

用户澄清配置名为 `viewer-settings.json`，要求以完整 Metaflow/SuperSplat 体验对标。接入两条路线的默认镜头、75° FOV 和默认动画；此前 8 万高斯的 LOD8 样例不再充当画质基准。完整华发分块已在手机显示，改进为 4 并发下载、视野优先细化、全量预载提前显示。找到本地定制 Viewer 工程并保留其未提交修改。构建、配置/流式/原生数学测试通过；实测图片及未完成差异见 `docs/harmonyos/viewer-settings-stream.md`。当前不宣称已实现完整交互、SH 或 Huawei 流式对等。

### 缓存阻塞修复

用户反馈进入缓存即卡顿。检查发现解码、合并、首次排序及纹理打包仍在 EGL 线程，已移到独立 loader；备用纹理按帧上传并复用存储，旧常驻场景在加载期间继续响应。增加可复现实机拖动/加载并行回归和上传耗时。华为 TiledGSNode 已增加请求下载适配器，但 PlayCanvas 清单返回节点后没有瓦片请求，记录为失败而非成功。浏览器重新可用，已核对官方 API、加载指南及此前文章。完整说明见 `docs/harmonyos/streaming-pipeline-20260914.md`。

### Compound scene suffix and 4M budget

Tested the user's proposal with byte-identical PlayCanvas metadata renamed
`huafa.scene.json`, also preserving `.scene.json` at the local engine URI.
Build, adapter regression and signed phone deployment passed; the native node
returned but no tile requests or visible scene followed. Recorded this separately
from the working native PLY path.

Enabled 4M OpenGL budget and captured eight drags, near-4M actual residency,
1.44 GiB sampled PSS, and multi-second preparation/55–58 ms sampled draw
pressure. Kept 8M unenabled pending optimization. Details and screenshots:
`docs/harmonyos/streaming-pipeline-20260914.md`.

### Cached turn publication

Clarified that the user's previously successful native streaming test was
in the phone's 第一现场 test app. Do not infer asset incompatibility or a
filename requirement from NextNews's current failed adapter test.

Removed the network busy barrier from camera selection and cached publication.
Regression verifies cached front/back detail can publish during pending
downloads. Build and signed deployment passed; the 2M phone loading/drag
regression passed (53 intervals). Full-scene CPU preparation remains a bottleneck;
this change is not a claim of complete SuperSplat parity.

### CPU and GPU cache reuse

Implemented exact source-range identities for incremental texture rows and
weighted LRU caches for decoded files and selected-range histories (8M each,
render budget unchanged). Initial row-only probe showed poor reuse when tight
packing shifted offsets. Final phone test demonstrated 1,110 reused/659
uploaded rows in one update and selected-range cache hits; other updates
still rebuilt most data. Recorded the limitation instead of claiming complete
SuperSplat parity. 2M motion regression passed (59 intervals); process PSS
sampled 1.54 GiB. Host ASan/UBSan row/cache/parser/math tests and signed
deployment passed. “Upload” means CPU-to-GPU, not network re-download.

### Stable pages and multitouch follow-up

Implemented the experimental stable source-page renderer, AABB/FOV/rear-penalty
selector, byte-counted LRU caches, persistent SHA-256-checked file caching,
versioned first-swap records and a 20-pose full-target-coverage benchmark.
`docs/harmonyos/stable-pages-20260914.md` records the intermediate 2M results and
known differences from SuperSplat. Default rendering remains the compatibility
route. The single-SOG Huawei comparison is separate from native tiled loading.

The user reported right-thumb contact stealing the left-thumb flight stick.
Root cause: reading global `touches[0]`, and resetting on any pointer-up. Added
independent stick/viewport pointer ownership and suppressed tap gestures after
combined input. Host regression covers both arrival orders, array reordering,
unrelated releases, owner release/cancel, no automatic ownership transfer and
subsequent independent taps.

Installed the fix on Mate 80 Pro Max/API26 and injected simultaneous two-finger
motion using the system `uinput -T -m` interface. Right-first held stick geometry
was exactly unchanged ([173,2163,288,2278]); left-first upward movement changed
only vertical geometry ([173,2116,288,2231]). Owner-up and system-cancel logs both
reported owner=-1 and zero x/y. A long two-finger hold invoked system recognition;
that cancellation is recorded separately from the shorter normal release.
Evidence: `evidence/stable-pages-20260914/multitouch.json`.

Registered LOD selection in native C++ after profiling ArkTS selection at tens
of milliseconds. Verified exact parity against the ArkTS selector for 3,443
leaves and eight pose/budget pairs, then observed 1–4 ms selection on device.
The latest 20-trial native-selector results are preserved alongside the earlier
AABB measurements. All trials still require full target coverage and report actual
Gaussian data uploads. Performance remains experimental rather than a passed
comparison with SuperSplat.
