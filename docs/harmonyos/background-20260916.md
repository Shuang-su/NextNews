# 配置背景色接入候选

接续完整 Viewer 画质任务，使用官方 v1.31.2 viewer.ts 中 background.color → camera.clearColor 规则。此前设置虽然解析，OpenGL 和 Huawei 实际固定为 .035/.045/.065，本次将 RGB 接入共用后端接口。

- OpenGL Native 接口校验三个有限数值，夹到显示 RGB 范围，互斥更新并唤醒渲染；渲染线程快照后用于 glClearColor，相同值不引发重复重绘。状态随场景外的 Renderer 保存，可在 EGL 重建后继续使用。
- Huawei 保存独立颜色副本，创建相机时和后续更新时设置 clearColor；不依赖旧相机引用。
- 共用控制更新根据当前配置同步，清除旧模型配置时恢复黑色；不修改原始 viewer-settings.json。

主机测试 test_viewer_background.cjs 覆盖参数校验/范围归一、Native 转发、Huawei 加载前配置、外部数组变更、复位和相机重建。Viewer/input 原有主机回归通过。配套 SDK 构建和签名通过。

安装尝试返回 No active devices found，未安装或视觉验收；没有用模拟对象测试冒充 Huawei/OpenGL 实际画面。最后已安装仍为 flight-preview，本包包含尚未真机验收的 e27a7e8 位移输入。

候选 artifacts/harmonyos/NextNews-background-preview.hap SHA256 a8d703046cd1a7a266b82bf125bd16314c2860396d3f56a58c5ff72c1bd5136a。

待真机：同一已知纯色背景、透明高斯混合、两后端切换、前后台及窗口重建、模型切换无残留。天空盒、色调映射/线性色彩管线、后处理、音频、高阶 SH 仍未完成；原性能门槛未通过。
