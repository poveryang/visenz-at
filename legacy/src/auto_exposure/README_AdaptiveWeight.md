# 自适应权重自动曝光策略

## 自适应权重策略

### 1. 边际效率计算

算法通过历史数据计算曝光时间和增益的边际效率，解决了多变量变化和单位不一致的问题：

```cpp
// 计算变化百分比（归一化处理）
et_change_pct = |et_change| / previous_et
eg_change_pct = |eg_change| / previous_eg
brt_change_pct = |brt_change| / previous_brt

// 判断主导变化参数
if (et_change_pct > eg_change_pct * 2.0) {
    // 曝光时间主导变化，直接计算边际效率
    marginal_et_efficiency = brt_change_pct / et_change_pct
} else if (eg_change_pct > et_change_pct * 2.0) {
    // 增益主导变化，直接计算边际效率
    marginal_eg_efficiency = brt_change_pct / eg_change_pct
} else {
    // 多变量同时变化，使用理论模型
    // 曝光时间：递减收益模型
    normalized_et = (current_et - min_et) / (max_et - min_et)
    marginal_et_efficiency = exp(-normalized_et * 1.5)
    
    // 增益：指数衰减模型（噪声放大）
    normalized_gain = (current_gain - min_gain) / (max_gain - min_gain)
    marginal_eg_efficiency = exp(-normalized_gain * 2.0)
}
```

### 2. 权重自适应更新

基于边际效率计算权重：

```cpp
// 效率比率
efficiency_ratio = et_efficiency / (et_efficiency + eg_efficiency)

// 平滑更新权重
new_et_weight = smoothing_factor * efficiency_ratio + (1 - smoothing_factor) * current_et_weight
new_eg_weight = 1.0 - new_et_weight

// 应用约束
et_weight = clamp(new_et_weight, min_weight, max_weight)
eg_weight = clamp(new_eg_weight, min_weight, max_weight)
```

### 3. 加权缩放因子计算

使用自适应权重计算曝光时间和增益的缩放因子：

```cpp
// 提升亮度时
et_scale_max = min(total_scale, max_et / current_et)
et_scale = 1.0 + (et_scale_max - 1.0) * et_weight
remaining_scale = total_scale / et_scale

if (remaining_scale > 1.0) {
    eg_scale = remaining_scale
    // 应用增益特定约束
}
```

## 配置参数

### 基础配置
- `initial_et_weight`: 曝光时间初始权重 (0.0-1.0)
- `initial_eg_weight`: 增益初始权重 (0.0-1.0)
- `smoothing_factor`: 权重更新平滑因子 (0.0-1.0)
- `history_window_size`: 效率计算历史窗口大小
- `min_weight`: 最小权重限制
- `max_weight`: 最大权重限制

### 预设配置

#### 保守配置 (Conservative)
- 优先使用曝光时间
- 较慢的权重适应
- 适合对图像质量要求高的场景

#### 激进配置 (Aggressive)
- 平衡使用曝光时间和增益
- 较快的权重适应
- 适合需要快速响应的场景

#### 低光配置 (Low-light)
- 适度偏好曝光时间
- 较高的增益容忍度
- 适合低光照环境

## 优势

1. **自适应性强**: 根据实际效果动态调整策略
2. **图像质量优化**: 在保证亮度的同时最小化噪声
3. **响应速度快**: 能够快速适应不同场景
4. **可配置性**: 提供多种预设配置和自定义参数
5. **多变量处理**: 智能处理曝光时间和增益同时变化的情况
6. **归一化计算**: 解决不同参数单位不一致的问题

## 注意事项

1. **历史窗口大小**: 过小的窗口可能导致权重变化过于频繁，过大的窗口可能导致响应迟钝
2. **平滑因子**: 过大的平滑因子可能导致权重变化过快，过小可能导致收敛缓慢
3. **权重约束**: 需要合理设置最小和最大权重，避免极端情况
4. **场景切换**: 在切换灯光或场景时，建议调用 `ResetHistory()` 重置历史记录
5. **主导变化阈值**: 当前设置为2.0倍，可根据实际情况调整判断主导变化的阈值
6. **理论模型参数**: 指数衰减模型的参数可根据实际相机特性进行调整

## 性能考虑

- 历史数据存储：使用固定大小的滑动窗口，内存占用可控
- 计算复杂度：边际效率计算为 O(1)，权重更新为 O(1)
- 实时性：所有计算都在曝光调整循环中完成，不影响实时性能 