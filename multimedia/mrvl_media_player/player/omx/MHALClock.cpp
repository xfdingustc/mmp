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

#include "MHALClock.h"

#undef  LOG_TAG
#define LOG_TAG "MHALClock"

namespace mmp {

MHALClock::MHALClock(MHALOmxCallbackTarget *cb_target) {
  OMX_LOGD("ENTER");
  mCallbackTarget_ = cb_target;
  memset(compName, 0x00, 64);
  sprintf(compName, "\"MHALClock\"");
  mNumPorts_ = 2;
  OMX_LOGD("EXIT");
}

MHALClock::~MHALClock(){
  OMX_LOGD("ENTER");
  OMX_LOGD("EXIT");
}

const char* MHALClock::getComponentRole() {
  return "clock.binary";
}

void MHALClock::stopClock() {
  OMX_LOGD("ENTER");
  OMX_TIME_CONFIG_CLOCKSTATETYPE conf;
  InitOmxHeader(&conf);
  conf.eState = OMX_TIME_ClockStateStopped;
  OMX_ERRORTYPE err = OMX_SetConfig(mCompHandle_, OMX_IndexConfigTimeClockState, &conf);
  if (err != OMX_ErrorNone) {
    // TODO: error handling
    OMX_LOGE("OMX_SetConfig(OMX_IndexConfigTimeClockState) failed with error %d", err);
  }
  OMX_LOGD("EXIT");
}

void MHALClock::startClock(MmpClockTime startTimeUs) {
  OMX_LOGD("ENTER, startTimeUs = %lld us", startTimeUs);
  OMX_TIME_CONFIG_CLOCKSTATETYPE conf;
  InitOmxHeader(&conf);
  conf.eState = OMX_TIME_ClockStateRunning;
  conf.nStartTime = static_cast<OMX_TICKS>(startTimeUs);
  conf.nOffset = static_cast<OMX_TICKS>(0);
  OMX_ERRORTYPE err = OMX_SetConfig(mCompHandle_, OMX_IndexConfigTimeClockState, &conf);
  if (err != OMX_ErrorNone) {
    // TODO: error handling
    OMX_LOGE("OMX_SetConfig(OMX_IndexConfigTimeClockState) failed with error %d", err);
  }
  OMX_LOGD("EXIT");
}

void MHALClock::setScale(mdouble scale) {
  OMX_LOGD("ENTER, scale = %f", scale);
  OMX_TIME_CONFIG_SCALETYPE conf;
  InitOmxHeader(&conf);
  conf.xScale = static_cast<OMX_S32>(scale * 0x10000);
  if (conf.xScale != 0x0 && conf.xScale != 0x10000) {
    // TODO: Support trick play.
    OMX_LOGE("Trick play is not supported: scale = %lf", scale);
    return;
  }
  OMX_ERRORTYPE err = OMX_SetConfig(mCompHandle_, OMX_IndexConfigTimeScale, &conf);
  if (err != OMX_ErrorNone) {
    // TODO: error handling
    OMX_LOGE("OMX_SetConfig(OMX_IndexConfigTimeScale) failed with error %d", err);
  }
  OMX_LOGD("EXIT");
}

uint64_t MHALClock::getCurrentMediaTime() {
  OMX_TIME_CONFIG_TIMESTAMPTYPE conf;
  InitOmxHeader(&conf);
  conf.nPortIndex = OMX_ALL;
  OMX_ERRORTYPE err = OMX_GetConfig(mCompHandle_, OMX_IndexConfigTimeCurrentMediaTime, &conf);
  if (err != OMX_ErrorNone) {
    // TODO: error handling
    OMX_LOGE("OMX_GetConfig(OMX_IndexConfigTimeCurrentMediaTime) failed with error %d", err);
  }
  OMX_LOGV("CurrentMediaTime = %lld", conf.nTimestamp);
  return (uint64_t)conf.nTimestamp;
}

OMX_ERRORTYPE MHALClock::OnOMXEvent(OMX_EVENTTYPE eEvent,
    OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData) {
  switch (eEvent) {
    case OMX_EventRendererFpsInfo:
      OMX_LOGI("event = OMX_EventRendererFpsInfo, data1 = %lu, data2 = %lu", nData1, nData2);
      if (mCallbackTarget_) {
        mCallbackTarget_->OnOmxEvent(EVENT_FPS_INFO, nData1, nData2);
      }
      break;
    default:
      return MHALOmxComponent::OnOMXEvent(eEvent, nData1, nData2, pEventData);
  }

  return OMX_ErrorNone;
}

}
