# 工程规则

## 基本工作方式

- 先读当前代码，再修改。不要仅根据记忆或文档猜实现。
- 搜索优先用 `rg`，文件列表优先用 `rg --files` 或 `find`。
- 修改要小步、可验证，避免跨模块大改。
- 不要回滚用户已有改动；工作树可能是脏的。
- 不要随意删除日志、样本、未跟踪文件，除非用户明确要求。
- 项目说明、方案文档和评审材料默认用中文。

## 架构边界

- `AT_CORE` 不直接依赖相机 SDK、socket 协议、GUI、设备服务。
- `smore-cam-cap` 只允许出现在 runner/adapter 边界。
- `ATSession` 是新版核心入口，职责保持薄：接收帧、构建观测、调用策略、输出 step result、记录 trace。
- 旧 `ATInterface` 只作为兼容过渡，不应继续把新策略塞进旧接口形态。
- 新模块优先服务主流程，不要先堆抽象名称。

## C++ 约束

- 当前 C++ 标准是 C++17。
- 优先使用清晰的值类型：`FrameInput`、`Observation`、`TuningDecision`、`StepResult`。
- 公共头文件保持稳定、简洁，避免暴露内部实验细节。
- Trace 字段要稳定，因为上位机、设备 runner、离线分析都依赖它。

## Python 上位机约束

- `capture_client.py` 是协议 adapter，屏蔽 socket/JSON/图像字节细节。
- `at_mvp_gui.py` 是工程 MVP 工具，不追求美观优先，优先保证控参、采图、AT trace、样本保存可靠。
- 连续 AT 的 trace 写入 `at_runner_trace.jsonl`。
- 采样图像和日志目录属于运行产物，不应提交。

## 测试要求

每次改核心流程后至少运行：

```bash
cmake --build build/at_core_baseline --target at_core_unit_test
./build/at_core_baseline/test/at_core_unit_test
git diff --check
```

改 Python 上位机后运行：

```bash
python3 -m py_compile tools/at_capture/capture_client.py tools/at_capture/at_mvp_gui.py
```

改 runner、协议或 SDK 边界后，需要在 Docker/设备上验证：

```bash
./scripts/build_imx8plus_in_docker.sh
```

如果 Docker 未启动或设备不在手边，要明确说明未完成的验证边界。
