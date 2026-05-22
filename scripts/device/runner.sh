#!/usr/bin/env bash
# 设备端 at_device_runner：读 device.env → 上传部署 → 启动 → 上位机协议冒烟
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=../build/common.sh
source "${SCRIPT_DIR}/../build/common.sh"

REPO="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# 默认值（可被 device.env 与环境变量覆盖）
PLATFORM="${PLATFORM:-}"
DEVICE_HOST="${DEVICE_HOST:-}"
DEVICE_SSH_PORT="${DEVICE_SSH_PORT:-}"
DEVICE_USER="${DEVICE_USER:-root}"
DEVICE_PASSWORD="${DEVICE_PASSWORD:-}"
REMOTE_BIN_DIR="${REMOTE_BIN_DIR:-/tmp/at_runner}"
REMOTE_LIB_DIR="${REMOTE_LIB_DIR:-/tmp/at_runner/lib}"
REMOTE_MODEL_DIR="${REMOTE_MODEL_DIR:-/tmp/at_runner/model}"
RUNNER_BIN_NAME="${RUNNER_BIN_NAME:-at_device_runner}"
REMOTE_LD_LIBRARY_PATH="${REMOTE_LD_LIBRARY_PATH:-}"
RUNNER_DEVICE="${RUNNER_DEVICE:-}"
RUNNER_PORT="${RUNNER_PORT:-8080}"
RUNNER_HEATMAP_MODEL="${RUNNER_HEATMAP_MODEL:-auto}"
RUNNER_HEATMAP_CONTEXT="${RUNNER_HEATMAP_CONTEXT:-timvx}"
RUNNER_HEATMAP_PRECISION="${RUNNER_HEATMAP_PRECISION:-uint8}"
RUNNER_HEATMAP_THRESHOLD="${RUNNER_HEATMAP_THRESHOLD:-40}"
RUNNER_LOG="${RUNNER_LOG:-/tmp/at_runner.log}"
DEPLOY_SHARED_LIBS="${DEPLOY_SHARED_LIBS:-auto}"
DEPLOY_HMAP_RUNTIME_LIBS="${DEPLOY_HMAP_RUNTIME_LIBS:-0}"
DEPLOY_EXTRA="${DEPLOY_EXTRA:-}"
SMOKE_STEPS="${SMOKE_STEPS:-10}"
SMOKE_HOST="${SMOKE_HOST:-}"
SMOKE_PORT="${SMOKE_PORT:-}"
AT_DEVICE_CONFIG="${AT_DEVICE_CONFIG:-${SCRIPT_DIR}/device.env}"
DEPLOY_MANIFEST="${DEPLOY_MANIFEST:-${SCRIPT_DIR}/deploy.manifest}"

load_config() {
  if [[ -f "${AT_DEVICE_CONFIG}" ]]; then
    # shellcheck source=/dev/null
    set -a && source "${AT_DEVICE_CONFIG}" && set +a
  fi
  # 兼容旧变量名：DEVICE_PORT 表示 SSH 端口
  if [[ -n "${DEVICE_PORT:-}" && -z "${DEVICE_SSH_PORT:-}" ]]; then
    DEVICE_SSH_PORT="${DEVICE_PORT}"
  fi
  if [[ -z "${REMOTE_LD_LIBRARY_PATH}" && -n "${REMOTE_LIB_DIR}" ]]; then
    REMOTE_LD_LIBRARY_PATH="${REMOTE_LIB_DIR}"
  fi
}

release_dir() {
  if [[ -n "${RELEASE_DIR:-}" ]]; then
    echo "${REPO}/${RELEASE_DIR#"${REPO}/"}"
  else
    at_release_dir "${REPO}" "${PLATFORM}"
  fi
}

usage() {
  cat <<EOF
用法: $(basename "$0") <deploy|start|stop|test|all|show|log>

  deploy  上传 runner（及可选 .so）并在设备上启动 --server
  start   仅启动 runner（不上传，用于重启）
  stop    SSH 到设备结束 runner 进程
  test    在上位机用 TCP 访问 DEVICE_HOST:RUNNER_PORT 做协议冒烟（不走 SSH）
  all     deploy → test → stop
  show    打印当前配置与待上传文件列表
  log     打印设备端 runner 进程、端口和日志尾部

配置: \${AT_DEVICE_CONFIG}（默认 scripts/device/device.env）
示例: cp scripts/device/device.env.example scripts/device/device.env

环境变量可覆盖配置文件中的任意项。
EOF
}

run_with_password() {
  if [[ -z "${DEVICE_PASSWORD}" ]]; then
    "$@"
    return
  fi
  if ! command -v expect >/dev/null 2>&1; then
    echo "已设置 DEVICE_PASSWORD 但未安装 expect: brew install expect" >&2
    exit 1
  fi
  # macOS 的 expect -c 不设置 $argv，用环境变量传命令更可靠
  export AT_EXPECT_CMD
  AT_EXPECT_CMD=$(printf '%q ' "$@")
  export AT_EXPECT_PASSWORD="${DEVICE_PASSWORD}"
  expect <<'EXPECT_EOF'
set timeout 120
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

remote_scp() {
  run_with_password scp -P "${DEVICE_SSH_PORT}" -O "$1" "${DEVICE_USER}@${DEVICE_HOST}:$2"
}

remote_ssh() {
  run_with_password ssh -p "${DEVICE_SSH_PORT}" "${DEVICE_USER}@${DEVICE_HOST}" "$1"
}

runner_remote_path() {
  echo "${REMOTE_BIN_DIR%/}/${RUNNER_BIN_NAME}"
}

# 在设备上结束 runner（仅匹配 device.env 中的安装路径）
remote_kill_runner_shell() {
  local runner="$1"
  local verify_port="${2:-1}"
cat <<EOF
pkill -f '${runner}' 2>/dev/null || true
ps 2>/dev/null | grep '${runner}' | grep -v grep | awk '{print \$1}' | xargs -r kill -9 2>/dev/null || true
sleep 1
EOF
  if [[ "${verify_port}" == "1" ]]; then
    cat <<EOF
if command -v ss >/dev/null 2>&1; then
  if ss -tln 2>/dev/null | grep -q ':${RUNNER_PORT} '; then
    echo "WARN: port ${RUNNER_PORT} still listening:"
    ss -tlnp 2>/dev/null | grep ':${RUNNER_PORT} ' || true
    exit 1
  fi
fi
echo "runner stopped, port ${RUNNER_PORT} free"
EOF
  fi
}

remote_prepare_runtime_libs_shell() {
  cat <<EOF
if [ -d '${REMOTE_LIB_DIR}' ]; then
  for f in '${REMOTE_LIB_DIR}'/*.so; do
    [ -e "\$f" ] || continue
    ln -sf "\$(basename "\$f")" "\$f.1" 2>/dev/null || true
  done
fi
EOF
}

is_hmap_runtime_lib() {
  case "$(basename "$1")" in
    libtengine-lite.so|libOpenVX.so|libOpenVXU.so|libGAL.so|libVSC.so|\
libArchModelSw.so|libCLC.so|libNNArchPerf.so)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

remote_cleanup_hmap_runtime_libs_shell() {
  [[ "${DEPLOY_HMAP_RUNTIME_LIBS}" != "1" ]] || return 0
  cat <<EOF
if [ -d '${REMOTE_LIB_DIR}' ]; then
  rm -f '${REMOTE_LIB_DIR}'/libtengine-lite.so '${REMOTE_LIB_DIR}'/libtengine-lite.so.1
  rm -f '${REMOTE_LIB_DIR}'/libOpenVX.so '${REMOTE_LIB_DIR}'/libOpenVX.so.1
  rm -f '${REMOTE_LIB_DIR}'/libOpenVXU.so '${REMOTE_LIB_DIR}'/libOpenVXU.so.1
  rm -f '${REMOTE_LIB_DIR}'/libGAL.so '${REMOTE_LIB_DIR}'/libGAL.so.1
  rm -f '${REMOTE_LIB_DIR}'/libVSC.so '${REMOTE_LIB_DIR}'/libVSC.so.1
  rm -f '${REMOTE_LIB_DIR}'/libArchModelSw.so '${REMOTE_LIB_DIR}'/libArchModelSw.so.1
  rm -f '${REMOTE_LIB_DIR}'/libCLC.so '${REMOTE_LIB_DIR}'/libCLC.so.1
  rm -f '${REMOTE_LIB_DIR}'/libNNArchPerf.so '${REMOTE_LIB_DIR}'/libNNArchPerf.so.1
fi
EOF
}

try_graceful_shutdown() {
  AT_STOP_HOST="${DEVICE_HOST}" AT_STOP_PORT="${RUNNER_PORT}" \
    python3 - "${REPO}" <<'PY' 2>/dev/null || true
import os, sys
repo = sys.argv[1]
sys.path.insert(0, os.path.join(repo, "tools", "at_capture"))
from capture_client import CaptureServiceClient
host = os.environ["AT_STOP_HOST"]
port = int(os.environ["AT_STOP_PORT"])
c = CaptureServiceClient(host=host, port=port, timeout=5.0)
try:
    c.connect()
    c.request({"command": "shutdown"})
finally:
    c.close()
PY
}

# 输出多行：本地绝对路径:设备绝对路径
plan_deploy_pairs() {
  local rel="${1:?}"
  local root="${2:?}"
  local local_path="${root}/${rel}"
  local remote_path

  case "${rel}" in
    bin/*)
      if [[ "$(basename "${rel}")" == "${RUNNER_BIN_NAME}" ]]; then
        remote_path="$(runner_remote_path)"
      else
        remote_path="${REMOTE_BIN_DIR%/}/$(basename "${rel}")"
      fi
      ;;
    lib/*)
      remote_path="${REMOTE_LIB_DIR%/}/$(basename "${rel}")"
      ;;
    model/*)
      remote_path="${REMOTE_MODEL_DIR%/}/$(basename "${rel}")"
      ;;
    *)
      if [[ "${rel}" == *:* ]]; then
        local_path="${root}/${rel%%:*}"
        remote_path="${rel#*:}"
      else
        echo "跳过无法解析的部署项: ${rel}" >&2
        return 0
      fi
      ;;
  esac

  if [[ ! -e "${local_path}" ]]; then
    echo "缺失: ${local_path}" >&2
    return 1
  fi
  echo "${local_path}:${remote_path}"
}

collect_deploy_pairs() {
  local root
  root="$(release_dir)"
  local pairs=()

  pairs+=("bin/${RUNNER_BIN_NAME}")

  if [[ "${DEPLOY_SHARED_LIBS}" == "1" || "${DEPLOY_SHARED_LIBS}" == "auto" ]]; then
    shopt -s nullglob
    local so
    for so in "${root}"/lib/*.so; do
      if [[ "${DEPLOY_HMAP_RUNTIME_LIBS}" != "1" ]] && is_hmap_runtime_lib "${so}"; then
        continue
      fi
      pairs+=("lib/$(basename "${so}")")
    done
    shopt -u nullglob
  fi

  shopt -s nullglob
  local model
  for model in "${root}"/model/*.tmfile; do
    pairs+=("model/$(basename "${model}")")
  done
  shopt -u nullglob

  if [[ -f "${DEPLOY_MANIFEST}" ]]; then
    while IFS= read -r line || [[ -n "${line}" ]]; do
      line="${line%%#*}"
      line="$(echo "${line}" | xargs 2>/dev/null || true)"
      [[ -z "${line}" ]] && continue
      pairs+=("${line}")
    done < "${DEPLOY_MANIFEST}"
  fi

  if [[ -n "${DEPLOY_EXTRA}" ]]; then
    local IFS=,
    read -ra extras <<< "${DEPLOY_EXTRA}"
    local item
    for item in "${extras[@]}"; do
      item="$(echo "${item}" | xargs)"
      [[ -n "${item}" ]] && pairs+=("${item}")
    done
  fi

  local rel pair
  for rel in "${pairs[@]}"; do
    plan_deploy_pairs "${rel}" "${root}" || return 1
  done
}

cmd_show() {
  load_config
  local root
  root="$(release_dir)"
  echo "config:     ${AT_DEVICE_CONFIG}"
  echo "release:    ${root}"
  echo "ssh:        ${DEVICE_USER}@${DEVICE_HOST}:${DEVICE_SSH_PORT}"
  echo "runner:     $(runner_remote_path)  device=${RUNNER_DEVICE}  port=${RUNNER_PORT}"
  echo "heatmap:    model=${RUNNER_HEATMAP_MODEL}  context=${RUNNER_HEATMAP_CONTEXT}  precision=${RUNNER_HEATMAP_PRECISION}  threshold=${RUNNER_HEATMAP_THRESHOLD}"
  echo "runtime:    deploy_hmap_runtime_libs=${DEPLOY_HMAP_RUNTIME_LIBS}"
  echo "smoke:      host=${SMOKE_HOST:-${DEVICE_HOST}}  port=${SMOKE_PORT:-${RUNNER_PORT}}"
  echo "deploy list:"
  collect_deploy_pairs | sed 's/^/  /'
}

cmd_log() {
  load_config
  local runner
  runner="$(runner_remote_path)"
  remote_ssh "echo '--- runner process ---'; \
ps 2>/dev/null | grep '${runner}' | grep -v grep || true; \
echo '--- listening port ---'; \
if command -v ss >/dev/null 2>&1; then ss -tlnp 2>/dev/null | grep ':${RUNNER_PORT} ' || true; else netstat -tlnp 2>/dev/null | grep ':${RUNNER_PORT} ' || true; fi; \
echo '--- runtime libs ---'; \
ls -l '${REMOTE_LIB_DIR}' 2>/dev/null | grep -E 'OpenVX|tengine|GAL|VSC' || true; \
echo '--- ldd runner ---'; \
LD_LIBRARY_PATH='${REMOTE_LIB_DIR}' ldd '${runner}' 2>/dev/null | grep -E 'tengine|OpenVX|GAL|VSC|CLC|ArchModel|NNArch|opencv|stdc' || true; \
echo '--- runner log ---'; \
tail -120 '${RUNNER_LOG}' 2>/dev/null || true"
}

cmd_upload() {
  local root
  root="$(release_dir)"
  if [[ ! -d "${root}" ]]; then
    echo "release 目录不存在: ${root}" >&2
    echo "请先编译: ./scripts/build/${PLATFORM}.sh 或设置 RELEASE_DIR" >&2
    exit 1
  fi

  echo "mkdir on device: ${REMOTE_BIN_DIR} ${REMOTE_LIB_DIR} ${REMOTE_MODEL_DIR}"
  remote_ssh "mkdir -p '${REMOTE_BIN_DIR}' '${REMOTE_LIB_DIR}' '${REMOTE_MODEL_DIR}'"
  remote_ssh "$(remote_cleanup_hmap_runtime_libs_shell)"

  local pair local_path remote_path
  while IFS= read -r pair; do
    [[ -z "${pair}" ]] && continue
    local_path="${pair%%:*}"
    remote_path="${pair#*:}"
    echo "upload ${local_path} -> ${DEVICE_HOST}:${remote_path}"
    remote_scp "${local_path}" "${remote_path}"
  done < <(collect_deploy_pairs)

  echo "upload done ($(collect_deploy_pairs | wc -l | tr -d ' ') file(s))"
  remote_ssh "$(remote_prepare_runtime_libs_shell)"
}

cmd_deploy() {
  load_config
  cmd_upload
  cmd_start
}

cmd_start() {
  load_config
  local runner
  runner="$(runner_remote_path)"
  local root heatmap_arg=""
  root="$(release_dir)"
  if [[ "${RUNNER_HEATMAP_MODEL}" != "0" && "${RUNNER_HEATMAP_MODEL}" != "OFF" && "${RUNNER_HEATMAP_MODEL}" != "off" ]]; then
    local remote_model=""
    if [[ "${RUNNER_HEATMAP_MODEL}" == "auto" ]]; then
      if [[ -f "${root}/model/model-uint8.tmfile" ]]; then
        remote_model="${REMOTE_MODEL_DIR%/}/model-uint8.tmfile"
      fi
    elif [[ -n "${RUNNER_HEATMAP_MODEL}" ]]; then
      remote_model="${RUNNER_HEATMAP_MODEL}"
    fi
    if [[ -n "${remote_model}" ]]; then
      heatmap_arg=" --heatmap-model '${remote_model}' --heatmap-context '${RUNNER_HEATMAP_CONTEXT}' --heatmap-precision '${RUNNER_HEATMAP_PRECISION}' --heatmap-threshold '${RUNNER_HEATMAP_THRESHOLD}'"
    fi
  fi
  local ld=""
  if [[ -n "${REMOTE_LD_LIBRARY_PATH}" ]]; then
    ld="export LD_LIBRARY_PATH='${REMOTE_LD_LIBRARY_PATH}'; "
  fi

  echo "start ${runner} on ${DEVICE_HOST}:${RUNNER_PORT} (device background, survives SSH exit)"
  remote_ssh "${ld}$(remote_kill_runner_shell "${runner}" 0); \
$(remote_cleanup_hmap_runtime_libs_shell); \
$(remote_prepare_runtime_libs_shell); \
chmod +x '${runner}' 2>/dev/null || true; \
nohup '${runner}' --server --device '${RUNNER_DEVICE}' --port '${RUNNER_PORT}' \
  ${heatmap_arg} \
  >'${RUNNER_LOG}' 2>&1 </dev/null & sleep 2; tail -5 '${RUNNER_LOG}'"
}

cmd_stop() {
  load_config
  local runner
  runner="$(runner_remote_path)"
  echo "stop runner on ${DEVICE_HOST}:${RUNNER_PORT}"
  echo "  1) 协议 shutdown（若服务仍响应）"
  try_graceful_shutdown
  echo "  2) SSH 结束进程并检查端口"
  remote_ssh "$(remote_kill_runner_shell "${runner}" 1)"
}

cmd_test() {
  load_config
  local host="${SMOKE_HOST:-${DEVICE_HOST}}"
  local port="${SMOKE_PORT:-${RUNNER_PORT}}"
  echo "smoke test (local TCP) -> ${host}:${port}  steps=${SMOKE_STEPS}"
  echo "  （上位机直连 runner 协议端口，不经过 SSH）"
  AT_SMOKE_HOST="${host}" AT_SMOKE_PORT="${port}" SMOKE_STEPS="${SMOKE_STEPS}" \
    python3 - "${REPO}" <<'PY'
import os
import sys

repo = sys.argv[1]
sys.path.insert(0, os.path.join(repo, "tools", "at_capture"))
from capture_client import CameraParams, CaptureServiceClient  # noqa: E402

host = os.environ["AT_SMOKE_HOST"]
port = int(os.environ["AT_SMOKE_PORT"])
steps = int(os.environ["SMOKE_STEPS"])

client = CaptureServiceClient(host=host, port=port, timeout=30.0)
try:
    client.connect()
    status = client.get_status()
    if not status.get("opened"):
        print("FAIL: camera not opened", status, file=sys.stderr)
        raise SystemExit(1)
    print("OK get_status", status.get("device"), status.get("width"), "x", status.get("height"))

    at_state = client.reset_at(CameraParams())
    print("OK reset_at", at_state)

    for i in range(steps):
        frame = client.at_step()
        trace = frame.trace or {}
        at_info = frame.at or {}
        heatmap = trace.get("heatmap") or {}
        heatmap_perf = trace.get("heatmap_perf") or {}
        print(
            f"OK at_step[{i}] step={trace.get('step')} "
            f"action={trace.get('action')} finished={at_info.get('finished')} "
            f"heatmap={heatmap.get('available')} "
            f"conf={heatmap.get('confidence')} source={heatmap.get('source')} "
            f"infer_ms={heatmap_perf.get('last_model_ms')} "
            f"avg_ms={heatmap_perf.get('avg_model_ms')} "
            f"avg_total_ms={heatmap_perf.get('avg_total_ms')} "
            f"init_ms={heatmap_perf.get('init_ms')}"
        )
        if at_info.get("finished"):
            break

    client.request({"command": "shutdown"})
    print("PASS smoke test")
except Exception as exc:
    print(f"FAIL: {exc}", file=sys.stderr)
    raise SystemExit(1) from exc
finally:
    client.close()
PY
}

main() {
  local action="${1:-}"
  case "${action}" in
    deploy) cmd_deploy ;;
    start)
      load_config
      cmd_start
      ;;
    stop) cmd_stop ;;
    test) cmd_test ;;
    all)
      cmd_deploy
      cmd_test
      cmd_stop
      ;;
    show) cmd_show ;;
    log) cmd_log ;;
    -h|--help|help|"")
      usage
      [[ -z "${action}" ]] && exit 0
      ;;
    *)
      echo "未知子命令: ${action}" >&2
      usage >&2
      exit 2
      ;;
  esac
}

main "$@"
