# visenz-at Agent 模板

这套模板用于把当前 `visenz-at` 项目的关键信息、工程规则和 AT 算法约束迁移到其他 agent 平台，例如 Cursor、Claude Code、Codeium、Continue 或内部 agent。

## 目录

```text
agent_templates/
  common/
    project_context.md
    engineering_rules.md
    at_algorithm_rules.md
  agents/
    implementation_agent.md
    review_agent.md
    planning_agent.md
  cursor/
    rules/
      visenz-at.mdc
```

## Cursor 使用方式

推荐先不要直接把模板写入项目根目录 `.cursor/`，确认内容后再复制：

```bash
mkdir -p .cursor/rules
cp agent_templates/cursor/rules/visenz-at.mdc .cursor/rules/visenz-at.mdc
```

如果希望 Cursor 更精细，可以把 `common/*.md` 的内容拆成多个 `.mdc` 规则，例如：

- `project-context.mdc`
- `engineering-rules.mdc`
- `at-algorithm-rules.mdc`

## 其他 Agent 平台使用方式

把下面内容按顺序拼到 agent 的 system/developer prompt 或 project instructions：

1. `common/project_context.md`
2. `common/engineering_rules.md`
3. `common/at_algorithm_rules.md`
4. 根据用途选择 `agents/*.md`

## 维护规则

- 当 `ATSession` 接口、`FlowConfig`、`at_device_runner` 协议或构建命令变化时，同步更新这些模板。
- 当前阶段文档和对话默认使用中文，便于快速评审和迁移。
- 模板只沉淀稳定规则，不记录临时实验结果；临时测试结论应放 trace、issue 或任务系统中。
