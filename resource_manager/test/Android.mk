LOCAL_PATH:= $(call my-dir)

# ================================================================
# test_rm
# ================================================================
include $(CLEAR_VARS)

LOCAL_MODULE := test_rm

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    MVBerlinRMTest.cpp

LOCAL_CFLAGS := -D_ANDROID_ -D__LINUX__

LOCAL_C_INCLUDES := \
    frameworks/av/include \
    frameworks/base/include \
    frameworks/base/include/utils \
    vendor/marvell/generic/frameworks/base/native/include \
    vendor/marvell/generic/frameworks/resource_manager/include \
    vendor/marvell/generic/frameworks/resource_manager/libs/berlin \
    $(VENDOR_SDK_INCLUDES)

LOCAL_SHARED_LIBRARIES  := \
    libbinder \
    librmclient \
    librmservice \
    libstagefright_foundation \
    libutils \
    liblog \
    libmvutils

include $(BUILD_NATIVE_TEST)
