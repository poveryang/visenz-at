# Legacy（只读参考）

本目录存放 AT 重构前的完整实现，**默认不参与构建**。

## 策略

- `readonly_freeze`：不在此目录叠加新策略或功能增量。
- 新主线见仓库根目录 `include/at_*.h`、`src/core/` 等（见 `docs/architecture/SOURCE_LAYOUT.md`）。
- 需要对照旧行为时，仅作阅读与 trace 对比参考。

## 恢复旧构建（本地调试，非主线）

```bash
# 需在独立 build 目录中手动指定 legacy 源路径（不推荐作为 CI 默认）
# 参见 legacy 内原始 CMakeLists 片段（根 CMake 已移除旧 AT/AT_CORE 目标）
```
