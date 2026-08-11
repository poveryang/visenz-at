#!/usr/bin/env bash
# imx8plus 交叉编译 → release/AT_v<VERSION>/imx8plus/（版本见 CMakeLists project VERSION）
#
# 环境变量:
#   ENABLE_AT_RUNNER   默认 ON（设备 runner）
#   ENABLE_AT_SHARED   默认 ON（产出产品 drop-in libAT.so，含 ATInterface）
#   SMORE_CAM_CAP_SDK_HOST  默认 ~/Projects/smore-cam-cap/release/vs1000p_2mp
#   ENABLE_YOLO        默认 ON（构建 AT YOLO 检测 provider；不影响 libAT.so）
#   YOLO_REPO_HOST     默认 ~/Projects/heatmap-model（需含 cpp/ 与 deploy/）
#   YOLO_MODEL_HOST    默认 <YOLO_REPO_HOST>/deploy/vs1000pro/yolov8n-gray/tmfile/barcode-yolov8n-gray-final-uint8.tmfile
#   YOLO_INSTALL_RUNTIME_LIBS 默认 OFF（部署默认使用板端 Tengine/Vivante 运行库）
#   BUILD_JOBS(16)  CLEAN_BUILD(1)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

GCC=/opt/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin
ENABLE_AT_RUNNER="${ENABLE_AT_RUNNER:-ON}"
ENABLE_AT_SHARED="${ENABLE_AT_SHARED:-ON}"
ENABLE_YOLO="${ENABLE_YOLO:-ON}"
YOLO_INSTALL_RUNTIME_LIBS="${YOLO_INSTALL_RUNTIME_LIBS:-OFF}"
CAMCAP_HOST="${SMORE_CAM_CAP_SDK_HOST:-${HOME}/Projects/smore-cam-cap/release/vs1000p_2mp}"
YOLO_HOST="${YOLO_REPO_HOST:-${HOME}/Projects/heatmap-model}"

cleanup_tengine_runtime_release_libs() {
  local release_lib
  release_lib="$(at_release_dir "$(at_repo_root)" imx8plus)/lib"
  [[ -d "${release_lib}" ]] || return 0
  rm -f \
    "${release_lib}/libtengine-lite.so" \
    "${release_lib}/libOpenVX.so" \
    "${release_lib}/libOpenVXU.so" \
    "${release_lib}/libGAL.so" \
    "${release_lib}/libVSC.so" \
    "${release_lib}/libArchModelSw.so" \
    "${release_lib}/libCLC.so" \
    "${release_lib}/libNNArchPerf.so"
}

extra_cmake=(
  -DCMAKE_C_COMPILER="${GCC}/aarch64-none-linux-gnu-gcc"
  -DCMAKE_CXX_COMPILER="${GCC}/aarch64-none-linux-gnu-g++"
  -DENABLE_AT_RUNNER="${ENABLE_AT_RUNNER}"
  -DENABLE_AT_SHARED="${ENABLE_AT_SHARED}"
  -DENABLE_AT_COMPAT=ON
  -DENABLE_CORE_TEST=OFF
  -DYOLO_INSTALL_RUNTIME_LIBS="${YOLO_INSTALL_RUNTIME_LIBS}"
)

if [[ "${ENABLE_AT_RUNNER}" == "ON" ]]; then
  if [[ ! -f "${CAMCAP_HOST}/include/camcap/camcap.h" ]]; then
    echo "camcap SDK not found: ${CAMCAP_HOST}" >&2
    echo "set SMORE_CAM_CAP_SDK_HOST to the vs1000p_2mp release directory" >&2
    exit 1
  fi
  export SMORE_CAM_CAP_SDK_HOST="${CAMCAP_HOST}"
  extra_cmake+=(-DSMORE_CAM_CAP_SDK_ROOT=/smore_cam_cap)
fi

if [[ "${ENABLE_YOLO}" == "ON" ]]; then
  if [[ ! -f "${YOLO_HOST}/cpp/include/yolo_detector.h" ]]; then
    echo "yolo detector SDK not found: ${YOLO_HOST}/cpp" >&2
    echo "set YOLO_REPO_HOST to the heatmap-model repository root, or set ENABLE_YOLO=OFF" >&2
    exit 1
  fi
  YOLO_MODEL="${YOLO_MODEL_HOST:-${YOLO_HOST}/deploy/vs1000pro/yolov8n-gray/tmfile/barcode-yolov8n-gray-final-uint8.tmfile}"
  export YOLO_REPO_HOST="${YOLO_HOST}"
  extra_cmake+=(-DYOLO_SDK_ROOT=/yolo_repo/cpp)
  if [[ -f "${YOLO_MODEL}" && "${YOLO_MODEL}" == "${YOLO_HOST}/"* ]]; then
    extra_cmake+=(-DYOLO_MODEL_FILE="/yolo_repo/${YOLO_MODEL#"${YOLO_HOST}/"}")
  else
    echo "warning: yolo model not found under repo: ${YOLO_MODEL}" >&2
    echo "runner will build, but you must provide --heatmap-model on device." >&2
  fi
fi

at_docker_build "$(at_repo_root)" imx8plus compiler:imx8plus build/imx8plus -- \
  "${extra_cmake[@]}"

if [[ "${ENABLE_YOLO}" == "ON" &&
      "${YOLO_INSTALL_RUNTIME_LIBS}" != "ON" && "${YOLO_INSTALL_RUNTIME_LIBS}" != "1" ]]; then
  cleanup_tengine_runtime_release_libs
fi
