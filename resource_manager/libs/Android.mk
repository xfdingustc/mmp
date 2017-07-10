LOCAL_PATH:= $(call my-dir)

# ================================================================
# librmclient
# ================================================================
include $(CLEAR_VARS)

LOCAL_MODULE := librmclient

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    IRMClient.cpp \
    IRMService.cpp \
    RMClient.cpp \
    RMClientListener.cpp \
    RMRequest.cpp \
    RMResponse.cpp

LOCAL_C_INCLUDES := \
    frameworks/base/include/utils \
    vendor/marvell/generic/frameworks/resource_manager/include \
    vendor/marvell/generic/frameworks/base/native/include

LOCAL_SHARED_LIBRARIES := \
    libbinder \
    libcutils \
    libstagefright_foundation \
    libutils \
    libmvutils

# TODO: remove this once resource manager is stable enough.
LOCAL_STRIP_MODULE := false

include $(BUILD_SHARED_LIBRARY)

# ================================================================
# librmservice
# ================================================================
include $(CLEAR_VARS)

LOCAL_MODULE := librmservice

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    IRMClient.cpp \
    IRMService.cpp \
    RMAllocator.cpp \
    RMClientListener.cpp \
    RMRequest.cpp \
    RMResponse.cpp \
    RMService.cpp \
    berlin/MVBerlinRMAllocator.cpp

LOCAL_C_INCLUDES := \
    frameworks/base/include/utils \
    vendor/marvell/generic/frameworks/resource_manager/include \
    vendor/marvell/generic/frameworks/resource_manager/libs/berlin \
    vendor/marvell/generic/frameworks/base/native/include \
    $(VENDOR_SDK_INCLUDES)

LOCAL_CFLAGS := -D_ANDROID_ -D__LINUX__

ifeq ($(MV_RM_DISABLE_PIP),TRUE)
LOCAL_CFLAGS += -DDISABLE_PIP
endif

LOCAL_SHARED_LIBRARIES := \
    libbinder \
    libcutils \
    librmclient \
    libstagefright_foundation \
    libutils
 
# TODO: remove this once resource manager is stable enough.
LOCAL_STRIP_MODULE := false

include $(BUILD_SHARED_LIBRARY)

# ================================================================
# resourcemanager
# ================================================================
include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
    frameworks/base/include/utils \
    vendor/marvell/generic/frameworks/resource_manager/include \
    vendor/marvell/generic/frameworks/resource_manager/libs/berlin \
    vendor/marvell/generic/frameworks/base/native/include
LOCAL_SRC_FILES += cmds/resourcemanager.cpp
LOCAL_SHARED_LIBRARIES += libbinder librmservice libutils liblog
LOCAL_MODULE := resourcemanager
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
