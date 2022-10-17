if [ ! -d "build/" ]; then
mkdir build_8plus
else
echo "dir build_8plus existed"
fi
cd build_8plus

/usr/bin/cmake \
-DCMAKE_BUILD_TYPE=Release \
-DCMAKE_C_COMPILER=/opt/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc \
-DCMAKE_CXX_COMPILER=/opt/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++ \
-DTARGET_PLATFORM="imx8plus" \
-DCMAKE_INSTALL_PREFIX="../release/imx8plus" \
-S .. \
-B .

/usr/bin/cmake \
--build . \
--target AT_TEST \
-- -j 16