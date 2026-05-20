#!/usr/bin/env bash
# nova (aarch32) 交叉编译 → release/AT_v<VERSION>/nova/
#
# 环境变量: ENABLE_AT_RUNNER(默认OFF)  BUILD_JOBS(16)  CLEAN_BUILD(1)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

GCC=/opt/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf/bin

at_docker_build "$(at_repo_root)" nova compiler:nova build/nova -- \
  -DCMAKE_C_COMPILER="${GCC}/arm-none-linux-gnueabihf-gcc" \
  -DCMAKE_CXX_COMPILER="${GCC}/arm-none-linux-gnueabihf-g++" \
  -DENABLE_AT_RUNNER="${ENABLE_AT_RUNNER:-OFF}" \
  -DENABLE_CORE_TEST=OFF
