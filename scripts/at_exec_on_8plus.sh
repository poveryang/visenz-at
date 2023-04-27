#!/bin/bash

# Set remote device ip
DEV_IP=192.168.8.21

# Make sure the save folder is created
save_dir=$HOME/Downloads/AT-Res-8p/$(date +%m%d%H%M%S)
if [ ! -d "$save_dir" ]; then
mkdir -p "$save_dir"  && echo "build folder is created"
fi

# Scp execute file and lib to remote device
scp -O ./release/vs/imx8plus/lib/libAT.so root@$DEV_IP:/tmp
scp -O ./release/vs/imx8plus/bin/AT_TEST root@$DEV_IP:/tmp
scp -O ./modules/hmap_generator/test/model/hmap-v2-qat-uint8.tmfile root@$DEV_IP:/tmp

# Connect to remote device(Scanner)
ssh -tt root@$DEV_IP << 'remotessh'
killall SmoreScanner
export LD_LIBRARY_PATH=/tmp:$LD_LIBRARY_PATH

mkdir /tmp/at_res
cd /tmp
./AT_TEST vs1000p
exit
remotessh

# Transfer images to local
scp -O -q "root@$DEV_IP:/tmp/at_res/*" $save_dir

# Clean remote files
ssh -tt root@$DEV_IP << remotessh
rm -r /tmp/at_res
exit
remotessh
