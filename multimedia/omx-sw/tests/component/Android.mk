LOCAL_PATH:= $(call my-dir)
INTERNAL_WERROR_BLACKLIST += $(LOCAL_PATH)
#=============================================================================

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := \
  $(LOCAL_PATH)/../../include \
  vendor/marvell/generic/frameworks/omx/include/kronos_1.1

LOCAL_CFLAGS            += -D_ANDROID_ -D__LINUX__

LOCAL_MODULE_TAGS       := tests
LOCAL_MODULE            := SwOmxCompTest

LOCAL_SHARED_LIBRARIES  := libutils
LOCAL_SHARED_LIBRARIES  += liblog
LOCAL_SHARED_LIBRARIES  += libomx-sw

LOCAL_SRC_FILES         := SwOmxCompTest.cpp

include $(BUILD_NATIVE_TEST)
