# 源码布局说明

## 目录原则

- **`include/`**：对外 API，文件名带 `at_` 前缀（如 `at_session.h`），`#include "at_session.h"`。
- **`src/`**：实现；内部头（如 `core/candidate_store.h`）不安装，仅 `PRIVATE` 包含路径 `src/`。
- **`legacy/`**：旧实现只读，不参与默认构建。

## 头文件 ↔ 实现对照

| 公共头 (`include/`) | 实现 (`src/`) | 类 / 入口 |
|---------------------|---------------|-----------|
| `at_types.h` | `core/at_session.cpp`（含 `GetLibVersion`） | 类型、配置、库版本 |
| `at_trace.h` | — | `StepTrace`、`StepResult` |
| `at_session.h` | `core/at_session.cpp` | **`AtSession::ProcessStep`**（策略状态机） |
| `at_orchestrator.h` | `orchestrator/at_orchestrator.cpp` | **`AtOrchestrator::ProcessStep`**（推荐对外入口） |
| `at_providers.h` | `providers/null_providers.cpp` | 扩展槽默认实现 |

内部（不对外）：

| 内部头 | 实现 |
|--------|------|
| `core/candidate_store.h` | `core/candidate_store.cpp` |
| `core/vision/image_quality.h` | `core/vision/image_quality.cpp` |
| `core/vision/roi_tracker.h` | `core/vision/roi_tracker.cpp` |

## 进程入口

| 场景 | 文件 |
|------|------|
| 库 API（推荐） | `AtOrchestrator::ProcessStep` |
| 仅策略 | `AtSession::ProcessStep` |
| 可执行骨架 | `apps/at_runner/main.cpp` |
| 单测 | `test/unit/at_core_unit_test.cpp` |
