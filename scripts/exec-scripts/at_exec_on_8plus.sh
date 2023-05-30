#!/bin/bash

# Set remote device ip and port
DEV_IP=10.80.158.32
DEV_PORT=100

# Set environment
script_dir=$(cd "$(dirname "$0")" || exit; pwd)
root_dir=$(cd "$script_dir/.." || exit; pwd)
build_dir=$root_dir/build

local_res_dir=$HOME/Downloads/AT-Res-8p/$(date +%m%d%H%M%S)
remote_res_dir=/tmp/at_res

if [ ! -d "$local_res_dir" ]; then
mkdir -p "$local_res_dir"  && echo "build folder is created"
fi

# Scp execute file and lib to remote device
scp -O -P $DEV_PORT "$build_dir/test/AT_TEST" root@$DEV_IP:/tmp
scp -O -P $DEV_PORT "$build_dir/libAT.so" root@$DEV_IP:/tmp
scp -O -P $DEV_PORT "$root_dir/modules/hmap_generator/test/model/hmap-v2-qat-uint8.tmfile" root@$DEV_IP:/tmp

# Connect to remote device(Scanner)
ssh -tt -p $DEV_PORT root@$DEV_IP << 'remotessh'
killall SmoreScanner
sleep 1
mkdir /tmp/at_res

export LD_LIBRARY_PATH=/tmp:$LD_LIBRARY_PATH
/tmp/AT_TEST vs1000p
exit
remotessh

# Transfer images to local
scp -O -q -P $DEV_PORT "root@$DEV_IP:$remote_res_dir/*" "$local_res_dir"

# Clean remote files
ssh -tt -p $DEV_PORT root@$DEV_IP << 'remotessh'
rm -r /tmp/at_res
exit
remotessh
