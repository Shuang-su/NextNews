# viewer-settings.json 与完整华发流式场景（2026-09-14）

这次修复完成镜头配置接入与流式调度改进，**尚未达到 Metaflow Viewer 完整体验对等**。此前 80,000 高斯的 converted.ply 是 LOD8 诊断样例，不作为线上画质对标。

## 数据与参考版本

- 用户配置原件：`/Users/szmg/Documents/splat-transform/outputs/huafa-ice-world-99-lcc2-stream-voxel-20260901/viewer-settings.json`，原样复制到 rawfile。SHA-256：`3ebe7727e35ccd7eae07845bd245c2100ed4be295cb49dd25e97a5988bb0c8be`。原文件不修改。
- version=2，初始位置 `[331.5092468261719,-73.70159149169922,305.30859375]`，目标 `[294.58247057795904,-76.48368607030395,272.58759588286796]`，垂直 FOV=75°。
- `animTracks=[]` 不等于没有动画：参考 Viewer 在默认模式生成室内 figure8 / 室外环绕动画。此次移植 Hermite 插值、重复/往返时间映射与默认轨迹；配置镜头不再次执行模型的 Rz(180°)。
- 完整数据沿用 `.local/sog-huafa/scene.json`：533 文件、3443 叶节点、9 级 LOD，非 8 万高斯副本。原始数据来源与准备方式见 streaming.md。
- 已找到本机 `/Users/szmg/Documents/supersplat-viewer`，HEAD `cf1ae4f`、package 1.18.2、PlayCanvas 2.17.1，存在用户未提交定制修改，未改写该工程。另有 `supersplat-viewer-1.26.3-camera-limits`（HEAD `5cb3f27`）及 baseline 目录。
- 此次 spline 精确回归仍对照项目此前固定的 `96f62515`。本地 1.18.2 的 `viewer.ts`/引擎采用距离/FOV 补偿、背后惩罚 5、预算平衡和 radial sorting；我们的球体视锥选择器不是该算法逐行移植，也不能把这些版本等同于已确认的线上发布版本。

## 已实现

两条渲染页面共享配置解析与动画计算，均可选择“配置默认镜头”和“播放配置动画”，支持暂停/继续。OpenGL 投影、椭圆投影与拾取使用实际 FOV；Huawei Camera 使用对应弧度。Huawei 页面手动操作从配置视角接续，前后台重建保留位置目标与 FOV。

本机默认完整流式入口自动应用华发初始镜头，其他任意流式地址不自动套用此场景配置。下载由串行改为最多 4 个并发；全量预载也先显示已有覆盖，再细化和继续预载。视野外维持粗层，细化预算按实际 FOV 和宽高比分配。停止时取消全部在途请求，批次全部完成后才进行资源清理。

配置支持仍有边界：当前内置的是华发配置；默认轨迹的 inside 判定使用该场景原始前景 AABB。没有通用配置文件选择器，annotations、后期、背景渐变、步进插值尚未接入；默认按钮与动画按钮需显式操作，并非完整自动播放/复位行为对等。

## 实测与证据

设备：HUAWEI Mate 80 Pro Max，API26，Maleoon935，GLES3.2；设备序列号仅留本机日志。

- CLI HAP 构建通过，复用本机签名安装启动成功；最新调试包在 `artifacts/harmonyos/NextNews-debug.hap`。
- 相同配置的两个小样例实际显示检查：[OpenGL](evidence/viewer-settings-20260914/gl-default.png)、[SpatialReconKit](evidence/viewer-settings-20260914/spatial-default.png)。两者均测试播放，仍只有小 PLY 数据，不能作为流式性能对比。
- 完整流式实际画面：[完整场景](evidence/viewer-settings-20260914/full-stream.png)，截图显示 48/533 已缓存、约 181 万个所选高斯；之后调度状态达到约 200 万。雪道、门头与标牌已可辨认。调度数量不替代 GPU 实际提交统计；此轮未测可用于宣称性能领先的 FPS。
- 单元回归：真实配置解析、75°投影、归一化镜头、默认轨迹、重复/往返边界、参考 spline 数值一致；流式预算/恶意清单、前后视野、45°/75°、4 并发、全量预载提前显示、相同选择不重复上传均通过。
- 原生 math regression 通过，包括稳定排序、透明拾取、Rz 协方差与 FOV。构建不是显示验收的替代。

复现：`bash scripts/harmonyos/build.sh`；项目环境下运行 `node scripts/harmonyos/test_viewer_settings.cjs` 与 `node scripts/harmonyos/test_stream.cjs .local/sog-huafa/scene.json`。完整流式按 streaming.md 启动本机服务器及 HDC 转发。

## 对标仍需完成

OpenGL 当前仍是 SH0、CPU 排序与合并后全局 GPU 上传；没有逐块 GPU 常驻更新、过渡淡化、精确复刻本地引擎预算平衡，不能宣称同等或更快。新的视锥策略首先解决当前画面分配问题，不是完成性能优化。

Huawei 官方 API26 明确提供 loadTiledGSNode、GSTile.uri（SOG）、setTileRequestCallback、notifyTileReady 与 setCamera，支持分块请求路径。其具体场景 JSON 清单不能直接假定等于我们的 scene.json 或 PlayCanvas lod-meta.json；当前 Huawei 页仍为 PLY，需要完成可验证的原生分块接入后，再用相同镜头/分辨率/质量做对照。官方参考：https://developer.huawei.com/consumer/cn/doc/harmonyos-references/spatial-recon-spatialrender 。
