#!/bin/bash

#
# Variables
#
LIBS="librmclient librmservice"
BINS="resourcemanager"
OUT_DIR=$ANDROID_BUILD_TOP/out/target/product/$device/system
PREBUILT_DIR=$ANDROID_BUILD_TOP/vendor/marvell/prebuilts/generic/common/resource_manager
MODULE_DIR=$ANDROID_BUILD_TOP/vendor/marvell/generic/frameworks/resource_manager

function usage()
{
    echo "Usage: $0 [CPU_NUM] <build/release>"
}

function build_module()
{
    echo "Compiling $LIBS"
    cd $ANDROID_BUILD_TOP
    make $LIBS -j $CPU_NUM
    if [ $? -ne 0 ]; then
        echo "Failed to compile $LIBS"
        exit 1
    fi
    make $BINS -j $CPU_NUM
    if [ $? -ne 0 ]; then
        echo "Failed to compile $BINS"
        exit 1
    fi

}

function release_module()
{
    # Remove source files
    rm -rf $MODULE_DIR/libs
    rm -rf $MODULE_DIR/test

    # Use the release makefile
    mv $MODULE_DIR/Android.release.mk $MODULE_DIR/Android.mk

    # Update the prebuilts
    mkdir -p $PREBUILT_DIR
    for lib in $LIBS; do
        cp $OUT_DIR/lib/$lib.so $PREBUILT_DIR
    done

    for bin in $BINS; do
        cp $OUT_DIR/bin/$bin $PREBUILT_DIR
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
