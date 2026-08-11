# Legacy ATInterface Compatibility Adapter

对外软件若仍调用旧 `at::ATInterface` API，可链接 `AT_COMPAT`（内部桥接新
`AtOrchestrator` / `AtSession`）。

## 产品侧集成

```text
头文件: include/at_interface.h, at_base.h, ar_base.h
库(静态): libAT_COMPAT.a + libAT.a (+ OpenCV)
库(动态 drop-in): 实体 libAT.so（ENABLE_AT_SHARED=ON；默认启用 YOLO 码区检测，忽略 en_hmap）
设备替换: 备份 /usr/lib/libAT.so 后覆盖，再启动 SmoreScanner
默认模型: `/usr/scanner/yolov8n-gray-test/model/barcode-yolov8n-gray-final-uint8.tmfile`
（可用环境变量 `AT_HEATMAP_MODEL` 覆盖）
```

调用顺序保持不变：

```text
ATInterface(en_hmap)
  -> Init(cam_conf, barcode, en_al, en_af, en_ae, en_ar)
  -> loop:
       GetNextParams()
       设参 / 采图
       finished = Run(image)
  -> GetBestParams() / GetARInfo()
```

## 行为说明

- **API 兼容**：旧签名可用；算法走新主线（非旧 AEQT/AF/AEST 流水线）。
- **CamParams ↔ CameraParams**：字段一致，适配层直接互转。
- **CamConf**：映射到 `SessionConfig` 的相机范围与步进；`ae_mode` 等无对应项忽略。
- **BarcodeWrapperBase**：可选；`en_ar=true` 时注入 `DecodeProvider` 桥。
- **ARInfo**：无 barcode SDK 时 `successful_code_type` 为 `int`；产品有 `barcode.h` 时可
  定义 `AT_COMPAT_USE_BARCODE_SDK` 以对齐旧类型。
- **GetVersion()**：返回 `at::GetLibVersion()`（CMake `project(AT VERSION …)`）。
- **en_hmap**：产品未使用；compat **默认启用 YOLO**（见上默认模型路径）。
- **运行日志**：默认写入 `/usr/scanner/Log/at/`，与宿主 stdout/journal 隔离；
  详见 [`AT_RUN_LOG.md`](AT_RUN_LOG.md)。

```bash
cat /usr/scanner/Log/at/latest_init.txt
cat "$(cat /usr/scanner/Log/at/latest.path)"
```

## 构建

```bash
cmake -S . -B build/at_core_baseline -DENABLE_CORE_TEST=ON -DENABLE_AT_COMPAT=ON -DPLATFORM=host
cmake --build build/at_core_baseline --target at_compat_unit_test
./build/at_core_baseline/at_compat_unit_test
```

`ENABLE_AT_COMPAT` 默认 `ON`。
