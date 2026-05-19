# Legacy 迁移映射

> 状态：已执行（Gate-1）。`legacy/` 为只读参考，默认不参与构建。

## 目录映射

| 原路径 | 新路径 | 说明 |
|--------|--------|------|
| `include/` | `legacy/include/` | 旧公共头（`at_interface.h` 等） |
| `include/at/` | `legacy/include/at/` | 旧 AT 契约（BrightnessPrecondition 流） |
| `src/` | `legacy/src/` | 旧实现（core、vision、auto_ae/af） |
| `apps/at_device_runner/` | `legacy/apps/at_device_runner/` | 旧设备 runner |
| `tools/at_capture/` | `legacy/tools/at_capture/` | 旧 GUI/客户端 |
| `test/` | `legacy/test/` | 旧单测与样例 |

## 新主线路径

| 路径 | 职责 |
|------|------|
| `include/at_*.h` | 新契约：`FrameContext`、`StepPhase` 主路径、trace schema |
| `src/core/` | 纯策略：`AtSession`、`candidate_store`、vision 工具 |
| `src/orchestrator/` | `AtOrchestrator`：providers + session |
| `src/modules/` | 三调节模块（首版逻辑内聚于 `at_session.cpp`，目录预留） |
| `src/providers/` | heatmap / decode / preprocess 扩展槽 |
| `apps/at_runner/` | 镜像测试服务（仅 I/O，Gate-1 起为骨架） |
| `tools/at_capture/` | 上位机（自 legacy 复制，协议待对齐新 runner） |
| `test/unit/` | 新 `at_core_unit_test` |

## CMake 目标映射

| 旧目标 | 新目标 |
|--------|--------|
| `AT_CORE` | `AT`（`src/` 静态库） |
| `AT`（SHARED + Barcode） | 不默认构建；见 `legacy/` |
| `at_device_runner` | `at_runner`（`ENABLE_AT_RUNNER`） |
| `at_core_unit_test` | `at_core_unit_test`（新 `test/unit/`） |

## 构建命令

```bash
cmake -S . -B build/at_core_baseline -DENABLE_CORE_TEST=ON
cmake --build build/at_core_baseline --target at_core_unit_test
./build/at_core_baseline/at_core_unit_test
```
