#!/bin/bash

# Set remote device ip
DEV_IP=192.168.8.21

# Set environment
target_platform="imx8plus"
script_dir=$(cd "$(dirname "$0")" || exit; pwd)
root_dir=$(cd "$script_dir/.." || exit; pwd)
build_dir=$root_dir/build-$target_platform
save_dir=$HOME/Downloads/AT-test-"$target_platform"/$(date +%m%d%H%M%S)

if [ ! -d "$save_dir" ]; then
mkdir -p "$save_dir"  && echo "build folder is created"
fi

# Scp execute file and lib to remote device
scp -O ./release/VS/imx8plus/lib/libAT.so root@$DEV_IP:/tmp
scp -O ./release/VS/imx8plus/bin/AT_TEST root@$DEV_IP:/tmp
scp -O ./modules/hmap-generator/model/hmap-v4-e200-uint8.tmfile root@$DEV_IP:/tmp

# Connect to remote device(Scanner)
ssh -tt root@$DEV_IP << remotessh
killall SmoreScanner
export LD_LIBRARY_PATH=/tmp:$LD_LIBRARY_PATH

mkdir /tmp/at_res
cd /tmp
./AT_TEST vs1000p
exit
remotessh

# Transfer images to local
scp -O -q root@$DEV_IP:/tmp/at_res/* $save_dir

# Clean remote files
ssh -tt root@$DEV_IP << remotessh
rm -r /tmp/at_res
exit
remotessh
