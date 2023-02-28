#!/bin/bash
#!/usr/bin/bash

# Set remote device ip
DEV_IP=192.168.8.21

# Clean local files
SAVE_DIR=${HOME}/Downloads/AT_RES_VS1000P/$(date +%m%d%H%M)
echo "$SAVE_DIR" && mkdir "$SAVE_DIR" -p

# Scp execute file and lib to remote device
scp -O ./release/VS/imx8plus/lib/libAT.so root@$DEV_IP:/tmp
scp -O ./release/VS/imx8plus/bin/AT_TEST root@$DEV_IP:/tmp
scp -O ./modules/hmap-generator/model/hmap-uint8-v7k-kl.tmfile root@$DEV_IP:/tmp
scp -O ./modules/hmap-generator/model/hmap-v3-e99-fp32.tmfile root@$DEV_IP:/tmp

# Connect to remote device(Scanner)
ssh -tt root@$DEV_IP << remotessh
killall SmoreScanner
killall SmorescannerUdpProcess
killall SmoreScannerUpgrade
export LD_LIBRARY_PATH=/tmp:$LD_LIBRARY_PATH

mkdir /tmp/at_res
cd /tmp
./AT_TEST vs1000p
exit
remotessh

# Transfer images to local
scp -O -q root@$DEV_IP:/tmp/at_res/\* $SAVE_DIR

# Clean remote files
ssh -tt root@$DEV_IP << remotessh
rm -r /tmp/at_res
exit
remotessh
