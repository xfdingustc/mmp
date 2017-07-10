LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES:= \
	IOnlineDebug.cpp

#	MediaPlayerOnlineDebug.cpp

LOCAL_SHARED_LIBRARIES := \
    libbinder \
    libcutils \
    libutils

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libonline_debug
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := OnlineDebugTools.cpp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := OnlineDebugTool
LOCAL_SHARED_LIBRARIES := \
	libonline_debug \
    libbinder \
    libcutils \
    libutils

include $(BUILD_EXECUTABLE)

