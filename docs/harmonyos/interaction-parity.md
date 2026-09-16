# SuperSplat Viewer 交互对齐

参考对象为 **Viewer 的触屏 Orbit 模式**，不是 SuperSplat Editor 的编辑工具。2026-09-11 核对官方开源仓库，固定提交 [96f62515](https://github.com/playcanvas/supersplat-viewer/tree/96f62515b99a28a20579041a656f7b1911c2964c)。阅读 `src/input/devices/touch.ts`、`src/cameras/orbit-controller.ts`、`src/cameras/camera-utils.ts` 和帮助面板。控制器为本项目独立实现，没有将整个 PlayCanvas 引擎移植到鸿蒙。

| 行为 | 本轮 OpenGL 实现/差异 |
|---|---|
| 单指环绕 | 保留拖动方向；灵敏度按视口短边归一化 |
| 双指平移与张合缩放 | 同时计算触点中心平移和指间距离缩放；不再由互斥手势二选一 |
| 手指加入/移除/取消 | 根据触点 ID 重新建立基准，不把触点数量变化算作位移 |
| 阻尼 | 采用时间相关指数插值，0.95 每毫秒衰减；输入停止后收敛并停止提交相机更新 |
| 平移后的环绕中心 | 保存半径归一化的世界坐标目标，旋转时不再让平移目标随视角转动 |
| 平移距离 | 使用 45° 垂直 FOV、相机距离和视口高度换算；本项目 FOV 未改为所有 Viewer 场景默认值 |
| 复位 | 返回本项目的包围盒初始视角，不读取 Viewer settings.json 的创作相机 |
| 画面与控制 | 支持收起工具扩大 XComponent；说明和性能信息按需展开；不是系统级全屏；面板改变视口时按高度比例调整距离，避免模型突然放大裁切 |
| 辅助操作 | 保留单指平移模式按钮及 +/−，方便模拟器及单指操作 |
| 单击聚焦 | 后台计算点击射线处投影高斯的透明度累积，以 0.5 不透明度阈值选择焦点；空白点击不移动。环绕模式换枢轴，飞行模式转向焦点，保留相机位置 |
| 双击切换 | 切换环绕/飞行；使用互斥的单击/双击识别，双击不同时执行两次聚焦。这一映射以用户要求为准 |
| 默认方向 | 与 Viewer `src/index.ts` 一致，导入实体绕 Z 轴 180°：x/y 取反，协方差 xz/yz 取反。相机 yaw 初值恢复为 0；先前 c17b23d 的 yaw π 实现已纠正 |
| 桌面输入 | 已接入左键拖动、右/中键平移、轴事件缩放，R 复位；鼠标拖动设备注入已通过，滚轮仍需真实外设复测；点选焦点已接入，F 快捷键尚未映射 |
| Fly / Walk / 碰撞 / 路径动画 | Fly 已实现：原地转头、WASD/QE 及前后按钮，切换时保留位置；Walk、碰撞和路径动画未实现 |

保留现有 0.05–20 倍缩放和俯仰 ±1.5 弧度限制，不宣称与 Viewer 所有范围、初始姿态和速度完全一致。

## 验证

`node scripts/harmonyos/test_orbit.cjs` 覆盖双指同时平移缩放、触点顺序变化、增减手指不跳变、取消、时间分步阻尼一致性、收敛后停止、缩放边界、复位、视口尺寸换算和世界坐标平移。

C++ ASan/UBSan 测试增加“平移目标经过旋转后仍位于相机视线中心”的解析检查。HAP 编译和模拟器安装通过。模拟器通过单指旋转、辅助平移、按钮缩放、复位、展开/收起工具与说明验证。后续已用系统 uinput 注入真实鼠标拖动、W 长按和双指同时平移缩放，记录在 stream/input-results.json；不得据此宣称真机手感完全相同。

证据：本机 `artifacts/harmonyos/orbit/`、`orbit-build.log`、`orbit-run.log`、`orbit-core-tests.log`。尺寸变化后额外补绘一帧，避免 EGL 旧尺寸缓冲滞留。排序已改为稳定 radix 索引排序；静态高斯放入 RGBA32F 数据纹理，每次旋转只上传 uint32 索引。平移、缩放、窗口尺寸变化不触发重复排序；模型变化会更新数据纹理。大场景仍需以连续帧数测量，不能用阻尼掩盖性能差距。

鼠标与触摸统一由 XComponent 上方透明输入层处理，过滤鼠标合成触摸，避免重复应用增量。该层使用可获焦点控件以接收键盘；失焦或退后台时释放按键。流式支持见 [streaming.md](streaming.md)。

## 悬浮工具栏与 GPU 裁剪（2026-09-11）

对照固定版本 Viewer `src/ui.ts`、输入控制器和 PlayCanvas
`gsplatCorner` / `clipCorner`，新增原生 SVG 图标栏、悬停文字、无障碍名称，
环绕/飞行独立高亮、复位、自动环绕、模型库、设置与帮助。
浮层显示不改变渲染视口；F 切换沉浸显示，H 帮助，空格自动环绕。
飞行提供二维触屏摇杆、升降按钮及速度滑块，鼠标与 WASDQE 保留。
图标为项目自绘 SVG，未复制商标。

GPU 顶点阶段按 alpha=1/255 的现有片元丢弃阈值缩紧椭圆支撑范围，
并对完全离屏的投影四边形执行保守剔除。深度/透明度剔除后才读取其余
协方差纹理。高斯预算、分辨率、协方差精度和 SH0 均保持不变。
诊断面板提供开关，用同一常驻选集和固定轨迹进行 A/B 检查。
CPU 仍执行后台 radix 排序；没有宣称改成 GPU 排序或完整迁移 WebGPU。

| Viewer 能力 | 原生状态 |
|---|---|
| 模式、聚焦、鼠标、触屏、缩放、复位 | 已实现，设备交互回归 |
| 图标、悬停提示、帮助、设置、沉浸视图 | 已实现；沉浸隐藏应用工具，保留系统安全区 |
| 飞行摇杆、速度 | 已实现；升降按钮为按次移动，无碰撞 |
| 自动旋转播放/暂停 | 已实现；不等同于 authored camera animation |
| LOD / full preload | 已实现原生适配机制，选集不保证与引擎完全相同 |
| 作者提供的时间线、注释导航、settings 导入 | 尚未实现 |
| 碰撞行走、XR、天空盒及后处理设置 | 尚未实现 |

以上最后两项需要对应资源和独立接口，不以无效图标冒充可用能力。

## Multitouch ownership regression (2026-09-14)

The flight stick previously read `event.touches[0]`. With a right-hand finger
already on the viewport, a left-hand stick press could therefore use the right
finger's coordinates. Any finger-up also reset the stick.

`PointerInput` now captures the changed down pointer inside the stick and keeps
that ID until its own up/cancel. Array order and unrelated releases cannot
transfer ownership. The viewport separately tracks its down pointers, excluding
the stick, so looking while flying does not accidentally become a two-finger
pan/pinch. Combined gestures suppress tap focus/mode switching. Hiding the stick,
switching modes, or backgrounding cancels movement.

Run `node scripts/harmonyos/test_pointer_input.cjs` for right-first/left-first,
reordered touches, independent release, cancellation, recapture prevention and
tap suppression cases. Device injection evidence is recorded in the work log.
