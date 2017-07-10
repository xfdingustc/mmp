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

#include "MHALAudioRenderer.h"
#include "MHALOmxUtils.h"

#undef  LOG_TAG
#define LOG_TAG "MHALAudioRenderer"

namespace mmp {

MHALAudioRenderer::MHALAudioRenderer(MHALOmxCallbackTarget *cb_target) {
  OMX_LOGD("ENTER");
  mCallbackTarget_ = cb_target;
  memset(compName, 0x00, 64);
  sprintf(compName, "\"MHALAudioRenderer\"");
  mNumPorts_ = 2;
  OMX_LOGD("EXIT");
}

MHALAudioRenderer::~MHALAudioRenderer() {
  OMX_LOGD("ENTER");
  OMX_LOGD("EXIT");
}

const char* MHALAudioRenderer::getComponentRole() {
  return "audio_renderer.pcm";
}

// TODO: should we implement set volume for each channel?
OMX_BOOL MHALAudioRenderer::setVolume(float leftVolume, float rightVolume) {
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  if ((leftVolume < 0.0) || (leftVolume > 1.0) ||
      (rightVolume < 0.0) || (rightVolume > 1.0)) {
    return OMX_FALSE;
  }

  OMX_PORT_PARAM_TYPE oParam;
  InitOmxHeader(&oParam);
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamAudioInit, &oParam);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioInit) with error %d", ret);
    return OMX_FALSE;
  }

  int volume = leftVolume * 100 + 0.5;
  OMX_LOGD("Set volume to %d out of 100.", volume);
  OMX_AUDIO_CONFIG_VOLUMETYPE configVolume;
  InitOmxHeader(&configVolume);
  configVolume.nPortIndex = oParam.nStartPortNumber;
  configVolume.bLinear = OMX_TRUE;
  configVolume.sVolume.nMin = 0;
  configVolume.sVolume.nMax = 100;
  configVolume.sVolume.nValue = volume;
  ret = OMX_SetConfig(mCompHandle_, OMX_IndexConfigAudioVolume, &configVolume);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetConfig(OMX_IndexConfigAudioVolume) with error %d", ret);
    return OMX_FALSE;
  }

  return OMX_TRUE;
}

OMX_BOOL MHALAudioRenderer::disablePort(const OMX_U32 portIdx) {
  OMX_ERRORTYPE ret = OMX_ErrorNone;

  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOmxHeader(&def);
  def.nPortIndex = portIdx;
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamPortDefinition, &def);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamPortDefinition) with error %d", ret);
    return OMX_FALSE;
  }

  def.bEnabled = OMX_FALSE;
  ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamPortDefinition, &def);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamPortDefinition) with error %d", ret);
    return OMX_FALSE;
  }

  return OMX_TRUE;
}

OMX_ERRORTYPE MHALAudioRenderer::OnOMXEvent(OMX_EVENTTYPE eEvent,
    OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData) {
  switch (eEvent) {
    case OMX_EventBufferFlag:
      OMX_LOGI("event = OMX_EventBufferFlag, data1 = %lu, data2 = %lu", nData1, nData2);
      if ((nData2 & OMX_BUFFERFLAG_EOS) != 0) {
        OMX_LOGI("EOS received!!!");
        if (mCallbackTarget_) {
          mCallbackTarget_->OnOmxEvent(EVENT_AUDIO_EOS);
        }
      }
      break;
    default:
      return MHALOmxComponent::OnOMXEvent(eEvent, nData1, nData2, pEventData);
  }

  return OMX_ErrorNone;
}

}
