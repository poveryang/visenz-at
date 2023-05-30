# Build AT4VS for i.MX8Plus

# Create build directory
mkdir -p ./build

# Configure
/usr/bin/cmake \
-DTARGET_PLATFORM="imx8plus" \
-DCMAKE_BUILD_TYPE=Release \
-DCMAKE_C_COMPILER=/opt/aarch64-imx8plus-linux-gnu/bin/aarch64-imx8plus-linux-gnu-gcc \
-DCMAKE_CXX_COMPILER=/opt/aarch64-imx8plus-linux-gnu/bin/aarch64-imx8plus-linux-gnu-g++ \
-DOpenCV_DIR=/opt/libs_aarch64/opencv_aarch64/lib/cmake/opencv4 \
-S . \
-B ./build

# Build
cmake --build ./build --parallel 8