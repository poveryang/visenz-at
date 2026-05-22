#!/usr/bin/env bash
# imx8plus 交叉编译 → release/AT_v<VERSION>/imx8plus/（版本见 CMakeLists project VERSION）
#
# 环境变量:
#   ENABLE_AT_RUNNER   默认 ON（设备 runner）
#   SMORE_CAM_CAP_SDK_HOST  默认 ~/Projects/smore-cam-cap/release/vs1000p_2mp
#   ENABLE_HMAP        默认 ON（构建 AT Tengine heatmap provider）
#   HMAP_SDK_HOST      默认 ~/Projects/heatmap-model/cpp
#   HMAP_INSTALL_RUNTIME_LIBS 默认 OFF（部署默认使用板端 Tengine/Vivante 运行库）
#   BUILD_JOBS(16)  CLEAN_BUILD(1)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

GCC=/opt/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin
ENABLE_AT_RUNNER="${ENABLE_AT_RUNNER:-ON}"
ENABLE_HMAP="${ENABLE_HMAP:-ON}"
HMAP_INSTALL_RUNTIME_LIBS="${HMAP_INSTALL_RUNTIME_LIBS:-OFF}"
CAMCAP_HOST="${SMORE_CAM_CAP_SDK_HOST:-${HOME}/Projects/smore-cam-cap/release/vs1000p_2mp}"
HMAP_HOST="${HMAP_SDK_HOST:-${HOME}/Projects/heatmap-model/cpp}"

cleanup_hmap_runtime_release_libs() {
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
  -DENABLE_CORE_TEST=OFF
)

if [[ "${ENABLE_AT_RUNNER}" == "ON" ]]; then
  if [[ ! -f "${CAMCAP_HOST}/include/camcap/camcap.h" ]]; then
    echo "camcap SDK not found: ${CAMCAP_HOST}" >&2
    echo "set SMORE_CAM_CAP_SDK_HOST to the vs1000p_2mp release directory" >&2
    exit 1
  fi
  export SMORE_CAM_CAP_SDK_HOST="${CAMCAP_HOST}"
  extra_cmake+=(-DSMORE_CAM_CAP_SDK_ROOT=/smore_cam_cap)

  if [[ "${ENABLE_HMAP}" == "ON" ]]; then
    if [[ ! -f "${HMAP_HOST}/include/hmap_generator.h" ]]; then
      echo "heatmap SDK not found: ${HMAP_HOST}" >&2
      echo "set HMAP_SDK_HOST to the heatmap-model/cpp directory, or set ENABLE_HMAP=OFF" >&2
      exit 1
    fi
    if [[ ! -f "${HMAP_HOST}/build/imx8plus/libHMAP.a" ]]; then
      echo "heatmap libHMAP.a not found: ${HMAP_HOST}/build/imx8plus/libHMAP.a" >&2
      echo "build heatmap-model/cpp first, or set ENABLE_HMAP=OFF" >&2
      exit 1
    fi
    if [[ ! -f "${HMAP_HOST}/artifacts/tmfile/model-uint8.tmfile" ]]; then
      echo "warning: heatmap model not found: ${HMAP_HOST}/artifacts/tmfile/model-uint8.tmfile" >&2
      echo "runner will build, but you must provide --heatmap-model on device." >&2
    fi
    export HMAP_SDK_HOST="${HMAP_HOST}"
    extra_cmake+=(-DHMAP_SDK_ROOT=/hmap_sdk -DHMAP_INSTALL_RUNTIME_LIBS="${HMAP_INSTALL_RUNTIME_LIBS}")
  fi
fi

at_docker_build "$(at_repo_root)" imx8plus compiler:imx8plus build/imx8plus -- \
  "${extra_cmake[@]}"

if [[ "${ENABLE_HMAP}" == "ON" && "${HMAP_INSTALL_RUNTIME_LIBS}" != "ON" && "${HMAP_INSTALL_RUNTIME_LIBS}" != "1" ]]; then
  cleanup_hmap_runtime_release_libs
fi
