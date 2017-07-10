LOCAL_PATH:= $(call my-dir)
INTERNAL_WERROR_BLACKLIST += $(LOCAL_PATH)
#=============================================================================

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/../../include \
  vendor/marvell/external/openkode/include \
  frameworks/native/include/media/openmax

LOCAL_CFLAGS            += -D_ANDROID_ -D__STDC_CONSTANT_MACROS

LOCAL_MODULE_TAGS       := tests
LOCAL_MODULE            := OmxEncTest


LOCAL_SHARED_LIBRARIES  += liblog
# TODO for SoC: replace libOmxCore with SoC OMX library name
#LOCAL_SHARED_LIBRARIES  += libOmxCore
LOCAL_SHARED_LIBRARIES  += libBerlinCore libopenkode

LOCAL_SRC_FILES         := OmxEncTest.cpp

include $(BUILD_NATIVE_TEST)
