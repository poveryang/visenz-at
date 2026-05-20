# AT Runner 上位机 MVP

这是 AT 效果优先工程闭环中的最小上位机工具。

目标设备默认是 ARM Linux，首期只面向 `imx8plus`。该工具运行在 host 侧，
主要用于 Windows 连接设备、手动控参、采图、保存样本和查看 AT 过程。

当前采图 SDK 仓库路径：

```text
/Users/yjunj/Projects/smore-cam-cap
```

## 接入策略

短期按一条在线闭环推进：

- 设备端：部署 `at_device_runner --server`，它链接 `AT` 核心库和 `smore-cam-cap` SDK，
  负责打开相机、手动控参、预览采图和执行 AT step。
- 上位机：Python 工具连接 `at_device_runner`，显示预览图像，触发 AT 执行，
  并记录每一步返回的图像、状态和 trace。
- AT core：只接收图像、相机参数、设备状态和 trace，不直接依赖相机 SDK。

`smore-cam-cap` 不作为 `AT_CORE` 的直接依赖，只在设备端 runner/adapter 边界链接。

## 组件

- `capture_client.py`：面向 `at_device_runner --server` 的 adapter。
- `at_mvp_gui.py`：Python 桌面工具，支持手动设置相机参数、图像预览、
  图像保存、AT step 执行和 trace 记录。
- `apps/at_runner/main.cpp`：设备端 AT runner 源码（安装名为 `at_device_runner`）。

## AT Runner 协议

当前 adapter 对齐 `at_device_runner --server`：

1. 客户端发送 4 字节 JSON 命令长度，使用网络字节序。
2. 客户端发送 UTF-8 JSON 命令。
3. 服务端返回 4 字节 JSON 响应头长度，使用网络字节序。
4. 服务端返回 UTF-8 JSON 响应头。
5. 如果是采图命令，继续返回响应头中声明长度的图像字节。

MVP 当前使用的命令：

- `get_status`
- `set_params`
- `capture`
- `close_lights`
- `reset_at`
- `at_step`

`set_params` 字段：

- `exposure_us`
- `gain`
- `focus`
- `lights`: 4 个整数

采图和 AT step 当前使用 `png` 编码。`at_step` 的响应会包含：

- `image`：本 step 采到的 PNG 图像。
- `trace`：本 step 的 AT trace。
- `at`：`finished`、`need_decode`、`step` 等执行状态。

## 设备端启动参考

部署当前工程生成的 `at_device_runner`。当前 AT 上位机默认连接：

```text
host: 设备 IP
port: 8080
```

设备端服务参考命令：

```bash
at_device_runner --server --device vs1000p_2mp --port 8080
```

注意：`at_device_runner --server` 会直接打开相机，不需要再启动 `capture_server`。

## 上位机工具运行方式

```bash
# 连接下位机时指定设备 IP
python3 tools/at_capture/at_mvp_gui.py --host 10.80.184.167 --port 8080 --output-dir captures

# 无 GUI 冒烟测试（见 scripts/device/README.md）
./scripts/device/runner.sh test
```

Windows host 环境需要安装依赖：

```powershell
python -m pip install opencv-python pillow numpy
```

保存样本时会生成：

```text
captures/
  images/
  manifest.jsonl
  at_runner_trace.jsonl
```

- `images/` 保存服务端返回的原始 PNG 字节。
- `manifest.jsonl` 保存图像路径、相机参数、设备状态、AT trace、采集时间和备注。
- `at_runner_trace.jsonl` 记录上位机交互事件。

## 设备端 AT runner 构建方式

先在 `smore-cam-cap` 仓库生成安装形态 SDK：

```bash
cmake -S /Users/yjunj/Projects/smore-cam-cap \
  -B /Users/yjunj/Projects/smore-cam-cap/build/imx8plus \
  -DDEVICE=vs1000p_2mp \
  -DCAMCAP_BUILD_TESTS=OFF
cmake --build /Users/yjunj/Projects/smore-cam-cap/build/imx8plus --target install
```

然后在当前 AT 仓库中交叉编译设备端 runner（imx8plus）：

```bash
# 默认 ENABLE_AT_RUNNER=ON，需本机存在 vs1000p_2mp SDK
./scripts/build/imx8plus.sh

# 产物: release/AT_v<version>/imx8plus/bin/at_device_runner
# 部署(上传+启动) / 测试: cp scripts/device/device.env.example device.env 后 ./scripts/device/runner.sh all
```

本地 core 单测：

```bash
cmake -S . -B build/at_core_baseline -DENABLE_CORE_TEST=ON
cmake --build build/at_core_baseline --target at_core_unit_test
./build/at_core_baseline/at_core_unit_test
```

设备端运行示例：

```bash
at_device_runner --device vs1000p_2mp --steps 8 --out-dir /tmp/at_run --save-images
```

输出内容：

- 标准输出：每个 step 一行 JSON trace。
- `/tmp/at_run/step_*.png`：每一步采到的图像。
- `/tmp/at_run/trace.jsonl`：完整 AT 过程 trace。

服务模式运行示例：

```bash
at_device_runner --server --device vs1000p_2mp --port 8080
```

上位机连接该服务后，可以：

- 手动设置曝光、增益、焦距和补光灯。
- 预览当前画面。
- Reset AT，按单步执行 AT。
- Run AT，连续执行 AT step，并把每一步图像和 trace 回传到上位机显示。
