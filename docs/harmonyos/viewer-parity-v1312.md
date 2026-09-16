# MetaFlow 优先的 Viewer 接续矩阵

主基准：MetaFlow `a871786dffdb195399f3e7427987d8db70296491` / PlayCanvas 2.21.3。
通用对照：SuperSplat Viewer v1.31.2 `96f62515b99a28a20579041a656f7b1911c2964c`。
参考工程只读。下面是 2026-09-16 的当前状态，后文保留历史交付记录。
“通过”仅涵盖写明的测试条件；不能扩展为整项 Viewer 完全等价。

| 项目 / 参考来源 | OpenGL | Huawei | 证据与剩余差距 |
| --- | --- | --- | --- |
| 共用镜头/模式/动画/标注选择，ViewerRuntime | 差异 | 差异 | 同源单体切换预览；窗口重建与并发切换仍需完整压力验收 |
| JSONC、旧版迁移、空镜头、step/spline，MetaFlow settings | 通过 | 通过 | 配置/轨道主机回归；此项仅指数据和数学，不指动画完整视觉验收 |
| CDN 项目列表、详情、默认镜头 | 通过 | 差异 | 49 项详情、14 棵层级主机检查；正式 CDN 前海 2M/10 标注真机显示；华为整场未通过 |
| MetaFlow 路由/资源覆盖/主体环境，index/viewer | 差异 | 差异 | 共用场景描述、相对地址、环境追加已接入；实际双资源同源视觉矩阵未完成 |
| 首次加载、真实进度、海报和原生首帧事件 | 差异 | 差异 | OpenGL 首帧事件及六次转向无重复遮罩通过；原生失败按场景编号显示，浮点 SH 限制真机错误提示通过；全冷/热/取消矩阵与华为事件仍欠缺 |
| 粒子揭示 AABB、三套参数、环境时间线 | 差异 | 阻塞 | 普通模型手机证据；角色/大型体素及迟到环境视觉对照未完成；Huawei 无等价揭示 |
| 工具栏/模型入口/菜单/帮助/时间轴 | 差异 | 差异 | 已安装共用 UI；完整横屏、安全区、悬浮和隐藏规则逐项对照未通过 |
| 标注投影/关联镜头/弹窗/深度遮挡 | 差异 | 阻塞 | GL 24 bit 前/后/半透明夹具及真实 CDN 通过；后处理会影响原生标注颜色，Web DOM 不会；Huawei 遮挡未实现 |
| 轻点/双点、焦点圆圈、世界单位输入 | 差异 | 差异 | 控制层回归与部分手机证据；所有输入设备及表面方向完整对照未完成 |
| 多指触点归属 | 差异 | 差异 | 指针重排/独立释放/取消/稀疏事件测试通过；全部真机按下/抬起排列未完成 |
| 手柄双摇杆，MetaFlow GamepadDevice | 阻塞 | 阻塞 | SDK 接入、4 单位/s、18 度/s、FOV 数学通过；无物理手柄，未验证轴方向和断连；多手柄求和与按设备断连清理已实现并通过主机测试，实际设备仍未验收 |
| 体素单体/32 块、步行/重力/跳跃/滑墙 | 差异 | 差异 | 主机与部分真机预览；显式 voxelSpace 已接入，全地形、边界及淘汰压力验收未完成 |
| GLB 碰撞 | 差异 | 差异 | 查询与手机合成夹具通过；真实 GLB 及特殊扩展受限 |
| 后处理/色调映射/HPR，MetaFlow camera frame | 差异 | 阻塞 | GL 独立离屏、五效果、七 tone maps，手机开/关/转向/恢复；像素对照和全性能成本未完成；Huawei 禁用并说明 |
| HDR/RGBP 全景天空盒，MetaFlow loadSkybox | 差异 | 阻塞 | 4K HDR 文件缓存手机显示/转向，192 MiB；网络失败、JPEG/RGBP 真机及全窗口重建未通过；Huawei 未接入 |
| 背景声音/静音/音量，MetaFlow sound | 差异 | 差异 | AVPlayer、系统中断/静音/duck 与生命周期回归完成；未完成可听真机与实际音频焦点验收 |
| 单体 JSON SOG / bundled SOG | 差异 | 差异 | 原始 JSON 不改名，下载独立 meta.json 缓存；原子发布/取消/缺纹理修复主机通过；新单体 JSON 真机及 Huawei 未通过 |
| 稳定页/编码纹理/全量预载/缓存 | 差异 | 阻塞 | GL 已有实验路径，取消/重试和版本保护；磁盘总 LRU 与完整压力回归仍欠缺；Huawei 多块请求/相机驱动变化未证实 |
| 高阶 SH | 差异 | 阻塞 | [单体 PLY/SOG SH1–3](sog-sh-20260917.md) 解析与压缩 SOG GPU 采样已接入；手机方向色/SH0 切换通过；[编码稳定页 SH](sh-pages-20260917.md) 三来源真机方向色、非零二/三阶项与 SH0 切换有证据；2,910 组桌面 GPU 数值通过，20 次小样例热重选零上传；浮点稳定页及同镜头 Web 完整对照未完成 |
| 2M / 4M / 8M 性能门槛 | 差异 | 阻塞 | 旧实验没有达标，新完整功能回归尚未重测；Huawei 整场不可作等价比较 |

最新报告：[场景/声音/环境](metaflow-scene-20260916.md)、
[后处理与 voxelSpace](metaflow-postfx-20260916.md)、
[天空盒](metaflow-skybox-20260916.md)、[手柄与缓存并发](gamepad-20260916.md)。

原性能门槛不变：2M 热缓存细节恢复 P95 ≤250 ms、文件缓存 ≤1.5 s；
4M 热缓存 ≤500 ms、交互帧时间 P95 ≤50 ms。之前的测量见
[编码页报告](encoded-pages-20260914.md)，本轮碰撞功能验证不能代替性能回归。

AR/VR、标注/模型编辑和生成不在这次 Viewer 范围内。

交付记录：[第一批共用 Viewer](viewer-common-20260915.md)、
[第二批体素/步行预览](collision-preview-20260915.md)。
[GLB 碰撞与缓存恢复](glb-collision-20260916.md)继续第二批实施。
[碰撞透视与时间轴](viewer-debug-timeline-20260916.md)补上局部几何检查与动画/步行控制隔离。
[标注候选](annotations-20260916.md)已完成构建与主机回归；手机断开后未完成视觉验收，深度功能保留实验开关。

[线上原生接入候选](cdn-native-20260916.md)补上真实项目 API、镜头/标注配置与
原始 lod-meta → meta.json + WebP 直读，复用浮点及稳定编码页路径。
49 项详情和 14 棵真实层级主机对照通过；实际 CDN 分块原生解码一致。该候选尚未真机验收。

[交互与 CDN 真机修复](interaction-20260916.md)：标注响应式位置、点击目标环、明确模型列表入口与稀疏多指防崩溃已安装。正式 CDN 前海冰雪世界 2M/10 标注实际显示；此前“CDN 候选尚未真机”由此部分功能证据更新，完整性能、深度标注及 Huawei 对照仍待验收。

[OpenGL 深度标注默认启用](depth-enabled-20260916.md)：真机 24 bit、前/后/半透明夹具、完整标注功能脚本、上下文重建与真实 CDN 2M 默认路径已验证。此前“待真机且默认关闭”状态由本记录更新；Huawei 遮挡仍未实现，完整像素/性能对标未通过。

[位移输入接续候选](displacement-input-20260916.md)修复飞行滚轮/捏合/缩放按钮残留的场景半径放大和游戏双指附加移动。主机及签名构建通过，尚未安装/完成真机事件尺度对照。

[背景色接入候选](background-20260916.md)：共用配置进入 OpenGL/Huawei clearColor，模型清理恢复黑色，参数/重建模拟测试和签名构建通过。安装时无连接设备，实际两后端画面待验收；天空盒和其他画质效果未完成。

## Native opening enhancement (2026-09-16)

[Particle opening](intro-20260916.md) is phone-verified for ordinary OpenGL samples, with enable/replay controls. It is not claimed as an upstream v1.31.2 effect. Encoded streaming and Huawei equivalence remain unverified.

MetaFlow clarification supersedes the random stagger: [loading/range-wave preview](metaflow-opening-20260916.md). Poster, exact progress/first-frame handoff, focus-centered reveal and specialized profiles remain pending.

[First-open-only loading](firstframe-20260916.md) supersedes the earlier target-refinement loading indicator. Phone verified: six camera turns keep loading UI absent; first EGL presentation releases loading before particle playback; reveal starts at the world camera target. Exact AABB radius and other opening effects remain pending.

[MetaFlow loading events and reveal profiles](metaflow-loading-events-20260916.md)
supersede the 500 ms opening acknowledgement and conservative radius for sources
with a world AABB. Posters, actual single-SOG byte progress and config-relative
resource resolution are implemented. Three reveal profiles exist; ordinary
profile has phone evidence, character/mega visual comparison is still pending.
Environment orchestration and the broader MetaFlow-first plan remain incomplete.
