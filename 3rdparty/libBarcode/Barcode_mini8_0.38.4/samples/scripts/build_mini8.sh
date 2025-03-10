#!/bin/bash
# Commands to build on ubuntu.
./scripts/create_config_header.sh
SCRIPT_DIR="$( cd "$(dirname "$0")" ; pwd -P)"
echo "SCRIPT_DIR=$SCRIPT_DIR"

unset LD_LIBRARY_PATH
source /opt/myir-imx-xwayland/4.14-sumo/environment-setup-aarch64-poky-linux
OPENCV_ROOT=$SCRIPT_DIR/../3rdparty/opencv_mini8/mier_opencv/lib/cmake/opencv4
GCC_VERSION="$(gcc --version | grep ^gcc | sed 's/^.* //g')" 

BUILD_DIR=$SCRIPT_DIR/../build_mini8
RELEASE_DIR=$SCRIPT_DIR/../release
rm -r $BUILD_DIR
mkdir -p $BUILD_DIR
echo "Build version into: $BUILD_DIR"

if [ -z ${CMAKE_BUILD_TYPE+x} ]; then
CMAKE_BUILD_TYPE=Release
fi

cd $BUILD_DIR
cmake .. \
    -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE \
    -DCMAKE_INSTALL_PREFIX=$RELEASE_DIR \
    -DCMAKE_SYSTEM_NAME="Linux" \
    -DCMAKE_SYSTEM_PROCESSOR="aarch64" \
    -DTARGET_ARCHITECTURE="arm64-v8a" \
    -DPLATFORM="mini8" \
    -DBUILD_WITH_1D=ON \
    -DBUILD_SAMPLES=ON \
    -DBUILD_WITH_HPC=ON \
    -DGCC_VERSION=$GCC_VERSION \
    -DTBB_TEST=OFF \
    $@ \
    || exit 1

# -DBUILD_WITH_PROFILE=ON \
make -j2
make install
cd ..

STRIP_TOOL=/opt/myir-imx-xwayland/4.14-sumo/sysroots/x86_64-pokysdk-linux/usr/bin/aarch64-poky-linux/aarch64-poky-linux-strip

if [ -f "release/lib/libtbb.so.12.5" ]; then
    if [ -f "release/arm64-v8a/lib/libtbb.so.12" ]; then
        rm release/arm64-v8a/lib/libtbb*
    fi

    cp -l release/lib/libtbb.so.12.5 release/arm64-v8a/lib/libtbb.so.12
    cp -l release/lib/libtbbmalloc_proxy.so.2.5 release/arm64-v8a/lib/libtbbmalloc_proxy.so.2
    cp -l release/lib/libtbbmalloc.so.2.5 release/arm64-v8a/lib/libtbbmalloc.so.2
fi

rm -rf release/include
rm -rf release/share
rm -rf release/lib
find release/arm64-v8a/include -mindepth 1 ! -name 'barcode.h' -exec rm -rf {} +
$STRIP_TOOL release/arm64-v8a/lib/*
