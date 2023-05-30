# Build AT4VS for i.MX8Plus

# Create build directory
mkdir -p /tmp/build

# Configure
source /opt/myir-imx-xwayland/4.14-sumo/environment-setup-aarch64-poky-linux

/usr/bin/cmake \
-DTARGET_PLATFORM="imx8mini" \
-DCMAKE_BUILD_TYPE=Release \
-DOpenCV_DIR=/opt/libs_aarch64/mier_opencv/lib/cmake/opencv4 \
-S /tmp \
-B /tmp/build

# Build
cmake --build /tmp/build --parallel 8
