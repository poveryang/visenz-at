#!/usr/bin/env bash
# imx8plus 交叉编译 → release/AT_v<VERSION>/imx8plus/（版本见 CMakeLists project VERSION）
#
# 环境变量:
#   ENABLE_AT_RUNNER   默认 ON（设备 runner）
#   SMORE_CAM_CAP_SDK_HOST  默认 ~/Projects/smore-cam-cap/release/vs1000p_2mp
#   BUILD_JOBS(16)  CLEAN_BUILD(1)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

GCC=/opt/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin
ENABLE_AT_RUNNER="${ENABLE_AT_RUNNER:-ON}"
CAMCAP_HOST="${SMORE_CAM_CAP_SDK_HOST:-${HOME}/Projects/smore-cam-cap/release/vs1000p_2mp}"

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
fi

at_docker_build "$(at_repo_root)" imx8plus compiler:imx8plus build/imx8plus -- \
  "${extra_cmake[@]}"
