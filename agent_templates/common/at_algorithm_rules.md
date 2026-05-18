# AT 算法规则

## 设计优先级

先把主流程想清楚，再设计模块边界。不要从模块名称或类图开始推流程。

当前阶段采用简化、可配置流程：

```text
BrightnessPrecondition -> FocusExplore -> LocalRefine -> Done
```

后续模块成熟后再逐步打开：

```text
RoiObserve
DecodeVerify
PostDecodeRefine
FallbackSearch
PreprocessProfile
```

## 核心原则

- 亮度前置必须早于对焦。明显欠曝或过曝时，focus、heatmap、ROI、decode 信号都不可靠。
- focus 不能长期只选单一最大 sharpness 峰；后续要支持多峰、top-K、峰宽、邻域稳定性。
- heatmap 是观测来源，不是直接调参模块。它应输出 ROI、confidence、feature strength、direction hint、stability。
- decode 是 checkpoint/验证信号，不是主调节大脑。decode 调用应受预算和时机控制。
- CandidatePool 负责记录、评分、比较和解释候选，不直接决定调参动作。
- Trace 是调试和评审的一等产物，每一步都应能解释当前阶段、动作、质量指标、候选选择和结束原因。

## 当前模块定位

- `Observation`：统一承载图像质量、ROI、heatmap、decode feedback、当前参数。
- `FlowConfig`：控制当前启用哪些阶段，以及每个阶段的最小步数或策略开关。
- `TuningDecision`：一次 step 的阶段、动作、下一参数、是否完成、完成原因。
- `StepTrace`：对外可观察记录，供 GUI、runner、离线回放分析。

## 当前简化策略

- 亮度前置：先把图像调到可判断范围，不追求最终最佳亮度。
- 对焦探索：当前仅做最小步进，后续加入 focus samples 和峰值分析。
- 局部 refine：当前是占位保持，后续接亮度/对比度/SNR/预处理 profile。
- decode：heatmap 高置信度时允许请求；周期 decode 默认关闭，避免 decoder 未接入前空转。

## 判断明显问题时优先看

- 是否有 no-op：`next_params == current_params` 且 action 不是 `Hold/Finish/RequestDecode`。
- 是否卡在单一阶段：例如重复 `RoiObserve/AdjustLight`。
- 是否 trace 里 `decode_used` 长期为 0，但反复 `RequestDecode`。
- 是否亮度前置过早退出，导致后续阶段在低质量图像上运行。
- 是否 focus 只在极小范围震荡，而不是形成有效扫描。
