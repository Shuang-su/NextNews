# Viewer v1.31.2 接续矩阵

固定参考：SuperSplat Viewer `96f62515b99a28a20579041a656f7b1911c2964c`。
本地只读参考在 `.local/supersplat-viewer-reference`，MetaFlow 原工程未修改。
以下“预览验证”只指列出的案例，不表示整项完成或性能达标。

| 用户批准的范围 | OpenGL | Huawei | 仍需完成 |
| --- | --- | --- | --- |
| 共用相机、模式、标注选择、动画状态和后端切换 | 单体预览验证 | 同源单体预览验证 | 切换压力测试；完整统计适配器 |
| 旧版设置迁移、空镜头、step/spline、默认环绕/figure8 | 主机测试与部分真机验证 | 共用控制层 | 完整远程配置资源解析、逐项官方动画中断对照 |
| 底部官方图标、菜单、帮助、时间轴 | 预览；独立拖动触点与播放恢复 | 同页复用 | 官方加载封面、全部隐藏/悬浮/横屏规则与细节 |
| 标注编号、正文、导航、关联镜头 | 原 10 标注预览；新增 GPU 深度实验/旁浮窗已构建，待真机 | 原 10 标注预览；新增共用浮窗待真机，遮挡二维回退 | 连接恢复后验收遮挡、字形/重叠、边界/横屏；完整官方行为仍未通过 |
| 官方轻点/双点、环绕与飞行 | 控制层单测及部分真机 | 共用控制层 | 键鼠/触控板/手柄完整映射与对照；步行转向曲线 |
| 多触点归属 | 既有摇杆/视角回归通过；跳跃有独立触点 | 共用输入层 | 真机更多先按/后按/松开组合及时间轴干扰 |
| 单体和实际 32 块体素 | 2M 整场步行预览 | 80k 单体 + 同一碰撞预览 | 全地形、跨块边界与淘汰/取消压力验收 |
| GLB 碰撞网格 | 主机查询与手机合成夹具预览 | 手机合成夹具预览 | 真实 GLB 碰撞资源及完整地形对照；特殊扩展见 GLB 报告 |
| 出生点、重力、跳跃、滑墙、复位 | 主机与部分真机；新增碰撞透视边线 | 共用物理与调试投影 | 坡道、台阶、窄道、墙角的完整轨迹；调试并非深度遮挡 |
| 缓存与取消/恢复 | 稳定页/SOG 编码实验；碰撞共享 4 请求上限 | 单体 + 共用碰撞缓存 | 全量预加载 UI 与整场回归；Huawei 原生瓦片仍未通过 |
| 背景、天空盒、声音 | 配置部分解析，未完整渲染/播放 | 背景与 OpenGL 仍有差异 | 完整接入和生命周期验证 |
| 色调映射、锐化、泛光、调色、暗角、色散 | 未完成独立后处理通道 | 能力等价性未通过 | 按同源截图逐项实现/记录明确差异 |
| 高阶 SH | 当前验证固定 SH0 | 当前验证固定 SH0 | 数据保留、渲染接入和逐帧对照 |
| 2M / 4M / 8M 性能门槛 | 原实验未达标 | 整场不可作等价比较 | 每条件至少 20 次；不降低 LOD 或分辨率冒充通过 |

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
