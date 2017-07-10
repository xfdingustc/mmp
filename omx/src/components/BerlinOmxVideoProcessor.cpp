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

#define LOG_TAG "BerlinOmxVideoProcessor"
#include <BerlinOmxVideoProcessor.h>
#include <BerlinOmxAmpVideoPort.h>

#define OMX_ROLE_IV_PROCESSOR_YUV "iv_processor.yuv"

namespace berlin {

OmxVideoProcessor::OmxVideoProcessor() {
  mIsInputComponent = OMX_TRUE;
}

OmxVideoProcessor::OmxVideoProcessor(OMX_STRING name) {
  strncpy(mName, name, OMX_MAX_STRINGNAME_SIZE);
  mIsInputComponent = OMX_TRUE;
}

OmxVideoProcessor::~OmxVideoProcessor() {
  OMX_LOGD("destroyed");
}

OMX_ERRORTYPE OmxVideoProcessor::initRole() {
  if (strncmp(mName, kCompNamePrefix, kCompNamePrefixLength)) {
    return OMX_ErrorInvalidComponentName;
  }
  char * role_name = mName + kCompNamePrefixLength;
  if (!strncmp(role_name, OMX_ROLE_IV_PROCESSOR_YUV,
      strlen(OMX_ROLE_IV_PROCESSOR_YUV))) {
    addRole(OMX_ROLE_IV_PROCESSOR_YUV);
  } else {
    return OMX_ErrorInvalidComponentName;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxVideoProcessor::initPort() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  mInputPort= new OmxAmpVideoPort(kVideoPortStartNumber, OMX_DirInput);
  addPort(mInputPort);
  mOutputPort = new OmxAmpVideoPort(kVideoPortStartNumber + 1, OMX_DirOutput);
  addPort(mOutputPort);
  return err;
}

OMX_ERRORTYPE OmxVideoProcessor::getParameter(OMX_INDEXTYPE index,
    OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch (index) {
    case OMX_IndexParamVideoPortFormat: {
      OMX_VIDEO_PARAM_PORTFORMATTYPE* video_param =
          reinterpret_cast<OMX_VIDEO_PARAM_PORTFORMATTYPE *>(params);
      err = CheckOmxHeader(video_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl *port = getPort(video_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo) {
         *video_param = port->getVideoParam();
      }
      break;
    }
    default:
      return OmxComponentImpl::getParameter(index, params);
  }
  return err;
}

OMX_ERRORTYPE OmxVideoProcessor::setParameter(OMX_INDEXTYPE index,
    OMX_PTR params) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch (index) {
    case OMX_IndexParamVideoPortFormat: {
      OMX_VIDEO_PARAM_PORTFORMATTYPE* video_param =
          reinterpret_cast<OMX_VIDEO_PARAM_PORTFORMATTYPE *>(params);
      err = CheckOmxHeader(video_param);
      if (OMX_ErrorNone != err) {
        break;
      }
      OmxPortImpl* port = getPort(video_param->nPortIndex);
      if (NULL == port) {
        err = OMX_ErrorBadPortIndex;
        break;
      }
      if (port->getDomain() == OMX_PortDomainVideo) {
        port->setVideoParam(video_param);
        port->updateDomainDefinition();
      }
      break;
    }
    default:
      return OmxComponentImpl::setParameter(index, params);
  }
  return err;
}

OMX_ERRORTYPE OmxVideoProcessor::getConfig(OMX_INDEXTYPE index,
    OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOGD("%s, index 0x%x, config %p", __func__, index, config);
  switch (index) {
    case OMX_IndexConfigCommonRotate:
    case OMX_IndexConfigCommonMirror:
    case OMX_IndexConfigCommonInputCrop:
      break;
    default:
      return OmxComponentImpl::getConfig(index, config);
  }
  return err;
}

OMX_ERRORTYPE OmxVideoProcessor::setConfig(OMX_INDEXTYPE index,
    OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOGD("%s, index 0x%x, config %p", __func__, index, config);
  switch (index) {
    case OMX_IndexConfigCommonRotate:
    case OMX_IndexConfigCommonMirror:
    case OMX_IndexConfigCommonInputCrop:
      break;
    default:
      return OmxComponentImpl::setConfig(index, config);
  }
  return err;
}

OMX_S32 OmxVideoProcessor::getVideoPlane() {
  OmxPortImpl *port = getPort(kVideoPortStartNumber + 1);
  if (port && port->getTunnelComponent()) {
    OmxComponentImpl *pComp =  static_cast<OmxComponentImpl *>(
        static_cast<OMX_COMPONENTTYPE *>(
            port->getTunnelComponent())->pComponentPrivate);
    if (pComp) {
      if (NULL != strstr(pComp->mName, "video_scheduler") ||
          NULL != strstr(pComp->mName, "iv_renderer")) {
        return static_cast<OmxVoutProxy *>(pComp)->getVideoPlane();
      }
    }
  }
  return -1;
}

OMX_ERRORTYPE OmxVideoProcessor::componentDeInit(OMX_HANDLETYPE hComponent) {
  // ASSERT(hComponent != NULL)
  OmxComponent *comp = reinterpret_cast<OmxComponent *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate);
  OmxVideoProcessor *processor = static_cast<OmxVideoProcessor *>(comp);
  delete processor;
  reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate = NULL;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxVideoProcessor::createComponent(
  OMX_HANDLETYPE* handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE* callBacks) {
  OmxVideoProcessor* comp = new OmxVideoProcessor(componentName);
  *handle = comp->makeComponent(comp);
  comp->setCallbacks(callBacks, appData);
  comp->componentInit();
  return OMX_ErrorNone;
}
}  // namespace berlin
