# 碰撞透视调试与动画时间轴预览

接续 `4b2dede`，继续固定 SuperSplat Viewer v1.31.2 / `96f62515`。
这批不改变模型、流式预算、碰撞查询或用户原始配置。

## 实现

- 模型菜单 → 碰撞 / 体素 → **显示碰撞边线**：青色为体素占用区域，紫色为 GLB 三角边，黄色为已选且就绪的碰撞资源边界。
- 这是透视线框，穿过模型可见；用途是检查位置关系，不是深度遮挡或官方材质效果。完整占用的八叉树节点显示外框；混合叶节点显示占用体素格。
- 使用两个渲染后端共用的世界相机投影，不重复施加 Z 轴 180° 变换。三维线段先裁近面、再裁屏幕边界。
- 每 500 ms 后台读取一次只读碰撞快照，优先镜头前方的 10 米立方区域。每次最多 20,000 个查询步骤、1,024 条边，达到限制会在界面说明。已缓存资源边界另行显示在同一上限内。线框投影至多 10 次/秒。
- 关闭显示时不提取边线，并移除 Canvas 合成层；场景替换、后台切换会清空图层并废弃过期结果。性能测试必须关闭此调试层。
- 时间轴仅在动画模式显示；环绕、飞行、步行按钮退出动画。步行相机不会再被时间轴位置与物理位置同时改写。
- 拖动开始临时暂停，正常松手恢复原播放状态；取消/离开控件/后台不自动恢复。时间轴拥有独立触点，其他手指不会改变其所有权；鼠标与左右键亦可调整时间。
- 动画中隐藏游戏摇杆；动画暂停仍保留时间轴。帮助/设置快捷键不再无条件暂停动画。

## 验证和范围

普通构建、签名构建通过。调试 HAP：
`artifacts/harmonyos/NextNews-viewer-debug-preview.hap`，SHA-256：
`ac92cd40b5bdba11394de80e1cf552fa344e6ec4ae08d9d487786c45083322b7`。

```bash
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer \
  python3 scripts/harmonyos/test_viewer_debug.py
node scripts/harmonyos/test_viewer_projection.cjs
node scripts/harmonyos/test_viewer_runtime.cjs
node scripts/harmonyos/test_pointer_input.cjs
bash scripts/harmonyos/build.sh
```

原生内存检查与测试覆盖单格/完整节点/早期叶/子节点上边界、旧版翻转、GLB 顶点、远处排除、边数及计算量上限。
主机交互测试覆盖第二指先抬、触点数组重排、拖出边界、取消，以及相机近面与屏幕裁剪。主机多指测试不能代替手机真实多指验收。

设备：Mate 80 Pro Max / SGT-AL10，API 26，系统 `7.0.0.105(SP10C00E105R3P3)`，1320×2848。
手机测试使用 `scripts/harmonyos/verify_walk.py --debug`，每个后端独立保存原始截图、布局与日志。

OpenGL 使用实际 32 块体素清单及 200 万整场；Huawei 使用 80k 单体和合成 `room.glb`。
后者只验证三角网格调试与步行功能，不证明真实建筑 GLB 对齐，也不用于同等画质/性能比较。
记录见 `docs/harmonyos/evidence/viewer-debug-20260916/`。

| 手机证据 | 本次观察 |
| --- | --- |
| [OpenGL 实际体素](evidence/viewer-debug-20260916/opengl-voxel.png) | 向下看时可见地面占用格；2M 整场预算保持不变 |
| [关闭调试](evidence/viewer-debug-20260916/opengl-debug-off.png) | 恢复无边线的场景与飞行控件；手机最后保留此状态 |
| [暂停后拖动](evidence/viewer-debug-20260916/opengl-timeline-paused.png) | 保持暂停，动画模式不出现游戏摇杆 |
| [Huawei GLB](evidence/viewer-debug-20260916/huawei-glb.png) | 紫色碰撞边线与单体模型可见；合成夹具不是生产几何 |
| [Huawei 时间轴](evidence/viewer-debug-20260916/huawei-timeline.png) | 播放中拖动后恢复播放 |

最终 OpenGL 测试采到 99 个步行位置，水平位置确实变化，跳起后回到地面高度。
这些是功能轨迹，未进行 20 次性能统计；开启调试层的数据不用于性能门槛。

首次 GLB 调试脚本误填了不存在的 `collision.glb`，在加载前主动中止；重新使用实际的 `room.glb`。
早期体素视图主要显示区域边界，因此把诊断提取中心移到镜头前方，并补充向下看地面的截图。
4,096 边逐帧绘制版本能显示实际体素，但明显拖慢输入；最终改为 1,024 边/10 Hz，关闭时移除合成层。
Huawei GLB 截图对应改限额前的 `91e8e676…` 包（该夹具仅 24 条边）；最终 `ac92cd40…` 在 OpenGL 实际体素上回归。
黄色线是资源包围盒，不代表所有框内区域可行走；完整 GLB 空间语义仍由原有碰撞模块判断。

## 仍未完成

完整计划继续按 [对标矩阵](viewer-parity-v1312.md)推进。标注深度遮挡与热点浮窗、全套输入/手柄、动画中断后恢复步行模式、
真实多指和完整地形/跨块压力验证仍需继续；背景音、天空盒、后处理、高阶 SH 及每条件 20 次性能验收也尚未完成。
Huawei 整场瓦片与原性能目标没有因本批功能验证而变为通过。
