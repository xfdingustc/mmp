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

#define LOG_TAG "BerlinOmxVideoScheduler"
#include <BerlinOmxVideoScheduler.h>
#include <BerlinOmxAmpVideoPort.h>
#include <BerlinOmxAmpClockPort.h>

#define OMX_ROLE_VIDEO_SCHEDULER_BINARY "video_scheduler.binary"

namespace berlin {

OmxVideoScheduler::OmxVideoScheduler() {
}

OmxVideoScheduler::OmxVideoScheduler(OMX_STRING name) {
  strncpy(mName, name, OMX_MAX_STRINGNAME_SIZE);
}

OmxVideoScheduler::~OmxVideoScheduler() {
  OMX_LOGD("destroyed");
}

OMX_ERRORTYPE OmxVideoScheduler::initRole() {
  if (strncmp(mName, kCompNamePrefix, kCompNamePrefixLength)) {
    return OMX_ErrorInvalidComponentName;
  }
  char * role_name = mName + kCompNamePrefixLength;
  if (!strncmp(role_name, OMX_ROLE_VIDEO_SCHEDULER_BINARY,
      strlen(OMX_ROLE_VIDEO_SCHEDULER_BINARY))) {
    addRole(OMX_ROLE_VIDEO_SCHEDULER_BINARY);
  } else {
    return OMX_ErrorInvalidComponentName;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxVideoScheduler::initPort() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  mInputPort = new OmxAmpVideoPort(kVideoPortStartNumber, OMX_DirInput);
  addPort(mInputPort);
  mOutputPort = new OmxAmpVideoPort(kVideoPortStartNumber + 1, OMX_DirOutput);
  addPort(mOutputPort);
  mClockPort = new OmxAmpClockPort(kClockPortStartNumber, OMX_DirInput);
  addPort(mClockPort);
  return err;
}

OMX_S32 OmxVideoScheduler::getVideoPlane() {
  OmxPortImpl *port = getPort(kVideoPortStartNumber + 1);
  if (port && port->getTunnelComponent()) {
    OmxComponentImpl *pComp =  static_cast<OmxComponentImpl *>(
        static_cast<OMX_COMPONENTTYPE *>(
            port->getTunnelComponent())->pComponentPrivate);
    if (pComp) {
      if (NULL != strstr(pComp->mName, "iv_renderer")) {
        return static_cast<OmxVoutProxy *>(pComp)->getVideoPlane();
      }
    }
  }
  return -1;
}

OMX_ERRORTYPE OmxVideoScheduler::componentDeInit(OMX_HANDLETYPE hComponent) {
  // ASSERT(hComponent != NULL)
  OmxComponent *comp = reinterpret_cast<OmxComponent *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate);
  OmxVideoScheduler *schduler = static_cast<OmxVideoScheduler *>(comp);
  delete schduler;
  reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate = NULL;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxVideoScheduler::createComponent(
    OMX_HANDLETYPE* handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE* callBacks) {
  OmxVideoScheduler* comp = new OmxVideoScheduler(componentName);
  *handle = comp->makeComponent(comp);
  comp->setCallbacks(callBacks, appData);
  comp->componentInit();
  return OMX_ErrorNone;
}

}  // namespace berlin
