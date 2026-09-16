# 线上项目与原始 SOG 流式接入候选

2026-09-16，接续 `4ae780d`。本批是 OpenGL 原生接入候选，**没有安装或操作用户正在体验的手机**。
主机验证和构建通过，不等于手机画面、交互或性能验收通过；完整 Viewer 范围仍见 [对标矩阵](viewer-parity-v1312.md)。

## 已实现

模型菜单增加「线上项目 · 数科 3DGS」：真实列表 API、名称搜索、20 条分页、封面、项目选择、取消和错误状态。
`modelId` 全程为字符串；实际列表中存在空标题，显示「未命名项目」而不使整页失败。
同一客户端缓存已取得详情，避免不必要地重复增加浏览计数。请求地址只在使用时去除两端空白。

- 单体：`modelFileUrl` 下载至私有缓存，后台 C++ 检查范围，再交给当前渲染适配器；`modelSetting` 同时接入镜头、动画和标注。
- 流式：`lodMeta` 后台 TaskPool 解析原始树，保留 AABB、每档 file/offset/count，`environment` 作为常驻叶节点。支持原始文件名，无需重命名为 `.scene.json`。
- 兼容没有 version 的旧清单及 version=1；真实“大运”有 19,043 个叶节点，因此 ArkTS 与 Native 两侧上限同步为 32,768，树最多 131,072 节点、16 档、1,024 分块、根 JSON 32 MiB。
- SOG 分块：按 `meta.json` 中 means/quats/scales/sh0 下载五张 WebP，C++ 直接读取；与 ZIP SOG 共用解码、协方差、Rz180 和编码 GPU 页实现，不重新打包或展开成整场 PLY。高阶 SH 仍未接入。
- `lodSettings` 接入当前 Viewer 控制层；`lodBin` 交给已有碰撞模块。切换场景、取消与页面退出不允许旧请求提交资源；根清单失败后重试保留该项目配置。
- 现有 `scene.json + chunk-XXXX.sog/ply` 路径保留；稳定 GPU 页与编码纹理仍遵循既有实验开关，未因主机构建成功改变默认渲染模式。

## 缓存和调度

纹理与碰撞共用 4 请求上限，分块最多四个任务。每个分块内按序取得五张纹理，后台原生解码继续使用既有队列。
完整分块才发布到选择列表，渲染中保留已有可用层级。缓存目录由入口 URL 和根清单内容生成 SHA-256 身份。
重开检查 ready 收据、六个文件的长度与文件名；不完整分块重新下载。收据在全部文件写入后原子提交。
恢复命中不下载纹理；原生仍对实际纹理、尺寸、数量与数值做验证。

未选中分块继续采用现有缓存回收策略，当前/上一视角及基础层受保护。单场景已完成分块预算 4 GiB，
预取受 512 MiB 阈值限制；并发提交保留字节预留，不能同时跨过完成数据预算。
单张纹理 32 MiB、单块总输入 128 MiB，临时下载另占空间。

限制：当前 CDN 文件缓存以源地址及根清单内容为版本；如果服务端在不改变根清单/URL 的情况下覆盖纹理，
尚无逐分量 ETag 重验证。跨场景缓存的全局 LRU、独立清缓存 UI 和下载字节进度封面仍需补齐。
现有本地 SHA 清单也改用 v4 缓存名字，旧 v3 缓存不会直接复用；应在后续全局缓存管理中处理迁移/回收。

## 验证结果

- 全部 49 份真实项目详情与配置通过业务映射测试；覆盖长 ID、空标题、前导空白、搜索编码、详情复用及无效响应。
- 14 份真实流式根清单逐个核对叶节点与每档范围，未改变 file/offset/count；未把跨 LOD 总数当作绘制数。
- 实际 CDN 前海冰雪世界 `streamed/3_0/meta.json`：526,546 高斯、五张纹理合计 5,343,680 字节。
  下载来源和 SHA 保存在 `.local/cdn-native-20260916/provenance.json`，不提交模型。
  ASan/UBSan 主机原生测试中，ZIP/散文件路径的每个位置、颜色、协方差完全相同；浮点/编码数据及 Rz180 对照通过。
  同时测试原本地 578,730 高斯分块，结果一致。
- 拒绝缺失/截断纹理、重复分量、目录穿越和符号链接。宿主缓存测试覆盖首次加载、重开零纹理请求、损坏缓存补载、取消不提交。
- 原流式调度、多指归属、镜头/标注/动画测试通过；12 组原生/ArkTS LOD 选择一致性回归通过。
- 配套 SDK 项目构建及本机调试签名构建通过。未改变全局工具链，未触碰 visionOS 目录。

可复现命令（先按 README 配置现有工具）：

```bash
node scripts/harmonyos/test_cdn_catalog.cjs
node scripts/harmonyos/test_cdn_manifest.cjs
node scripts/harmonyos/test_cdn_cache.cjs
node scripts/harmonyos/test_stream.cjs
node scripts/harmonyos/test_viewer_runtime.cjs
node scripts/harmonyos/test_pointer_input.cjs
node scripts/harmonyos/test_lod_parity.cjs .local/sog-huafa/scene.json
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer \
  /Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/build-tools/cmake/bin/cmake \
  --build .local/sog-test-build --parallel 8
python3 scripts/harmonyos/test_sog_unbundled.py .local/cdn-native-20260916/huafa-3_0.sog
bash scripts/harmonyos/build.sh
```

真实 API 测试复用 `artifacts/harmonyos/cdn-discovery-20260916`，不重新调用线上详情。
该目录可用已有采集脚本重新取得；结果可能随线上项目变化。

## 待真机与后续

本机包：`artifacts/harmonyos/NextNews-cdn-viewer-preview.hap`。SHA-256：`c618049e898ab957179c3315c4e9c4cbbac0fa22e3c9bf7b66b64b30ddee878a`。手机仍保留之前 `6d79cea` 演示。
先在手机打开一个单体和前海冰雪世界 CDN，确认实际画面、镜头、10 标注、实际 32 块体素和重开缓存；
再测快速切场景/取消、后台恢复、横屏、多指、冷/文件热/GPU 热各 20 次。新路径性能没有新结论。
华为单体复用同一配置但需实测；原始分块目录没有伪装成 Huawei `loadTiledGSNode` 成功路径。

然后继续官方 UI/隐藏与加载封面、完整输入、动画中断和步行恢复、天空盒/音频/后处理/高阶 SH。
不能宣称所有功能与 SuperSplat 完全相同，也不能以本次主机测试替代 2M/4M/8M 性能门槛。
