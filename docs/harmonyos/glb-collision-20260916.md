# GLB 碰撞与缓存恢复 · 2026-09-16

接续体素/步行预览 `850528d`，属于第二批的继续实现，完整对标尚未完成。

## 已实现

- 原生 GLB 2.0 嵌入式静态三角形读取，支持 float POSITION、8/16/32 位索引和
  非索引三角形。限制 64 MiB / 100 万三角形、4 MiB JSON / 64 层嵌套，并校验
  buffer、accessor、对齐、步幅、有限值和索引边界。
- 移植固定官方 `mesh-collision.ts` 的三角形 BVH、射线、球体和胶囊查询；胶囊使用
  官方六点采样与一次细化。与体素共用 60 Hz 物理、出生点、跳跃、重力、滑墙和复位。
- 和官方该版本 `fromGlb` 一样读取 mesh resource 的原始顶点：**不施加节点变换**，
  不额外 Rz180。应输入已经烘焙到 Viewer 世界坐标的碰撞网格。
- 单体 GLB 完整就绪前不能步行；就绪后视为完整静态碰撞资源。没有加载的体素区域
  仍保留未知状态。GLB、体素共用 128 MiB 原生引用保护缓存、256 MiB 文件缓存。
- 暂停/恢复增加任务版本，旧 HTTP 响应不能在恢复后发布为当前结果；文件缓存写入
  与 LRU 淘汰串行提交，失败清理 `.part`，损坏缓存清理后可重试，清单失败可重新打开。
- 华为诊断页也长期持有 GSNode；共用 Viewer 的华为统计面板不再留下上一个 OpenGL
  后端的数字。华为 GPU/排序数据没有等价来源时显示未提供。

压缩扩展（含 Draco、meshopt、量化）、sparse accessor、变形目标和非三角形图元
当前明确拒绝。没有外部 buffer 下载器，也没有执行动画/蒙皮。拒绝时显示原因，
不将这些能力标为兼容。

## 可复现测试

```bash
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer python3 scripts/harmonyos/test_mesh_collision.py
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer python3 scripts/harmonyos/test_collision.py
node scripts/harmonyos/test_collision_lifecycle.cjs
node scripts/harmonyos/test_collision_resources.cjs
bash scripts/harmonyos/build.sh
```

主机通过 300 组 GLB 射线/球/胶囊官方差分和原有 250 组体素差分；额外覆盖
地面、墙、出生点、长按跳跃、损坏文件、超限索引和过深 JSON，ASan/UBSan 无报告。
受控传输测试通过过期响应、暂停恢复、损坏缓存重试、磁盘写入失败、清单重开与停止。

真机入口测试使用**合成 GLB 夹具**，不会修改真实体素或模型：

```bash
python3 scripts/harmonyos/make_collision_fixture.py --out artifacts/harmonyos/collision-fixture
python3 -m http.server 8781 --bind 127.0.0.1 --directory artifacts/harmonyos/collision-fixture
# 新终端；继续使用已有场景服务 8768 与体素服务 8780
bash -c 'source scripts/harmonyos/env.sh; python3 scripts/harmonyos/verify_walk.py --backend Huawei --collision-url http://127.0.0.1:8781/room.glb --out artifacts/harmonyos/glb-huawei-new'
```

夹具地面 y=-83.12、墙面 x=330，配有独立 provenance.json，**不能作为真实华发
碰撞几何重合的证据**。Mate 80 Pro Max / SGT-AL10、API 26、7.0.0.105 上，
Huawei 同源 80k 单体配合该夹具记录 84 个位置样本：x 最低 330.1999，保持在墙面前
约 .2 m，z 从 305.3086 移动至 302.7472；跳跃采样升高 .760 m 后落地。
模型显示、移动后画面已人工检查；墙体本身尚未提供调试可视化，因此阻挡/滑动结论
来自已知夹具几何与连续位置轨迹，而不是截图中的真实建筑。

OpenGL 2M 整场配合相同夹具记录 81 个位置样本，x 最低 330.1999，z 移动至
302.5722，跳跃后落地。移动后和后台返回图像均已检查。
筛选证据在 [glb-collision-20260916](evidence/glb-collision-20260916/)。

最终包重新回到实际 **32 块体素**，完成 OpenGL 2M 步行/跳跃/后台返回回归：
82 个位置样本，最后落地，恢复画面已人工检查。手机保留该实际场景，合成夹具没有
替换用户的真实资源。最终调试 HAP 位于 `artifacts/harmonyos/NextNews-walk-glb-preview.hap`，
SHA-256：`84e20ee3653ce5432547e136e880ce6c371c2dc705eca4912f46a2662fd0928f`。

首次 UI 脚本将新 URL 插入旧文本中，下载失败，该轮无效；修正为先全选替换，并且
在确认碰撞已就绪后才继续执行步行测试。原始失败记录保留在忽略目录。

## 未完成

真实生产 GLB 的几何重合、完整坡道/台阶/墙角/窄道与跨块轨迹、调试可视化和手机
多指组合仍需继续验证。此处主机测试和合成夹具不能替代它们。
性能仍需按原 2M/4M/8M 条件分别重复 20 次，本次不作性能达标声明。
完整剩余项见 [固定版本对标矩阵](viewer-parity-v1312.md)。
