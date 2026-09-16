# 共用体素与步行实验 · 2026-09-15

**第二批的初步实现，尚未完成完整碰撞与导航验收。**
第一批共用 Viewer 预览见 [对应报告](viewer-common-20260915.md)。

## 本次代码

C++ 模块与 OpenGL/EGL 独立。ArkTS/Huawei 和 OpenGL 共用同一个体素缓存、物理步进、
出生点以及世界坐标镜头。已接入原生八叉树读取、射线/球/胶囊查询和 60 Hz 步行。
支持 v1.0 的 X/Y 翻转和 v1.1 世界坐标语义；元数据旋转字符串不再自动重复施加。

- 固定物理步长 1/60 秒，最多 10 步，胶囊高 1.5 m / 半径 .2 m、眼高 1.3 m、
  重力 9.8、跳跃 4 m/s、悬浮高度 .2 m、弹簧 800、阻尼 57，地面探测使用五条射线。
- 迭代碰撞采用官方四次约束投影；增加短距离子步，避免一次大步跨过薄墙。
- 出生点按官方五米范围内的圆柱净空搜索。异常高成本搜索有 200 万候选上限，
  达到上限显式失败；不会伪造出生点或进入没有数据的区域。
- 分块缓存按实际二进制字节计量：原生 128 MiB，文件 256 MiB；所选区域和查询快照
  引用受保护，LRU 只淘汰不使用的内容。当前位置及相邻区域优先，移动方向追加预取。
- 体素加载两个任务；与 SOG 共用全局最多四个网络请求。文件就绪后在原生后台解析，
  版本号阻止上一场景的结果覆盖新场景。
- 缺少碰撞区域时暂停位移。旋转/飞行切换仍可用，后台清除按键/跳跃与速度。
- 配置接受 collisionUrl、voxelUrl、voxelManifestUrl。当前接入 HTTP(S) 单体
  `.voxel.json/.voxel.bin` 与分块清单。**GLB 碰撞网格明确未接入**。
- 步行按钮/快捷键 3；游戏模式 WASD/摇杆移动、空格/按钮跳跃；Shift/Ctrl 改变速度。
  普通模式点击前往，双击加速；未完成官方转向曲线与完整手柄映射。

## 数据与测试

使用本机原始真实 32 块清单：

`/Users/szmg/Documents/splat-transform/outputs/huafa-ice-world-99-lcc2-stream-voxel-20260901/voxel/v24-tiled/voxel-tiles.json`

没有改写源数据，也没有使用 39 块规划清单。32 块的元数据和二进制均通过主机解析；
这只证明格式校验成功，不证明全部区域没有几何问题。

```bash
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer python3 scripts/harmonyos/test_collision.py
node scripts/harmonyos/test_collision_resources.cjs
bash scripts/harmonyos/build.sh
```

通过：250 组与官方 v1.31.2 的随机射线/球/胶囊差分查询；地面稳定、墙体阻挡、
长按跳跃不重复触发、复位、未知边界停止；网格并集/孔洞、引用保护和字节 LRU；
损坏指针、深度、版本、非整数计数、截断文件与非有限值。ASan+UBSan 无报告。
SOG 524,598 高斯额外验证了模型取景边界只施加一次 Viewer 变换。

真机测试命令（调试 App，连接同一台手机）：

```bash
# 终端一；仅提供本机已有真实数据
python3 -m http.server 8780 --bind 127.0.0.1 --directory /Users/szmg/Documents/splat-transform/outputs/huafa-ice-world-99-lcc2-stream-voxel-20260901/voxel/v24-tiled
# 已有 8768 场景服务仍需运行；脚本建立两个 HDC 转发
bash -c 'source scripts/harmonyos/env.sh; python3 scripts/harmonyos/verify_walk.py --out artifacts/harmonyos/walk-new-run'
```

设备仍为 Mate 80 Pro Max / SGT-AL10、API 26 / 7.0.0.105。
原始证据在 `artifacts/harmonyos/walk-20260915`。首次由于场景端口转发缺失，只加载了
碰撞数据，**该次无效**；脚本也发现了非法 swipe 速度，已修正。随后 `scene-recheck`
实际显示完整 2,000,000 高斯，并显示步行出生点和摇杆移动。此轮末尾日志缓冲只留下
五个静止样本，无法验证瞬时跳跃；后台返回截图曾黑屏，稍后截图正常。
最终 `final-trace` 连续记录 82 个位置样本：从 (331.509, -81.620, 305.309)
移动至 (329.211, -81.620, 302.635)，跳跃采样最高比站立眼位高 0.709 m，最后落地。
返回前台 5 秒截图已人工检查，画面与镜头恢复；这不代表恢复耗时达标，短暂黑屏的
准确时长尚未量化。筛选证据在 [collision-20260915](evidence/collision-20260915/)。

另修复静止步行持续通知渲染器的问题：仅镜头发生变化时提交，避免空闲时强制重画。

华为单体样例（80,000 高斯）也复测了共用步行：86 个日志样本，移动约 3.5 m，
跳跃采样升高 .586 m，最终落地，移动/后台返回截图均已人工检查。采样频率只有 2 Hz，
不同回合的采样最高点不可用来比较真实跳跃最高高度。这个稀疏单体只用于控制模块验证，
不能和 OpenGL 整场 2M 的画质或帧率作等价对照。

华为初次移动后曾只剩灰色背景。短时调用 renderFrame 的尝试无效，已撤回。
GSNode 原先只保存在 load() 的局部变量；改为后端长期持有并随场景释放后，此轮复测
移动、跳跃和后台恢复显示正常。保留前后原始证据，不把初次脚本正常结束视为通过。

调试包 `artifacts/harmonyos/NextNews-walk-voxel-preview.hap`；SHA-256：
`0ab207dcead3493658e8065587a1fa7cf526a956041b8b4e7f0f0c71153015db`。配套源码、构建与签名安装均完成，包保存在本地，不提交签名或包缓存。

## 未通过项

- 整场流式 + 步行目前只在 OpenGL 测试；Huawei 单体 + 共用碰撞已完成上述预览验证，
  Huawei 整场瓦片仍未通过。
- GLB 网格、体素可视化、全部斜坡/墙角/狭窄通道/跨块轨迹、下载取消和压力测试未完成。
- 官方步行可用性尺寸条件、完整转向/手柄映射及部分 UI 行为仍有差距。
- 未完成 2M/4M/8M 每条件 20 次性能回归。当前实验不能替代原性能门槛。
- 天空盒、声音、高阶 SH、后处理及标注遮挡仍需继续实现。
