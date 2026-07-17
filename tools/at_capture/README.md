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
- 上位机：Python 工具连接 `at_device_runner`，显示预览图像并触发设备端 AT；
  常规运行只轮询轻量状态和 trace，预览图是可选、低频、按需回传的。
- AT core：只接收图像、相机参数、设备状态和 trace，不直接依赖相机 SDK。

`smore-cam-cap` 不作为 `AT_CORE` 的直接依赖，只在设备端 runner/adapter 边界链接。

## 组件

- `capture_client.py`：面向 `at_device_runner --server` 的 adapter。
- `at_mvp_gui.py`：Python 桌面工具，支持手动设置相机参数、图像预览、
  图像保存、AT step 执行和 trace 记录。
- `apps/at_runner/main.cpp`：设备端 AT runner 源码（安装名为 `at_device_runner`）。
- `src/providers/yolo_detect_provider.cpp`：AT 的 YOLO 码区检测 provider，
  负责加载 `heatmap-model/cpp` 的 YOLOv8 Tengine/TIM-VX 模型。

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
- `capture_heatmap`
- `close_lights`
- `reset_at`
- `at_step`
- `run_at_async` / `get_run_status` / `stop_at_async` / `get_preview`

`set_params` 字段：

- `exposure_us`
- `gain`
- `focus`
- `lights`: 4 个整数

采图和 AT step / preview 默认使用 `jpeg` 编码（可用 `encoding=png` 或
`jpeg_quality=1..100` 覆盖）。启用 heatmap 后，`at_step` 返回的图像会融合
heatmap 伪彩色并绘制 ROI，便于观察 AT 过程；`capture` 仍返回当前相机图像。
`capture_heatmap` 只采当前帧并执行 heatmap 推理，不推进 AT 状态，适合采集模型验证集。
`at_step` 的响应会包含：

- `image`：本 step 采到的 PNG 图像。
- `trace`：本 step 的 AT trace，包含 `heatmap` 字段。
- `at`：`finished`、`need_decode`、`step` 等执行状态。

`run_at_async` 适合实际性能测试：AT 循环一直在设备端执行，关键路径不进行 PNG
编码、磁盘写入或网络发送。`get_run_status` 只返回最新 trace 和进度；设置
`preview_every > 0` 后，设备仅保存对应步的原始帧，主机调用 `get_preview` 时才编码并
拉取最新一张图。因此预览默认关闭（`0`），不会影响设备端测时。

## 设备端启动参考

部署当前工程生成的 `at_device_runner`。当前 AT 上位机默认连接：

```text
host: 设备 IP
port: 8080
```

设备端服务参考命令：

```bash
at_device_runner --server --device vs1000p_2mp --port 8080

# 启用 YOLO 码区检测
at_device_runner --server --device vs1000p_2mp --port 8080 \
  --heatmap-model /tmp/at_runner/model/barcode-yolov8n-gray-final-uint8.tmfile \
  --heatmap-context timvx
```

注意：`at_device_runner --server` 会直接打开相机，不需要再启动 `capture_server`。
设备端 heatmap 在 AT provider 内同进程加载，`capture` 只采图不推理，`at_step` 才会推理。
部署脚本默认不上传 `heatmap-model` 工程自带的 Tengine/Vivante `.so`，运行时使用板端 `/usr/lib`。

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
# 默认 ENABLE_YOLO=ON：使用 ~/Projects/heatmap-model 的 YOLO 检测器构建 AT_YOLO_TENGINE 并安装 yolov8 tmfile
# 部署(上传+启动) / 测试: cp scripts/device/device.env.example scripts/device/device.env 后 ./scripts/device/runner.sh all
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
- `/tmp/at_run/trace.jsonl`：完整 AT 过程 trace；启用 heatmap 时包含 `heatmap_perf`，
  记录模型初始化耗时、最近一帧推理耗时和平均推理耗时。

服务模式运行示例：

```bash
at_device_runner --server --device vs1000p_2mp --port 8080
```

上位机连接该服务后，可以：

- 手动设置曝光、增益、焦距和补光灯。
- 预览当前画面。
- Reset AT，按单步执行 AT。
- Run on Device，连续在设备端执行 AT；界面轮询进度和 trace。预览频率为 `0` 时完全
  不传图，设置为 N 时每 N 步可异步查看最新一帧。

## Heatmap 验证集采集

`heatmap_dataset_capture.py` 在上位机侧按参数网格连续采集样本。默认只调用
`capture` 保存原始图像，不做 heatmap 推理；需要同时记录模型响应时增加
`--with-heatmap`。

图像文件名会包含参数，格式固定且对齐：

```text
s0001_e003000_g050_f0300_l1111.png
```

- `e`：曝光时间，6 位，最大按 100000 对齐。
- `g`：增益，3 位。
- `f`：对焦位置，4 位。
- `l`：补光灯，固定 4 位。

连续对焦采集：

```bash
python3 tools/at_capture/heatmap_dataset_capture.py \
  --host 10.80.184.167 --port 8080 \
  --output-dir datasets/hmap_focus_001 \
  --exposure 3000 --gain 50 --focus-range 0:1023:20 \
  --lights "1111;1000;0100" --settle-ms 120
```

不同增益下扫描曝光：

```bash
python3 tools/at_capture/heatmap_dataset_capture.py \
  --host 10.80.184.167 --port 8080 \
  --output-dir datasets/hmap_exp_gain_001 \
  --exposure-range 200:20000:400 --gains 20,50,80,110 \
  --focus 300 --lights "1111" --settle-ms 120
```

固定曝光扫描增益：

```bash
python3 tools/at_capture/heatmap_dataset_capture.py \
  --host 10.80.184.167 --port 8080 \
  --output-dir datasets/hmap_gain_001 \
  --exposure 3000 --gain-range 1:128:4 \
  --focus 300 --lights "1111"
```

补光灯组合：

```bash
# 指定组合
python3 tools/at_capture/heatmap_dataset_capture.py \
  --host 10.80.184.167 --output-dir datasets/hmap_light_001 \
  --exposure 3000 --gain 50 --focus 300 \
  --lights "0000;1000;0100;0010;0001;1111"

# 全部 16 种组合
python3 tools/at_capture/heatmap_dataset_capture.py \
  --host 10.80.184.167 --output-dir datasets/hmap_light_all_001 \
  --exposure 3000 --gain 50 --focus 300 --all-lights
```

输出目录：

```text
datasets/hmap_xxx/
  images/
  manifest.jsonl
  sweep_plan.json
```

默认保存原始图像，并在 `manifest.jsonl` 中记录采集参数。增加 `--with-heatmap`
后会额外记录 heatmap trace 和推理耗时；若需要直接保存融合图，再增加 `--overlay`。
如果只需要采集训练/验证原图，不需要额外参数。

## Heatmap 指标统计

基础指标统计：

```bash
python3 tools/at_capture/heatmap_dataset_metrics.py \
  --manifest datasets/hmap_exp_gain_001/manifest.jsonl \
  --output datasets/hmap_exp_gain_001/metrics.json \
  --samples-output datasets/hmap_exp_gain_001/sample_metrics.jsonl \
  --group-by gain lights
```

当前指标包括：

- 图像亮度、对比度、清晰度、饱和像素比例、噪声代理值。
- heatmap available rate、confidence、feature strength、ROI 面积占比。
- TIM-VX 推理耗时。
- 可选人工标注 ROI 后的 IoU 和 ROI 中心命中率。

人工标注文件格式：

```json
{"image_path":"images/capture_xxx.png","roi":{"x":100,"y":80,"width":300,"height":160}}
```
