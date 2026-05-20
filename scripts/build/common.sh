#!/usr/bin/env bash
# scripts/build 与 scripts/device 共用：版本解析、仓库路径、Docker 交叉编译。
# 用法: source "$(dirname "$0")/common.sh"  或  source "${SCRIPT_DIR}/common.sh"
set -euo pipefail

at_read_version() {
  local root="${1:-}"
  if [[ -z "${root}" ]]; then
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[1]}")" && pwd)"
    root="$(cd "${script_dir}/../.." && pwd)"
  fi
  sed -n 's/^project(AT VERSION //p' "${root}/CMakeLists.txt" | tr -d ' )'
}

at_release_root() {
  local repo_root="$1"
  echo "${repo_root}/release/AT_v$(at_read_version "${repo_root}")"
}

at_release_dir() {
  local repo_root="$1" platform="$2"
  echo "$(at_release_root "${repo_root}")/${platform}"
}

at_repo_root() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[1]}")" && pwd)"
  realpath "${script_dir}/../.."
}

at_container_cleanup() {
  [[ -n "${AT_CONTAINER_ID:-}" ]] || return 0
  docker stop "${AT_CONTAINER_ID}" >/dev/null 2>&1 || true
  docker rm "${AT_CONTAINER_ID}" >/dev/null 2>&1 || true
}

# Docker 内 cmake configure + build + install
# 参数: <repo_root> <platform> <image> <build_subdir> [--setup 'shell'] [--] [cmake 参数...]
at_docker_build() {
  local repo_root="$1" platform="$2" image="$3" build_subdir="$4"
  shift 4

  local setup_cmd=""
  if [[ "${1:-}" == "--setup" ]]; then
    shift
    setup_cmd="$1"
    shift
  fi
  [[ "${1:-}" == "--" ]] && shift
  local extra_cmake=("$@")

  local build_dir="/workspace/${build_subdir}"
  local install_dir
  install_dir="$(at_release_dir "${repo_root}" "${platform}")"
  local jobs="${BUILD_JOBS:-16}"

  echo "repo:     ${repo_root}"
  echo "version:  $(at_read_version "${repo_root}")"
  echo "platform: ${platform}"
  echo "build:    ${build_dir}"
  echo "install:  ${install_dir}"

  if [[ "${CLEAN_BUILD:-1}" == "1" ]]; then
    rm -rf "${repo_root}/${build_subdir}"
  fi

  local docker_volumes=(-v "${repo_root}:/workspace")
  if [[ -n "${SMORE_CAM_CAP_SDK_HOST:-}" ]]; then
    docker_volumes+=(-v "${SMORE_CAM_CAP_SDK_HOST}:/smore_cam_cap:ro")
    echo "camcap sdk: ${SMORE_CAM_CAP_SDK_HOST} -> /smore_cam_cap"
  fi

  AT_CONTAINER_ID="$(
    docker run -d --platform linux/amd64 \
      "${docker_volumes[@]}" \
      "${image}" sleep infinity
  )"
  trap at_container_cleanup EXIT

  local configure=(
    /usr/bin/cmake
    -DPLATFORM="${platform}"
    "${extra_cmake[@]}"
    -S /workspace
    -B "${build_dir}"
  )
  local build_install=(
    /usr/bin/cmake
    --build "${build_dir}"
    --target install
    --
    -j "${jobs}"
  )

  if [[ -n "${setup_cmd}" ]]; then
    docker exec "${AT_CONTAINER_ID}" bash -lc "${setup_cmd} && ${configure[*]} && ${build_install[*]}"
  else
    docker exec "${AT_CONTAINER_ID}" "${configure[@]}"
    docker exec "${AT_CONTAINER_ID}" "${build_install[@]}"
  fi

  echo "done: ${install_dir}"
}
