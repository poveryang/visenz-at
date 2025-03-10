#!/bin/bash 
# Commands to build on ubuntu.  

./scripts/create_config_header.sh 
SCRIPT_DIR="$( cd "$(dirname "$0")" ; pwd -P)"
echo "SCRIPT_DIR=$SCRIPT_DIR"
GCC_VERSION="$(gcc --version | grep ^gcc | sed 's/^.* //g')"
unset LD_LIBRARY_PATH
# source /opt/myir-imx-xwayland/4.14-sumo/environment-setup-aarch64-poky-linux
OPENCV_ROOT=$SCRIPT_DIR/../3rdparty/opencv_aarch64/opencv_aarch64/lib/cmake/opencv4
BUILD_DIR=$SCRIPT_DIR/../build_rknn_dl
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
    -DTARGET_ARCHITECTURE="arm64-v8a" \
    -DBUILD_SAMPLES=ON \
    -DTHREAD_NUM=4 \
    -DBUILD_WITH_DL=ON \
    -DBUILD_WITH_1D=ON \
    -DWITH_RKNN=ON \
    -DBUILD_WITH_1DDL=OFF \
    -DBUILD_WITH_ONED_BENCHMARK=OFF \
    -DINFERENCE_TYPE="aarch64_rknn" \
    -DCMAKE_TOOLCHAIN_FILE=$SCRIPT_DIR/../cmake/aarch64-linux-gnu.cmake \
    -DBUILD_WITH_IMAGE_RESIZE=ON \
    -DGCC_VERSION=$GCC_VERSION \
    -DBUILD_WITH_HPC=ON \
    -DGCC_VERSION=$GCC_VERSION \
    -DTBB_TEST=OFF \
    -DWITH_SPD_LOG=OFF \
    -DPRINT_PROFILE_INFO_ENABLE=ON \
    $@ \
    || exit 1
# -DCMAKE_VERBOSE_MAKEFILE=ON \
# -DBUILD_WITH_PROFILE=ON \
make -j8
make install
cd ..

STRIP_TOOL=$SCRIPT_DIR/../3rdparty/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/bin/strip

if [ -f "release/lib/libtbb.so.12.5" ]; then
    if [ -f "release/arm64-v8a/lib/libtbb.so.12" ]; then
        rm release/arm64-v8a/lib/libtbb*
    fi

    cp -l release/lib/libtbb.so.12.5 release/arm64-v8a/lib/libtbb.so.12
    cp -l release/lib/libtbbmalloc_proxy.so.2.5 release/arm64-v8a/lib/libtbbmalloc_proxy.so.2
    cp -l release/lib/libtbbmalloc.so.2.5 release/arm64-v8a/lib/libtbbmalloc.so.2

    cp release/lib/libtbb.so.12.5 release/arm64-v8a/lib/libtbb.so.12.5
    cp release/lib/libtbbmalloc_proxy.so.2.5 release/arm64-v8a/lib/libtbbmalloc_proxy.so.2.5
    cp release/lib/libtbbmalloc.so.2.5 release/arm64-v8a/lib/libtbbmalloc.so.2.5
fi

rm -rf release/include/
rm -rf release/bin/
rm -rf release/lib/
rm -rf release/share/
$STRIP_TOOL release/arm64-v8a/lib/*so*
