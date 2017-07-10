/*
**                Copyright 2012, MARVELL SEMICONDUCTOR, LTD.
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

#define LOG_TAG "BerlinOmxComponentImpl"
#include <BerlinOmxComponentImpl.h>
#include <string.h>

namespace berlin {

#ifdef _ANDROID_
static const char *androidOmxExtensions[] = {
  "OMX.google.android.index.enableAndroidNativeBuffers",
  "OMX.google.android.index.getAndroidNativeBufferUsage",
  "OMX.google.android.index.storeMetaDataInBuffers",
  "OMX.google.android.index.useAndroidNativeBuffer",
  "OMX.google.android.index.prepareForAdaptivePlayback",
};
#endif

OmxComponentImpl::OmxComponentImpl() :
    mActiveRole(NULL),
    mStateInprogress(OMX_FALSE),
    mState(OMX_StateLoaded),
    mTargetState(OMX_StateMax),
    mFlushedPorts(0) {
    mLock = kdThreadMutexCreate(KD_NULL);
    mAmpHandle = NULL;
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
    InitOmxHeader(&mContainerType);
#endif
#ifdef OMX_IndexExt_h
    InitOmxHeader(&mResourceInfo);
#endif
#ifdef OMX_IVCommonExt_h
    InitOmxHeader(&mDrm);
#endif
    mSecure = OMX_FALSE;
}

OmxComponentImpl::~OmxComponentImpl() {
  mRoles.clear();
  port_iterator it;
  for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
    delete it->second;
  }
  mPortMap.clear();
  kdThreadMutexFree(mLock);
}

OMX_ERRORTYPE OmxComponentImpl::getComponentVersion(
    OMX_STRING componentName,
    OMX_VERSIONTYPE *componentVersion,
    OMX_VERSIONTYPE *specVersion,
    OMX_UUIDTYPE *componentUUID){
  setSpecVersion(specVersion);
  setComponentVersion(componentVersion);
  return OMX_ErrorNone;
}

OmxPortImpl * OmxComponentImpl::getPort(OMX_U32 port_index) {
  port_iterator it = mPortMap.find(port_index);
  if (it == mPortMap.end()) {
    return NULL;
  } else {
    return it->second;
  }
}

OMX_ERRORTYPE OmxComponentImpl::getParameterDomainInit(
    OMX_PORT_PARAM_TYPE *port_param,
    OMX_PORTDOMAINTYPE domain) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  err = CheckOmxHeader(port_param);
  if (OMX_ErrorNone != err) {
    return err;
  }
  port_iterator it;
  OMX_U32 port_start_num = 0xFFFFFFFF;
  OMX_U32 ports_count = 0;
  for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
    if (it->second->getDomain() == domain) {
      ports_count++;
      if (it->second->getPortIndex() < port_start_num) {
        port_start_num = it->second->getPortIndex();
      }
    }
  }
  port_param->nPorts = ports_count;
  port_param->nStartPortNumber = port_start_num;
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::getParameter(
    OMX_INDEXTYPE index, OMX_PTR params) {
  OMX_LOGV("Get component %s's parameter index %s", mName, OmxIndex2String(index));
  if (mState == OMX_StateInvalid) {
    return OMX_ErrorInvalidState;
  }
  if (params == NULL) {
    return OMX_ErrorBadParameter;
  }
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch(index) {
    case OMX_IndexParamPortDefinition: {
      OMX_PARAM_PORTDEFINITIONTYPE *port_def =
          reinterpret_cast<OMX_PARAM_PORTDEFINITIONTYPE *>(params);
// In order that MediaCodec API can work on JB.
// On JB the version is different from the version of Stagefright.
#ifndef DISABLE_CHECK_HEADER
      err = CheckOmxHeader(port_def);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("CheckOmxHeader error %d", err);
        return err;
      }
#endif
      OmxPortImpl *port = getPort(port_def->nPortIndex);
      if (NULL == port) {
        OMX_LOGE("getPort is NULL");
        return OMX_ErrorBadPortIndex;
      } else {
        port->getDefinition(port_def);
      }


      OMX_LOGV("********** Port Definition **********");
      OMX_LOGV(" nSize = %d nPortIndex = %d", port_def->nSize, port_def->nPortIndex);
      OMX_LOGV(" eDir = %s", OmxDir2String(port_def->eDir));
      OMX_LOGV(" Buffer Count Actual = %d, Min = %d, Size = %d",
          port_def->nBufferCountActual, port_def->nBufferCountMin, port_def->nBufferSize);
      OMX_LOGV("*************************************");
      break;
    }
    case OMX_IndexParamCompBufferSupplier: {
      OMX_PARAM_BUFFERSUPPLIERTYPE *buffer_supplier =
          reinterpret_cast<OMX_PARAM_BUFFERSUPPLIERTYPE *>(params);
      err = CheckOmxHeader(buffer_supplier);
      if (OMX_ErrorNone != err) {
        return err;
      }
      port_iterator it;
      it = mPortMap.find(buffer_supplier->nPortIndex);
      if (it == mPortMap.end()) {
        return OMX_ErrorBadPortIndex;
      } else {
        buffer_supplier->eBufferSupplier = it->second->getBufferSupplier();
      }
      break;
    }
    case OMX_IndexParamAudioInit: {
      OMX_PORT_PARAM_TYPE *port_param =
          reinterpret_cast<OMX_PORT_PARAM_TYPE *>(params);
      err = getParameterDomainInit(port_param, OMX_PortDomainAudio);
      break;
    }
    case OMX_IndexParamImageInit: {
      OMX_PORT_PARAM_TYPE *port_param =
          reinterpret_cast<OMX_PORT_PARAM_TYPE *>(params);
      err = getParameterDomainInit(port_param, OMX_PortDomainImage);
      break;
    }
    case OMX_IndexParamVideoInit: {
      OMX_PORT_PARAM_TYPE *port_param =
          reinterpret_cast<OMX_PORT_PARAM_TYPE *>(params);
      err = getParameterDomainInit(port_param, OMX_PortDomainVideo);
      break;
    }
    case OMX_IndexParamOtherInit: {
      OMX_PORT_PARAM_TYPE *port_param =
          reinterpret_cast<OMX_PORT_PARAM_TYPE *>(params);
      err = getParameterDomainInit(port_param, OMX_PortDomainOther);
      break;
    }
    case OMX_IndexParamStandardComponentRole: {
      OMX_PARAM_COMPONENTROLETYPE *role =
          reinterpret_cast<OMX_PARAM_COMPONENTROLETYPE *>(params);
      err = CheckOmxHeader(role);
      if (OMX_ErrorNone != err) {
        return err;
      }
      err = getRole(reinterpret_cast<OMX_STRING>(role->cRole));
      break;
    }
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
    case OMX_IndexParamMediaContainer: {
        OMX_MEDIACONTAINER_INFOTYPE *container_type =
            reinterpret_cast<OMX_MEDIACONTAINER_INFOTYPE *>(params);
        *container_type = mContainerType;
        break;
    }
#endif
#ifdef OMX_IndexExt_h
      case OMX_IndexParamResourceInfo: {
        OMX_RESOURCE_INFO *res_info =
            reinterpret_cast<OMX_RESOURCE_INFO *>(params);
        *res_info = mResourceInfo;
        break;
      }
#endif
#ifdef OMX_IVCommonExt_h
    case OMX_IndexConfigCommonDRM: {
      OMX_CONFIG_DRMTYPE *drm =
          reinterpret_cast<OMX_CONFIG_DRMTYPE *>(params);
      *drm = mDrm;
      break;
    }
#endif
    default:
      err = OMX_ErrorUnsupportedIndex;
      break;
  }
  OMX_LOGV("get parameter return %d", err);
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::setParameter(
    OMX_INDEXTYPE index, const OMX_PTR params) {
  if (mState == OMX_StateInvalid) {
    return OMX_ErrorInvalidState;
  }
  if (mState != OMX_StateLoaded && mState != OMX_StateWaitForResources) {
    return OMX_ErrorIncorrectStateOperation;
  }
  if (params == NULL) {
    return OMX_ErrorBadParameter;
  }
  OMX_ERRORTYPE err = OMX_ErrorNone;
  switch(index) {
    case OMX_IndexParamPortDefinition: {
      OMX_PARAM_PORTDEFINITIONTYPE *port_def =
          reinterpret_cast<OMX_PARAM_PORTDEFINITIONTYPE *>(params);
      err = CheckOmxHeader(port_def);
      if (err!= OMX_ErrorNone) {
        return err;
      }
      OmxPortImpl *port = getPort(port_def->nPortIndex);
      if (NULL == port) {
        return OMX_ErrorBadPortIndex;
      } else {
        port->setDefinition(port_def);
        port->updateDomainParameter();
      }
      break;
    }
    case OMX_IndexParamCompBufferSupplier: {
      OMX_PARAM_BUFFERSUPPLIERTYPE *buffer_supplier =
          reinterpret_cast<OMX_PARAM_BUFFERSUPPLIERTYPE *>(params);
      err = CheckOmxHeader(buffer_supplier);
      if (OMX_ErrorNone != err) {
        return err;
      }
      port_iterator it;
      it = mPortMap.find(buffer_supplier->nPortIndex);
      if (it == mPortMap.end()) {
        return OMX_ErrorBadPortIndex;
      } else {
        it->second->setBufferSupplier(buffer_supplier->eBufferSupplier);
      }
      break;
    }
    case OMX_IndexParamStandardComponentRole: {
      OMX_PARAM_COMPONENTROLETYPE *role =
          reinterpret_cast<OMX_PARAM_COMPONENTROLETYPE *>(params);
      err = CheckOmxHeader(role);
      if (OMX_ErrorNone != err) {
        return err;
      }
      err = setRole((OMX_STRING)role->cRole);
      break;
    }
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
    case OMX_IndexParamMediaContainer: {
        OMX_MEDIACONTAINER_INFOTYPE *container_type =
            reinterpret_cast<OMX_MEDIACONTAINER_INFOTYPE *>(params);
        mContainerType = *container_type;
        break;
    }
#endif
#ifdef OMX_IndexExt_h
      case OMX_IndexParamResourceInfo: {
        OMX_RESOURCE_INFO *res_info =
            reinterpret_cast<OMX_RESOURCE_INFO *>(params);
        mResourceInfo = *res_info;
        break;
      }
#endif
#ifdef OMX_IVCommonExt_h
    case OMX_IndexConfigCommonDRM: {
      OMX_LOGD("case OMX_IndexConfigCommonDRM");
      OMX_CONFIG_DRMTYPE *drm =
          reinterpret_cast<OMX_CONFIG_DRMTYPE *>(params);
      mDrm = *drm;
      break;
    }
#endif
    default:
      err = OMX_ErrorUnsupportedIndex;
      break;
  }
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::getRole(OMX_STRING role) {
  if (mActiveRole == NULL) {
    mActiveRole = mRoles.front();
  }
  strncpy(role, mActiveRole,OMX_MAX_STRINGNAME_SIZE);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxComponentImpl::setRole(OMX_STRING role) {
  vector<const char *>::iterator it;
  for (it = mRoles.begin(); it != mRoles.end(); it++) {
    if(!strncmp(role, *it, OMX_MAX_STRINGNAME_SIZE)) {
      mActiveRole = *it;
      break;
    }
  }
  if (it == mRoles.end()) {
    strcpy(role,"\0");
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxComponentImpl::getConfig(
    OMX_INDEXTYPE index, OMX_PTR config) {
  return OMX_ErrorUnsupportedIndex;
}

OMX_ERRORTYPE OmxComponentImpl::setConfig(
    OMX_INDEXTYPE index, const OMX_PTR config) {
  return OMX_ErrorUnsupportedIndex;
}

OMX_ERRORTYPE OmxComponentImpl::getExtensionIndex(
    const OMX_STRING name, OMX_INDEXTYPE *index) {
  OMX_LOGD("%s %d name=\"%s\"", __FUNCTION__, __LINE__, name);
  unsigned int i;
  for (i = 0; i < NELM(androidOmxExtensions); ++i) {
    if (strcmp(androidOmxExtensions[i], name) == 0) {
      *index = static_cast<OMX_INDEXTYPE> (OMX_IndexVendorStartUnused + i);
      return OMX_ErrorNone;
    }
  }
  return OMX_ErrorUnsupportedIndex;
}

OMX_BOOL OmxComponentImpl::checkAllPortsResource(OMX_BOOL check_alloc) {
  port_iterator it;
  if (check_alloc) {
    for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
      if (OMX_FALSE == it->second->isEnabled() ||
          OMX_TRUE == it->second->isTunneled()) {
        continue;
      }
      if (OMX_FALSE == it->second->isPopulated()) {
        return OMX_FALSE;
      }
    }
    return OMX_TRUE;
  } else {
    for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
      if (OMX_FALSE == it->second->isEnabled() ||
          OMX_TRUE == it->second->isTunneled())
        continue;
      if (OMX_TRUE == it->second->isPopulated())
        return OMX_FALSE;
      if (it->second->mBufferList.size() != 0) {
        return OMX_FALSE;
      }
    }
    return OMX_TRUE;
  }
}

OMX_ERRORTYPE OmxComponentImpl::useBuffer(
    OMX_BUFFERHEADERTYPE **bufHdr,
    OMX_U32 portIndex,
    OMX_PTR appPrivate,
    OMX_U32 size,
    OMX_U8 *buffer) {
  OMX_ERRORTYPE err =OMX_ErrorNone;
  OmxPortImpl *port = getPort(portIndex);
  if (NULL == port) {
    return OMX_ErrorBadPortIndex;
  }
  if (OMX_StateInvalid == mState) {
    return OMX_ErrorInvalidState;
  } else if (OMX_FALSE == port->isEnabled()) {
    return OMX_ErrorIncorrectStateOperation;
  } else if (mState != OMX_StateLoaded &&
      mState != OMX_StateWaitForResources &&
      port->mInTransition == OMX_FALSE) {
    return OMX_ErrorIncorrectStateOperation;
  }
  OMX_LOGV("%s port %lu use buffer", mName, portIndex);
  err = port->useBuffer(bufHdr,appPrivate,size,buffer);
  if (port->mBufferList.size() == port->getBufferCount()) {
    if (port->isEnabled()) {
      port->mDefinition.bPopulated = OMX_TRUE;
      if (port->mInTransition == OMX_TRUE) {
        port->mInTransition = OMX_FALSE;
        postEvent(OMX_EventCmdComplete, OMX_CommandPortEnable, portIndex);
      }
    }
    if (OMX_TRUE == mStateInprogress && OMX_StateIdle == mTargetState &&
        (mState == OMX_StateLoaded || mState == OMX_StateWaitForResources)) {
      if (checkAllPortsResource(OMX_TRUE)) {
        mStateInprogress = OMX_FALSE;
        mState = OMX_StateIdle;
        postEvent(OMX_EventCmdComplete, OMX_CommandStateSet, OMX_StateIdle);
      }
    }
  }
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::allocateBuffer(
    OMX_BUFFERHEADERTYPE **bufHdr,
    OMX_U32 portIndex,
    OMX_PTR appPrivate,
    OMX_U32 size) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OmxPortImpl *port = getPort(portIndex);
  if (NULL == port) {
    OMX_LOGE("%s %d: return OMX_ErrorBadPortIndex",__FUNCTION__, __LINE__);
    return OMX_ErrorBadPortIndex;
  }
  if (mState == OMX_StateInvalid) {
    OMX_LOGE("%s %d: return OMX_ErrorInvalidState",__FUNCTION__, __LINE__);
    return OMX_ErrorInvalidState;
  } else if (port->isEnabled() == OMX_FALSE) {
    OMX_LOGE("%s %d: return OMX_ErrorIncorrectStateOperation",__FUNCTION__, __LINE__);
    return OMX_ErrorIncorrectStateOperation;
  } else if (mState != OMX_StateLoaded && mState != OMX_StateWaitForResources &&
      port->mInTransition == OMX_FALSE) {
    OMX_LOGE("%s %d: return OMX_ErrorIncorrectStateOperation",__FUNCTION__, __LINE__);
    return OMX_ErrorIncorrectStateOperation;
  }
  OMX_LOGV("%s port %lu allocate buffer", mName, portIndex);
  err = port->allocateBuffer(bufHdr, appPrivate, size);
  if (port->mBufferList.size() == port->getBufferCount()) {
    OMX_LOGV("%s %d: Allocate %d buffers",__FUNCTION__, __LINE__,
        port->mBufferList.size());
    if (port->isEnabled()) {
      port->mDefinition.bPopulated = OMX_TRUE;
      if (port->mInTransition == OMX_TRUE) {
        port->mInTransition = OMX_FALSE;
        OMX_LOGV("Enable port after allocateBuffer");
        postEvent(OMX_EventCmdComplete, OMX_CommandPortEnable, portIndex);
      }
    }
    if (mStateInprogress == OMX_TRUE && mTargetState == OMX_StateIdle &&
        (mState == OMX_StateLoaded || mState == OMX_StateWaitForResources)) {
      if (checkAllPortsResource(OMX_TRUE)) {
        mStateInprogress = OMX_FALSE;
        mState = OMX_StateIdle;
        OMX_LOGV("Change to idle after allocateBuffer");
        postEvent(OMX_EventCmdComplete, OMX_CommandStateSet, OMX_StateIdle);
      }
    }
  }
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::freeBuffer(
    OMX_U32 portIndex,
    OMX_BUFFERHEADERTYPE *buffer) {
  OMX_ERRORTYPE err =OMX_ErrorNone;
  OmxPortImpl *port = getPort(portIndex);
  if (NULL == port) {
    OMX_LOGE("Bad port index %lu",portIndex);
    return OMX_ErrorBadPortIndex;
  }
  if (port->isEnabled() == OMX_TRUE) {
    if (!(mStateInprogress == OMX_TRUE && mState == OMX_StateIdle
          && mTargetState == OMX_StateLoaded)) {
      // TODO: it may cause SEG fault when the buffer is in using.
      // Need remove the buffer header in the bufferQueue too.
      OMX_LOGE ("%s,%d: error OMX_ErrorPortUnpopulated", __FUNCTION__,__LINE__);
      postEvent(OMX_EventError, OMX_ErrorPortUnpopulated, portIndex);
      //return OMX_ErrorIncorrectStateOperation;
    }
  } else {
    if (mState != OMX_StateIdle && mState !=  OMX_StatePause &&
        mState != OMX_StateExecuting) {
      OMX_LOGE ("%s,%d: error OMX_ErrorIncorrectStateOperation",
          __FUNCTION__,__LINE__);
      return OMX_ErrorIncorrectStateOperation;
    }
  }
  OMX_LOGV("%s port %lu free buffer %p", mName, portIndex, buffer);
  err = port->freeBuffer(buffer);
  if (port->mBufferList.size() == 0) {
    OMX_LOGV("All buffer freed on port %lu", port->getPortIndex());
    if (port->isEnabled() == OMX_FALSE && port->mInTransition == OMX_TRUE) {
      port->mInTransition = OMX_FALSE;
      OMX_LOGV("Disbale Port after freeBuffer");
      postEvent(OMX_EventCmdComplete, OMX_CommandPortDisable, portIndex);
    }
    if (mStateInprogress == OMX_TRUE && mTargetState == OMX_StateLoaded &&
        (mState == OMX_StateIdle || mState == OMX_StateWaitForResources )) {
      if (checkAllPortsResource(OMX_FALSE)) {
        componentTunnelTearDown();
        mStateInprogress = OMX_FALSE;
        mState = OMX_StateLoaded;
        OMX_LOGV("Trans to Loaded when freeBuffer");
        postEvent(OMX_EventCmdComplete, OMX_CommandStateSet, OMX_StateLoaded);
      }
    }
  }
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::useEGLImage(
    OMX_BUFFERHEADERTYPE **bufHdr,
    OMX_U32 portIndex,
    OMX_PTR appPrivate,
    void *eglImage) {
  return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE OmxComponentImpl::emptyThisBuffer(
    OMX_BUFFERHEADERTYPE *buffer) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  port_iterator it = mPortMap.find(buffer->nInputPortIndex);
  if (it == mPortMap.end()) {
    return OMX_ErrorBadPortIndex;
  }
  OmxPortImpl *port = it->second;
  if (!port->isEnabled()) {
    return OMX_ErrorIncorrectStateOperation;
  }
  OMX_LOGV("%s empty buffer %p, size %d, flag %x, ts "TIME_FORMAT,
      mName, buffer, buffer->nFilledLen, buffer->nFlags, TIME_ARGS(buffer->nTimeStamp));
  port->pushBuffer(buffer);
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::fillThisBuffer(
    OMX_BUFFERHEADERTYPE *buffer) {
  OMX_ERRORTYPE err =OMX_ErrorNone;
  port_iterator it = mPortMap.find(buffer->nOutputPortIndex);
  if (it == mPortMap.end()) {
    return OMX_ErrorBadPortIndex;
  }
  OmxPortImpl *port = it->second;
  if (!port->isEnabled()) {
    return OMX_ErrorIncorrectStateOperation;
  }
  OMX_LOGV("%s fill buffer %p", mName, buffer);
  port->pushBuffer(buffer);
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::returnBuffer(OmxPortImpl * port,
    OMX_BUFFERHEADERTYPE *buf) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mTargetState == OMX_StatePause) {
     return err;
  }
  if (buf->hMarkTargetComponent == getComponentHandle()) {
    postMark(buf->pMarkData);
    buf->hMarkTargetComponent = NULL;
    buf->pMarkData = NULL;
  }
  if (port->isTunneled()) {
    // TODO: Handle tunnel mode
  } else {
    port->returnBuffer(buf);
    if (port->isInput()) {
        postEmptyBufferDone(buf);
    } else {
        postFillBufferDone(buf);
    }
  }
  return err;
}


OMX_ERRORTYPE OmxComponentImpl::returnBuffers() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  port_iterator it;
  for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
    OmxPortImpl *port = it->second;
    if (port->isEnabled() == OMX_FALSE)
      continue;
    if (port->isTunneled()) {
      // TODO: Handle tunnel mode
    } else {
      OMX_U32 wait_count = 10;
      if (port->isInput()) {
        while (port->getCachedBuffer() != NULL) {
          // Sleep(100);
          if (wait_count-- == 0) {
             postEmptyBufferDone(port->getCachedBuffer());
             port->returnBuffer(port->getCachedBuffer());
          }
        }
        while (!port->isEmpty()) {
          postEmptyBufferDone(port->popBuffer());
          port->returnBuffer(port->getCachedBuffer());
        }
      } else {
        while (port->getCachedBuffer() != NULL) {
          // Sleep(100);
          if (wait_count-- == 0) {
             postFillBufferDone(port->getCachedBuffer());
             port->returnBuffer(port->getCachedBuffer());
          }
        }
        while (!port->isEmpty()) {
          postFillBufferDone(port->popBuffer());
          port->returnBuffer(port->getCachedBuffer());
        }
      }
    }
  }
  OMX_LOGV("%s, %d: ret %x", __FUNCTION__, __LINE__, err);
  return err;
}

void OmxComponentImpl::returnCachedBuffers(OmxPortImpl *port) {
  OMX_U32 count = 0;
  OMX_BUFFERHEADERTYPE *buffer;
  OMX_BOOL exit = OMX_FALSE;
  while (!exit) {
    buffer = port->returnCachedbuffer();
    if (NULL == buffer) {
      exit = OMX_TRUE;
      continue;
    }
    if (port->isInput()) {
      postEmptyBufferDone(buffer);
    } else {
      postFillBufferDone(buffer);
    }
    OMX_LOGD("return cache buffer %p", buffer);
    port->returnBuffer(buffer);
    count++;
  }
  OMX_LOGV("returnCachedBuffers port %d  count %d", port, count);
}


OMX_ERRORTYPE OmxComponentImpl::sendCommand(
    OMX_COMMANDTYPE cmd, OMX_U32 param, OMX_PTR cmdData) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOGV("%s:%d, cmd %d, param %x, cmdData %x",
      __FUNCTION__,__LINE__, cmd, param, cmdData);
  if(mState == OMX_StateInvalid) {
    postEvent(OMX_EventError, OMX_ErrorInvalidState, (OMX_U32)NULL);
    return OMX_ErrorInvalidState;
  }
  kdThreadMutexLock(mLock);
  OmxCommand cmd_item ;
  cmd_item.cmd = cmd;
  cmd_item.param = param;
  cmd_item.cmdData = cmdData;
  mCmdQueue.push(cmd_item);
#ifndef ASYNC_COMMAND
  err = processCommand();
#endif
  kdThreadMutexUnlock(mLock);
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::processCommand() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OmxCommand cmd_item = mCmdQueue.front();
  OMX_COMMANDTYPE cmd = cmd_item.cmd;
  OMX_U32 param = cmd_item.param;
  mCmdQueue.pop();
  /** Fill in the message */
  switch (cmd) {
    case OMX_CommandStateSet: {
      OMX_STATETYPE target_state = (OMX_STATETYPE)cmd_item.param;
      err = setState(target_state);
      if (err == OMX_ErrorNone && mStateInprogress == OMX_FALSE) {
        postEvent(OMX_EventCmdComplete, cmd, param);
      } else if (err != OMX_ErrorNone) {
        postEvent(OMX_EventError, cmd, param);
      }
      err = OMX_ErrorNone;
      break;
    }
    case OMX_CommandFlush: {
      port_iterator it, p;
      if (param == OMX_ALL) {
        for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
          it->second->flushPort();
        }
        flush();
        for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
          flushPort(it->second);
          it->second->flushComplete();
          postEvent(OMX_EventCmdComplete, OMX_CommandFlush,
              it->second->getPortIndex());
        }
      } else {
        it = mPortMap.find(param);
        if (it == mPortMap.end()) {
          err = OMX_ErrorBadPortIndex;
          break;
        } else {
          mFlushedPorts++;
          if (1 == mFlushedPorts) {
            for (p = mPortMap.begin(); p != mPortMap.end(); p++) {
              p->second->flushPort();
            }
            flush();
          }
          flushPort(it->second);
          it->second->flushComplete();
          if (mFlushedPorts == getFlushPortsCounts()) {
            mFlushedPorts = 0;
          }
          postEvent(OMX_EventCmdComplete, cmd, param);
        }
      }
      break;
    }
    case OMX_CommandPortDisable: {
      port_iterator it;
      if (param == OMX_ALL) {
        for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
          OmxPortImpl *port = it->second;
          port->disablePort();
          if (mState != OMX_StateLoaded && mState != OMX_StateWaitForResources &&
              port->mBufferList.size() != 0) {
            port->mInTransition = OMX_TRUE;
            flushPort(port);
          } else {
            postEvent(OMX_EventCmdComplete, cmd, port->getPortIndex());
          }
          onPortEnableCompleted(port->getPortIndex(), OMX_FALSE /* disabled */);
        }
      } else {
        OmxPortImpl *port = getPort(param);
        if (NULL == port) {
          err = OMX_ErrorBadPortIndex;
          break;
        } else {
          port->disablePort();
          if (mState != OMX_StateLoaded && mState != OMX_StateWaitForResources &&
              port->mBufferList.size() != 0) {
            port->mInTransition = OMX_TRUE;
            flushPort(port);
          } else {
            postEvent(OMX_EventCmdComplete, cmd, param);
          }
          onPortEnableCompleted(port->getPortIndex(), OMX_FALSE /* disabled */);
        }
      }
      break;
    }
    case OMX_CommandPortEnable: {
      port_iterator it;
      if (param == OMX_ALL) {
        for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
          OmxPortImpl *port = it->second;
          port->enablePort();
          if (mState != OMX_StateLoaded && mState != OMX_StateWaitForResources) {
            port->mInTransition = OMX_TRUE;
          } else {
            postEvent(OMX_EventCmdComplete, cmd, port->getPortIndex());
          }
          onPortEnableCompleted(port->getPortIndex(), OMX_TRUE /* enabled */);
        }
      } else {
        OmxPortImpl *port = getPort(param);
        if (NULL == port) {
          err = OMX_ErrorBadPortIndex;
          break;
        } else {
          port->enablePort();
          if (mState != OMX_StateLoaded && mState != OMX_StateWaitForResources) {
            port->mInTransition = OMX_TRUE;
          } else {
            postEvent(OMX_EventCmdComplete, cmd, param);
          }
          onPortEnableCompleted(port->getPortIndex(), OMX_TRUE /* enabled */);
        }
      }
      break;
    }
    case OMX_CommandMarkBuffer: {
      port_iterator it;
      OMX_MARKTYPE *mark = (OMX_MARKTYPE *)cmd_item.cmdData;
      if (param == OMX_ALL) {
        for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
          it->second->markBuffer(mark);
        }
      } else {
        it = mPortMap.find(param);
        if (it == mPortMap.end()) {
          OMX_LOGE("OMX_CommandMarkBuffer return OMX_ErrorBadPortIndex");
          err = OMX_ErrorBadPortIndex;
          break;
        } else {
          it->second->markBuffer(mark);
        }
      }
      break;
    }
#ifdef OMX_CORE_EXT
    case OMX_CommandDisconnectPort:
      onPortDisconnect(OMX_TRUE, cmd_item.cmdData);
      break;
    case OMX_CommandClockStart:
      onPortClockStart(OMX_TRUE);
      break;
    case OMX_CommandClockPause:
      onPortClockStart(OMX_FALSE);
      break;
#endif
#ifdef VIDEO_DECODE_ONE_FRAME_MODE
    case OMX_CommandDecode_ONE_Frame:
      onPortVideoDecodeMode(param);
      break;
#endif
    default:
      err = OMX_ErrorUnsupportedIndex;
      break;
  }
  return err;
}

inline OMX_ERRORTYPE OmxComponentImpl::isStateTransitionValid(
    OMX_STATETYPE old_state, OMX_STATETYPE new_state) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
//TODO: For bugit#6812, revert it when MooPlayer side check state
#if 0
  if (old_state == new_state) {
    return OMX_ErrorSameState;
  }
#endif
  if (new_state == OMX_StateInvalid) {
    mState = OMX_StateInvalid;
    return OMX_ErrorInvalidState;
  }
  switch (old_state) {
    case OMX_StateLoaded:
    case OMX_StateWaitForResources:
      if (new_state == OMX_StateExecuting ||
          new_state == OMX_StatePause) {
        err = OMX_ErrorIncorrectStateTransition;
      }
      break;
    case OMX_StateIdle:
      if (new_state == OMX_StateWaitForResources) {
        err = OMX_ErrorIncorrectStateTransition;
      }
      break;
    case OMX_StateExecuting:
    case OMX_StatePause:
      if (new_state == OMX_StateWaitForResources ||
          new_state == OMX_StateLoaded) {
        err = OMX_ErrorIncorrectStateTransition;
      }
      break;
    default:
#ifdef OMX_IL_1_2
      err = OMX_ErrorReserved_0x8000100A;//OMX_ErrorInvalidState;
#else
      err = OMX_ErrorInvalidState;
#endif
      break;
  }
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::getState(OMX_STATETYPE *state) {
  *state = mState;
  OMX_LOGD("%s, current state is %s(%d)", mName, OmxState2String(*state), *state);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxComponentImpl::setState(OMX_STATETYPE state) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
//TODO: For bugit#6812, revert it when MooPlayer side check state
#if 1
  if (mState == state) {
    OMX_LOGD("target state %s, same as current, just return", OmxState2String(mState));
    mStateInprogress = OMX_FALSE;
    return err;
  }
#endif
  mTargetState = state;
  err = isStateTransitionValid(mState, mTargetState);
  if (OMX_ErrorNone != err) {
    postEvent(OMX_EventError, err, (OMX_U32)NULL);
    return err;
  }
  OMX_LOGD("%s, state change, %s(%d) -> %s(%d)", mName,
      OmxState2String(mState), mState, OmxState2String(mTargetState), mTargetState);
  if (mState == OMX_StateLoaded && state == OMX_StateIdle) {
    err = prepare();
    if (hasPendingResource()) {
      OMX_LOGD("%s, state change in progress, %s(%d) -> %s(%d)", mName,
          OmxState2String(mState), mState, OmxState2String(mTargetState), mTargetState);
      mStateInprogress = OMX_TRUE;
    }
  } else if (mState == OMX_StateIdle && state == OMX_StateExecuting) {
    err = start();
  } else if (mState == OMX_StateIdle && state == OMX_StatePause) {
    err =  preroll();
  } else if (mState == OMX_StateIdle && state == OMX_StateLoaded) {
    err = release();
    if (hasPendingResource()) {
      mStateInprogress = OMX_TRUE;
    } else {
      componentTunnelTearDown();
    }
  } else if ((mState == OMX_StateExecuting ||  mState == OMX_StatePause)
      && state == OMX_StateIdle) {
    err = stop();
    err = returnBuffers();
  } else if (mState == OMX_StateExecuting && state == OMX_StatePause) {
    err = pause();
  } else if (mState == OMX_StatePause && state == OMX_StateExecuting) {
    err = resume();
  }

  if (OMX_ErrorNone == err && mStateInprogress != OMX_TRUE) {
    mState = state;
  }
  return err;
}

OMX_BOOL OmxComponentImpl::hasPendingResource() {
  port_iterator it;
  for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
    if (it->second->isEnabled() && !it->second->isTunneled()) {
      return OMX_TRUE;
    } else if (it->second->mInTransition == OMX_TRUE) {
      return OMX_TRUE;
    }
  }
  return OMX_FALSE;
}

OMX_U32 OmxComponentImpl::getFlushPortsCounts() {
  port_iterator it;
  OMX_U32 flush_counts = 0;
  for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
    if (it->second->getDomain() == OMX_PortDomainOther) {
      continue;
    }
    flush_counts++;
  }
  return flush_counts;
}

OMX_ERRORTYPE OmxComponentImpl::flushPort(OmxPortImpl *port) {
  OMX_LOGD("[%s : port %d], flush", mName, port->getPortIndex());
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (port->isTunneled()) {
    // TODO: Handle tunnel mode
  } else {
    if (port->isInput()) {
      OMX_U32 count =10;
      while (port->getCachedBuffer() != NULL) {
        //Sleep(100);
        if (count-- == 0) {
          postEmptyBufferDone(port->getCachedBuffer());
          port->returnBuffer(port->getCachedBuffer());
        }
      }
      while (!port->isEmpty()) {
        postEmptyBufferDone(port->popBuffer());
        port->returnBuffer(port->getCachedBuffer());
      }

    } else {
      OMX_U32 count =10;
      while (port->getCachedBuffer() != NULL) {
        //Sleep(100);
        if (count-- == 0) {
          postFillBufferDone(port->getCachedBuffer());
          port->returnBuffer(port->getCachedBuffer());
        }
      }
      while (!port->isEmpty()) {
        postFillBufferDone(port->popBuffer());
        port->returnBuffer(port->getCachedBuffer());
      }
    }
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxComponentImpl::componentRoleEnum(
    OMX_U8 *role, OMX_U32 index) {
  if (index >= mRoles.size()) {
    return OMX_ErrorNoMore;
  }
  if (NULL != role) {
    strncpy((char *)role, mRoles[index], OMX_MAX_STRINGNAME_SIZE);
  }
  return OMX_ErrorNone;
}

void OmxComponentImpl::addRole(const char *role) {
  mRoles.push_back(role);
}

void OmxComponentImpl::addPort(OmxPortImpl *port) {
  mPortMap.insert(port_pair(port->mDefinition.nPortIndex, port));
  //port->mComponent = this;
}

OMX_ERRORTYPE OmxComponentImpl::componentTunnelRequest(
    OMX_U32 port,
    OMX_HANDLETYPE tunneledComp,
    OMX_U32 tunneledPort,
    OMX_TUNNELSETUPTYPE *tunnelSetup) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_COMPONENTTYPE *tunnelComp =
      static_cast<OMX_COMPONENTTYPE *>(tunneledComp);
  OMX_LOGV("[%s: %d] -> %p: %d", mName, port, tunneledComp, tunneledPort);
  OmxPortImpl *port1 = getPort(port);
  if (port1 == NULL) {
    return OMX_ErrorBadParameter;
  }
  if (mState != OMX_StateLoaded && port1->isEnabled()) {
    return OMX_ErrorIncorrectStateOperation;
  }
  if (port1->getDir() == OMX_DirOutput) {
    // Tunnel stage 1, output port to input port
    tunnelSetup->eSupplier = OMX_BufferSupplyOutput;
    tunnelSetup->nTunnelFlags = 0;
  } else if (port1->getDir() == OMX_DirInput) {
    // Tunnel stage 2, input port to output
    if (tunnelComp == NULL) {
      port1->setTunnel(tunnelComp, tunneledPort);
      return OMX_ErrorNone;
    }
    OMX_PARAM_PORTDEFINITIONTYPE port_def;
    InitOmxHeader(&port_def);
    port_def.nPortIndex = tunneledPort;
    err = OMX_GetParameter(tunneledComp,
        OMX_IndexParamPortDefinition,
        &port_def);
    if (port_def.eDomain != port1->getDomain() ||
        port_def.eDir == port1->getDir()) {
        return OMX_ErrorPortsNotCompatible;
    }
    // Check the codec type is compatible

    // Set the buffer size and count
    OMX_BOOL need_set_parameter = OMX_FALSE;
    if (port_def.nBufferSize > port1->getBufferSize()) {
      port1->setBufferSize(port_def.nBufferSize);
    } else {
      port_def.nBufferSize = port1->getBufferSize();
      need_set_parameter = OMX_TRUE;
    }

    if (port_def.nBufferCountMin > port1->getBufferCountMin()) {
      port1->setBufferCountMin(port_def.nBufferCountMin);
    } else {
      port_def.nBufferCountMin = port1->getBufferCountMin();
      need_set_parameter = OMX_TRUE;
    }

    if (need_set_parameter) {
      err = OMX_SetParameter(tunneledComp,
          OMX_IndexParamPortDefinition,
          &port_def);
      if (err != OMX_ErrorNone) {
        return OMX_ErrorPortsNotCompatible;
      }
    }
    port1->setBufferSupplier(tunnelSetup->eSupplier);
    OMX_PARAM_BUFFERSUPPLIERTYPE supplier;
    InitOmxHeader(&supplier);
    supplier.nPortIndex = tunneledPort;
    supplier.eBufferSupplier = tunnelSetup->eSupplier;
    err = OMX_SetParameter(tunneledComp,
        OMX_IndexParamCompBufferSupplier,
        &supplier);
    if (err != OMX_ErrorNone) {
      return OMX_ErrorPortsNotCompatible;
    }
    OMX_LOGV("[%p: %d] -> [%p: %d] setup complete",
        getComponentHandle(), port, tunneledComp, tunneledPort);
    port1->setTunnel(tunneledComp, tunneledPort);
    OmxComponentImpl *comp2 =
        static_cast<OmxComponentImpl *>(tunnelComp->pComponentPrivate);
    OmxPortImpl *port2 = comp2->getPort(tunneledPort);
    if (port2) {
      port2->setTunnel(getComponentHandle(), port);
    }
  }
  // if only support proprietary communication
  // return OMX_ErrorTunnelingUnsupported;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxComponentImpl::componentTunnelTearDown() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  port_iterator it;
  for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
    // Considered the JB switch audio track case, we just tear down the tunneled video port first.
    if (it->second->isTunneled() && it->second->getDomain() == OMX_PortDomainVideo) {
      OMX_HANDLETYPE tunnel_handle = it->second->getTunnelComponent();
      if (NULL != tunnel_handle) {
        OmxComponentImpl *tunnel_component = static_cast<OmxComponentImpl *>(
            reinterpret_cast<OMX_COMPONENTTYPE *>(tunnel_handle)->pComponentPrivate);
        if (NULL != tunnel_component) {
          OmxPortImpl *tunnel_port = tunnel_component->getPort(it->second->getTunnelPort());
          if (NULL != tunnel_port) {
            tunnel_port->tearDownTunnel();
          }
        } else {
          OMX_LOGV("the tunneled component is deleted.");
        }
      } else {
        OMX_LOGV("the tunneled handle is deleted.");
      }
      it->second->tearDownTunnel();
    } else {
      OMX_LOGV("the port is non-tunneled or not video port.");
    }
  }
  return err;
}

void OmxComponentImpl::onPortFlushCompleted(OMX_U32 portIndex) {

}

void OmxComponentImpl::onPortEnableCompleted(
    OMX_U32 portIndex, OMX_BOOL enabled) {

}

OMX_BOOL OmxComponentImpl::onPortDisconnect(
    OMX_BOOL disconnect, OMX_PTR type) {
  return OMX_TRUE;
}

void OmxComponentImpl::onPortClockStart(OMX_BOOL start) {

}

#ifdef VIDEO_DECODE_ONE_FRAME_MODE
void OmxComponentImpl::onPortVideoDecodeMode(
    OMX_U32 enable, OMX_S32 mode) {

}
#endif

OMX_ERRORTYPE OmxComponentImpl::componentInit() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  err = initRole();
  mActiveRole = mRoles.front();
  err = initPort();
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::componentDeInit(
    OMX_HANDLETYPE hComponent) {
  OmxComponentImpl *comp = static_cast<OmxComponentImpl *>(static_cast<
      OmxComponent *>(((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate));
  delete comp;
  return OMX_ErrorNone;
}

}  // namespace berlin
