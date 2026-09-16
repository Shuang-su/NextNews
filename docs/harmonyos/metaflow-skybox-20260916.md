# MetaFlow 全景天空盒接续

参考仍为 MetaFlow `a871786dffdb195399f3e7427987d8db70296491` 的
`metaflow-viewer/src/index.ts:loadSkybox`、`viewer.ts` 与 PlayCanvas 2.21.3。
MetaFlow 强制 RGBP，使用无 mip 的等距柱状全景，不是立方体图集。

## 实现

- 异步读取 HDR / PNG / JPEG，HDR 保留线性浮点，普通图片按 RGBP 还原；
  限制 64 MiB、2:1、8 Mi 像素以及有限 RGBA16F 数值。
- 仅一个解码任务同时运行；过期请求在解码前和发布时检查版本。
- 原生 RGBA16F 全景纹理，每帧最多上传 4 MiB，完成后才绘制。
  全景使用相机朝向和 FOV，不随相机平移，不改变模型坐标。
- 天空盒进入场景后处理，避免重复色调映射。销毁 GL 后释放纹理；
  页面销毁取消旧发布，清除配置释放 CPU 数据。显示 GPU 就绪和内存统计。
- HTTPS 地址缓存、可读失败提示与手动重试。华为路径明确显示未验证。

## 实测

Mate 80 Pro Max / API 26 / Maleoon 935 / 1320×2848，
调试 HAP `artifacts/harmonyos/NextNews-metaflow-skybox.hap`。
公开夹具为 PlayCanvas `examples/assets/hdri/empty-room.hdr`，4096×2048。
手机直接请求 raw.githubusercontent.com 失败；通过 HDC 的调试应用文件传输
把同一文件预置到对应 URL 缓存，然后使用系统文件选择器导入测试设置。
因此这是**文件缓存路径**证据，不能记为网络加载通过。

`artifacts/harmonyos/metaflow-skybox-20260916/room.png` 与 `room-turned.png`
已人工检查：全景实际显示，镜头转向后能看到房间另一侧，模型仍叠加显示。
`room-settings.json` 报告就绪、192.0 MiB（128 MiB CPU + 64 MiB GPU）。
初始 `initial.png`、`cached.png` 是下载尚未就绪的黑色背景，不能作为成功证据。

主机 ASan/UBSan 测试覆盖 PNG/HDR、损坏/截断、比例/尺寸及 HDR 溢出；
异步测试覆盖过期下载、隐藏页面、重复地址、错误提示和后端切换。
项目构建、签名构建和安装通过。尚未完成 Web 同镜头逐像素比较，
JPEG/RGBP 真机夹具、完整网络冷热缓存与 GL 窗口重建压力测试。

## 尚存差距

- 无 cubemap/atlas/WebP 全景支持；当前格式限制会明确报错。
- 首次加载仍允许主体先显示、天空盒随后就绪，与参考的 ready 合流时机有差异。
- 纹理缓存有单文件上限，尚未纳入全应用磁盘缓存总字节 LRU。
- 华为天空盒等价路径没有通过验收。
- 不代表完整 Viewer、性能或其他渲染后端已完成。
