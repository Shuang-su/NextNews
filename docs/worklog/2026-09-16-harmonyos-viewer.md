# 2026-09-16 HarmonyOS Viewer 工作记录

继续已批准的三批 Viewer 计划。固定 SuperSplat Viewer v1.31.2；AR/VR 与编辑不在本批。

本次增加静态 GLB 碰撞读取/BVH，与两个后端共用体素步行模块；补上缓存取消、
恢复、损坏缓存重试与写入失败清理。300 组 Mesh 官方差分与原有 250 组 Voxel
差分通过，内存检查无报告。详情：[GLB 碰撞与缓存恢复](../harmonyos/glb-collision-20260916.md)。

Mate 80 Pro Max / API 26：两个后端分别验证合成 GLB 夹具的阻挡/滑动/跳跃。
这不是生产碰撞几何重合验证，OpenGL 2M 和 Huawei 80k 也不作相等画质/性能比较。
首次自动输入地址插入旧 URL，已记录失败并修正测试脚本。

最终包安装后回归了实际 32 块体素与 OpenGL 2M 整场；手机保留实际场景。
未上传原始模型、签名、账号、HAP 或工具缓存；其他平台改动保持原样。

后续按 [完整对标矩阵](../harmonyos/viewer-parity-v1312.md)继续：先补碰撞调试显示、
标注遮挡与浮窗、官方 UI 和输入细节（包括时间轴在步行时的行为）、跨块和多指
真机验收；再完成天空盒/声音/后处理/高阶 SH，以及每条件 20 次性能回归。
Huawei 整场流式和原性能门槛仍未通过，完整计划尚未完成。

继续补上共用碰撞透视线框和动画时间轴控制隔离：
[调试与时间轴报告](../harmonyos/viewer-debug-timeline-20260916.md)。线框限制数量、后台提取，
不改变实际渲染预算。时间轴绑定独立手指并恢复拖动前的播放状态，步行不显示时间轴。
后续优先标注遮挡/浮窗、完整官方输入与真实多指/跨块验证，再继续第三批画质与性能。

继续实现[标注候选](../harmonyos/annotations-20260916.md)：共用热点旁正文、边缘翻转/收边，
OpenGL 底层深度 + 透明高斯 + 25% 轮廓，10 KiB 数字图集及独立原创夹具。
HAP/签名构建、标注与原交互单测、原生 ASan/UBSan 通过。
早期包曾安装，但 UI 验证在夹具入口处中止，随后手机已不在 HDC 列表；无本批有效遮挡截图。
最终候选恢复默认二维热点，深度标注明确为实验；最终包尚未安装回手机。
菜单切换增加滚动复位，测试失败改为保留当前布局/截图。Huawei 继续二维回退，不声称等价遮挡。
后续先重连验收并回归真实 10 标注，再继续完整计划；不能把本次构建结果当作手机验收完成。

用户提供线上 3DGS 平台后，捕获实际列表/详情接口，读取 49 个项目及全部配置：
35 个单体 SOG、14 个流式。42 个 LOD 配置/清单与每场景一个分块 JSON 可读取，49 个 Viewer 配置通过现有解析器。
发现原始 CDN 流式为 JSON + WebP 分量，需要接入适配，不能冒充当前打包 SOG 路径已兼容。
记录 [CDN 接口及资源索引](../harmonyos/cdn-api-20260916.md)，保留前导空白、超大清单的实际处理情况。
手机重新连上后，按用户“打开看看”要求装回已验证 6d79cea 预览包并打开 2M 流式与 10 标注；
已截图确认。当前供用户亲自查看，停止自动点击，不安装标注候选打断观看。

继续原生 OpenGL 对齐并接入正式接口：[线上原生接入候选](../harmonyos/cdn-native-20260916.md)。
模型菜单支持列表、搜索、分页和项目选择；单体配置与流式 lodSettings/lodBin 接入共用控制层。
原始 LOD 树后台解析，C++ 直接读取 meta.json + WebP，复用现有浮点/编码页路径。
全部 49 详情映射、14 真实树范围一致性、磁盘热缓存和取消主机测试通过。
实际 CDN 526,546 高斯分块与 ZIP 路径逐高斯位置/颜色/协方差完全一致，ASan/UBSan 异常输入通过。
普通和调试签名 HAP 构建通过；新包仅保留本机，不打断用户当前手机体验。
本批仍需真机验收、跨场景全局缓存管理及 CDN 原地覆盖重验证；完整官方 UI/效果/输入及性能计划继续。

## 标注跟随与线上模型真机修复

完成响应式热点位置、世界坐标点击环、右上角模型列表和搜索可读性。真机定位并修复稀疏 fingerList 引发的 localX TypeError；15 vp 累计拖动抑制点击。原生拾取返回点击射线命中深度处的位置。主机输入/投影/配置、原生 ASan/UBSan 及配套 SDK 构建通过。最终签名包已安装，正式 CDN 前海冰雪世界实际显示 2M 高斯/10 标注/点击目标环，证据和包校验值见 docs/harmonyos/interaction-20260916.md。完整官方功能与 20 次性能门槛仍未完成，不将本次画面验证当作性能通过。

## 环绕近距离缩放

按用户反馈对照官方 zoomRange，移除共享输入与原生渲染的场景比例硬限制，改为 0.01 世界单位；动态裁剪面防止靠近时沿用整场静态近裁剪。不同半径测试及原生 ASan/UBSan 通过，已安装真机。完整证据边界和包哈希见 docs/harmonyos/orbit-zoom-20260916.md；不宣称完整画质/性能对照通过。

## 深度标注默认交付

用户指出默认无深度属实，旧版仅实验开关。真机完整 verify_annotations.py 通过并人工检查前/后/半透明遮挡和重建画面后，启用 OpenGL 默认 GPU 标注；最终包重装后真实 CDN 2M 自动显示逐像素遮挡。证据、哈希和 Huawei 未实现状态见 depth-enabled-20260916.md。

## 游戏控制大步进修复

按用户反馈定位场景半径放大摇杆、键盘及升降按钮位移。共享控制改为官方 4 世界单位/秒，键盘 .992/.993 加减速、Shift/Ctrl 倍率；触控轴释放立即停止；升降按钮触点独占、按住连续移动。不同场景大小及帧率测试通过，真机 CDN 2M 完成升降和前后移动观察，包已安装，详见 flight-input-20260916.md。

## 自动接续：飞行位移输入

发现 zoom/pinch 仍使用归一化位移，修复为世界位移队列，按官方 pinch/wheel 系数并隔离游戏控制双指；增加尺度、单次消费、取消及游戏隔离回归。构建/签名通过，候选未安装；不重复将前一包真机截图算作新输入验证。详见 displacement-input-20260916.md。

## 自动接续：背景色

将 background.color 从仅解析接入共用后端和实际 OpenGL/Huawei 相机清屏颜色，保留重建状态，切换模型清除旧配置。主机与构建签名通过。安装时未发现设备，候选未装；详见 background-20260916.md，后续不能将模拟后端测试当作画面通过。

## Particle opening after main merge

Continued from PR #2 merge on `codex/viewer-parity-next`. Added GPU opacity/size stagger, first-selection start, no LOD replay, pause/resume timeline and UI enable/replay controls. Built, signed and installed on Mate 80 Pro Max; inspected early/middle/final public-sample captures. Host timeline sanitizer and existing viewer/input/background regressions pass. See [report](../harmonyos/intro-20260916.md) for evidence and outstanding encoded-stream/Huawei/performance checks. No visionOS files changed.

## MetaFlow correction: loading panel and two range waves

User clarified that the opening must advance spatially, and loading must follow
MetaFlow. Located the actual local MetaFlow source (a871786), replaced random
stagger with dot/lift radial waves and the generic spinner with cyan sweep,
stage/elapsed and stopped red failure presentation. Phone-tested CDN loading and
panel removal; native/stream/input/runtime regressions and HAP builds pass.
[Detailed report and remaining differences](../harmonyos/metaflow-opening-20260916.md).

## First-open-only loading correction

Separated opening from recurring LOD activity, added per-request presented-frame acknowledgement and loading-component disappearance handoff, protected old upload generations and background/resume. Reveal origin now follows the camera target, frozen across LOD commits. Final HAP phone-tested with six alternating turns and Home/resume; all checks retained loading-panel dismissal. Native sanitizer, page handoff, viewer/input/stream regressions and both builds pass. [Evidence and remaining gaps](../harmonyos/firstframe-20260916.md).

## MetaFlow-first implementation: loading events and reveal profiles

Continued from `3434e53` in PR #5. Implemented native first-presentation events,
request/surface/subscription invalidation, CDN posters and real byte-based SOG
progress, config-relative resource resolution, exact supplied world AABB reveal
range, three motion profiles and GPU oscillation. Normal view no longer shows LOD
cache diagnostics. Configured animation is held behind initial loading.

See [implementation, checks and unfinished scope](../harmonyos/metaflow-loading-events-20260916.md).
This is the opening/configuration batch only; the full approved plan is not yet
complete. Signed HAP and device images remain local ignored artifacts.
