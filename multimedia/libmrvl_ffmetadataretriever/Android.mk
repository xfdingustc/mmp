LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    RawDataSource.cpp  \
    FileDataSource.cpp  \
    MRVLFFMetadataRetriever.cpp

LOCAL_SHARED_LIBRARIES := \
    libffmpeg_avcodec \
    libffmpeg_avformat \
    libffmpeg_avutil \
    libmedia \
    libstagefright \
    libutils

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libmrvl_metadataretriever

LOCAL_C_INCLUDES := \
    frameworks/native/include/media \
    frameworks/native/include/media/openmax \
    frameworks/av/media/libstagefright \
    frameworks/av/media/libstagefright/include \

ifeq ($(TARGET_PRODUCT_CATEGORY),GTV)
LOCAL_C_INCLUDES += \
    vendor/tv/external/ffmpeg
endif

ifeq ($(TARGET_PRODUCT_CATEGORY),A3CE)
LOCAL_C_INCLUDES += \
    vendor/marvell/a3ce/external/ffmpeg
endif

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)

