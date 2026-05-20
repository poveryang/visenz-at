# 交叉编译（Docker）

产物安装到 `release/AT_v<version>/<platform>/`（版本由 `common.sh` 从根目录 `CMakeLists.txt` 解析）。

```bash
# 单平台（imx8plus 含 at_device_runner，默认 ON）
./scripts/build/imx8plus.sh

# 全平台
./scripts/build/all.sh

# 仅库、不编 runner
ENABLE_AT_RUNNER=OFF ./scripts/build/imx8plus.sh
```

| 脚本 | 平台 |
|------|------|
| `imx8plus.sh` | imx8plus + runner（需 smore-cam-cap SDK） |
| `imx8mini.sh` | imx8mini |
| `nova.sh` | nova |
| `rknn.sh` | rknn |
| `all.sh` | 依次构建上表 |

## 环境变量

| 变量 | 默认 | 说明 |
|------|------|------|
| `BUILD_JOBS` | `16` | 并行编译线程 |
| `CLEAN_BUILD` | `1` | 配置前删除 `build/<platform>` |
| `ENABLE_AT_RUNNER` | imx8plus: `ON`，其它: `OFF` | 是否编译 `at_device_runner` |
| `SMORE_CAM_CAP_SDK_HOST` | `~/Projects/smore-cam-cap/release/vs1000p_2mp` | 仅 imx8plus runner |
| `CONTINUE_ON_ERROR` | `1` | 仅 `all.sh`：单平台失败是否继续 |

设备部署与联调见 [`../device/README.md`](../device/README.md)。
