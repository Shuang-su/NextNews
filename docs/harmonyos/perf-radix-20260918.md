# OpenGL 精确深度排序优化 — 2026-09-18

接续 `8add558`。范围仅 C++/OpenGL、固定 SH0；Spatial Recon 已按用户决定停止研究。

## 改动和正确性

将稳定浮点深度排序从四轮 8-bit 基数排序改为三轮 11/11/10-bit；最后一轮直接写索引，省去完整 Item 散写和再次提取索引。保留全部 32-bit 深度键、正负零归一、相同深度输入顺序、线程独立可复用工作空间。高斯、预算、LOD、坐标、画质及分辨率不改。

新增 `test_sort.py` / `sort_test.cpp`，以 `std::stable_sort` 为独立参考，覆盖随机视角、重复深度、正负零、极小/极大有限坐标、空场景、分页/PLY 布局及工作空间缩小。ASan/UBSan 与原 core fixtures 通过。200 万、400 万优化编译测试也逐索引比对通过。

Mac 200 万合成点五次排序：旧版 11.5–13.8 ms，新版 7.1–9.6 ms。此数据仅为主机验证，不作为手机性能门槛。

## 复测发现的加载故障

首次400万复测未进入有效测量：`publishCached` 因临时回退点数超预算停止会话。这份原始清单有143个叶子的点数并非随LOD单调递减，已缓存的较粗层可能比预算内目标层更大。现改为：目标总量合法、仅回退超限时保留上一帧和下载队列，目标就绪后提交；目标本身超限仍拒绝。没有修改清单、提高预算、截断点数或降低目标细节。

`test_stream.cjs` 新增120点缓存粗层、80点目标、100点预算的回归：等待时不发布超预算数据且保留待发布状态；目标到达后完整发布；实际超预算目标仍拒绝。全部流式测试通过。

失败测量保留于本机 `.local/perf-radix-20260918/4m`，不用于P95统计；修复后使用新目录复测。

## 真机复测

同一 Mate 80 Pro Max（SGT-AL10、API26、HarmonyOS7.0.0.105、Maleoon935），同一华发/前海场景与原配置镜头。固定 SH0、1320×2623、FOV75，GPU 编码稳定页；两个预热姿态后 20 次转向，另测 20 秒连续转动。截图是实机输出。

| 条件 | 上轮P95 | 本轮P95 | 门槛 | 状态 |
|---|---:|---:|---:|---|
|200万 GPU热缓存恢复|332ms|280ms|≤250ms|差异|
|200万连续转动帧间隔|33.401ms|33.893ms|记录值|差异（未改善）|
|400万 GPU热缓存恢复|610ms|546ms|≤500ms|差异|
|400万连续转动帧间隔|98.833ms|97.853ms|≤50ms|差异|

200万准备阶段P95 191.69ms、其中排序121.13ms；上轮分别229.61/156.87ms。200万原生提交到swap为237.61ms，400万482.53ms；端到端还含选择/桥接及状态采样延迟，不能用较小的原生数值代替门槛。

两档均20次、完整预算和覆盖率1、网络载荷0、Gaussian页上传0；连续帧分别882/424帧。200万数据取自同一排序实现、LOD修复之前，400万为修复之后；200万未触发该回退故障。证据见 [200万](evidence/perf-radix-20260918/2m/result.json) / [400万](evidence/perf-radix-20260918/4m/result.json)。

本轮没有重测文件缓存、800万或Web；此前文件缓存3.46秒门槛未通过的状态保留。当前改动是阶段优化，不是完成验收。400万连续绘制仍约98ms，需要进一步定位GPU绘制/同步成本；200万还需减少准备与提交阶段开销。旧值来自 [上轮验收](acceptance-20260918.md)，两轮没有随机化温度和测试顺序，差值不能全部归因于代码。保持完整覆盖率和零热缓存上传；未通过项不更改为通过。

## 复现

```bash
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer python3 scripts/harmonyos/test_sort.py
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer python3 scripts/harmonyos/test_sort.py --timing --count 4000000
bash scripts/harmonyos/build.sh
source scripts/harmonyos/env.sh
python3 scripts/harmonyos/benchmark_pages.py --encoded --continuous --budget 2000000 --out .local/perf-radix-20260918/2m
python3 scripts/harmonyos/benchmark_pages.py --encoded --continuous --budget 4000000 --out .local/perf-radix-20260918/4m-fixed
```

运行真机脚本前安装当前签名调试包，并确保本机8768测试资源服务与手机反向端口可用。输出目录必须不存在。
