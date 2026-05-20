#!/usr/bin/env bash
# rknn 交叉编译 → release/AT_v<VERSION>/rknn/
#
# 环境变量: ENABLE_AT_RUNNER(默认OFF)  BUILD_JOBS(16)  CLEAN_BUILD(1)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

GCC=/opt/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin

at_docker_build "$(at_repo_root)" rknn compiler:imx8plus build/rknn -- \
  -DCMAKE_C_COMPILER="${GCC}/aarch64-none-linux-gnu-gcc" \
  -DCMAKE_CXX_COMPILER="${GCC}/aarch64-none-linux-gnu-g++" \
  -DENABLE_AT_RUNNER="${ENABLE_AT_RUNNER:-OFF}" \
  -DENABLE_CORE_TEST=OFF
