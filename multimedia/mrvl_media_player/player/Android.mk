LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

ENABLE_MRVL_A3CE := true
ENABLE_STAGEFRIGHT_HLS := true
HAS_RESOURCE_MANAGER := flase
#
#resource manager is enabled in A3CE
#add this limitation so that MMP can pass build under various Android version
#
$(warning $(TARGET_PRODUCT_CATEGORY))
ifeq ($(TARGET_PRODUCT_CATEGORY),A3CE)
    HAS_RESOURCE_MANAGER := true
endif

LOCAL_SRC_FILES :=                                         \
    MmpMediaDefs.cpp                                       \
    MRVLMediaPlayer.cpp                                    \
    MRVLFFPlayer.cpp                                       \
    CorePlayer.cpp                                         \
    datasource/RawDataSource.cpp                           \
    datasource/HttpDataSource.cpp                          \
    datasource/http_source/buffers.cpp                     \
    datasource/http_source/curl_transfer_engine.cpp        \
    datasource/http_source/curl_transfer_engine_thread.cpp \
    datasource/http_source/a3ce_curl_transfer_engine.cpp   \
    datasource/http_source/thread_utils.cpp                \
    datasource/FileDataSource.cpp                          \
    demuxer/FFMPEGDataReader.cpp                           \
    demuxer/TimedTextDemux.cpp                             \
    demuxer/formatters/AVCCFormatter.cpp                   \
    demuxer/formatters/RVFormatter.cpp                     \
    OnlineDebug/MediaPlayerOnlineDebug.cpp                 \
    omx/BerlinOmxProxy.cpp                                 \
    omx/MHALOmxComponent.cpp                               \
    omx/MHALDecoderComponent.cpp                           \
    omx/MHALVideoDecoder.cpp                               \
    omx/MHALVideoRenderer.cpp                              \
    omx/MHALVideoProcessor.cpp                             \
    omx/MHALVideoScheduler.cpp                             \
    omx/MHALAudioDecoder.cpp                               \
    omx/MHALAudioRenderer.cpp                              \
    omx/MHALClock.cpp                                      \
    omx/MHALOmxUtils.cpp                                   \
    subtitle/MHALSubtitleEngine.cpp                        \
    subtitle/ASSParser.cpp                                 \
    subtitle/TimedText3GPPParser.cpp                       \
    render/AndroidAudioOutput.cpp                          \
    render/AndroidVideoOutput.cpp                          \
    render/MmpSystemClock.cpp

#####################################
# Add frameworks
#####################################
LOCAL_SRC_FILES +=                                         \
    frameworks/mmp_buffer.cpp                              \
    frameworks/mmp_bufferlist.cpp                          \
    frameworks/mmp_caps.cpp                                \
    frameworks/mmp_element.cpp                             \
    frameworks/mmp_pad.cpp


ifeq ($(ENABLE_STAGEFRIGHT_HLS), true)
    LOCAL_SRC_FILES +=  \
        datasource/HttpLiveDataSource.cpp
endif


LOCAL_SHARED_LIBRARIES :=     \
    libonline_debug           \
    libmrvl_httplive

LOCAL_SHARED_LIBRARIES +=     \
    libutils                  \
    libcutils                 \
    libbinder                 \
    libffmpeg_avutil          \
    libffmpeg_avcodec         \
    libffmpeg_avformat        \
    libcurl                   \
    libmmu                    \
    libmedia                  \
    libstagefright            \
    libstagefright_foundation \
    libicui18n                \
    libicuuc                  \
    libstlport                \
    libdl                     \
    libopenkode               \
    libEGL                    \
    libGLESv2                 \
    libgui                    \
    libui                     \
    libskia


ifeq ($(HAS_RESOURCE_MANAGER), true)
    LOCAL_SHARED_LIBRARIES += \
        libresourcemanager
endif

LOCAL_C_INCLUDES :=                                          \
    $(VENDOR_SDK_INCLUDES)                                   \
    $(LOCAL_PATH)                                            \
    $(LOCAL_PATH)/OnlineDebug                                \
    $(LOCAL_PATH)/datasource                                 \
    $(LOCAL_PATH)/datasource/http_source                     \
    $(LOCAL_PATH)/omx                                        \
    $(LOCAL_PATH)/subtitle                                   \
    $(LOCAL_PATH)/frameworks                                 \
    $(LOCAL_PATH)/../../base_utils/include


ifeq ($(ENABLE_STAGEFRIGHT_HLS), true)
    LOCAL_C_INCLUDES +=  \
        $(LOCAL_PATH)/datasource/httplive
endif

LOCAL_C_INCLUDES +=                                          \
    bionic                                                   \
    external/icu4c/i18n                                      \
    external/icu4c/common                                    \
    external/stlport/stlport                                 \
    external/skia/include/core                               \
    external/skia/include/effects                            \
    external/skia/src/core                                   \
    frameworks/av/include/media                              \
    frameworks/av/media/libstagefright                       \
    frameworks/native/include/gui                            \
    frameworks/native/include/ui                             \
    frameworks/native/include/media/hardware                 \
    frameworks/native/include/utils                          \
    system/core/include/utils                                \
    vendor/marvell/external/curl/include                     \
    vendor/marvell/external/openkode/include                 \
    vendor/marvell/external/ffmpeg                           \
    vendor/marvell/external/ffmpeg/libavcodec                \
    vendor/marvell/external/ffmpeg/libavformat               \
    vendor/marvell/external/ffmpeg/libavutil                 \
    vendor/marvell/generic/frameworks/multimedia/hardware    \
    vendor/marvell/generic/frameworks/omx/include            \
    vendor/marvell/generic/frameworks/omx/include/extension  \
    vendor/marvell/generic/frameworks/omx/include/kronos_1.2



ifeq ($(HAS_RESOURCE_MANAGER), true)
    LOCAL_C_INCLUDES +=  \
        vendor/marvell/a3ce/frameworks/resource_manager/include
endif

LOCAL_CFLAGS := -D_ANDROID_ -D__LINUX__
LOCAL_ARM_MODE := arm

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

ifeq ($(ENABLE_STAGEFRIGHT_HLS), true)
    LOCAL_CFLAGS += -DENABLE_HLS
endif

ifeq ($(ENABLE_MRVL_A3CE), true)
    LOCAL_CFLAGS += -DANDROID_A3CE
endif

ifeq ($(HAS_RESOURCE_MANAGER), true)
    LOCAL_CFLAGS += -DENABLE_RESOURCE_MANAGER
endif

ifeq ($(TARGET_PRODUCT_CATEGORY),A3CE)
    LOCAL_CFLAGS += -DGRALLOC_EXT
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= libmrvl_media_player

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
