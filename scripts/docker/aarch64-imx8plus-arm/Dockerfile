FROM yjunj/compiler-imx8plus:arm

# Download and extract OpenCV
RUN wget https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/opencv/8plus_opencv_with_tps-4.5.2-linux-aarch64.tar.gz && \
    tar -xf 8plus_opencv_with_tps-4.5.2-linux-aarch64.tar.gz && \
    rm 8plus_opencv_with_tps-4.5.2-linux-aarch64.tar.gz && \
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
