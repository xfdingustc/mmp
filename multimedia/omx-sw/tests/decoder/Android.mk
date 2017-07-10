LOCAL_PATH:= $(call my-dir)
INTERNAL_WERROR_BLACKLIST += $(LOCAL_PATH)
#=============================================================================

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/../../include \
  vendor/marvell/generic/frameworks/omx/include/kronos_1.1

ifeq ($(TARGET_PRODUCT_CATEGORY),GTV)
LOCAL_C_INCLUDES += \
  vendor/tv/external/ffmpeg
endif

ifeq ($(TARGET_PRODUCT_CATEGORY),A3CE)
LOCAL_C_INCLUDES += \
  vendor/marvell/external/ffmpeg
endif

LOCAL_CFLAGS            += -D_ANDROID_ -D__STDC_CONSTANT_MACROS -D__LINUX__

LOCAL_MODULE_TAGS       := tests
LOCAL_MODULE            := SwOmxDecoderTest

LOCAL_SHARED_LIBRARIES  := \
  libutils \
  libffmpeg_avcodec \
  libffmpeg_avformat \
  libffmpeg_avutil


LOCAL_SHARED_LIBRARIES  += liblog
LOCAL_SHARED_LIBRARIES  += libomx-sw

LOCAL_SRC_FILES         := SwOmxDecoderTest.cpp

include $(BUILD_NATIVE_TEST)



