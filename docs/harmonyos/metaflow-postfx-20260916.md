# MetaFlow 后处理与显式体素坐标（2026-09-16）

本批接续 `5a09199`。独立 OpenGL 离屏通道已实现并完成部分真机回归；不代表全功能或像素级对齐完成。

## 实现

- 原生 GLES 后处理：CAS 锐化、色散、13 tap 下采样/9 tap 叠加上采样泛光、调色、七种色调映射、暗角。
- 以 MetaFlow 固定版本声明的 PlayCanvas **2.21.3** 为公式来源。保留 gamma 空间高斯输出和最终 compose 的 GAMMA_NONE 规则。未启用后处理时，非线性色调映射在高斯输出前执行，避免误变成整图映射。
- RGBA8 和显式 RGBA16F，保持当前渲染分辨率。泛光沿用高精度目标约束；从菜单开启泛光时会同时开启高精度。关闭效果释放纹理/帧缓冲；窗口销毁释放全部 GL 对象。
- 设置菜单可切换各效果、色调映射和高精度。配置效果参数保留；全部参数的交互滑杆尚未补齐。
- 效果创建失败时保留基础渲染并显示原因，不静默降低分辨率/预算。能力表改为具体效果清单，华为为空并明确未通过。
- 统计增加后处理是否实际开启和额外缓冲字节。GPU 查询涵盖高斯与后处理；零值计时显示不可用。移除容易误读的“耗时折算 fps”，改为 CPU 提交耗时（非屏幕帧率）。
- 体素显式 world/Rz180 参数进入原生读取，优先于旧版自动规则；未指定时保留既有兼容行为。分块包围盒按同一显式坐标转换，新副本不改写原始清单。

## 验证与证据

手机：HUAWEI Mate 80 Pro Max / SGT-AL10 / API 26，1320×2848。

前海冰雪世界：五效果同时开启、转向、总开关关闭/重新开启、Home 后恢复均有实际画面。原生诊断记录额外缓冲 **47.9 MiB → 0.0 MiB**，关闭时释放。这是当前窗口/配置的内存观察，不是性能门槛验收。

本地证据目录：`artifacts/harmonyos/metaflow-postfx-20260916/`，包括 `all-effects.png`、`effects-after-turn.png`、`disabled.png`、`re-enabled.png`、`foreground-restored.png`、`diagnostics-stats.json`、`diagnostics-disabled.json`。

配套 SDK 构建与签名通过。主机回归通过效果参数映射/基线旁路/能力区分、49 个实际 CDN 配置、声音与加载配置；体素新增显式坐标覆盖测试及既有 250 组参考差分查询通过 ASan/UBSan。显式体素坐标的实场步行尚未真机验收。

## 重现

```bash
bash scripts/harmonyos/build.sh
node scripts/harmonyos/test_viewer_effects.cjs
node scripts/harmonyos/test_collision_resources.cjs
node scripts/harmonyos/test_collision_lifecycle.cjs
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer python3 scripts/harmonyos/test_collision.py
```

`post_shaders.h` 可由固定版本 npm 包重建：

```bash
python3 scripts/harmonyos/generate_post_shaders.py .local/metaflow-engine-2.21.3/package
```

生成器拒绝不同引擎版本。缓存包来源 `https://registry.npmjs.org/playcanvas/-/playcanvas-2.21.3.tgz`，不升级全局或参考工程依赖。

## 待验收/剩余差异

- 同源、同镜头、同参数的 MetaFlow Web 像素对照尚未进行；本批照片只证明原生效果在运行。
- GPU 标注目前进入离屏画面，也会受后处理影响；与 Web DOM 标注效果隔离仍有差异。
- 天空盒、渐变背景、高阶 SH、声音真机、主体/环境双资源完整矩阵仍未完成。
- 华为 tiled、后处理、标注深度仍未通过；不能以 OpenGL 结果代替华为结果。
- 性能 20 次缓存条件回归未重测。旧门槛失败结果继续有效；400/800 万保持实验状态。
