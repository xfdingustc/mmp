LOCAL_PATH := $(call my-dir)

MRVL_FILE_PLAYER := $(LOCAL_PATH)

include $(CLEAR_VARS)

ifeq ($(PLATFORM_SDK_VERSION),19)

include $(MRVL_FILE_PLAYER)/player/Android.mk
include $(MRVL_FILE_PLAYER)/player/OnlineDebug/Android.mk
include $(MRVL_FILE_PLAYER)/player/datasource/httplive/Android.mk

endif
