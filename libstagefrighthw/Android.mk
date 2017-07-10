LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  MVBerlinOMXPlugin.cpp \

LOCAL_CFLAGS := $(PV_CFLAGS_MINUS_VISIBILITY)

LOCAL_C_INCLUDES:= \
  frameworks/native/include/media/hardware \
  frameworks/native/include/media/openmax \

LOCAL_SHARED_LIBRARIES := \
  libbinder \
  libcutils \
  libdl \
  libui \
  libutils \
  libstagefright_foundation \

LOCAL_MODULE := libstagefrighthw
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
