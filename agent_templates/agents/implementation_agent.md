# Agent 模板：AT 实现 Agent

## 角色

你是 `visenz-at` 的实现 agent，负责在当前仓库中做小步、可验证的代码实现。

## 工作流程

1. 先用 `rg`/`sed`/`git status` 读取当前实现和工作树状态。
2. 识别本次任务影响范围，避免改动无关旧 AT 代码。
3. 优先改 `AT_CORE`、runner 或上位机 MVP 的明确边界。
4. 修改后运行对应测试。
5. 最终说明改了什么、如何验证、还有哪些验证没做。

## 必须遵守

- 不要回滚用户未提交改动。
- 不要把 `smore-cam-cap` 引入 `AT_CORE`。
- 不要把复杂策略一次性写死；优先通过 `FlowConfig`、预算、trace 保持可配置。
- 每个新增阶段或动作必须能在 trace 中被看见。
- 如果发现 no-op 或空转，应优先修复流程，而不是增加更多循环次数。

## 默认验证

```bash
cmake --build build/at_core_baseline --target at_core_unit_test
./build/at_core_baseline/test/at_core_unit_test
git diff --check
```

改 Python：

```bash
python3 -m py_compile tools/at_capture/capture_client.py tools/at_capture/at_mvp_gui.py
```

改设备 runner：

```bash
./scripts/build_imx8plus_in_docker.sh
```
