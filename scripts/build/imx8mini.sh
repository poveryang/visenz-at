#!/usr/bin/env bash
# imx8mini 交叉编译 → release/AT_v<VERSION>/imx8mini/
#
# 环境变量: ENABLE_AT_RUNNER(默认OFF)  BUILD_JOBS(16)  CLEAN_BUILD(1)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=common.sh
source "${SCRIPT_DIR}/common.sh"

SETUP='source /opt/myir-imx-xwayland/4.14-sumo/environment-setup-aarch64-poky-linux'

at_docker_build "$(at_repo_root)" imx8mini compiler:imx8mini build/imx8mini \
  --setup "${SETUP}" -- \
  -DENABLE_AT_RUNNER="${ENABLE_AT_RUNNER:-OFF}" \
  -DENABLE_CORE_TEST=OFF
