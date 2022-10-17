#!/bin/bash

if [ ! -d "build_8mini/" ]; then
mkdir build_8mini
else
echo "dir build_8mini existed"
fi
cd build_8mini

sudo source /opt/myir-imx-xwayland/4.14-sumo/environment-setup-aarch64-poky-linux

/usr/bin/cmake \
-DCMAKE_BUILD_TYPE=Release \
-DTARGET_PLATFORM="imx8mini" \
-DCMAKE_INSTALL_PREFIX="../release/imx8mini" \
-S .. \
-B .

/usr/bin/cmake \
--build . \
--target install \
-- -j 16