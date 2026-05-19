#!/usr/bin/env bash
# 供 build_*_in_docker.sh source，不要直接执行。
set -euo pipefail

# shellcheck source=at_version.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/at_version.sh"

at_repo_root() {
  local script_dir
  script_dir="$(cd "$(dirname "${BASH_SOURCE[1]}")" && pwd)"
  realpath "${script_dir}/.."
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
