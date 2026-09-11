# SpatialReconKit 继续排查 — 2026-09-11

## 结论

当前模拟器上仍未实现 SpatialReconKit 渲染。华为[套件简介](https://developer.huawei.com/consumer/cn/doc/HarmonyOS-Guides/spatial-recon-introduction)（页面标注更新 2026-07-28）明确说明暂不支持模拟器。系统能力检查通过、SDK 接口存在，均不能替代设备支持判断。

进一步执行不导入 SpatialReconKit、不读取模型、不创建 C++ EGL 上下文的最小 ArkGraphics3D `Scene.load()` 测试，仍报 `Creating scene manager failed`。因此失败在基础场景初始化阶段，尚未进入 PLY 格式、相机朝向或 GS 节点验证。此结果不等于断言所有模拟器上的 ArkGraphics3D 都不可用。

## 实测与日志

| 测试 | 结果 |
|---|---|
| C++ 页面进入 SpatialViewer | SceneManager 创建失败 |
| SpatialViewer 独立首屏，先于 C++ 上下文创建 | 同样失败 |
| 仅 `Scene.load()`，未导入 GS 插件与模型 | 同样失败；新进程 PID 466 |

最小测试源码：`apps/harmonyos/tests/manual/BaseSceneProbe.ets`。只在忽略的签名工程副本中替换 Index 页面运行，未改变主查看器入口。完整应用已重新构建并恢复。

原始 `viewer-runtime.log` 的连续错误为：

```text
lume_widget: backend gles
ohos_lume: EGL version too old. 1.4 or later requried.
ohos_lume: Failed to create a context
ohos_lume: Device not created successfully, invalid render interface.
lume_widget: Failed to create render context
ohos_lume: RenderContextJS::InitRenderManager() ctx is invalid
ohos_lume: Failed to build object (SceneManager).
```

这说明引擎报告 EGL 初始化问题，不能直接据此认定系统真实 EGL 版本低于 1.4：该错误也可能来自上下文初始化失败或驱动行为。没有修改系统驱动、伪造版本、关闭 TLS 或更换 SDK 来掩盖错误。

本次最小测试日志只保留了 JS 场景错误和部分 Vulkan 探测消息；不把旧进程的完整 EGL 错误链冒充为新测试日志。

证据在 `artifacts/harmonyos/spatial-investigation/`：`base-scene-run.log`、`base-scene-ui.json`、`base-scene.png`、`base-scene-hilog.txt`、`final-build.log`。原始隔离测试记录仍在上级 artifacts 目录。

## 复现最小测试

先生成签名工程副本，再在副本中放入测试页面；以下操作仅影响忽略的 `.local` 目录及模拟器已安装的调试应用：

```bash
bash scripts/harmonyos/debug-build.sh
cp apps/harmonyos/tests/manual/BaseSceneProbe.ets .local/signed-harmonyos/apps/harmonyos/entry/src/main/ets/pages/Index.ets
source scripts/harmonyos/env.sh
(cd .local/signed-harmonyos/apps/harmonyos && devecocli run --device NextNews_API26)
```

完成后恢复正常应用：

```bash
bash scripts/harmonyos/debug-build.sh
(cd .local/signed-harmonyos/apps/harmonyos && devecocli run --skip-build --device NextNews_API26)
```

## 下一步真机验证

使用已开启 USB 调试并获授权的 HarmonyOS 真机，确认该设备和所在地区满足套件支持要求。当前调试签名仅用于已登记设备；新真机需要重新生成包含其设备授权的调试签名。不能直接将模拟器 HAP 当作所有真机均可安装的包。

在真机依次验证最小基础 Scene、GS 插件、一个小 PLY、三类样例及 260k；日志新增 `contextAvailable`、`pluginLoaded`、`sceneCreated`、`modelLoaded` 阶段和耗时，错误含业务 code，便于区分失败阶段。真机支持也不保证第三方 PLY 子集一定兼容，只有进入模型加载阶段后才能判断。

当前未连接受支持真机，GS 成功画面仍无证据；C++ 查看器维持可用。
