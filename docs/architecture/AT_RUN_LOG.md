# AT 算法运行日志（AtRunLog）

与宿主软件（SmoreScanner / journal / main.log）**完全隔离**的算法侧日志。

## 目标

- 每次 AT 运行可完整回看过程（begin → step* → end）
- 简洁：只记决策与观测，不刷屏
- 不依赖宿主是否把 stdout 接到 journal

## 落盘位置

优先级：

1. `SessionConfig.run_log.dir`（若非空）
2. 环境变量 `AT_LOG_DIR`
3. 设备默认：`/usr/scanner/Log/at`
4. 回落：`/tmp/visenz_at`

目录内容：

| 文件 | 含义 |
|------|------|
| `run_<id>.jsonl` | 单次运行完整过程 |
| `latest.path` | 最近一次 run 文件路径 |
| `latest_init.txt` | Init 人类可读摘要 |
| 自动裁剪 | 仅保留最近 30 个 `run_*.jsonl` |

## 快速查看（设备）

```bash
cat /usr/scanner/Log/at/latest_init.txt
cat "$(cat /usr/scanner/Log/at/latest.path)"
```

`latest_init.txt` 示例：

```text
status=ready
version=5.4.0
heatmap_model=/usr/scanner/yolov8n-gray-test/model/barcode-yolov8n-gray-final-uint8.tmfile
run_log=/usr/scanner/Log/at/run_20260717_xxxxxx.jsonl
log_dir=/usr/scanner/Log/at
```

## JSONL 事件（schema=1）

仅三种 `event`：

### begin

版本、模型、flow/budget/camera 摘要。

### step

逐步决策：`phase` / `action` / `params` / `next_params` / `quality` / `heatmap` / `reason` / `finish_reason`。

### end

步数、耗时、最终参数与 `finish_reason`。

## 开关

- 产品 compat 路径：默认 **开启**
- Runner / 单测：`SessionConfig.run_log.enable = true` 时开启
- 关闭：`run_log.enable = false`，或不要写盘即可

## 相关代码

- `include/at_run_log.h` + `src/core/at_run_log.cpp`
- `AtOrchestrator::Reset` / `ProcessStep` 自动 begin/step/end
- `src/compat/at_interface_adapter.cpp` 默认启用并写 Init 状态
