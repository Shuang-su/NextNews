# 标注遮挡与热点浮窗候选版本

本次接续第一批 Viewer 功能。固定参考为 SuperSplat Viewer v1.31.2，
`96f62515b99a28a20579041a656f7b1911c2964c` 的
[annotation.ts](https://github.com/playcanvas/supersplat-viewer/blob/96f62515b99a28a20579041a656f7b1911c2964c/src/annotation.ts)。
**构建和主机测试通过；新增功能尚未完成真机视觉验收。深度标注默认关闭，保留实验开关。**

## 本次实现

- 两个后端共用热点旁正文浮窗：右侧优先，空间不足翻到左侧，按 8 vp 边距收进视口；箭头跟随热点。内容按纯文本显示，长正文可滚动。
- 镜头后方标注不显示；向前但在屏幕外的选中标注保留边界浮窗，导航按钮仍可访问。触碰场景关闭选中浮窗，操作标注不会传给场景导航。
- 热点外观保持 25 vp，手机点击区域扩至 44 vp。浮窗增加关闭按钮，属于移动端适配差异。
- OpenGL 实验路径在高斯之前绘制可写深度的标注，再绘制透明高斯，最后叠加 25% 标注轮廓。目的是得到逐像素透明遮挡，而不是用 CPU 拾取把整个标注变灰。没有 GPU 深度读回或逐标注全场扫描。
- 编号使用一次生成的 320×32 单通道数字图集（10 KiB），位置最多 1000 个；切换配置更新一次位置数据，移动相机只改变矩阵。无效数据在 Native 边界拒绝。
- EGL 尝试 24 / 16 bit 深度，无深度配置或图集/程序失败时保留二维热点。诊断菜单显示实际就绪状态。
- GPU 标注对象随 EGL 上下文释放并重建；CPU 配置保留。关闭实验开关恢复二维热点，隐藏控件时停止两次标注绘制。
- 修正共享菜单切换后可能沿用之前滚动位置的问题，避免新菜单顶部入口不可见。

Huawei 暂时继续使用共用二维热点与浮窗。SDK 的材质排序提示尚不能证明 GSPlugin 能提供
“标注底层—透明高斯—标注轮廓”相同顺序，因此不声称已经实现等价的深度遮挡。

## 构建与测试

```bash
bash scripts/harmonyos/build.sh
node scripts/harmonyos/test_viewer_annotations.cjs
node scripts/harmonyos/test_viewer_runtime.cjs
node scripts/harmonyos/test_viewer_projection.cjs
node scripts/harmonyos/test_pointer_input.cjs
DEVELOPER_DIR=/Applications/Xcode-27.0.0-beta.5.app/Contents/Developer python3 scripts/harmonyos/test_core.py
```

均通过。覆盖边缘翻转/夹紧、横竖视口、相机后方、近面、屏幕外选中热点、纯文本正文，
以及原有配置迁移、镜头过渡、时间轴和独立触点归属。原生解析/math 在 ASan/UBSan 下通过，
包括新夹具 PLY。**这些不是实际 GPU 绘制或真实多指验收。**

候选调试包位于本地 `artifacts/harmonyos/NextNews-annotation-preview.hap`，SHA256：
`8e8f26de6a8e7e5a806b82a2635bcfc522048586eedb6efd3029f82b76a81d7d`。
此最终候选包尚未装回手机。

本轮曾成功安装早期包 `6ecc47dbf6545205f1ae8ead0e6cee514f00e4ff9273d108c25d3ed6b0f51929`，
设备仍是 Mate 80 Pro Max / SGT-AL10、API 26、`7.0.0.105(SP10C00E105R3P3)`。
自动 UI 测试在寻找“标注遮挡夹具”入口时中止；随后检查设备列表为空，无法继续采集。
未得到本次遮挡截图，不能把安装成功计为视觉通过。早期包还没有最终的默认关闭实验开关及菜单复位修正。

## 连接恢复后的验收入口

安装最终候选后，“设置 → 性能信息 → 标注遮挡夹具”加载独立的原创测试资源，并启用实验深度标注。
该夹具只有 3 个红色 SH0 高斯和 5 个世界坐标标注，分别覆盖前景、后景、半透明遮挡、镜头后方和屏幕外。
没有使用或修改用户模型。

```bash
# 生成独立副本，输出目录必须不存在
python3 scripts/harmonyos/make_annotation_fixture.py --out artifacts/harmonyos/annotation-fixture-new
# 已连接手机时，先 source scripts/harmonyos/env.sh（bash）
python3 scripts/harmonyos/verify_annotations.py --out artifacts/harmonyos/annotation-device-new
python3 scripts/harmonyos/verify_viewer_common.py --out artifacts/harmonyos/viewer-common-new
```

第二个脚本检查后景热点可点、正文/关闭、前后导航、镜头后方隐藏、屏幕外浮窗收边、深度能力、
Huawei 二维回退、前后台、切回 OpenGL 重建、控件隐藏/恢复，并保存截图待人工核对。
尚需对照官方同源画面核验字形、热点重叠、部分遮挡、灰度/抗锯齿和横屏布局。

## 限制与后续

完整对标仍按 [矩阵](viewer-parity-v1312.md)推进。原有 HUD 的 GPU 时间只计高斯绘制，
现标为“高斯 GPU”；帧提交/交换时间包含标注，但不能替代系统级持续帧时间。
新增深度附件和两次绘制的成本尚未做 20 次性能回归。未降低模型 LOD、预算或分辨率。
Huawei 瓦片、真实多指、完整地形/跨块、声音/天空盒/后处理/高阶 SH 及原性能门槛均没有因此变为通过。
