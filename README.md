# ViSenz_AT4VS

Camera auto tuning algorithms for Smore-ViScanner.

## Change log

### 3.3.2 --- 2022-11-08

Refactor:

- 重构AT对外接口，使功能逻辑更清晰
- 重构内部执行流程，使兼容不同下游视觉任务(不限于解码)

Added:

- AE模块中新增快速曝光控制功能，用于进行粗曝光控制
- AF模块前默认执行粗曝光控制，避免在完全过曝或欠曝条件下出现的对焦失败
- AT执行过程中，在关键采样点执行解码任务并分析码区图像质量，最终成像结果取最优样本位置

