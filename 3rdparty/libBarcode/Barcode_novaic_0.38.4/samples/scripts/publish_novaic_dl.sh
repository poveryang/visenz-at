#!/bin/bash
# Commands to build on ubuntu.
set -e

SCRIPT_DIR="$( cd "$(dirname "$0")" ; pwd -P)"
echo "SCRIPT_DIR=$SCRIPT_DIR"
# SDK_VERSION is passed from CD
if [ -z "${SDK_VERSION}" ]; then
    read -p "Enter version : " -r VERSION
else
    VERSION=${SDK_VERSION#"v"}
fi
PUBLISH_NAME=Barcode_novaic_$VERSION
PUBLISH_DIR=$SCRIPT_DIR/../publish/$PUBLISH_NAME
mkdir -p $SCRIPT_DIR/../publish
mkdir -p $PUBLISH_DIR
echo "Publish version into: $PUBLISH_DIR"

# 3rdparty
cp $SCRIPT_DIR/../release/novaic_glibc/armv7-a/3rdparty $PUBLISH_DIR -r
# bin
mkdir -p $PUBLISH_DIR/bin
cp -fr $SCRIPT_DIR/../release/novaic_glibc/armv7-a/bin/test_barcode* $PUBLISH_DIR/bin
# config
cp $SCRIPT_DIR/../release/novaic_glibc/armv7-a/config $PUBLISH_DIR -r
# include
mkdir -p $PUBLISH_DIR/include
cp -fr $SCRIPT_DIR/../release/novaic_glibc/armv7-a/include/barcode* $PUBLISH_DIR/include
# lib
cp $SCRIPT_DIR/../release/novaic_glibc/armv7-a/lib $PUBLISH_DIR -r
# samples
mkdir -p $PUBLISH_DIR/samples
cp $SCRIPT_DIR/../release/novaic_glibc/armv7-a/samples/test_barcode*.cpp $PUBLISH_DIR/samples/
cp $SCRIPT_DIR/../samples/common.h $PUBLISH_DIR/samples/
cp -r $SCRIPT_DIR/../cmake $PUBLISH_DIR/samples/
cp -r $SCRIPT_DIR/../modules/3rdparty_tools/nlohmann $PUBLISH_DIR/samples/
mkdir -p $PUBLISH_DIR/samples/scripts
cp $SCRIPT_DIR/../scripts/download.sh $PUBLISH_DIR/samples/scripts
cp $SCRIPT_DIR/../scripts/publish_novaic_dl.sh $PUBLISH_DIR/samples/scripts
mkdir -p $PUBLISH_DIR/samples/scripts/python
cp $SCRIPT_DIR/cmpscripts/filter_results.py $PUBLISH_DIR/samples/scripts/python
cp $SCRIPT_DIR/../samples/CMakeLists_publish.txt $PUBLISH_DIR/samples/CMakeLists.txt
# docs
# cp $SCRIPT_DIR/../modules/hpc/docs/html $PUBLISH_DIR -r
# cp $SCRIPT_DIR/../modules/hpc/docs/index.html $PUBLISH_DIR/index.html

tar -zcvf $PUBLISH_DIR.tar.gz $PUBLISH_DIR
# rm -r $PUBLISH_DIR

REPO=https://devops-generic.pkg.coding.smoa.cloud/x-barcode/exe
# CD_USER and CD_PW are passed from CD
if [ -z "${CD_USER}" ]; then
    read -p "Enter username : " -r USER
else
    USER=${CD_USER}
fi
if [ -z "${CD_PW}" ]; then
    read -p "Enter password : " -s PWD
else
    PWD=${CD_PW}
fi
curl -u "${USER}:${PWD}" -X PUT "${REPO}/$PUBLISH_NAME.tar.gz" -T $PUBLISH_DIR.tar.gz > $SCRIPT_DIR/../release/novaic_glibc/armv7-a/url_report.json

while read line
do
    echo $line
done < $SCRIPT_DIR/../release/novaic_glibc/armv7-a/url_report.json