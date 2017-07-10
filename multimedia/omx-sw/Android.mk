#close this module because not pass normal build
ifeq (false, true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=                    \
  omx_core/OmxComponentFactory.cpp    \
  omx_core/OmxCore.cpp                \
  omx_common/BerlinOmxLog.cpp         \
  omx_common/mmu_eventinfo.cpp        \
  omx_common/mmu_hash.cpp             \
  omx_common/mmu_mvalue.cpp            \
  omx_common/EventQueue.cpp           \
  omx_common/FFMpegAudioDecoder.cpp   \
  omx_common/FFMpegVideoDecoder.cpp   \
  omx_components/OmxComponent.cpp     \
  omx_components/OmxComponentImpl.cpp \
  omx_components/OmxPortImpl.cpp      \
  omx_components/OmxVideoDecoder.cpp  \
  omx_components/OmxVideoPort.cpp     \
  omx_components/OmxAudioDecoder.cpp  \
  omx_components/OmxAudioPort.cpp

LOCAL_MODULE := libomx-sw
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

LOCAL_CFLAGS := -D_ANDROID_ -D__LINUX__

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=

LOCAL_SHARED_LIBRARIES :=   \
  liblog                    \
  libstagefright_foundation \
  libstlport                \
  libutils                  \
  libffmpeg_avutil          \
  libffmpeg_avcodec         \
  libffmpeg_avformat        \
  libffmpeg_swresample      \
  libffmpeg_swscale         \
  libgui                    \
  libui


LOCAL_C_INCLUDES :=                                        \
  $(LOCAL_PATH)/include                                    \
  bionic                                                   \
  external/stlport/stlport                                 \
  frameworks/native/include/gui                            \
  frameworks/native/include/ui                             \
  frameworks/native/include/media/hardware                 \
  vendor/marvell/generic/frameworks/omx/include/kronos_1.1 \
  vendor/tv/external/ffmpeg

include $(BUILD_SHARED_LIBRARY)

endif
