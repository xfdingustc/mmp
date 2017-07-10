LOCAL_PATH:= $(call my-dir)
INTERNAL_WERROR_BLACKLIST += $(LOCAL_PATH)
#=============================================================================

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/../../include \
  vendor/marvell/external/openkode/include \
  frameworks/native/include/media/openmax

LOCAL_CFLAGS            := -DLOG_TAG=\"OmxCoreTest\"
LOCAL_CFLAGS            += -D_ANDROID_ -D__LINUX__

LOCAL_MODULE_TAGS       := tests
LOCAL_MODULE            := OmxCoreTest

LOCAL_SHARED_LIBRARIES  := libutils
LOCAL_SHARED_LIBRARIES  += liblog
# TODO for SoC: replace libOmxCore with SoC OMX library name
#LOCAL_SHARED_LIBRARIES  += libOmxCore
LOCAL_SHARED_LIBRARIES  += libBerlinCore

LOCAL_SRC_FILES         := OmxCoreTest.cpp

include $(BUILD_NATIVE_TEST)
