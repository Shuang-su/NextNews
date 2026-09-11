# 2026-09-11 · NextNews 鸿蒙开发工作记录

## 目标与当前决定

为 NextNews 建立独立的 `apps/harmonyos` 原生应用和 `docs/harmonyos` 文档，后续平台各用独立目录。已部署 HarmonyOS 7 / API 26 工具链，完成 C++ / XComponent / OpenGL ES 3.0 的 3DGS 查看器。按用户最新决定，暂停 SpatialReconKit 模拟器尝试，继续 OpenGL 路线并逐步对齐 SuperSplat Viewer 的浏览交互。

这是一份实际工作记录。未通过的运行项目不以“编译通过”替代；测试数量、耗时与限制见[验证报告](../harmonyos/verification.md)。

## 工作经过

| 阶段 | 操作及遇到的情况 | 处理与结果 |
|---|---|---|
| 官方工具下载 | 端内浏览器点击下载未获得可靠下载完成证据，链接变灰不能说明下载完成或失败 | 用户在 Safari 登录并允许网站下载，提供正在下载的 Mac ARM Studio、CLI 和插件文件；未断言端内浏览器完全不支持下载 |
| 安装与校验 | 部分文件仍带 `.download` 后缀；存在 Windows 安装包 | 等待可用的 Mac ARM 安装文件，安装 Studio 26.0.0.821；校验 DMG 与应用代码签名；不使用 Windows 包；本地 SHA 未与官网 SHA 比对 |
| 工具隔离 | NextNews 后续还有其他平台任务 | 使用 IDE 配套 Node/OHPM/Hvigor/Native SDK/CMake/HDC；CLI 在忽略的 `.tools` 下，环境脚本不修改全局 shell 或 Xcode 选择 |
| 镜像网络故障 | CLI 连接拒绝；IDE SSL 握手失败；代理隧道返回 503 | 参照用户打开的官方代理文档，将 IDE 指向已有本机代理 127.0.0.1:1082；连接测试通过；用户再次下载后成功。未关闭 TLS 校验，未将网络错误认定为华为服务器宕机 |
| 模拟器部署 | 用户明确同意协议、下载与启动 | API 26 手机镜像 7.0.0.106 安装完成，创建 NextNews_API26，HDC 连接 127.0.0.1:5555；空白 Stage 工程签名、安装并显示 Hello World |
| 平板镜像 | IDE 平板镜像向导显示安装完成 | 尚未创建平板模拟器或验收；不能沿用手机运行结论 |
| 账号与签名 | DevEco CLI 需要开发者授权 | 用户完成 OAuth；签名配置保存在忽略的本机副本，证书和账号材料不进 Git；源工程 IDE 构建未签名，调试脚本单独生成签名 HAP |
| 原生渲染 | 实现协方差投影、SH0、实例化椭圆、透明混合和后台 CPU 排序 | 三类内置样例及 260k 导入模型实际显示；保留系统文件选择导入、错误提示及前后台行为 |
| 数据准备 | 普通 PLY 不一定是 3DGS；Tripo 输出存在 opacity 的 -Inf 端点 | 严格验证字段；压缩数据在 Mac 解码；仅明确的 opacity 无限端点有限化，其他非法数值拒绝；输出独立副本和 provenance |
| 画面比例问题 | 与 PlayCanvas SH0 参考画面比较时，转换模型比例不一致 | 发现 XComponent 原生窗口缓冲几何未同步，增加 SET_BUFFER_GEOMETRY 后修复 |
| 参考画面特征 | biker 倒置、Tripo smoke 很暗 | 参考与原生均有相同特征，保留源坐标和颜色；未当作渲染修复随意旋转或提亮 |
| 主机测试挂起 | macOS 27 上旧 Xcode 16 ASan 在 dyld 初始化挂起 | 单条命令指定本机 Xcode 27，ASan/UBSan 测试通过；系统 Xcode 选择未修改 |
| IDE 检查 | IDE 存在内部错误及快捷键冲突提示；linter 显示 0 文件 | IDE 构建确实成功；零文件 lint 不计为验证通过，不宣称整个 IDE 完全兼容 |
| 稳定性与压力 | 损坏文件、普通点云、NaN、截断、超数量、超体积 | 六类非法文件经系统文件选择器均拒绝，合法 260k 通过；三模型循环 3 轮共 9 次通过；持续帧率和长期内存泄漏未测量 |
| SpatialReconKit 尝试 | 插件加载后 SceneManager 创建失败，独立首屏也失败 | 继续隔离，最小 Scene.load 不导入 GS/模型/C++ 上下文仍失败；历史日志含 EGL 上下文失败；官网明确 Kit 暂不支持模拟器，详情见专项报告 |
| 当前交互迭代 | 原版需按钮切换平移，双指缩放与平移互斥，调试信息占据画面 | 按 SuperSplat Viewer 触屏 orbit 交互调整，双指并行平移/缩放、相机阻尼、固定世界环绕中心、可收起工具及按需显示诊断。差异见对齐清单；面板尺寸变化时补绘 EGL 新缓冲，并保持模型屏幕尺度 |

## 关键提交

- `522c21b`：初始项目及原生查看器基础。
- `513e892`：两条原生渲染路线。
- `16a8ab7`：解析/文档完善。
- `cd9235f`：工具版本记录。
- `6777f54`：XComponent 缓冲几何修复与隔离签名构建。
- `2a01724`：模拟器渲染、导入测试及限制报告。
- `2eb4410`：最小 ArkGraphics3D 隔离测试与官方支持限制。

这些记录描述阶段快照；后续变更以本仓库提交历史及最新验证记录为准。

## 结果与后续项

C++ 路线运行可用，模拟器 260k 时 CPU 排序约 140 ms，是下一步性能优化点。UI 折算 fps 不代表持续交互帧率，PSS 为单次进程快照，不能推断真机表现。双指完整设备注入、鼠标/键盘完整映射、点选焦点、fly/walk、窗口重建与长时间压力仍需按项推进。

SpatialReconKit 暂停模拟器排障，源码和最小测试保留；后续有受支持真机时可复测。离线 SOG/SPZ/KSPLAT 解码、流式 LOD、模型生成及业务页面不属于本轮实现。

## 索引

- [工具版本](../harmonyos/toolchain.md)
- [构建与模型准备](../harmonyos/README.md)
- [运行验证](../harmonyos/verification.md)
- [SpatialReconKit 专项排查](../harmonyos/spatial-investigation.md)
- [SuperSplat Viewer 交互对齐](../harmonyos/interaction-parity.md)

完整日志、调试 HAP、原始截图和签名材料保留本机，未将账户材料或 SDK 上传仓库。样例许可与来源单独记录于 THIRD_PARTY_NOTICES 和模型 manifest。


## 后续：鼠标、飞行与流式

按用户追加要求继续 OpenGL 路线：

- 为 XComponent 加透明输入层，明确区分鼠标与合成触摸。最初绑定在原生表面上的鼠标回调不可靠；透明层恢复左键拖动。空容器键盘焦点仍不可靠，改成透明可聚焦控件后，系统 W 长按注入确实使相机移动。
- 添加环绕/飞行切换，保留相机世界位置；飞行旋转只改变朝向。接入 WASD/QE、前后按钮、右/中键平移及轴事件；滚轮注入未证实有效，保留为外设复测项。
- 系统多指输入验证通过：两指张合与平移同时改变距离和目标位置。之前“未获得多指设备注入证据”的结论由本次记录补充。
- 将既有华发 LOD8 诊断块的 524,474 个高斯拆为 32 个标准 PLY 空间块；不改源文件。HTTP 清单与逐块下载实际运行，按视角选最多 8 块、缓存保留 16 块；转动视角观察到新 GET 请求，停止后切回内置模型正常。
- 这只是分块按需流式，不是完整多层 LOD；未选区域可能缺失，细节切换可能突变。没有宣称已接入完整华发项目或任意 SuperSplat 托管链接。
- 构建、控制器/飞行数学测试、原生相机断言、32 个分块的完整性及解析验证通过。运行证据、失败尝试和最终结果均留在 artifacts/harmonyos。

详见 [流式说明](../harmonyos/streaming.md) 与更新后的[交互清单](../harmonyos/interaction-parity.md)。

## Default 180° viewing direction

User requested default 180° rotation alongside the pending SuperSplat parity work.
Applied as camera yaw π around the vertical axis (source model data is unchanged).
ArkTS initial/reset pose and Native initial/post-load pose now agree, preventing
asynchronous model loading from resetting the rendered camera back to yaw zero.

Validation: controller regression checks passed; unsigned and signed HAP builds
passed; installed and launched on NextNews_API26 (127.0.0.1:5555). Device UI
confirmed initial yaw 3.142 and, after dragging and resetting,
`3.142,0.000,1.000,0.000,0.000,0.000 · orbit`. Local screenshots:
`artifacts/harmonyos/stream/default180-initial.png` and `default180-reset.png`.
Build logs: `default180-build.log`, `default180-signed.log`, `default180-run.log`
under `artifacts/harmonyos`.

Outstanding larger request remains: single-click scene picking/focus, double-click
orbit/flight toggle, native SOG loading and complete hierarchical scene coverage,
and controlled same-scene Web Viewer performance comparison. The default-angle
change does not complete those mechanisms or establish performance parity.

## Native Viewer parity, SOG and orientation correction

The user's screenshot showed the biker still upside down. Upstream `src/index.ts` uses `entity.setLocalEulerAngles(0, 0, 180)`. The earlier c17b23d camera yaw π implementation passed its pose assertion but did not implement this import transform. Corrected to model Rz(180°): negate position x/y and covariance xz/yz, transform scene/LOD bounds, restore camera yaw zero. `stream/rz180-public.png` shows the biker upright. Original assets are unchanged.

Single-click picking accumulates projected Gaussian opacity on a background worker and preserves the camera eye when changing the orbit pivot or aiming in flight. Exclusive tap gestures map double-click to orbit/flight. Touch pan/pinch and mouse/key flight remain. Device tap/double-tap and changed focus pose are recorded in `stream/focus-double-verified.{json,png}`.

Added native SOG v2 decoding with libwebp 1.6.0, nlohmann/json 3.12.0 and SDK zlib. Compared 524,598 points with independent splat-transform SH0 PLY: maximum position error 0.0000305176, color 0.000001, relative covariance 0.00000293553. ASan/UBSan suite rejects truncation, CRC corruption, bad counts/codebooks/scales, inconsistent textures, JSON nesting and traversal (`final-sog-tests.log`).

The full Huafa source has 148,571,989 finest-level points and 297,170,328 across 9 foreground LOD levels. The old 524,474-point file was a partial LOD8 diagnostic. The new adapter bundles 532 foreground resources plus an environment resource, retaining original leaf ranges. Missing ranges are valid sparse source levels, not a reason to copy arbitrary finer files into coarse coverage. The selector retains the complete original coarse layer and replaces available finer ranges within the budget.

Added progressive/all-package preload modes, range validation, decoded/selected-range caches, and 1/2/4-million quality budgets. The reference Viewer uses 1/2 million on mobile and 2/4 million on desktop. This is a budget correspondence, not identical LOD selection. `.local/sog-huafa` is served over loopback/HDC on 8768. Real loading passed 1.87 million resident points during progressive refinement. Repeated decompression was identified and replaced by selected-range caching. Preload bypasses unnecessary LOD selection; unchanged camera/budget reuses it.

Performance: O2 compilation, stable radix depth sort, RGBA32F Gaussian data texture, uint32 instanced indices and order reuse for translation/zoom. Host 260k sort matched reference ordering exactly: radix 1.87 ms versus comparator 67.98 ms (single host sample). Camera pacing is 16 ms. A fixed 10-second trajectory counts actual frames. At identical 80k SH0 / 1144×671 / 45° FOV: native emulator 600 frames in 10.01 s = 59.9 fps, upstream WebGL 60.1 fps. Both are refresh-limited; no claim of universal parity or superiority.

Web build uses Viewer 1.31.2 and engine 2.22.1, Chrome ANGLE Metal/M3 Ultra; native is NextNews_API26 virtual Mali GLES3. Playwright npm's global-cache permission conflict was resolved using a project-local cache. First-use emulator IME setup interrupted a UI script; setup proceeded under existing authorization. Source, build scripts and worklog are versioned; signing files, generated model bundles and artifacts remain local.

### 最后回归：模型方向与视口

- 用户指出人物仍然倒置后，对照 Viewer 的实体 Rz180，纠正了旧版仅修改 yaw 的错误。位置 x/y 与协方差 xz/yz 同步变换，原始模型文件保持不变。公开人物正立，真实鼠标单击改变焦点，反复切换三个模型通过。
- 异步排序后的 200 万高斯固定轨迹实测 391 帧 / 10.00 秒，即 39.1 fps；之前同类轨迹为 17.9 fps。两次 LOD 选集不能视为完全相同，因此不声明精确倍数或全面超过 Web。400 万高斯已观察到实际选入；未完成其持续帧率测试。
- 后台返回保留 200 万场景。模拟器旋转命令返回成功但图像仍为竖屏，不能作为横屏验收。收起面板曾自动按高度改变相机距离，导致大场景视点异常；移除这一行为，视口改变保持世界相机位置。
- 最终源码、签名构建与模拟器安装记录为 final-orientation-build / signed / run.log。签名、原始素材和体积较大的测试产物继续留在本机忽略目录中。

- 最后大场景截图仍有模糊环境和默认平视下薄片状前景。视口修复只消除隐式移动相机，尚未证明解决完整场景画质；该项明确保持未验收。流式机制和帧率记录不能替代与 Web 的同视角图像对照。

### GPU 与 Viewer 图标交互

- 对照固定版本 SuperSplat Viewer 的 UI、飞行摇杆与 PlayCanvas 高斯 shader，改成全画面原生视窗和悬浮 SVG 图标栏。设置、模型库、帮助与性能信息使用浮层；新增自动环绕、速度调节、触屏摇杆和 F/H/空格快捷键。
- 首次使用子组件 align 并没有把工具栏放到 Stack 底部，截图暴露了图标重叠；改用相对位置及自身尺寸平移后回归正常。诊断面板与设置分开显示，避免按钮出现在滚动区域之外。工具栏点击后的键盘焦点改为延后申请，并转发快捷键；用 W down / interval / up 验证实际移动。
- GPU 实现透明度支撑边界缩紧、离屏椭圆剔除、深度剔除后的延迟协方差读取和 uniform 位置缓存。加入可关闭优化的 A/B 诊断与异步 GPU 时间查询；模拟器不支持计时扩展，明确显示不可用。
- 初轮同常驻 200 万选集的新视口开/关/开均约 60 fps，不能把旧视口17.9/39.1 fps 与这些结果混算提升比例。公开样例同视角截图差异最大仅1个8位通道单位。
- 继续追踪环境模糊发现旧上限允许单轴约12288像素，而参考引擎限制在 min(1024,视口尺寸)。补齐屏幕尺寸上限，并单独验证大场景；这一项可能改变旧版极大高斯的画面，不以“完全无损”描述。
- 最终尺寸上限版本在同常驻 200 万选集 / 1256×2526 下，开启600帧/10.01秒（59.9 fps），关闭601帧/10.02秒（60.0 fps）。不宣称提升比例。截图中建筑屋顶前景重新可见，但环境模糊仍保留为未解决的图像对齐项。
- F 快捷键初次自动断言失败，进一步读取布局发现工具已隐藏，但按钮 id/高亮没有随状态变化；根因是 builder 按值参数。改为独立 ViewerTool 组件，通过 @Prop 更新图标、名称和选中状态，补充可复现 verify_toolbar.py 回归。
