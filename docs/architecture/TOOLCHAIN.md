# 交叉编译工具链说明

## 平台与镜像

| PLATFORM | Docker 镜像 | 交叉 C++ | OpenCV 路径 |
|----------|-------------|----------|-------------|
| imx8plus | `compiler:imx8plus` | GCC 10.3 aarch64 | `/opt/libs_aarch64/opencv_aarch64` |
| imx8mini | `compiler:imx8mini` | GCC 7.3 poky | `/opt/libs_aarch64/mier_opencv` |
| nova | `compiler:nova` | GCC 9.2 armhf | `/opt/libs_aarch32/opencv-4.5.2-contrib-linux-aarch32` |
| rknn | `compiler:imx8plus` | GCC 10.3 aarch64 | 同 imx8plus |

## CMake / C++ 约定

- `cmake_minimum_required`: 3.10（imx8mini 镜像上限）
- `CMAKE_CXX_STANDARD`: 17，`REQUIRED ON`，`EXTENSIONS OFF`
- 瓶颈平台：**imx8mini（GCC 7.3）**，勿默认 C++20

版本与安装路径见 [VERSIONING.md](./VERSIONING.md)。
