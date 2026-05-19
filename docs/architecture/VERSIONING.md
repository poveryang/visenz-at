# 版本号维护

## 唯一来源

[`CMakeLists.txt`](../CMakeLists.txt)：

```cmake
project(AT VERSION 1.0.0)   # 发版时只改这里
```

- 安装目录：`release/AT_v1.0.0/<platform>/`
- 库 API：`at::GetLibVersion()` → `"1.0.0"`

## 软件用法

```cpp
#include "at_types.h"
// 链接 libAT.a

const char *ver = at::GetLibVersion();
```

实现编译在 `src/core/at_session.cpp` 末尾（与 `AtSession` 同属 core，无需单独 `at_version.cpp`）。

`at::kTraceVersion` 为 trace 协议版本，不是库发布版本。
