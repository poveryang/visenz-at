# AT 采图上位机 MVP

这是 AT 效果优先工程闭环中的最小上位机工具。

目标设备默认是 ARM Linux，首期只面向 `imx8plus`。该工具运行在 host 侧，
主要用于 Windows 连接设备、手动控参、采图、保存样本和记录 trace。

当前采图服务仓库路径：

```text
/Users/yjunj/Projects/smore-cam-cap
```

## 组件

- `capture_client.py`：面向命令式采图服务的 adapter。
- `at_mvp_gui.py`：Python 桌面工具，支持手动设置相机参数、图像预览、
  图像保存和 trace 记录。

## 默认协议

默认 adapter 使用带长度头的 TCP 协议：

1. 客户端发送 4 字节命令长度，使用网络字节序。
2. 客户端发送 UTF-8 命令字符串。
3. 对于 `GET_FRAME`，服务端返回 4 字节图像长度，再返回图像编码字节。

MVP 当前使用的命令：

- `SET_EXP_PARAMS <exp_time> <exp_gain>`
- `SET_FOCUS_POS <focus_pos>`
- `SET_LIGHTS <l0> <l1> ...`
- `GET_FRAME`

真实下位机采图服务可以继续使用这套协议，也可以在 `CaptureServiceClient`
后面替换为 socket、HTTP 或 RPC。

## 运行方式

```bash
python3 tools/at_capture/at_mvp_gui.py --host 127.0.0.1 --port 8080 --output-dir captures
```

Windows host 环境需要安装依赖：

```powershell
python -m pip install opencv-python pillow numpy
```

保存样本时会生成 `.png` 图像和对应的 `.json` 元信息。
交互事件会追加到 `capture_trace.jsonl`。
