/*
**                Copyright 2013, MARVELL SEMICONDUCTOR, LTD.
** THIS CODE CONTAINS CONFIDENTIAL INFORMATION OF MARVELL.
** NO RIGHTS ARE GRANTED HEREIN UNDER ANY PATENT, MASK WORK RIGHT OR COPYRIGHT
** OF MARVELL OR ANY THIRD PARTY. MARVELL RESERVES THE RIGHT AT ITS SOLE
** DISCRETION TO REQUEST THAT THIS CODE BE IMMEDIATELY RETURNED TO MARVELL.
** THIS CODE IS PROVIDED "AS IS". MARVELL MAKES NO WARRANTIES, EXPRESSED,
** IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY, COMPLETENESS OR PERFORMANCE.
**
** MARVELL COMPRISES MARVELL TECHNOLOGY GROUP LTD. (MTGL) AND ITS SUBSIDIARIES,
** MARVELL INTERNATIONAL LTD. (MIL), MARVELL TECHNOLOGY, INC. (MTI), MARVELL
** SEMICONDUCTOR, INC. (MSI), MARVELL ASIA PTE LTD. (MAPL), MARVELL JAPAN K.K.
** (MJKK), MARVELL ISRAEL LTD. (MSIL).
*/

#define LOG_TAG "mv_audio_hw"

#include <cutils/log.h>
#include <cutils/properties.h>

#include <OutputControl.h>

using namespace android;
using marvell::OutputControl;

static sp<OutputControl> gOutputControl = NULL;

extern "C" bool isLowlatncy() {
  // Add low latency mode for ubitus
  char prop_value[PROPERTY_VALUE_MAX];
  if (property_get("service.media.low_latency", prop_value, NULL) > 0) {
    ALOGD("Get low latency value as %s.", prop_value);
    if (strcasecmp("true", prop_value) == 0) {
      return true;
    } else {
      return false;
    }
  } else {
      ALOGV("Can't get lowlatency mode.");
      return false;
  }
}
extern "C" int setMasterVolume(float volume) {
    ALOGD("setMasterVolume(%f)", volume);
    if (gOutputControl == NULL) {
        gOutputControl = new OutputControl();
    }
    status_t ret = gOutputControl->setMasterVolume(volume);
    if (ret != android::OK) {
        ALOGE("set master volume fail, volume=%f, ret=%d", volume, ret);
        return 1;
    }
    ALOGD("set master volume success, volume=%f", volume);
    return 0;
}

extern "C" int getMasterVolume(float* volume) {
    ALOGD("getMasterVolume(%p)", volume);
    if (!volume) {
        ALOGE("invalid parameter");
        return 1;
    }
    if (gOutputControl == NULL) {
        gOutputControl = new OutputControl();
    }
    status_t ret = gOutputControl->getMasterVolume(volume);
    if (ret != android::OK) {
        ALOGE("get master volume fail, volume=%p, ret=%d", volume, ret);
        return 1;
    }
    ALOGD("get master volume success, volume=%f", *volume);
    return 0;
}

