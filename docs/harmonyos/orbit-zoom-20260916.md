# 环绕缩放限制修复

官方 v1.31.2 的 src/cameras/orbit-controller.ts 设置 zoomRange=(0.01, Infinity)，单位为世界坐标。本地旧版 ArkTS 和 C++ 各自把归一化 zoom 限制为 [0.05,20]，导致最近距离为场景半径的 15%，大场景无法近看。

本次共享控制层按场景半径换算 0.01 世界单位下限，去除 20 上限，拒绝非有限/非正缩放输入；原生 MakeView 同步，取消 focus 的旧半径比例距离限制。原生裁剪面依官方 viewer.ts，按相机前方场景边界调整，far 使用一个浮点 ULP 余量防止边界中心被错误剔除。相机归一化数据、镜头配置和原始模型不改写。

验证：0.001、1、1000 半径场景的下限、向外超过旧上限及非法输入主机测试通过。原生相机/拾取/排序 ASan/UBSan、输入和导航环测试通过；配套 SDK 构建、签名、安装通过。

Mate80ProMax SGT-AL10 / API26 / 7.0.0.105(SP10C00E105R3P3) 真机完成连续 20 次放大，小模型进入内部后画面变暗，不能把这一截图当作近距离画质通过。随后重新加载正式 CDN 前海冰雪世界做近距离观察。诊断自动化在寻找复位按钮时失败，不将其记录为通过；完整双指、鼠标及 Web 同镜头视觉差分仍待完成。

已安装包 artifacts/harmonyos/NextNews-zoom-preview.hap，SHA256 7e2dd9369ce002e4f331d24867fc2bc1dafe41ca3deed6b3087350c1f2d22373。

本批修复共享 Viewer 和 C++/OpenGL；历史独立 SpatialViewer 页面仍有旧 pinch 限制和静态裁剪面，华为独立页需要后续同步及实测。完整功能及性能门槛仍按 viewer-parity-v1312.md 验收。
