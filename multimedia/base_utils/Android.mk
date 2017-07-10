LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES +=                                  \
    src/mmu_baseobj.cpp                             \
    src/mmu_hash.cpp                                \
    src/mmu_mvalue.cpp                              \
    src/mmu_mstring.cpp                             \
    src/mmu_thread.cpp

LOCAL_SHARED_LIBRARIES +=     \
    libstlport                \



LOCAL_C_INCLUDES +=                                        \
    bionic    \
    external/stlport/stlport                                 \
    $(LOCAL_PATH)/include


LOCAL_CFLAGS := -D_ANDROID_ -D__LINUX__
LOCAL_ARM_MODE := arm

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)


LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libmmu

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

