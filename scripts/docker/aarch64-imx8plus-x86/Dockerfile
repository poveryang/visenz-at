FROM ubuntu:latest

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    tar \
    xz-utils

# Download and extract compiler
RUN wget http://artifactory.smoa.cloud/artifactory/SMScanner/nxp_tengine_inference/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu.tar.xz && \
    tar -xvf gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu.tar.xz && \
    rm gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu.tar.xz && \
    mv gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu aarch64-imx8plus-linux-gnu && \
    mv aarch64-imx8plus-linux-gnu /opt/

# Set default compiler path
ENV PATH="/opt/aarch64-imx8plus-linux-gnu/bin:${PATH}"

# Download and extract OpenCV
RUN wget http://artifactory.smoa.cloud/artifactory/SMScanner/nxp_tengine_inference/opencv-4.5.2-linux-aarch64.tar.gz && \
    tar -xzf opencv-4.5.2-linux-aarch64.tar.gz && \
    rm opencv-4.5.2-linux-aarch64.tar.gz && \
    mkdir -p /opt/libs_aarch64 && \
    mv opencv_aarch64 /opt/libs_aarch64/

# Download and extract Barcode SDK
RUN wget https://devops-generic.pkg.coding.smoa.cloud/smore-visenz/3rdparty/Barcode_8plus_dl_0.25.16.tar.gz && \
    tar -xzf Barcode_8plus_dl_0.25.16.tar.gz && \
    rm Barcode_8plus_dl_0.25.16.tar.gz && \
    mkdir -p /opt/libs_aarch64 && \
    mv ./publish/Barcode_8plus_dl_0.25.16 /opt/libs_aarch64/

# Download and extract tengine_lib to Barcode SDK
RUN wget http://artifactory.smoa.cloud/artifactory/SMScanner/nxp_tengine_inference/tengine_lib.tar.gz && \
    tar -xzf tengine_lib.tar.gz && \
    rm tengine_lib.tar.gz && \
    mv tengine_lib Barcode_8plus_dl*/
