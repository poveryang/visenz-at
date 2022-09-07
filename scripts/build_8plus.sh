if [ ! -d "build/" ];then
  mkdir build
else
  echo "Directory 'build' existed"
fi

cd build || exit

/usr/bin/cmake
-DCMAKE_BUILD_TYPE=Release \
-DCMAKE_C_COMPILER=/opt/aarch64-imx8plus-linux-gnu/bin/aarch64-imx8plus-linux-gnu-gcc \
-DCMAKE_CXX_COMPILER=/opt/aarch64-imx8plus-linux-gnu/bin/aarch64-imx8plus-linux-gnu-g++ \
-DTARGET_PLATFORM=imx8plus
-S .. \
-B .

/usr/bin/cmake
--build .
--target AT_TEST
-- -j 16
