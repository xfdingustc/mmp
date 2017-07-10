LOCAL_PATH:= $(call my-dir)
INTERNAL_WERROR_BLACKLIST += $(LOCAL_PATH)
#=============================================================================

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/../../include \
  vendor/marvell/external/openkode/include \
  frameworks/native/include/media/openmax


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
LOCAL_MODULE            := OmxDecTunnelModeTest

LOCAL_SHARED_LIBRARIES  := \
  libutils \
  libopenkode \
  libffmpeg_avcodec \
  libffmpeg_avformat \
  libffmpeg_avutil

LOCAL_SHARED_LIBRARIES  += liblog
# TODO for SoC: replace libOmxCore with SoC OMX library name
#LOCAL_SHARED_LIBRARIES  += libOmxCore
LOCAL_SHARED_LIBRARIES  += libBerlinCore

LOCAL_SRC_FILES         := OmxDecTunnelModeTest.cpp

include $(BUILD_NATIVE_TEST)
