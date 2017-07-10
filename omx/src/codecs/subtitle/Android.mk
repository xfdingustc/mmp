LOCAL_PATH := $(call my-dir)

#libBerlinSubtitleDecoder.so

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    src/BerlinOmxSrtDecoder.cpp \
    src/StringConvertor.cpp



LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/../../../include \
    $(LOCAL_PATH)/../../../include/kronos_1.2

LOCAL_MODULE := libBerlinSubtitleDecoder
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_PRELINK_MODULE := false
LOCAL_CFLAGS := -D_ANDROID_ -D__LINUX__

include $(BUILD_SHARED_LIBRARY)
