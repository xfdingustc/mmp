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

#define LOG_TAG "BerlinOmxSubtitleScheduler"

#include "BerlinOmxSubtitleScheduler.h"
#include "BerlinOmxSubtitlePort.h"

namespace berlin {
OmxSubtitleScheduler::OmxSubtitleScheduler() {
}

OmxSubtitleScheduler::OmxSubtitleScheduler(OMX_STRING name) {
  strncpy(mName, name, OMX_MAX_STRINGNAME_SIZE);
}

OmxSubtitleScheduler::~OmxSubtitleScheduler() {
  OMX_LOGD("destroyed");
}

OMX_ERRORTYPE OmxSubtitleScheduler::initRole() {
  if (strncmp(mName, kCompNamePrefix, kCompNamePrefixLength)) {
    return OMX_ErrorInvalidComponentName;
  }
  char * role_name = mName + kCompNamePrefixLength;
  if (!strncmp(role_name, OMX_ROLE_SUBTITLE_SCHEDULER_BINARY,
      strlen(OMX_ROLE_SUBTITLE_SCHEDULER_BINARY))) {
    addRole(OMX_ROLE_SUBTITLE_SCHEDULER_BINARY);
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxSubtitleScheduler::initPort() {
  OMX_ERRORTYPE err = OMX_ErrorNoMore;

  mInputPort = new OmxSubtitlePort(kTextPortStartNumber, OMX_DirInput);

  addPort(mInputPort);
  //mOutputPort = new OmxSubtitlePort(kTextPortStartNumber + 1, OMX_DirOutput);
  //addPort(mOutputPort);

  //OMX_PARAM_PORTDEFINITIONTYPE def;
  //mInputPort->getDefinition(&def);
  //def.nBufferCountActual = 64;
  //mInputPort->setDefinition(&def);
  return err;
}

OMX_ERRORTYPE OmxSubtitleScheduler::getParameter(OMX_INDEXTYPE index, OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch (index) {
    default:
      return OmxComponentImpl::getParameter(index, params);
  }
  return err;
}

OMX_ERRORTYPE OmxSubtitleScheduler::setParameter(OMX_INDEXTYPE index, OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  return err;
}

OMX_ERRORTYPE OmxSubtitleScheduler::getConfig(OMX_INDEXTYPE index, OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  return err;
}

OMX_ERRORTYPE OmxSubtitleScheduler::setConfig(OMX_INDEXTYPE index, const OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  return err;
}

OMX_ERRORTYPE OmxSubtitleScheduler::componentDeInit(OMX_HANDLETYPE hComponent) {
  // ASSERT(hComponent != NULL)
  OmxComponent *comp = reinterpret_cast<OmxComponent *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate);
  OmxSubtitleScheduler *subtitle_scheduler = static_cast<OmxSubtitleScheduler *>(comp);
  delete subtitle_scheduler;
  return OMX_ErrorNone;
}


OMX_ERRORTYPE OmxSubtitleScheduler::createComponent(
    OMX_HANDLETYPE* handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE* callBacks) {
  OmxSubtitleScheduler* comp = new OmxSubtitleScheduler(componentName);
  *handle = comp->makeComponent(comp);
  comp->setCallbacks(callBacks, appData);
  comp->componentInit();
  return OMX_ErrorNone;
}

}
