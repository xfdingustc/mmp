#!/bin/bash

#
# Variables
#
LIBS="libstagefrighthw"
OUT_DIR=$ANDROID_BUILD_TOP/out/target/product/$device/system/lib
PREBUILT_DIR=$ANDROID_BUILD_TOP/vendor/marvell/prebuilts/generic/common
MODULE_DIR=$ANDROID_BUILD_TOP/vendor/marvell/generic/frameworks/libstagefrighthw

function usage()
{
    echo "Usage: $0 [CPU_NUM] <build/release>"
}

#
# Compile the targets
#
function build_module()
{
    cd $ANDROID_BUILD_TOP
    echo "Compiling $LIBS"
    make $LIBS -j $CPU_NUM
    if [ $? -ne 0 ]; then
        echo "Failed to compile $LIBS"
        exit 1
    fi
}

function release_module()
{
    cd $ANDROID_BUILD_TOP
#
# Rename Android.release.mk and DebugableModules.xml to avoid being removed
#
    mv $MODULE_DIR/Android.release.mk $MODULE_DIR/.Android.mk

#
# Remove unnecessary files
#
    rm -rf $MODULE_DIR/*

#
# Keep release makefile
#
    mv $MODULE_DIR/.Android.mk $MODULE_DIR/Android.mk

#
# Update prebuilt files
#
    mkdir -p $PREBUILT_DIR/libstagefrighthw
    for lib in $LIBS; do
      cp $OUT_DIR/$lib.so $PREBUILT_DIR/libstagefrighthw
    done
}


if [ $# -eq 2 ]; then
    CPU_NUM=$1
    COMMAND=$2
elif [ $# -eq 1 ]; then
    CPU_NUM=$1
    COMMAND=build
else
    usage
    exit 1
fi

echo "The CPU_NUM is $CPU_NUM: COMMAND is $COMMAND"

case $COMMAND in
    build)
        build_module
        ;;
    release)
        release_module
        ;;
    *)
        echo "Unsupported command: $COMMAND"
        usage
        exit 1
        ;;
esac

exit 0
