#!/usr/bin/env bash
# 备份设备 /usr/lib/libAT.so 并上传 release 中的产品 drop-in 动态库。
# 不启动 SmoreScanner；由人工验证上位机。
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../build/common.sh
source "${SCRIPT_DIR}/../build/common.sh"

REPO="$(cd "${SCRIPT_DIR}/../.." && pwd)"
AT_DEVICE_CONFIG="${AT_DEVICE_CONFIG:-${SCRIPT_DIR}/device.env}"

DEVICE_HOST="${DEVICE_HOST:-}"
DEVICE_SSH_PORT="${DEVICE_SSH_PORT:-22}"
DEVICE_USER="${DEVICE_USER:-root}"
DEVICE_PASSWORD="${DEVICE_PASSWORD:-}"
PLATFORM="${PLATFORM:-imx8plus}"

if [[ -f "${AT_DEVICE_CONFIG}" ]]; then
  # shellcheck source=/dev/null
  set -a && source "${AT_DEVICE_CONFIG}" && set +a
fi

if [[ -z "${DEVICE_HOST}" ]]; then
  echo "DEVICE_HOST 未设置（请配置 scripts/device/device.env）" >&2
  exit 1
fi

REL="$(at_release_dir "${REPO}" "${PLATFORM}")"
LIB_SRC="${REL}/lib/libAT.so"
if [[ ! -f "${LIB_SRC}" || -L "${LIB_SRC}" ]]; then
  echo "需要实体文件 ${LIB_SRC}（非软链）。请先: ENABLE_AT_SHARED=ON ./scripts/build/imx8plus.sh" >&2
  ls -la "${REL}/lib"/libAT.so* 2>/dev/null || true
  exit 1
fi

run_with_password() {
  if [[ -z "${DEVICE_PASSWORD}" ]]; then
    "$@"
    return
  fi
  if ! command -v expect >/dev/null 2>&1; then
    echo "已设置 DEVICE_PASSWORD 但未安装 expect" >&2
    exit 1
  fi
  export AT_EXPECT_CMD
  AT_EXPECT_CMD=$(printf '%q ' "$@")
  export AT_EXPECT_PASSWORD="${DEVICE_PASSWORD}"
  expect <<'EXPECT_EOF'
set timeout 180
log_user 1
spawn bash -c $env(AT_EXPECT_CMD)
expect {
  -re "(?i)password:" {
    send "$env(AT_EXPECT_PASSWORD)\r"
    exp_continue
  }
  eof
}
catch wait result
exit [lindex $result 3]
EXPECT_EOF
}

remote_ssh() {
  run_with_password ssh -o StrictHostKeyChecking=no -p "${DEVICE_SSH_PORT}" \
    "${DEVICE_USER}@${DEVICE_HOST}" "$1"
}

remote_scp() {
  run_with_password scp -o StrictHostKeyChecking=no -P "${DEVICE_SSH_PORT}" -O \
    "$1" "${DEVICE_USER}@${DEVICE_HOST}:$2"
}

TS="$(date +%Y%m%d_%H%M%S)"
# 根分区很小：实体库放到 /usr/scanner（独立分区），/usr/lib/libAT.so 仅软链
REMOTE_LIB_REAL="${REMOTE_LIB_REAL:-/usr/scanner/lib/libAT.so}"
BAK="/tmp/libAT.so.bak.${TS}"
TMP="/tmp/libAT.so.new.${TS}"

echo "local:  ${LIB_SRC}"
echo "remote: ${DEVICE_USER}@${DEVICE_HOST}:${DEVICE_SSH_PORT}"
echo "real:   ${REMOTE_LIB_REAL}"
echo "backup: ${BAK}"

echo "=== device probe ==="
remote_ssh "ls -la /usr/lib/libAT.so* ${REMOTE_LIB_REAL} 2>/dev/null || true; echo ---; (pgrep -a SmoreScanner || echo 'no_SmoreScanner'); echo ---; df -h / /usr/scanner 2>/dev/null | grep -v '^Filesystem' || df -h"

echo "=== backup ==="
remote_ssh "mkdir -p '$(dirname "${REMOTE_LIB_REAL}")' /tmp/visenz_at; if [ -e /usr/lib/libAT.so ]; then cp -aL /usr/lib/libAT.so '${BAK}'; elif [ -e '${REMOTE_LIB_REAL}' ]; then cp -a '${REMOTE_LIB_REAL}' '${BAK}'; fi; ls -la '${BAK}' 2>/dev/null || echo 'no previous lib'"

echo "=== upload ==="
remote_scp "${LIB_SRC}" "${TMP}"
# 实体写入 /usr/scanner/lib；/usr/lib/libAT.so 改为软链，避免根分区空间不足
remote_ssh "mkdir -p '$(dirname "${REMOTE_LIB_REAL}")'; \
  rm -f /usr/lib/libAT.so /usr/lib/libAT.so.[0-9]* /usr/lib/libAT.so.bak.*; \
  cp -f '${TMP}' '${REMOTE_LIB_REAL}' && chmod 0755 '${REMOTE_LIB_REAL}' && rm -f '${TMP}'; \
  ln -sf '${REMOTE_LIB_REAL}' /usr/lib/libAT.so; \
  ls -la /usr/lib/libAT.so '${REMOTE_LIB_REAL}' '${BAK}'; \
  (command -v ldconfig >/dev/null && ldconfig || true); \
  echo OK"

echo "done. 请重启 SmoreScanner 后触发一次 AT，然后查看算法日志："
echo "  cat /usr/scanner/Log/at/latest_init.txt"
echo "  cat \"\$(cat /usr/scanner/Log/at/latest.path)\""
echo "回滚: ssh ${DEVICE_USER}@${DEVICE_HOST} \"cp -a ${BAK} ${REMOTE_LIB_REAL}\""
echo "说明: docs/architecture/AT_RUN_LOG.md"
