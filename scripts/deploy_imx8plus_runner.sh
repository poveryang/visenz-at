#!/usr/bin/env bash
# 将交叉编译的 at_device_runner 部署到 imx8plus 设备 /tmp
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=at_version.sh
source "${SCRIPT_DIR}/at_version.sh"

REPO="$(cd "${SCRIPT_DIR}/.." && pwd)"
VERSION="$(at_read_version "${REPO}")"
BIN="${REPO}/release/AT_v${VERSION}/imx8plus/bin/at_device_runner"

DEVICE_HOST="${DEVICE_HOST:-10.80.184.167}"
DEVICE_PORT="${DEVICE_PORT:-201}"
DEVICE_USER="${DEVICE_USER:-root}"
REMOTE_PATH="${REMOTE_PATH:-/tmp/at_device_runner}"

if [[ ! -f "${BIN}" ]]; then
  echo "binary not found: ${BIN}" >&2
  echo "run: ENABLE_AT_RUNNER=ON ./scripts/build_imx8plus_in_docker.sh" >&2
  exit 1
fi

echo "deploy ${BIN} -> ${DEVICE_USER}@${DEVICE_HOST}:${REMOTE_PATH}"
if [[ -n "${DEVICE_PASSWORD:-}" ]]; then
  "${SCRIPT_DIR}/_ssh_with_password.exp" "${DEVICE_PASSWORD}" \
    scp -P "${DEVICE_PORT}" -O "${BIN}" "${DEVICE_USER}@${DEVICE_HOST}:${REMOTE_PATH}"
else
  scp -P "${DEVICE_PORT}" -O "${BIN}" "${DEVICE_USER}@${DEVICE_HOST}:${REMOTE_PATH}"
fi
echo "done"
