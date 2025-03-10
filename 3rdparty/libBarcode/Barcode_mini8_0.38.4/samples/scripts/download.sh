#!/bin/bash
# Commands to build on ubuntu.

set -e

PLATFORM=${1:-"all"} # linux macos
SCRIPT_DIR="$( cd "$(dirname "$0")" ; pwd -P)"
ROOT_DIR=$SCRIPT_DIR/../

need_download () {
    DIRECTORY=$1
    if [ "`ls -A $DIRECTORY`" = "" ]; then
        echo "$DIRECTORY is indeed empty,start download!"
        return 0
    else
        echo "$DIRECTORY is not empty,cancel download!"
        return 1
    fi
}

need_download_dir () {
    DIRECTORY=$1
    PACKAGE_NAME=$2
    if [[ "$PACKAGE_NAME" == "" ]]; then
        if [ "`ls -A $DIRECTORY`" = "" ]; then
            echo "$DIRECTORY is indeed empty, start download!"
            return 0
        else
            echo "$DIRECTORY is not empty, cancel download!"
            return 1
        fi
    else
        if [ "`ls -A $DIRECTORY | grep $PACKAGE_NAME`" = "" ] || [ "`ls -A $DIRECTORY/$PACKAGE_NAME`" = "" ]; then
            echo "$DIRECTORY/$PACKAGE_NAME is indeed empty or not exist, start download!"
            return 0
        else
            echo "$DIRECTORY/$PACKAGE_NAME is not empty, cancel download!"
            return 1
        fi
    fi
}

# download_unzip () {
#     URL=$1
#     DIR=$2
#     echo "downloading from $URL and putting in $DIR"
#     rm -rf $DIR
#     mkdir -p $DIR
#     wget $URL -O $DIR/package.zip
#     unzip $DIR/package.zip -d $DIR
#     rm $DIR/package.zip
# }

download_unzip () {
    URL=$1
    DIR=$2
    FILE=$3
    echo "downloading from $URL and putting in $DIR"
    if [[ "$FILE" == "" ]]; then
        rm -rf $DIR
        mkdir -p $DIR
        wget $URL -O $DIR/package.zip
        unzip $DIR/package.zip -d $DIR
        rm $DIR/package.zip
    else
        if [ ! -d $DIR ]; then
            mkdir $DIR
        fi
        wget $URL -O $DIR/$FILE
        unzip $DIR/$FILE -d $DIR
        rm $DIR/$FILE
    fi
}

# download_tgz () {
#     URL=$1
#     DIR=$2
#     echo "downloading from $URL and putting in $DIR"
#     rm -rf $DIR
#     mkdir -p $DIR
#     wget $URL -O $DIR/package.tgz
#     tar -zxvf $DIR/package.tgz -C $DIR
#     rm $DIR/package.tgz
# }

download_tgz () {
    URL=$1
    DIR=$2
    FILE=$3
    echo "downloading from $URL and putting in $DIR"
    if [[ "$FILE" == "" ]]; then
        rm -rf $DIR
        mkdir -p $DIR
        wget $URL -O $DIR/package.tgz
        tar -xvf $DIR/package.tgz -C $DIR
        rm $DIR/package.tgz
    else
        if [ ! -d $DIR ]; then
            mkdir $DIR
        fi
        wget $URL -O $DIR/$FILE
        tar -xvf $DIR/$FILE -C $DIR
        rm $DIR/$FILE
    fi
}

download_tbz () {
    URL=$1
    DIR=$2
    FILE=$3
    echo "downloading from $URL and putting in $DIR"
    if [[ "$FILE" == "" ]]; then
        rm -rf $DIR
        mkdir -p $DIR
        wget $URL -O $DIR/package.tbz
        tar -jxvf  $DIR/package.tgz -C $DIR
        rm $DIR/package.tgz
    else
        if [ ! -d $DIR ]; then
            mkdir $DIR
        fi
        wget $URL -O $DIR/$FILE
        tar -jxvf  $DIR/$FILE -C $DIR
        rm $DIR/$FILE
    fi
}

download_file () {
    URL=$1
    DIR=$2
    FILE=$3
    mkdir -p $DIR
    echo "downloading from $URL and putting in $DIR"
    wget $URL -O $DIR/$FILE
}

CLANG_TIDY_DIR=/usr/bin/clang-tidy

OPENCV_MINI8_URL=https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/opencv/mini8/opencv-contrib-4.5.2-linux-aarch64.zip
OPENCV_MINI8_DIR=$ROOT_DIR/3rdparty/opencv_mini8

OPENCV_LINUX_URL=https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/opencv/opencv-contrib-4.5.2-linux.zip
OPENCV_LINUX_DIR=$ROOT_DIR/3rdparty/opencv_linux

OPENCV_ANDROID_URL=https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/opencv/opencv-4.5.2-android-sdk.zip
OPENCV_ANDROID_DIR=$ROOT_DIR/3rdparty/opencv_android

OPENCV_AARCH64_URL=https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/opencv/8plus_opencv_with_tps-4.5.2-linux-aarch64.tar.gz
OPENCV_AARCH64_DIR=$ROOT_DIR/3rdparty/opencv_aarch64

C8MMX_COMPILER_INSTALL_DIR='/opt/myir-imx-xwayland/4.14-sumo/environment-setup-aarch64-poky-linux'
C8MMX_COMPILER_URL=https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/toolchains/MYD-C8MMX-ISO-20200801.tar.gz
C8MMX_COMPILER_DIR=$ROOT_DIR/3rdparty/MYD-C8MMX-ISO-20200801

ANDROID_NDK_URL=https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/toolchains/android-ndk-r23b-linux.zip
ANDROID_NDK_DIR=$ROOT_DIR/3rdparty/android_ndk

ASANLIB_AARCH64_URL=https://devops-generic.pkg.coding.smoa.cloud/x-barcode/package/asanlib/asanlib_8plus.zip?version=latest
ASANLIB_AARCH64_DIR=$ROOT_DIR/3rdparty/asanlib_aarch64

TENGINE_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/x-barcode/model/tengine/coarseloc/v13_exp31_coarse_models_250115.zip"
TENGINE_DECODER_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/tengine/mstd_0811.smartmore"
TENGINE_MODEL_DIR=$ROOT_DIR/3rdparty/tengine_model

TENGINE_FPCLS_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/x-barcode/model/tengine/fpcls/2dfpcls.zip?version=v6.17_250115"
TENGINE_1DFPCLS_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/x-barcode/model/tengine/fpcls/1dfpcls_v6.11.zip"
TENGINE_FPCLS_MODEL_DIR=$ROOT_DIR/3rdparty/tengine_fpcls_model

TENGINE_FINE_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/x-barcode/model/tengine/precloc/2dprecloc.zip?version=v3.26_250115"
TENGINE_1DFINE_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/x-barcode/model/tengine/precloc/1dprecloc_v3.21.zip"
TENGINE_FINE_MODEL_DIR=$ROOT_DIR/3rdparty/tengine_fine_model

TENGINE_VERSIONCLS_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/x-barcode/model/tengine/2dversioncls_v3.7.zip"
TENGINE_VERSIONCLS_MODEL_DIR=$ROOT_DIR/3rdparty/tengine_versioncls_model

TENGINE_BINARY_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/x-barcode/model/tengine/2dbinary.zip?version=v2.13_0110"
TENGINE_BINARY_MODEL_DIR=$ROOT_DIR/3rdparty/tengine_binary_model

TENGINE_1D_DL_DECODER_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/x-barcode/model/tengine/1d_decoder_all_exp123.zip?version=latest"
TENGINE_1D_DL_DECODER_MODEL_DIR=$ROOT_DIR/3rdparty/tengine_1d_dl_decoder_model

SMMODEL_DIR=$ROOT_DIR/3rdparty/smmodel
DETECT_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/win/v6_release_junya_dyinput.smartmore"
DECODE_1D_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/win/bardecoder1d.smartmore"

AARCH64_CROSS_COMPILE_PACKAGE_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/toolchains/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu.tar.xz"
AARCH64_CROSS_COMPILE_PACKAGE_DIR=$ROOT_DIR/3rdparty/

OPENCV_MACOS_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/opencv/opencv-4.5.2-macos-arm64.zip"
OPENCV_MACOS_DIR=$ROOT_DIR/3rdparty/opencv_macos

ML_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/ios/v3.zip"
ML_MODEL_DIR=$ROOT_DIR/3rdparty/mlmodel

IOS_MLMODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/ios/v5_nas_app.zip"
IOS_MLMODEL_DIR=$ROOT_DIR/3rdparty/sm

MNN_LIB_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/android/MNN-2.0.0.zip"
MNN_LIB_DIR=$ROOT_DIR/3rdparty/MNN

MNN_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/android/mnn_model.zip"
MNN_MODEL_DIR=$ROOT_DIR/3rdparty/mnn_model

LIBSODIUM_ANDROID_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/android/libsodium-android.zip"
LIBSODIUM_ANDROID_DIR=$ROOT_DIR/3rdparty/libsodium-android

LIBSODIUM_IOS_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/ios/libsodium-1.0.18-iOS-universal.zip"
LIBSODIUM_IOS_DIR=$ROOT_DIR/3rdparty/libsodium-ios

CRYPTO_TOOL_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/crypto.zip"
CRYPTO_TOOL_DIR=$ROOT_DIR/3rdparty/crypto

HAILO_LIB_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/hailo/aarch64_nxp_hailo_4.10.0.zip"
HAILO_LIB_DIR=$ROOT_DIR/3rdparty/hailo_8plus

HAILO_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/hailo/model/hailo_v9.zip"
HAILO_MODEL_DIR=$ROOT_DIR/3rdparty/hailo_model/

HAILO_FINE_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/deeplearning/hailo/model/precloc_exp210_hailo.zip"
HAILO_FINE_MODEL_DIR=$ROOT_DIR/3rdparty/hailo_fine_model/

VS600_CROSS_COMPILER_PACKAGE_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/toolchains/gcc-arm-9.2-2019.12-x86_64-arm-none-linux-gnueabihf.tar.xz"
VS600_CROSS_COMPILE_PACKAGE_DIR=$ROOT_DIR/3rdparty/

# NOVAIC_MODEL_URL="https://devops-generic.pkg.coding.smoa.cloud/x-barcode/model/novaic/novaic_model.zip?version=240617"
NOVAIC_MODEL_DIR=$ROOT_DIR/3rdparty/novaic_model/

NOVAIC_VS600_MODEL_DIR=$NOVAIC_MODEL_DIR/vs600_56x
NOVAIC_VS800_MODEL_DIR=$NOVAIC_MODEL_DIR/vs800_52x

# url of vs600 1d dl decoder models
_NOVAIC_BAED_URL="https://devops-generic.pkg.coding.smoa.cloud/x-barcode/model/novaic"
_NORMNAL_1D_DL_DECODER_VERSION="1d_decoder_all_exp28.smartmore"
_LIGHT_1D_DL_DECODER_VERSION="1d_dl_decoder_v1.0.2_light3x.smartmore"

_VS600_1D_DL_DECODER_BAED_URL="$_NOVAIC_BAED_URL/vs600_56x/1d_dl_decoder"
NOVAIC_VS600_1D_DL_DECODER_MODEL_URL="$_VS600_1D_DL_DECODER_BAED_URL/$_NORMNAL_1D_DL_DECODER_VERSION"
NOVAIC_VS600_1D_DL_DECODER_LIGHT_MODEL_URL="$_VS600_1D_DL_DECODER_BAED_URL/$_LIGHT_1D_DL_DECODER_VERSION"

# url of vs800  1d dl decoder models
_VS800_1D_DL_DECODER_BAED_URL="$_NOVAIC_BAED_URL/vs800_52x/1d_dl_decoder"
NOVAIC_VS800_1D_DL_DECODER_MODEL_URL="$_VS800_1D_DL_DECODER_BAED_URL/$_NORMNAL_1D_DL_DECODER_VERSION"
NOVAIC_VS800_1D_DL_DECODER_LIGHT_MODEL_URL="$_VS800_1D_DL_DECODER_BAED_URL/$_LIGHT_1D_DL_DECODER_VERSION"
QUALITY_LIB_URL="https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/quality_lib_20231207.zip?version=latest"
QUALITY_LIB_DIR=$ROOT_DIR/3rdparty/quality_lib/

if [ $PLATFORM == 'macos' ]
then
    if need_download $OPENCV_MACOS_DIR
    then
        download_unzip $OPENCV_MACOS_URL $OPENCV_MACOS_DIR
    fi

    if need_download $ML_MODEL_DIR
    then
        download_unzip $ML_MODEL_URL $ML_MODEL_DIR
    fi
elif [ $PLATFORM == 'ios' ]
then
    if need_download $IOS_MLMODEL_DIR
    then
        download_unzip $IOS_MLMODEL_URL $IOS_MLMODEL_DIR
    fi

    if need_download $LIBSODIUM_IOS_DIR 
    then 
        download_unzip $LIBSODIUM_IOS_URL $LIBSODIUM_IOS_DIR
    fi

    if need_download $CRYPTO_TOOL_DIR 
    then 
        download_unzip $CRYPTO_TOOL_URL $CRYPTO_TOOL_DIR
    fi
elif [ $PLATFORM == 'linux' ]
then
    download_unzip $OPENCV_LINUX_URL $OPENCV_LINUX_DIR
elif [ $PLATFORM == 'mini8' ]
then
    if need_download $OPENCV_MINI8_DIR
    then
        download_unzip $OPENCV_MINI8_URL $OPENCV_MINI8_DIR
    fi

    if need_download $QUALITY_LIB_DIR
    then
        download_unzip $QUALITY_LIB_URL $QUALITY_LIB_DIR
    fi

elif [ $PLATFORM == 'hailo' ]
then
    if need_download $HAILO_LIB_DIR
    then
        download_unzip $HAILO_LIB_URL $HAILO_LIB_DIR
    fi
    if need_download $HAILO_MODEL_DIR
    then
        download_unzip $HAILO_MODEL_URL $HAILO_MODEL_DIR
    fi
    if need_download $HAILO_FINE_MODEL_DIR
    then
        download_unzip $HAILO_FINE_MODEL_URL $HAILO_FINE_MODEL_DIR
    fi
    if need_download $TENGINE_MODEL_DIR "$(basename $TENGINE_MODEL_URL)"
    then
        download_unzip $TENGINE_MODEL_URL $TENGINE_MODEL_DIR "$(basename $TENGINE_MODEL_URL)"
    fi

    if need_download $OPENCV_AARCH64_DIR
    then
        download_tgz $OPENCV_AARCH64_URL $OPENCV_AARCH64_DIR
    fi
    if need_download $TENGINE_FINE_MODEL_DIR "$(basename $TENGINE_FINE_MODEL_URL)"
    then
        download_unzip $TENGINE_FINE_MODEL_URL $TENGINE_FINE_MODEL_DIR "$(basename $TENGINE_FINE_MODEL_URL)"
        download_unzip $TENGINE_1DFINE_MODEL_URL $TENGINE_FINE_MODEL_DIR "$(basename $TENGINE_1DFINE_MODEL_URL)"
    fi
    if need_download $TENGINE_FPCLS_MODEL_DIR "$(basename $TENGINE_FPCLS_MODEL_URL)"
    then
        download_unzip $TENGINE_FPCLS_MODEL_URL $TENGINE_FPCLS_MODEL_DIR "$(basename $TENGINE_FPCLS_MODEL_URL)"
        download_unzip $TENGINE_1DFPCLS_MODEL_URL $TENGINE_FPCLS_MODEL_DIR "$(basename $TENGINE_1DFPCLS_MODEL_URL)"
    fi
    if need_download $TENGINE_VERSIONCLS_MODEL_DIR "$(basename $TENGINE_VERSIONCLS_MODEL_URL)"
    then
        download_unzip $TENGINE_VERSIONCLS_MODEL_URL $TENGINE_VERSIONCLS_MODEL_DIR "$(basename $TENGINE_VERSIONCLS_MODEL_URL)"
    fi
    if need_download $TENGINE_BINARY_MODEL_DIR "$(basename $TENGINE_BINARY_MODEL_URL)"
    then
        download_unzip $TENGINE_BINARY_MODEL_URL $TENGINE_BINARY_MODEL_DIR "$(basename $TENGINE_BINARY_MODEL_URL)"
    fi
    if need_download $TENGINE_1D_DL_DECODER_MODEL_DIR "$(basename $TENGINE_1D_DL_DECODER_MODEL_URL)"
    then
        download_unzip $TENGINE_1D_DL_DECODER_MODEL_URL $TENGINE_1D_DL_DECODER_MODEL_DIR "$(basename $TENGINE_1D_DL_DECODER_MODEL_URL)"
    fi
    if need_download_dir $AARCH64_CROSS_COMPILE_PACKAGE_DIR gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu
    then
        download_tgz $AARCH64_CROSS_COMPILE_PACKAGE_URL $AARCH64_CROSS_COMPILE_PACKAGE_DIR "$(basename $AARCH64_CROSS_COMPILE_PACKAGE_URL)"
    fi

elif [ $PLATFORM == 'linux_dl' ]
then
    if need_download $OPENCV_LINUX_DIR
    then
        download_unzip $OPENCV_LINUX_URL $OPENCV_LINUX_DIR
    fi
    if need_download $SMMODEL_DIR
    then
        download_file $DETECT_MODEL_URL $SMMODEL_DIR "$(basename $DETECT_MODEL_URL)"
        download_file $DECODE_1D_MODEL_URL $SMMODEL_DIR "$(basename $DECODE_1D_MODEL_URL)"
    fi
    if need_download $TENGINE_MODEL_DIR "$(basename $TENGINE_MODEL_URL)"
    then
        download_unzip $TENGINE_MODEL_URL $TENGINE_MODEL_DIR "$(basename $TENGINE_MODEL_URL)"
    fi
    if need_download $TENGINE_FINE_MODEL_DIR "$(basename $TENGINE_FINE_MODEL_URL)"
    then
        download_unzip $TENGINE_FINE_MODEL_URL $TENGINE_FINE_MODEL_DIR "$(basename $TENGINE_FINE_MODEL_URL)"
        download_unzip $TENGINE_1DFINE_MODEL_URL $TENGINE_FINE_MODEL_DIR "$(basename $TENGINE_1DFINE_MODEL_URL)"
    fi
    if need_download $TENGINE_FPCLS_MODEL_DIR "$(basename $TENGINE_FPCLS_MODEL_URL)"
    then
        download_unzip $TENGINE_FPCLS_MODEL_URL $TENGINE_FPCLS_MODEL_DIR "$(basename $TENGINE_FPCLS_MODEL_URL)"
        download_unzip $TENGINE_1DFPCLS_MODEL_URL $TENGINE_FPCLS_MODEL_DIR "$(basename $TENGINE_1DFPCLS_MODEL_URL)"
    fi
    if need_download $TENGINE_VERSIONCLS_MODEL_DIR "$(basename $TENGINE_VERSIONCLS_MODEL_URL)"
    then
        download_unzip $TENGINE_VERSIONCLS_MODEL_URL $TENGINE_VERSIONCLS_MODEL_DIR "$(basename $TENGINE_VERSIONCLS_MODEL_URL)"
    fi
    if need_download $TENGINE_BINARY_MODEL_DIR "$(basename $TENGINE_BINARY_MODEL_URL)"
    then
        download_unzip $TENGINE_BINARY_MODEL_URL $TENGINE_BINARY_MODEL_DIR "$(basename $TENGINE_BINARY_MODEL_URL)"
    fi
    if need_download $TENGINE_1D_DL_DECODER_MODEL_DIR "$(basename $TENGINE_1D_DL_DECODER_MODEL_URL)"
    then
        download_unzip $TENGINE_1D_DL_DECODER_MODEL_URL $TENGINE_1D_DL_DECODER_MODEL_DIR "$(basename $TENGINE_1D_DL_DECODER_MODEL_URL)"
    fi
elif [ $PLATFORM == 'aarch64_dl' ]
then
    if need_download $ASANLIB_AARCH64_DIR
    then
        download_unzip $ASANLIB_AARCH64_URL $ASANLIB_AARCH64_DIR
    fi

    if need_download $OPENCV_AARCH64_DIR
    then
        download_tgz $OPENCV_AARCH64_URL $OPENCV_AARCH64_DIR
    fi

    if need_download $TENGINE_MODEL_DIR "$(basename $TENGINE_MODEL_URL)"
    then
        download_unzip $TENGINE_MODEL_URL $TENGINE_MODEL_DIR "$(basename $TENGINE_MODEL_URL)"
    fi
    if need_download_dir $TENGINE_MODEL_DIR "$(basename $TENGINE_DECODER_URL)"
    then
        download_file $TENGINE_DECODER_URL $TENGINE_MODEL_DIR "$(basename $TENGINE_DECODER_URL)"
    fi
    if need_download $TENGINE_FINE_MODEL_DIR "$(basename $TENGINE_FINE_MODEL_URL)"
    then
        download_unzip $TENGINE_FINE_MODEL_URL $TENGINE_FINE_MODEL_DIR "$(basename $TENGINE_FINE_MODEL_URL)"
        download_unzip $TENGINE_1DFINE_MODEL_URL $TENGINE_FINE_MODEL_DIR "$(basename $TENGINE_1DFINE_MODEL_URL)"
    fi
    if need_download $TENGINE_FPCLS_MODEL_DIR "$(basename $TENGINE_FPCLS_MODEL_URL)"
    then
        download_unzip $TENGINE_FPCLS_MODEL_URL $TENGINE_FPCLS_MODEL_DIR "$(basename $TENGINE_FPCLS_MODEL_URL)"
        download_unzip $TENGINE_1DFPCLS_MODEL_URL $TENGINE_FPCLS_MODEL_DIR "$(basename $TENGINE_1DFPCLS_MODEL_URL)"
    fi
    if need_download $TENGINE_VERSIONCLS_MODEL_DIR "$(basename $TENGINE_VERSIONCLS_MODEL_URL)"
    then
        download_unzip $TENGINE_VERSIONCLS_MODEL_URL $TENGINE_VERSIONCLS_MODEL_DIR "$(basename $TENGINE_VERSIONCLS_MODEL_URL)"
    fi
    if need_download $TENGINE_BINARY_MODEL_DIR "$(basename $TENGINE_BINARY_MODEL_URL)"
    then
        download_unzip $TENGINE_BINARY_MODEL_URL $TENGINE_BINARY_MODEL_DIR "$(basename $TENGINE_BINARY_MODEL_URL)"
    fi
    if need_download $TENGINE_1D_DL_DECODER_MODEL_DIR "$(basename $TENGINE_1D_DL_DECODER_MODEL_URL)"
    then
        download_unzip $TENGINE_1D_DL_DECODER_MODEL_URL $TENGINE_1D_DL_DECODER_MODEL_DIR "$(basename $TENGINE_1D_DL_DECODER_MODEL_URL)"
    fi
    if need_download_dir $AARCH64_CROSS_COMPILE_PACKAGE_DIR gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu
    then
        download_tgz $AARCH64_CROSS_COMPILE_PACKAGE_URL $AARCH64_CROSS_COMPILE_PACKAGE_DIR "$(basename $AARCH64_CROSS_COMPILE_PACKAGE_URL)"
    fi
    
elif [ $PLATFORM == 'vs600' ]
then
    if need_download_dir $VS600_CROSS_COMPILE_PACKAGE_DIR arm-ca9-linux-gnueabihf-8.4
    then
        download_tgz $VS600_CROSS_COMPILER_PACKAGE_URL $VS600_CROSS_COMPILE_PACKAGE_DIR "$(basename $VS600_CROSS_COMPILER_PACKAGE_URL)"
    fi
    
    # if need_download $NOVAIC_MODEL_DIR
    # then
    #     download_unzip $NOVAIC_MODEL_URL $NOVAIC_MODEL_DIR
    # fi
    # download vs600/vs800 1d dl decoder always
    #download_file $NOVAIC_VS600_1D_DL_DECODER_MODEL_URL $NOVAIC_VS600_MODEL_DIR "$(basename $NOVAIC_VS600_1D_DL_DECODER_MODEL_URL)"
    #download_file $NOVAIC_VS600_1D_DL_DECODER_LIGHT_MODEL_URL $NOVAIC_VS600_MODEL_DIR "$(basename $NOVAIC_VS600_1D_DL_DECODER_LIGHT_MODEL_URL)"
    #download_file $NOVAIC_VS800_1D_DL_DECODER_MODEL_URL $NOVAIC_VS800_MODEL_DIR "$(basename $NOVAIC_VS800_1D_DL_DECODER_MODEL_URL)"
    #download_file $NOVAIC_VS800_1D_DL_DECODER_LIGHT_MODEL_URL $NOVAIC_VS800_MODEL_DIR "$(basename $NOVAIC_VS800_1D_DL_DECODER_LIGHT_MODEL_URL)"

elif [ $PLATFORM == 'android' ]
then

    if need_download $CRYPTO_TOOL_DIR 
    then 
        download_unzip $CRYPTO_TOOL_URL $CRYPTO_TOOL_DIR
    fi

    if need_download $OPENCV_ANDROID_DIR
    then
        download_unzip $OPENCV_ANDROID_URL $OPENCV_ANDROID_DIR
    fi

    if need_download $ANDROID_NDK_DIR
    then
        download_unzip $ANDROID_NDK_URL $ANDROID_NDK_DIR
    fi

    if need_download $MNN_LIB_DIR
    then
        download_unzip $MNN_LIB_URL $MNN_LIB_DIR
    fi

    if need_download $MNN_MODEL_DIR
    then
        download_unzip $MNN_MODEL_URL $MNN_MODEL_DIR
    fi

    if need_download $LIBSODIUM_ANDROID_DIR 
    then 
        download_unzip $LIBSODIUM_ANDROID_URL $LIBSODIUM_ANDROID_DIR
    fi
elif [ $PLATFORM == 'all' ]
then

    if need_download $CRYPTO_TOOL_DIR 
    then 
        download_unzip $CRYPTO_TOOL_URL $CRYPTO_TOOL_DIR
    fi

    if need_download $LIBSODIUM_IOS_DIR
    then 
        download_unzip $LIBSODIUM_IOS_URL $LIBSODIUM_IOS_DIR
    fi

    if need_download $MNN_LIB_DIR
    then
        download_unzip $MNN_LIB_URL $MNN_LIB_DIR
    fi

    if need_download $MNN_MODEL_DIR
    then
        download_unzip $MNN_MODEL_URL $MNN_MODEL_DIR
    fi

    if need_download $LIBSODIUM_ANDROID_DIR 
    then 
        download_unzip $LIBSODIUM_ANDROID_URL $LIBSODIUM_ANDROID_DIR
    fi

    if need_download $OPENCV_MINI8_DIR
    then
        download_unzip $OPENCV_MINI8_URL $OPENCV_MINI8_DIR
    fi

    if need_download $OPENCV_LINUX_DIR
    then
        download_unzip $OPENCV_LINUX_URL $OPENCV_LINUX_DIR
    fi

    if need_download $SMMODEL_DIR
    then
        download_file $DETECT_MODEL_URL $SMMODEL_DIR "$(basename $DETECT_MODEL_URL)"
        download_file $DECODE_1D_MODEL_URL $SMMODEL_DIR "$(basename $DECODE_1D_MODEL_URL)"
    fi
    
    if need_download $OPENCV_AARCH64_DIR
    then
        download_tgz $OPENCV_AARCH64_URL $OPENCV_AARCH64_DIR
    fi

    if need_download  $C8MMX_COMPILER_INSTALL_DIR -a need_download $C8MMX_COMPILER_DIR
    then
        download_tgz $C8MMX_COMPILER_URL $C8MMX_COMPILER_DIR
        cd $C8MMX_COMPILER_DIR/MYD-C8MMX-ISO-20200801/03-Tools/ToolsChain/
        ./myir-imx-xwayland-glibc-x86_64-fsl-image-qt5-validation-imx-aarch64-toolchain-4.14-sumo.sh -y
    fi

    if need_download $OPENCV_ANDROID_DIR
    then
        download_unzip $OPENCV_ANDROID_URL $OPENCV_ANDROID_DIR
    fi

    if need_download $ANDROID_NDK_DIR
    then
        download_unzip $ANDROID_NDK_URL $ANDROID_NDK_DIR
    fi

    if need_download $TENGINE_MODEL_DIR "$(basename $TENGINE_MODEL_URL)"
    then
        download_unzip $TENGINE_MODEL_URL $TENGINE_MODEL_DIR "$(basename $TENGINE_MODEL_URL)"
    fi
    if need_download_dir $TENGINE_MODEL_DIR "$(basename $TENGINE_DECODER_URL)"
    then
        download_file $TENGINE_DECODER_URL $TENGINE_MODEL_DIR "$(basename $TENGINE_DECODER_URL)"
    fi

    if need_download $TENGINE_FINE_MODEL_DIR "$(basename $TENGINE_FINE_MODEL_URL)"
    then
        download_unzip $TENGINE_FINE_MODEL_URL $TENGINE_FINE_MODEL_DIR "$(basename $TENGINE_FINE_MODEL_URL)"
        download_unzip $TENGINE_1DFINE_MODEL_URL $TENGINE_FINE_MODEL_DIR "$(basename $TENGINE_1DFINE_MODEL_URL)"
    fi
    if need_download $TENGINE_FPCLS_MODEL_DIR "$(basename $TENGINE_FPCLS_MODEL_URL)"
    then
        download_unzip $TENGINE_FPCLS_MODEL_URL $TENGINE_FPCLS_MODEL_DIR "$(basename $TENGINE_FPCLS_MODEL_URL)"
        download_unzip $TENGINE_1DFPCLS_MODEL_URL $TENGINE_FPCLS_MODEL_DIR "$(basename $TENGINE_1DFPCLS_MODEL_URL)"
    fi
    if need_download $TENGINE_BINARY_MODEL_DIR "$(basename $TENGINE_BINARY_MODEL_URL)"
    then
        download_unzip $TENGINE_BINARY_MODEL_URL $TENGINE_BINARY_MODEL_DIR "$(basename $TENGINE_BINARY_MODEL_URL)"
    fi
    if need_download $TENGINE_1D_DL_DECODER_MODEL_DIR "$(basename $TENGINE_1D_DL_DECODER_MODEL_URL)"
    then
        download_unzip $TENGINE_1D_DL_DECODER_MODEL_URL $TENGINE_1D_DL_DECODER_MODEL_DIR "$(basename $TENGINE_1D_DL_DECODER_MODEL_URL)"
    fi
    if need_download_dir $AARCH64_CROSS_COMPILE_PACKAGE_DIR gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu
    then
        download_tgz $AARCH64_CROSS_COMPILE_PACKAGE_URL $AARCH64_CROSS_COMPILE_PACKAGE_DIR "$(basename $AARCH64_CROSS_COMPILE_PACKAGE_URL)"
    fi

    apt-get -y install tcl tk expect

    if need_download_dir $VS600_CROSS_COMPILE_PACKAGE_DIR arm-ca9-linux-gnueabihf-8.4
    then
        download_tbz $VS600_CROSS_COMPILER_PACKAGE_URL $VS600_CROSS_COMPILE_PACKAGE_DIR "$(basename $VS600_CROSS_COMPILER_PACKAGE_URL)"
    fi

fi

if [ $PLATFORM == 'macos' ] || [ $PLATFORM == 'mini8' ]
then
    SYSTEM_VERSION="None"
else
    SYSTEM_VERSION=`lsb_release -r --short`
fi

if [ "$SYSTEM_VERSION" = "20.04" ]; then
    cd $ROOT_DIR/3rdparty
    wget https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/libilmbase12_2.2.0-11ubuntu2_amd64.deb
    wget https://devops-generic.pkg.coding.smoa.cloud/smore-scanner/package/libopenexr22_2.2.0-11.1ubuntu1.6_amd64.deb
    dpkg -i libilmbase12_2.2.0-11ubuntu2_amd64.deb
    dpkg -i libopenexr22_2.2.0-11.1ubuntu1.6_amd64.deb
fi
