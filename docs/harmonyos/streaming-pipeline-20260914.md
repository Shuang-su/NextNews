# 缓存阶段阻塞修复与原生分块接入（2026-09-14）

## 结果与界限

OpenGL 的缓存更新原本在 EGL 绘制线程执行解码、合并、首次排序和纹理打包。它不阻塞 ArkUI，并不代表不阻塞画面。现在这些工作进入独立加载线程；新场景在备用纹理中按帧上传，上传期间保持旧场景可绘制，准备完才切换。实机连续拖动测试检查的就是 **加载中旧场景能否继续出帧**，不把缓存数字增加当作响应性证据。

Huawei TiledGSNode 已增加真实的请求适配器与页面入口；现有 PlayCanvas 清单可以让接口返回节点，但尚无瓦片回调/可见模型，因此原生完整流式验收仍未通过。

## 对照本机 SuperSplat 的流程

读取 `/Users/szmg/Documents/supersplat-viewer`（1.18.2，PlayCanvas 2.17.1，含用户定制修改；未改动其代码）：

1. `viewer.ts` 先将 LOD 限定到最粗层，首批资源就绪后开放细节范围，并使用移动端 100/200 万、桌面 200/400 万预算；背后惩罚为 5。
2. `gsplat-octree-instance.js` 根据距离、FOV 补偿选择目标层；`selectDesiredLodIndex` 优先利用已加载层，`prefetchNextLod` 分阶段请求下一档更细资源。
3. `pendingDecrements` 等状态在新资源准备期间保留旧层，而不是先撤下旧层造成空白。
4. `gsplat-sog-resource.js` 持有打包属性纹理；`gsplat-work-buffer` 管理渲染工作缓冲。它不是每个细化步骤都把整个场景重新展开成一大份 CPU float 数组。
5. unified sorter 使用独立排序工作流；本地 Viewer 也有 `config.gpusort` 开关，不能据此假定每个运行配置都采用 GPU 排序。

当前修复借鉴的是异步准备、保持旧画面和受限资源提交的流程。我们的 LOD 算法、数据布局、SH 与 GPU 分块更新尚未完整复刻。

## OpenGL 修改

- 增加 loader / renderer / sorter 三条职责分离的线程。
- loader 独占解码及范围缓存，生成不可变 Scene、初始排序索引与浮点纹理数据。
- 单个待提交结果采用最新请求代次；取消、切换模型、窗口重建不会提交旧请求结果。
- renderer 向备用纹理每帧上传最多 64 行，即 4 MiB；同时使用旧纹理和旧索引绘制。
- 纹理容量按二次幂增长并复用前后两份存储，避免小幅 LOD 变化每次都重新分配纹理。
- 完成后一起切换 Scene、纹理和初始索引，再为当前相机刷新排序。旧 Scene 和大块 CPU 像素缓存交给 loader 回收，避免在绘制线程集中释放。
- 增加最近上传批次耗时。绘制/交换耗时与上传耗时是不同口径；两者都不是严格 GPU 帧时间。

这仍会合并整个常驻集，CPU 准备可能耗时数秒；加载期间可操作不等于细节立即更新。纹理分配/驱动调用也不能承诺始终低于固定毫秒数。逐块压缩属性常驻、增量工作缓冲和更低延迟的 LOD 提交仍待完成。

## Huawei 修改与实际探测

`SpatialStream.ets` 调用真实 `loadTiledGSNode`，先注册 `setTileRequestCallback` 再设置驱动相机；限制两个并发请求，重复路径去重，在 `.part` 完整写入并 rename 后才 `notifyTileReady`。拒绝越界或跨来源瓦片路径；退出注销回调并取消请求，场景销毁后清理缓存。十秒无请求会显示诊断文字，不把 Promise 成功显示成加载完成。

实机：HUAWEI Mate 80 Pro Max，API26。测试 URI 为本机完整数据的 `lod-meta.json`。初次请求发现本机服务已停止，启动服务和 HDC 反向转发后清单 HTTP200。`loadTiledGSNode` 返回，显式设置可见性/缩放/相机，并复位、拖动相机后，仍未触发任何 SOG 下载，画面为空。证据：[原生未请求瓦片](evidence/streaming-pipeline-20260914/native-no-tiles.png)。

这证明当前资产/接入组合未成功，不证明 Huawei 不支持 SOG。官方 API 明确支持 SOG 瓦片，示例使用 `sanyapo.scene.json`；现有文档未给出该 JSON 的具体 schema 或可下载示例。仍需有效原生清单或生产工具来确认兼容性，不能直接重命名 PlayCanvas 清单就宣称完成。

浏览器已恢复使用，重新核对 [SpatialRender API](https://developer.huawei.com/consumer/cn/doc/harmonyos-references/spatial-recon-spatialrender)、[加载指南](https://developer.huawei.com/consumer/cn/doc/harmonyos-guides/spatial-recon-load) 和此前提供的 [性能治理文章](https://developer.huawei.com/consumer/cn/blog/topic/03223859612145383)。加载指南的 MP4/PLY/GLB 说明不能覆盖新版 TiledGSNode 的分块能力。

## 验证

- 配套 API26 SDK/HAP 构建通过，复用本机签名安装手机。
- `test_spatial_stream.cjs`：请求去重、两个并发、落盘后通知、路径拒绝、缓存命中通知、退出注销回调通过；这是适配器单元测试，不证明引擎渲染成功。
- 现有配置、清单、预算和调度回归通过。
- 实机回归命令：

```bash
source scripts/harmonyos/env.sh
bash scripts/harmonyos/serve_stream.sh .local/sog-huafa 8768
# 在另一个项目环境终端执行，会重启本项目 debug App：
python3 scripts/harmonyos/verify_stream_responsiveness.py
```

输出保存 UI 截图、无设备序列号的帧统计 CSV 与判定 JSON。连续采样中，同一旧场景保持 loading 且帧数增长，才计为加载期间出帧。这个回归不是 FPS 基准，也不能宣称已达到 Web Viewer 性能。

最终纹理复用版本的回归通过：记录到 **40 个相邻采样区间**，两端均为 loading、旧场景数量一致（大于 50 万）且帧数增加；后半段常驻集为 200 万。可检查 [原始 CSV](evidence/streaming-pipeline-20260914/frames.csv)、[判定结果](evidence/streaming-pipeline-20260914/result.json) 与 [加载期间的画面](evidence/streaming-pipeline-20260914/loading-motion.png)。这支持“缓存更新不再独占绘制线程”的结论；记录里 CPU 准备仍可到数秒、部分上传/切换批次仍为数十毫秒，因此不作“完全无卡顿”或“快于 Web”的结论。

本机测试服务增加 `bash scripts/harmonyos/serve_stream.sh .local/sog-huafa 8768 --background`，使用独立后台进程，绑定仍仅为 127.0.0.1；PID 与日志在忽略的 `.local` 目录。这样结束前台工具进程不必同时结束手机的测试数据源。手机访问仍依赖 HDC 连接，非部署公网服务。

最终包还执行了一次 Home→重新进入：200 万常驻场景恢复可见，截图留在本机 `artifacts/harmonyos/loading-motion-delivery/resumed.png`。这覆盖一次前后台恢复，不代替所有窗口/中断情形的压力测试。

## Follow-up: `.scene.json` suffix on the phone

Copied the original PlayCanvas `lod-meta.json` byte-for-byte to
`huafa.scene.json`; the server returned HTTP 200. The adapter now also saves
the downloaded bytes as `stream.scene.json` before `loadTiledGSNode`, so the
local engine URI has the same compound suffix as the official example.
On Mate 80 Pro Max / API 26 the node promise returned, but the tile callback
remained silent beyond 15 seconds and the viewport stayed empty. Renaming
alone did not produce streaming rendering in this integration; this does not
establish that Huawei does not support SOG. Schema and engine setup remain
to be checked. Evidence: ignored `artifacts/harmonyos/spatial-scene-suffix/`.
The adapter regression now checks the actual local URI and saved filename;
it passes, and both build and signed phone deployment passed.

## Four-million budget probe

Enabled the existing 4M setting on the same phone and full Huafa stream.
The native log reached 3,995,526 resident Gaussians; the UI selection reached
4,000,000. Eight alternating drag gestures retained a visible scene, including
while replacements loaded. SmartPerf sampled process PSS 1,505,718 KiB
(about 1.44 GiB). Some preparation updates took 7.9–8.6 seconds and sampled
Draw/swap costs reached 55–58 ms. The 12 FPS samples mix idle, drag and loading
and must not be interpreted as sustained interactive FPS. Evidence is in
`evidence/streaming-pipeline-20260914/budget-4m.json` and `budget-4m.png`.

Keep 4M as an optional quality tier; optimize its preparation and drawing before
raising the default. 8M remains untested and requires coordinated resident/GPU
capacity changes. The phone is left on the 4M tier for manual evaluation.
