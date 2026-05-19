#!/usr/bin/env bash
# 从 CMakeLists.txt 的 project(AT VERSION ...) 读取版本号

at_read_version() {
  local root="${1:-}"
  if [[ -z "${root}" ]]; then
    local script_dir
    script_dir="$(cd "$(dirname "${BASH_SOURCE[1]}")" && pwd)"
    root="$(cd "${script_dir}/.." && pwd)"
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
