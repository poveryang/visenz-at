#!/bin/bash

# Get the directory of target project
script_dir=$(dirname "$0")
project_dir=$(realpath "$script_dir/..")
echo "Project directory: $project_dir"

# Clear the build directory
rm -rf "$project_dir/build/rknn"

# Build the docker image
container_id=$(docker run -it \
    --platform linux/amd64 \
    -d \
    -v "$project_dir":/workspace \
    compiler:imx8plus)

# Build the project in the docker container
docker exec "$container_id" bash -c "
    /usr/bin/cmake \
        -DPLATFORM='rknn' \
        -DCMAKE_C_COMPILER='/opt/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc' \
        -DCMAKE_CXX_COMPILER='/opt/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++' \
        -S /workspace \
        -B /workspace/build/rknn &&
    /usr/bin/cmake \
        --build /workspace/build/rknn \
        --target install \
        -- -j 16
"

# Stop and remove the docker container
docker stop "$container_id"
docker rm "$container_id"