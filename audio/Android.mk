#
#                Copyright 2013, MARVELL SEMICONDUCTOR, LTD.
# THIS CODE CONTAINS CONFIDENTIAL INFORMATION OF MARVELL.
# NO RIGHTS ARE GRANTED HEREIN UNDER ANY PATENT, MASK WORK RIGHT OR COPYRIGHT
# OF MARVELL OR ANY THIRD PARTY. MARVELL RESERVES THE RIGHT AT ITS SOLE
# DISCRETION TO REQUEST THAT THIS CODE BE IMMEDIATELY RETURNED TO MARVELL.
# THIS CODE IS PROVIDED "AS IS". MARVELL MAKES NO WARRANTIES, EXPRESSED,
# IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY, COMPLETENESS OR PERFORMANCE.
#
# MARVELL COMPRISES MARVELL TECHNOLOGY GROUP LTD. (MTGL) AND ITS SUBSIDIARIES,
# MARVELL INTERNATIONAL LTD. (MIL), MARVELL TECHNOLOGY, INC. (MTI), MARVELL
# SEMICONDUCTOR, INC. (MSI), MARVELL ASIA PTE LTD. (MAPL), MARVELL JAPAN K.K.
# (MJKK), MARVELL ISRAEL LTD. (MSIL).
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := audio.primary.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_CFLAGS := -D_ANDROID_ -D__LINUX__

LOCAL_CFLAGS += -DPLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)

LOCAL_SRC_FILES := \
  audio_hw.c \
  audiohwsourcegain.cpp \
  audiotunneling.cpp\
  util.cpp

LOCAL_C_INCLUDES := \
  $(VENDOR_SDK_INCLUDES) \
  external/tinyalsa/include \
  vendor/marvell/generic/frameworks/av_settings/include \
  vendor/marvell/generic/frameworks/av_settings/libs \
  vendor/marvell/generic/frameworks/audioloopback/libaudiohw

ifeq ($(PLATFORM_SDK_VERSION),19)
LOCAL_C_INCLUDES += frameworks/rs/server
endif

ifeq ($(PLATFORM_SDK_VERSION),17)
LOCAL_C_INCLUDES += frameworks/native/include/utils
endif

LOCAL_SHARED_LIBRARIES := \
  libampclient \
  libbinder \
  lib_avsettings \
  lib_avsettings_client \
  libcutils \
  liblog \
  libOSAL \
  libtinyalsa \
  libaudiohw \
  libutils

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
