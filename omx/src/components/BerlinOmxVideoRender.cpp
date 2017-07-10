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

#define LOG_TAG "BerlinOmxVideoRender"
#include <BerlinOmxVideoRender.h>
#include <BerlinOmxAmpVideoPort.h>

#ifdef _OMX_GTV_
#include <MVBerlinRMTypes.h>
#endif

#define OMX_ROLE_IV_RENDERER_YUV_OVERLAY "iv_renderer.yuv.overlay"

namespace berlin {

OmxVideoRender::OmxVideoRender() {
}

OmxVideoRender::OmxVideoRender(OMX_STRING name) {
  strncpy(mName, name, OMX_MAX_STRINGNAME_SIZE);
}

OmxVideoRender::~OmxVideoRender() {
  OMX_LOGD("destroyed");
}

OMX_ERRORTYPE OmxVideoRender::initRole() {
  if (strncmp(mName, kCompNamePrefix, kCompNamePrefixLength)) {
    return OMX_ErrorInvalidComponentName;
  }
  char * role_name = mName + kCompNamePrefixLength;
  if (!strncmp(role_name, OMX_ROLE_IV_RENDERER_YUV_OVERLAY,
      strlen(OMX_ROLE_IV_RENDERER_YUV_OVERLAY))) {
    addRole(OMX_ROLE_IV_RENDERER_YUV_OVERLAY);
  } else {
    return OMX_ErrorInvalidComponentName;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxVideoRender::initPort() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  mInputPort= new OmxAmpVideoPort(kVideoPortStartNumber, OMX_DirInput);
  addPort(mInputPort);
  return err;
}

OMX_ERRORTYPE OmxVideoRender::getParameter(OMX_INDEXTYPE index, OMX_PTR params) {
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

OMX_ERRORTYPE OmxVideoRender::setParameter(OMX_INDEXTYPE index, OMX_PTR params) {
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

OMX_ERRORTYPE OmxVideoRender::getConfig(OMX_INDEXTYPE index,
    OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOGD("%s, index 0x%x, config %p", __func__, index, config);
  switch (index) {
    case OMX_IndexConfigCommonRotate:
    case OMX_IndexConfigCommonMirror:
    case OMX_IndexConfigCommonInputCrop:
    case OMX_IndexConfigCommonScale:
      break;
    default:
      return OmxComponentImpl::getConfig(index, config);
  }
  return err;
}

OMX_ERRORTYPE OmxVideoRender::setConfig(OMX_INDEXTYPE index,
    OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOGD("%s, index 0x%x, config %p", __func__, index, config);
  switch (index) {
    case OMX_IndexConfigCommonRotate:
    case OMX_IndexConfigCommonMirror:
    case OMX_IndexConfigCommonInputCrop:
    case OMX_IndexConfigCommonScale:
      break;
    default:
      return OmxComponentImpl::setConfig(index, config);
  }
  return err;
}

OMX_S32 OmxVideoRender::getVideoPlane() {
#if (defined(OMX_IndexExt_h) && defined(_OMX_GTV_))
  if (mResourceInfo.nResourceSize > 0) {
    OMX_LOGD("Resouce size %d, value 0x%x",
        mResourceInfo.nResourceSize, mResourceInfo.nResource[0]);
    return MVBerlinRMUtility::SurfaceID2DispPlane(mResourceInfo.nResource[0]);
  }
#endif
  return 0;
}

OMX_ERRORTYPE OmxVideoRender::componentDeInit(OMX_HANDLETYPE hComponent) {
  OmxComponent *comp = reinterpret_cast<OmxComponent *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate);
  OmxVideoRender *writer = static_cast<OmxVideoRender *>(comp);
  delete writer;
  reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate = NULL;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxVideoRender::createComponent(
    OMX_HANDLETYPE* handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE* callBacks) {
  OmxVideoRender* comp = new OmxVideoRender(componentName);
  *handle = comp->makeComponent(comp);
  comp->setCallbacks(callBacks, appData);
  comp->componentInit();
  return OMX_ErrorNone;
}
}  // namespace berlin
