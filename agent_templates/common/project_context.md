# 项目上下文：visenz-at

## 项目目标

`visenz-at` 是 AT 算法工程化重构分支。当前阶段目标不是追求完整策略一次到位，而是先建立可测试、可解释、可在线联调的 AT 框架：

- 效果优先，调节过程要更可控、更稳定。
- 简单场景快速收敛，困难场景允许受控增加调节时间。
- 算法接口允许重构和简化，旧 `ATInterface` 仅作为过渡兼容层。
- 当前优先平台是 `imx8plus`；其他平台后续再加。

## 当前核心方向

新版核心入口：

- `include/at/at_session.h`
- `include/at/at_types.h`
- `include/at/at_trace.h`
- `src/core/session_controller.cpp`
- `src/core/candidate_pool.cpp`
- `src/vision/image_quality.cpp`
- `src/vision/roi_tracker.cpp`

核心接口：

- `ATSession::Step(const FrameInput&) -> StepResult`
- `FrameInput` 输入图像、当前相机参数、可选 ROI、可选 heatmap、可选 decode feedback。
- `StepResult` 输出下一步参数、是否完成、是否需要 decode、best candidate 和 trace。
- `FlowConfig` 用于配置阶段流，当前默认流程先保持简单。

当前默认阶段流：

```text
BrightnessPrecondition -> FocusExplore -> LocalRefine -> Done
```

`RoiObserve`、周期性 `DecodeVerify`、`PostDecodeRefine`、`FallbackSearch` 暂时不作为默认流程，后续在 heatmap、decode、focus 模块成熟后再逐步打开。

## 工程闭环

设备端：

- `apps/at_device_runner/main.cpp`
- 链接 `AT_CORE` 和 `smore-cam-cap` SDK。
- 可单次执行 AT，也可 `--server` 方式提供上位机连接。

上位机：

- `tools/at_capture/capture_client.py`
- `tools/at_capture/at_mvp_gui.py`
- 用于手动控参、采图预览、保存样本、Run AT、查看 trace。

采图 SDK：

```text
/Users/yjunj/Projects/smore-cam-cap
```

`smore-cam-cap` 不应成为 `AT_CORE` 的直接依赖，只能在 runner/adapter 边界链接。

## 常用命令

本机核心单测：

```bash
cmake --build build/at_core_baseline --target at_core_unit_test
./build/at_core_baseline/test/at_core_unit_test
```

Python 上位机语法检查：

```bash
python3 -m py_compile tools/at_capture/capture_client.py tools/at_capture/at_mvp_gui.py
```

imx8plus 交叉编译：

```bash
./scripts/build_imx8plus_in_docker.sh
```

设备端 runner 示例：

```bash
at_device_runner --device vs1000p_2mp --steps 8 --out-dir /tmp/at_run --save-images
at_device_runner --server --device vs1000p_2mp --port 8080
```

上位机 GUI 示例：

```bash
python3 tools/at_capture/at_mvp_gui.py --host <device-ip> --port 8080 --output-dir captures
```

## 当前未完成能力

- heatmap 真实模型尚未接入，当前只是输入/观测接口。
- decode SDK 尚未接入 runner 主流程，当前 `need_decode` 主要是策略信号。
- `FocusExplore` 目前是最小占位，尚未做多峰、峰宽、邻域稳定性分析。
- `PostDecodeRefine` 和预处理 profile 仍是后续阶段。
