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

#include "MHALVideoRenderer.h"

#undef  LOG_TAG
#define LOG_TAG "MHALVideoRenderer"

namespace mmp {

MHALVideoRenderer::MHALVideoRenderer(MHALOmxCallbackTarget *cb_target) {
  OMX_LOGD("ENTER");
  mCallbackTarget_ = cb_target;
  memset(compName, 0x00, 64);
  sprintf(compName, "\"MHALVideoRenderer\"");
  mNumPorts_ = 1;
  OMX_LOGD("EXIT");
}

MHALVideoRenderer::~MHALVideoRenderer(){
  OMX_LOGD("ENTER");
  OMX_LOGD("EXIT");
}

const char* MHALVideoRenderer::getComponentRole() {
  return "iv_renderer.yuv.overlay";
}

void MHALVideoRenderer::setGeometry(int x, int y, int width, int height) {
  OMX_LOGD("ENTER");
  OMX_LOGD("EXIT");
}

OMX_ERRORTYPE MHALVideoRenderer::OnOMXEvent(OMX_EVENTTYPE eEvent,
    OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData) {
  switch (eEvent) {
    case OMX_EventBufferFlag:
      OMX_LOGI("event = OMX_EventBufferFlag, data1 = %lu, data2 = %lu", nData1, nData2);
      if ((nData2 & OMX_BUFFERFLAG_EOS) != 0) {
        OMX_LOGI("EOS received!!!");
        if (mCallbackTarget_) {
          mCallbackTarget_->OnOmxEvent(EVENT_VIDEO_EOS);
        }
      }
      break;
    default:
      return MHALOmxComponent::OnOMXEvent(eEvent, nData1, nData2, pEventData);
  }

  return OMX_ErrorNone;
}

}
