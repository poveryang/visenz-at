#!/bin/bash
#!/usr/bin/bash

# Set remote device ip
DEV_IP=169.254.19.136

# Clean local files
SAVE_DIR=./AT_RES/VS1000pro/$(date +%m%d%H%M)
echo "$SAVE_DIR" && mkdir "$SAVE_DIR"

# Scp execute file and lib to remote device
scp -O ./release/imx8plus/lib/libAT.so root@$DEV_IP:/tmp
scp -O ./release/imx8plus/bin/AT_TEST root@$DEV_IP:/tmp

# Connect to remote device(Scanner)
ssh -tt root@$DEV_IP << remotessh
killall SmoreScanner
killall SmorescannerUdpProcess
killall SmoreScannerUpgrade
export LD_LIBRARY_PATH=/tmp:$LD_LIBRARY_PATH

mkdir /tmp/at_res
cd /tmp
./AT_TEST
exit
remotessh

# Transfer images to local
scp -O -q root@$DEV_IP:/tmp/at_res/\* $SAVE_DIR

# Clean remote files
ssh -tt root@$DEV_IP << remotessh
rm -r /tmp/at_res
rm /tmp/AT_TEST
rm /tmp/libAT.so
exit
remotessh
