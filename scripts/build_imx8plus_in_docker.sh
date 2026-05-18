#!/bin/bash
set -euo pipefail

# Get the directory of target project
script_dir=$(cd "$(dirname "$0")" && pwd)
project_dir=$(realpath "$script_dir/..")
sdk_host_root=${SMORE_CAM_CAP_SDK_HOST_ROOT:-/Users/yjunj/Projects/smore-cam-cap/release/vs1000p_2mp}
sdk_container_root=${SMORE_CAM_CAP_SDK_CONTAINER_ROOT:-/opt/smore-cam-cap-sdk}
enable_at_device_runner=${ENABLE_AT_DEVICE_RUNNER:-ON}

echo "Project directory: $project_dir"
echo "Enable AT device runner: $enable_at_device_runner"

docker_mounts=(-v "$project_dir":/workspace)
cmake_runner_args=(-DENABLE_AT_DEVICE_RUNNER="$enable_at_device_runner")

if [[ "$enable_at_device_runner" == "ON" ]]; then
    echo "smore-cam-cap SDK: $sdk_host_root"
    if [[ ! -f "$sdk_host_root/include/camcap/camcap.h" ]]; then
        echo "Missing SDK header: $sdk_host_root/include/camcap/camcap.h" >&2
        exit 1
    fi
    if [[ ! -f "$sdk_host_root/lib/libcamcap.a" ]]; then
        echo "Missing SDK library: $sdk_host_root/lib/libcamcap.a" >&2
        exit 1
    fi
    if [[ ! -f "$sdk_host_root/lib/libcamcap_opencv_adapter.a" ]]; then
        echo "Missing SDK library: $sdk_host_root/lib/libcamcap_opencv_adapter.a" >&2
        exit 1
    fi

    docker_mounts+=(-v "$sdk_host_root:$sdk_container_root:ro")
    cmake_runner_args+=(-DSMORE_CAM_CAP_SDK_ROOT="$sdk_container_root")
fi

# Clear the build directory
rm -rf "$project_dir/build/imx8plus"

# Start the docker container
container_id=$(docker run -it \
    --platform linux/amd64 \
    -d \
    "${docker_mounts[@]}" \
    compiler:imx8plus \
    sleep infinity)

cleanup() {
    docker stop "$container_id" >/dev/null || true
    docker rm "$container_id" >/dev/null || true
}
trap cleanup EXIT

# Build the project in the docker container
docker exec "$container_id" \
    /usr/bin/cmake \
        -DPLATFORM=imx8plus \
        -DCMAKE_C_COMPILER=/opt/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc \
        -DCMAKE_CXX_COMPILER=/opt/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++ \
        "${cmake_runner_args[@]}" \
        -S /workspace \
        -B /workspace/build/imx8plus

docker exec "$container_id" \
    /usr/bin/cmake \
        --build /workspace/build/imx8plus \
        --target install \
        -- -j 16
