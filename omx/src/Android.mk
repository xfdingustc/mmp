LOCAL_PATH := $(call my-dir)

$(warning $(TARGET_PRODUCT_CATEGORY))


OMX_HEADER_PATH := $(LOCAL_PATH)/../include/kronos_1.2
OMX_EXT_HEADER_PATH := $(LOCAL_PATH)/../include/extension

TEE_TOP_ABS_PATH := vendor/marvell-sdk/tee
include $(TEE_TOP_ABS_PATH)/build/tee/tee_client_api.mk

# libBerlinCore

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  utils/BerlinOmxLog.cpp \
  utils/BerlinOmxUtils.cpp \
  core/BerlinOmxCore.cpp \
  core/DynamicOmxCore.cpp \
  components/BerlinOmxAmpAudioPort.cpp \
  components/BerlinOmxAmpClockPort.cpp \
  components/BerlinOmxAmpPort.cpp \
  components/BerlinOmxAmpVideoPort.cpp \
  components/BerlinOmxComponent.cpp \
  components/BerlinOmxComponentImpl.cpp \
  components/BerlinOmxPortImpl.cpp \
  components/BerlinOmxVoutProxy.cpp \
  utils/mediahelper/src/mediahelper.c \
  utils/BerlinOmxFpsCalculator.cpp \
  utils/BerlinOmxParseXmlCfg.cpp


LOCAL_MODULE := libBerlinCore
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_PRELINK_MODULE := false
LOCAL_CFLAGS := -D_ANDROID_ -D__LINUX__ -DTARGET_LITTLE_ENDIAN=1
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)
ifneq ($(TARGET_PRODUCT),berlin_generic)
ifeq ($(TARGET_PRODUCT_CATEGORY),GTV)
LOCAL_CFLAGS += -DOMX_CORE_EXT -DVIDEO_DECODE_ONE_FRAME_MODE -DVOUT_SEND_DISPLAYED
endif
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := \
  libteec

LOCAL_SHARED_LIBRARIES := \
  libdl \
  liblog \
  libopenkode \
  libstagefright_foundation \
  libutils \
  libexpat

LOCAL_SHARED_LIBRARIES += libampclient libOSAL

LOCAL_SHARED_LIBRARIES += libstlport

LOCAL_C_INCLUDES :=                         \
  $(OMX_HEADER_PATH)                        \
  $(OMX_EXT_HEADER_PATH)                    \
  $(LOCAL_PATH)/../include                  \
  vendor/marvell/external/openkode/include  \
  $(LOCAL_PATH)/utils/mediahelper/include   \
  bionic                                    \
  external/stlport/stlport                  \
  frameworks/native/include/media/hardware  \
  frameworks/native/include  \
  system/core/include  \
  $(TARGET_MARVELL_AMP_TOP)/include         \
  $(TARGET_MARVELL_AMP_TOP)/include/isl   \
  $(TEEC_ABS_INCLUDES)


ifneq ($(TARGET_PRODUCT),sequoia)
LOCAL_CFLAGS += -DKEEP_LAST_FRAME
endif


ifeq ($(TARGET_PRODUCT_CATEGORY),A3CE)
LOCAL_CFLAGS += -DDISABLE_CHECK_HEADER -DENABLE_DTS -DENABLE_EXT_RA
endif

#open this marco if you want to disable playready support
#LOCAL_CFLAGS += -DDISABLE_PLAYREADY

include $(BUILD_SHARED_LIBRARY)


#libBerlinVideo

include $(CLEAR_VARS)
INTERNAL_WERROR_BLACKLIST += $(LOCAL_PATH)

LOCAL_SRC_FILES := \
  components/BerlinOmxAmpVideoDecoder.cpp \
  components/BerlinOmxVideoEncoder.cpp \
  components/BerlinOmxVideoProcessor.cpp \
  components/BerlinOmxVideoRender.cpp \
  components/BerlinOmxVideoScheduler.cpp \
  components/BerlinOmxCreateVideoComponent.cpp

LOCAL_MODULE := libBerlinVideo
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_PRELINK_MODULE := false
LOCAL_CFLAGS := -D_ANDROID_ -D__LINUX__ -DTARGET_LITTLE_ENDIAN=1
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)
ifneq ($(TARGET_PRODUCT),berlin_generic)
ifeq ($(TARGET_PRODUCT_CATEGORY),GTV)
LOCAL_CFLAGS += -DVIDEO_DECODE_ONE_FRAME_MODE
endif
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES := \
  libteec

LOCAL_SHARED_LIBRARIES := \
  libBerlinCore \
  libcutils \
  liblog \
  libopenkode \
  libstlport \
  libui \
  libutils \
  libbinder \
  lib_avsettings \
  lib_avsettings_impl \
  lib_avsettings_client

LOCAL_SHARED_LIBRARIES += libampclient libOSAL libdrmclient
LOCAL_STATIC_LIBRARIES += libyuv_static

LOCAL_C_INCLUDES :=                                     \
  $(OMX_HEADER_PATH)                                    \
  $(OMX_EXT_HEADER_PATH)                                \
  $(LOCAL_PATH)/../include                              \
  vendor/marvell/external/openkode/include              \
  vendor/marvell/generic/frameworks/av_settings/include \
  $(LOCAL_PATH)/utils/mediahelper/include               \
  bionic                                                \
  external/libyuv/files/include                         \
  external/stlport/stlport                              \
  frameworks/native/include/media/hardware              \
  frameworks/native/include                 \
  system/core/include                \
  $(TARGET_MARVELL_AMP_TOP)/include                     \
  $(TARGET_MARVELL_AMP_TOP)/include/isl                    \
  $(TEEC_ABS_INCLUDES)


ifeq ($(TARGET_PRODUCT_CATEGORY),GTV)
LOCAL_C_INCLUDES += \
  vendor/marvell/gtv/frameworks/resource_manager \
  vendor/tv/frameworks/base/include
LOCAL_CFLAGS += -D_OMX_GTV_ -DWMDRM_SUPPORT
endif

ifeq ($(TARGET_PRODUCT_CATEGORY),A3CE)
LOCAL_CFLAGS += -DNOTIFY_RESOLUTION_CHANGE
endif

#open this marco if you want to disable playready support
#LOCAL_CFLAGS += -DDISABLE_PLAYREADY

include $(BUILD_SHARED_LIBRARY)



#libBerlinAudio

include $(CLEAR_VARS)
INTERNAL_WERROR_BLACKLIST += $(LOCAL_PATH)

LOCAL_SRC_FILES := \
  components/BerlinOmxAmpAudioDecoder.cpp \
  components/BerlinOmxAmpAudioRenderer.cpp \
  components/BerlinOmxCreateAudioComponent.cpp

LOCAL_MODULE := libBerlinAudio
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_PRELINK_MODULE := false
LOCAL_CFLAGS := -D_ANDROID_ -D__LINUX__ -DTARGET_LITTLE_ENDIAN=1
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=

LOCAL_SHARED_LIBRARIES := \
  libBerlinCore \
  liblog \
  libopenkode \
  libstlport \
  libui \
  libutils \
  libbinder \
  lib_avsettings \
  lib_avsettings_client

LOCAL_SHARED_LIBRARIES += libampclient libOSAL libdrmclient

LOCAL_C_INCLUDES := \
  $(OMX_HEADER_PATH) \
  $(OMX_EXT_HEADER_PATH) \
  $(LOCAL_PATH)/../include \
  vendor/marvell/external/openkode/include \
  vendor/marvell/generic/frameworks/av_settings/include \
  vendor/marvell/generic/frameworks/av_settings/libs \
  $(LOCAL_PATH)/utils/mediahelper/include \
  bionic \
  external/stlport/stlport \
  frameworks/native/include/media/hardware \
  frameworks/native/include  \
  system/core/include  \
  $(TARGET_MARVELL_AMP_TOP)/include \
  $(TARGET_MARVELL_AMP_TOP)/include/isl \
  $(TEEC_ABS_INCLUDES)

ifeq ($(TARGET_PRODUCT_CATEGORY),GTV)
LOCAL_CFLAGS += -D_OMX_GTV_ -DWMDRM_SUPPORT
endif

ifeq ($(TARGET_PRODUCT_CATEGORY),A3CE)
LOCAL_CFLAGS += -DDISCONNECT_AREN_CLOCK -DDISCONNECT_AREN_ADEC -DENABLE_LPCM_BLURAY  -DENABLE_EXT_RA
endif

#open this marco if you want to disable playready support
#LOCAL_CFLAGS += -DDISABLE_PLAYREADY

include $(BUILD_SHARED_LIBRARY)


#libBerlinClock

include $(CLEAR_VARS)
INTERNAL_WERROR_BLACKLIST += $(LOCAL_PATH)

LOCAL_SRC_FILES := \
  components/BerlinOmxAmpClock.cpp

LOCAL_MODULE := libBerlinClock
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)
LOCAL_PRELINK_MODULE := false
LOCAL_CFLAGS := -D_ANDROID_ -D__LINUX__ -DTARGET_LITTLE_ENDIAN=1
LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)
ifneq ($(TARGET_PRODUCT),berlin_generic)
ifeq ($(TARGET_PRODUCT_CATEGORY),GTV)
LOCAL_CFLAGS += -DOMX_CORE_EXT -DCLOCK_AV_SYNC_OPTION
endif
endif

LOCAL_ARM_MODE := arm

LOCAL_STATIC_LIBRARIES :=

LOCAL_SHARED_LIBRARIES := \
  libBerlinCore \
  liblog \
  libopenkode \
  libstlport \
  libui \
  libutils

ifeq ($(PLATFORM_SDK_VERSION),19)
LOCAL_SHARED_LIBRARIES += libMVutils
endif

LOCAL_SHARED_LIBRARIES += libampclient

LOCAL_C_INCLUDES := \
  $(OMX_HEADER_PATH) \
  $(OMX_EXT_HEADER_PATH) \
  $(LOCAL_PATH)/../include \
  vendor/marvell/external/openkode/include \
  bionic \
  external/stlport/stlport \
  frameworks/native/include/media/hardware \
  frameworks/native/include  \
  system/core/include  \
  $(TARGET_MARVELL_AMP_TOP)/include \
  $(TARGET_MARVELL_AMP_TOP)/include/isl \
  $(TEEC_ABS_INCLUDES)

ifneq ($(PLATFORM_SDK_VERSION),19)
LOCAL_C_INCLUDES += \
  frameworks/native/include/utils
else
LOCAL_C_INCLUDES += \
  vendor/marvell/generic/frameworks/utils
endif

ifeq ($(TARGET_PRODUCT_CATEGORY),A3CE)
endif

#open this marco if you want to disable playready support
#LOCAL_CFLAGS += -DDISABLE_PLAYREADY

include $(BUILD_SHARED_LIBRARY)

