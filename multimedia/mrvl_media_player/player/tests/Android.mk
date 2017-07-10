LOCAL_PATH:= $(call my-dir)
### MRVLFFPlayer multi instances test #########

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= ffplayertest.cpp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= mrvl_ffplayer_test

LOCAL_SHARED_LIBRARIES := \
	libmrvl_media_player \
	libglib-2.0 \
	libgthread-2.0 \
	libgmodule-2.0 \
	libgobject-2.0 \
	libavutil \
	liblog \
	libavcodec \
	libavformat \
	libswscale \
	libOSAL \
	libDrmApp \
	libDivXPlayer \
	libDivXMediaFormat \
	libDivXDrm \
	libbinder \
	libmedia


ifeq ($(BOARD_MV88DE3100), true)
    LOCAL_SHARED_LIBRARIES += \
        libPESingleCPU
endif

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/../ \
    $(LOCAL_PATH)/../OnlineDebug \
    external/ffmpeg/libavcodec \
    external/ffmpeg/libavformat \
    external/ffmpeg/libavutil \
    external/ffmpeg/ \
    external/glib \
    external/glib/android \
    external/glib/glib


ifeq ($(BOARD_MV88DE3100), true)
    LOCAL_C_INCLUDES += \
        $(VENDOR_MARVELL_GALOIS_INC)
endif

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

include $(BUILD_EXECUTABLE)



### MediaPlayer test #######

include $(CLEAR_VARS)

LOCAL_SRC_FILES := MediaPlayerTest.cpp
LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE := media_player_test

LOCAL_SHARED_LIBRARIES := \
    libglib-2.0 \
    libgthread-2.0 \
    libgmodule-2.0 \
    libgobject-2.0 \
    libutils \
    libcutils \
    libbinder \
    libmedia \
    libgui \
    libmrvl_media_player \
    libmrvlplayer \
    libmrvl_media_lib \
    libavutil \
    libavcodec \
    libavformat \
    libswscale \
    libOSAL \
    libDrmApp \
    libstagefright \
    libstagefright_foundation \
    libonline_debug

ifeq ($(BOARD_MV88DE3100), true)
    LOCAL_SHARED_LIBRARIES += \
        libPESingleCPU
endif

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/../ \
    frameworks/base/include \
    external/ffmpeg/libavcodec \
    external/ffmpeg/libavformat \
    external/ffmpeg/libavutil \
    external/ffmpeg/ \
    external/glib \
    external/glib/android \
    external/glib/glib


ifeq ($(BOARD_MV88DE3100), true)
    LOCAL_C_INCLUDES += \
        $(VENDOR_MARVELL_GALOIS_INC)
endif

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

include $(BUILD_EXECUTABLE)

