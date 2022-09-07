#!/bin/bash

# Set remote device ip
DEV_IP=192.168.1.21

# Clean local files
SAVE_DIR=${HOME}/Desktop/AT_RES/$(date +%m%d%H%M)
echo "$SAVE_DIR" && mkdir "$SAVE_DIR"

# Scp execute file and lib to remote device
scp ../build/libAT.so root@$DEV_IP:/tmp
scp ../build/AT_TEST root@$DEV_IP:/tmp

# Connect to remote device(Scanner)
ssh -tt root@$DEV_IP << remotessh
killall /usr/bin/SmoreScanner
export LD_LIBRARY_PATH=/tmp:$LD_LIBRARY_PATH

mkdir /tmp/at_res
cd /tmp
./AT_TEST
exit
remotessh

# Transfer images to local
scp -q root@$DEV_IP:/tmp/at_res/\* $SAVE_DIR

# Clean remote files
ssh -tt root@$DEV_IP << remotessh
rm -r /tmp/at_res
exit
remotessh