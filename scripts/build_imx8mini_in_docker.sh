#!/bin/bash

# Get the directory of target project
script_dir=$(dirname "$0")
project_dir=$(realpath "$script_dir/..")
echo "Project directory: $project_dir"

# Clear the build directory
rm -rf "$project_dir/build/imx8mini"

# Build the docker image
container_id=$(docker run -it \
    --platform linux/amd64 \
    -d \
    -v "$project_dir":/workspace \
    compiler:imx8mini)

# Build the project in the docker container
docker exec "$container_id" bash -c "
    source /opt/myir-imx-xwayland/4.14-sumo/environment-setup-aarch64-poky-linux &&
    /usr/bin/cmake \
        -DPLATFORM='imx8mini' \
        -S /workspace \
        -B /workspace/build/imx8mini &&
    /usr/bin/cmake \
        --build /workspace/build/imx8mini \
        --target install \
        -- -j 16
"

# Stop and remove the docker container
docker stop "$container_id"
docker rm "$container_id"