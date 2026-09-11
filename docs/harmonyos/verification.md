# 鸿蒙开发环境与 3DGS 验证报告

验证日期：2026-09-11。C++ 原生查看器已在本机手机模拟器运行；华为 SpatialReconKit 对照路线尚未通过运行验收。完整计划仍有真机及专项验证项，不能视为全部验收完成。

## 环境与构建

- Apple Silicon / macOS 27；DevEco Studio 26.0.0.821、配套 SDK 26.0.0.105 / API 26 Release。准确路径及依赖版本见 [toolchain.md](toolchain.md)。安装盘校验及应用代码签名验证通过；未将本地 SHA 与官网 SHA 作一致性断言。
- 手机镜像安装于 `/Users/szmg/Library/Huawei/Sdk/system-image/HarmonyOS-7.0.0/phone_all_arm`，镜像版本 7.0.0.106；模拟器 `NextNews_API26`，4 GB RAM、6 GB 用户存储，HDC `127.0.0.1:5555`。
- 网络最初存在拒绝连接、TLS 握手和代理隧道 503；IDE 使用现有系统代理 `127.0.0.1:1082` 后，由用户再次下载成功。手机镜像已实际启动。额外平板镜像安装向导也显示安装完成，但平板未创建或验收。
- 空白 Stage 工程完成构建、签名、安装、启动并显示 Hello World。
- 查看器命令行构建、签名、HDC 安装与启动通过。IDE 对源工程执行“构建所有模块”也通过：`entry:assembleHap`，818 ms，退出码 0。IDE 源工程构建为未签名 HAP；签名调试副本由独立脚本生成，凭据留在本机。
- 当前调试包：`artifacts/harmonyos/NextNews-debug.hap`。应用源代码提交 `6777f54`。复现命令见 [README.md](README.md)。
- IDE 存在内部错误提示及快捷键冲突提示，未阻止本次构建；不据此断言 macOS 27 所有 IDE 功能均兼容。

## C++ / XComponent / GLES 3.0

已实际创建 EGL 上下文并渲染实例化椭圆高斯。图形字符串为 `OpenGL ES 3.0 (4.1 Metal - 91.7) / Mali-G77`，这是模拟器暴露的接口，不代表真实手机 GPU。

三类内置样例均可加载：公开 biker 80,000、本机 Tripo 输出 32,768、splat-transform 解码产物 80,000。另通过系统文件选择导入 260,000 高斯模型并旋转。模型连续切换 3 轮共 9 次均返回正确数量和就绪状态。旋转、平移、按钮缩放和复位进行了交互验证；双指缩放已实现，尚无独立双指注入验收记录。

以相同准备后的 SH0 PLY 在本机 PlayCanvas 2.18.1 参考查看器中定性比较形状、颜色和朝向；未进行像素级差分或完整遮挡测试矩阵。公开 biker 原始坐标在两边均呈倒置，Tripo smoke 样例在两边均很暗，未私自调整源坐标或颜色。比较中发现并修复了 XComponent 原生窗口缓冲尺寸不同步造成的比例失真，最终截图使用修复后的版本。

导入测试通过：

| 数据 | 结果 |
|---|---|
| 非 PLY 损坏内容 | `Not a PLY file` |
| 仅 xyz 普通点云 | `Missing float Gaussian field` |
| x 含 NaN | `Non-finite Gaussian value` |
| 截断二进制载荷 | `Truncated or unexpected PLY payload` |
| 300,001 个顶点 | `Invalid vertex count` |
| 129 MiB 文件 | 文件超过 128 MiB |
| 合法 260,000 高斯 | 正常显示 |

以上通过系统浏览器下载、文件选择器导入完成，错误时原模型仍保留。期间多次浏览器/文件选择器与 App 前后台切换后可继续使用。进入华为对照页面再返回也保留 C++ 页面。旋转模拟器左右方向操作成功，但 App 仍为竖屏，因此**不将该操作计为窗口尺寸变化或窗口重建通过**。同进程强制表面重建、长时间前后台循环、内存泄漏曲线仍待专项验证。

主机 C++ 解析、协方差、排序及异常数据测试通过 ASan/UBSan，三类样例与 260k 压力数据解析通过。使用每条命令指定的 Xcode 27 工具链绕过本机 Xcode 16 ASan 初始化挂起；未修改系统 Xcode 选择。Linter 检查文件数为零，未计为测试通过。

## 模拟器诊断快照

以下为单次快照，存在调度波动；加载不含系统文件下载和复制时间。

| 模型 | 高斯数 | 加载 ms | 排序 ms | 提交/交换 ms | 缓冲估算 MiB |
|---|---:|---:|---:|---:|---:|
| 公开样例 | 80,000 | 40 | 47.2 | 6.3 | 11.9 |
| Tripo | 32,768 | 17 | 24.1 | 2.0 | 4.9 |
| 转换样例 | 80,000 | 39 | 44.0 | 4.3 | 11.9 |
| 260k 导入 | 260,000 | 127 | 137.9 | 7.2 | 38.7 |

260k 时 `hidumper --mem` 的进程 Total PSS 为 106,548 kB，约 104.1 MiB，为单次进程内存快照。UI 的 fps 是 `1000 / (排序耗时 + 帧耗时)` 的折算，**没有测量持续交互帧率**；空闲时不连续绘制。这些值不可外推为真机性能。CPU 排序在 260k 时已明显影响交互，需要后续优化。

## 华为原生对照路线

`SpatialViewer` 编译通过，能力检查通过；运行在 `Scene.load()` 阶段失败，界面显示 `Creating scene manager failed`。系统日志包含 `InitRenderManager ctx is invalid` 和 `Failed to build object SceneManager`。将它设为独立首屏、先于 C++ 上下文创建也重现，未证明由两个渲染器争用引起。

因此目前没有 SpatialReconKit 模型成功显示的证据。需在受支持真机复测或进一步定位 SDK/模拟器 SceneManager 初始化问题。保留错误提示、场景释放和返回 C++ 页面能力。AR 放置 API 的调研记录见 [rendering-routes.md](rendering-routes.md)，未宣称实现或测试 AR 会话。

## 本机交付证据

源码位于 `apps/harmonyos`；私有仓库为 [Shuang-su/NextNews](https://github.com/Shuang-su/NextNews)。签名、SDK、安装盘、日志、测试临时文件和 HAP 均未上传 Git。

`artifacts/harmonyos/` 保存：

- `NextNews-debug.hap`、`NextNews-debug.hap.sha256`、`debug-build.log`。
- `emulator-boot.png`、`smoke-running.png`。
- 最终画面 `accepted-public.png`、`accepted-tripo.png`、`accepted-converted.png`；同名 JSON 包含设备 UI 与诊断数值。
- `qa-stress.png`、`stress-orbit.png`、`stress-memory.txt`。
- `device-import-checks.json`、`model-switches.json`、`qa-*.png`。
- `spatial-final.png`、`spatial-isolated-errors.log`、`route-return.png`。

源文件 provenance、许可及转换命令见各 rawfile 模型旁 manifest 和 `THIRD_PARTY_NOTICES.md`。模型预处理输出独立副本；普通点云不会被当作 3DGS 接受。

## SpatialReconKit 后续定位

最小基础 Scene.load 在不加载 GS 插件、模型及 C++ 上下文时仍失败。官网明确暂不支持模拟器；详情与可复现诊断见 [spatial-investigation.md](spatial-investigation.md)。当前调试包已补充阶段日志和官方支持范围提示，C++ 渲染代码未改动。
