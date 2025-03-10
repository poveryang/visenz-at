#!/bin/bash

# Get the directory of target project
script_dir=$(dirname "$0")
project_dir=$(realpath "$script_dir/..")
echo "Project directory: $project_dir"

# Clear the build directory
rm -rf "$project_dir/build/nova"

# Build the docker image
container_id=$(docker run -it \
    --platform linux/amd64 \
    -d \
    -v "$project_dir":/workspace \
    compiler:nova)

# Build the project in the docker container
docker exec "$container_id" bash -c "
    /usr/bin/cmake \
        -DPLATFORM='nova' \
        -DCMAKE_C_COMPILER='/opt/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc' \
        -DCMAKE_CXX_COMPILER='/opt/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-g++' \
        -S /workspace \
        -B /workspace/build/nova &&
    /usr/bin/cmake \
        --build /workspace/build/nova \
        --target install \
        -- -j 16
"
# Stop and remove the docker container
docker stop "$container_id"
docker rm "$container_id"