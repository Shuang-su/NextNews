# SOG 与分层流式加载

当前支持 SOG v2 的 ZIP 容器、WebP 属性纹理、量化位置、四元数、尺度码本和 SH0 颜色码本；在 Native 工作线程解码，不经过 ArkWeb。高阶 SH 不参与当前渲染。ZIP 验证长度、目录、CRC、解压预算和文件名；模型验证有限值、码本、纹理尺寸与数量。单文件最大 128 MiB，最多 400 万高斯。网络分块最大 32 MiB。

原来的“最近 8 块”已经移除。平面 v1 清单加载全部块；v2 清单保存 SuperSplat LOD 树每个叶节点的各层 `file/offset/count`。先载入全部最粗层，再按相机距离、朝向、区域尺寸和高斯预算选择更细层。缺失下载保留已缓存的较粗层；原始树某层明确没有高斯的叶节点保持空范围，不额外重复绘制其他层。环境 SOG 作为常驻叶节点。

默认 200 万常驻高斯，对应核对版本 Viewer 的移动端高画质预算；可以切换 100 万或 400 万（后者对应其桌面高画质预算）。选择器并非 PlayCanvas 引擎 LOD 算法逐行移植，屏幕误差和切换时机仍可能不同。没有过渡淡化、Range 断点续传、精确视锥裁剪和预测预取。

“全量预载”在所有压缩包下载完成后提交首个流式场景；**全量下载与同时绘制全部层级是两件事**。本例所有层级约 2.97 亿高斯，下载约 3.1 GiB；渲染仍选择预算内且不重复的层级。磁盘预载上限 4 GiB，空间不足会报错。渐进模式在约 512 MiB 后淘汰不需要的文件，当前选择和基础覆盖文件保持固定，因此这不是严格 512 MiB 的总进程内存上限。

Native 缓存已解码文件和每个文件实际选中的区间；未变的区间无需重新解码。选择变化后仍合并为全局场景并更新 GPU 数据纹理，尚非 GPU 逐块增量更新。场景、排序索引、GPU 数据和解码缓存分别占用内存；UI 的缓冲估算不是进程 RSS。

## 数据准备与本机启动

本机完整源：`/Users/szmg/Documents/splat-transform/outputs/huafa-ice-world-99-lcc2-stream-voxel-20260901/streamed-v260-earliest/lod-meta.json`。最高细节约 1.49 亿高斯，9 个 LOD 层级，532 个前景 SOG 包加 1 个环境包。旧 `.local/stream-huafa` 只是 LOD8 诊断样例，不是画质基准。

```bash
python3 scripts/harmonyos/prepare_sog_stream.py /path/to/lod-meta.json .local/new-sog-stream
bash scripts/harmonyos/serve_stream.sh .local/new-sog-stream 8768
```

脚本保留原始坐标、层级范围和数量，只把现有属性 WebP 打包为 SOG、生成客户端清单并移除高阶 SH 元数据；不重新量化、不降采样、不修改源文件。输出目录必须不存在。准备结果的 provenance.json 记录来源与清单 SHA-256。App 的入口为同目录 `scene.json`，不是直接粘贴 SuperSplat 展示网页。

当前本机试验数据在 `.local/sog-huafa`，地址 `http://127.0.0.1:8768/scene.json`。服务器绑定回环地址，HDC 反向转发端口。先选择渐进/全量模式，再连接；“重试”恢复失败调度，“停止”取消下载并保留最后场景。后台暂停新请求，回前台继续；退出后清理本次磁盘缓存。

## 清单 v2

```json
{
  "version": 2,
  "bounds": [0, 0, 0, 10],
  "levels": 2,
  "chunks": [
    {"file":"chunk-0000.sog","count":1000,"bytes":12000,"bounds":[0,0,0,10]}
  ],
  "leaves": [
    {"bounds":[0,0,0,10],"lods":[[0,0,1000],[0,0,100]]}
  ]
}
```

示例数字只说明结构。每个 lod 是文件索引、起始高斯、数量，层级 0 最细；`[-1,0,0]` 表示原始树在该层没有数据。清单上限 4 MiB、1024 个包、10000 个叶节点和 16 个层级。坐标在读取后统一应用 Rz(180°)，索引顺序保持不变。

`node scripts/harmonyos/test_stream.cjs` 验证恶意清单和预算选择；可加本机 scene.json 路径检查真实树。SOG 的独立解码比较与损坏文件测试命令见 verification.md。
