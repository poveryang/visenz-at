#!/usr/bin/env bash
# 依次交叉编译各平台
#
# 环境变量:
#   CONTINUE_ON_ERROR  默认 1，失败仍继续下一平台
#   CLEAN_BUILD        默认 1，传给各 build_* 脚本
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPORT_DIR="${REPO_ROOT}/captures/build_all"
mkdir -p "${REPORT_DIR}"

CONTINUE_ON_ERROR="${CONTINUE_ON_ERROR:-1}"
FAILED=()
PASSED=()

log() { echo "[build_all] $*"; }

log "AT version $(at_read_version "${REPO_ROOT}") → $(at_release_root "${REPO_ROOT}")"

run_build() {
  local name="$1" script="$2"
  local logfile="${REPORT_DIR}/build_${name}.log"
  log "building ${name} -> ${logfile}"
  if ( set -o pipefail; "${script}" 2>&1 | tee "${logfile}" ); then
    PASSED+=("${name}")
    log "OK ${name}"
  else
    FAILED+=("${name}")
    log "FAIL ${name} (see ${logfile})"
    [[ "${CONTINUE_ON_ERROR}" == "1" ]] || exit 1
  fi
}

BUILDS=(
  "imx8plus:${SCRIPT_DIR}/build_imx8plus_in_docker.sh"
  "imx8mini:${SCRIPT_DIR}/build_imx8mini_in_docker.sh"
  "nova:${SCRIPT_DIR}/build_nova_in_docker.sh"
  "rknn:${SCRIPT_DIR}/build_rknn_in_docker.sh"
)

for entry in "${BUILDS[@]}"; do
  name="${entry%%:*}"
  script="${entry#*:}"
  chmod +x "${script}"
  run_build "${name}" "${script}"
done

SUMMARY="${REPORT_DIR}/summary.md"
{
  echo "# build_all 结果"
  echo ""
  echo "版本: $(at_read_version "${REPO_ROOT}")"
  echo "时间: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
  echo ""
  echo "## 成功 (${#PASSED[@]})"
  for p in "${PASSED[@]}"; do echo "- ${p}"; done
  echo ""
  echo "## 失败 (${#FAILED[@]})"
  if [[ ${#FAILED[@]} -eq 0 ]]; then
    echo "- (无)"
  else
    for p in "${FAILED[@]}"; do echo "- ${p} → build_${p}.log"; done
  fi
} > "${SUMMARY}"

log "summary: ${SUMMARY}"
if [[ ${#FAILED[@]} -gt 0 ]]; then
  exit 1
fi
