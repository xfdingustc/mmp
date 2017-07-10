/*
**                Copyright 2014, MARVELL SEMICONDUCTOR, LTD.
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

#define LOG_TAG "OmxComponentImpl"
#include <OmxComponentImpl.h>
#include <string.h>

namespace mmp {

#ifdef _ANDROID_
static const char *androidOmxExtensions[] = {
  "OMX.google.android.index.enableAndroidNativeBuffers",
  "OMX.google.android.index.getAndroidNativeBufferUsage",
  "OMX.google.android.index.storeMetaDataInBuffers",
  "OMX.google.android.index.useAndroidNativeBuffer",
};
#endif

OmxComponentImpl::OmxComponentImpl(char *name) :
    mActiveRole(NULL),
    mStateInprogress(OMX_FALSE),
    mState(OMX_StateLoaded),
    mTargetState(OMX_StateLoaded),
    mFlushedPorts(0),
    mOmxEventQueue(NULL) {
  char *threadName = "";
  if (strstr(name, "video_decoder")) {
    threadName = "omx_v_sw_dec";
  } else if (strstr(name, "audio_decoder")) {
    threadName = "omx_a_sw_dec";
  }
  mOmxEventQueue = new OmxEventQueue(threadName);
  mOmxEventQueue->start();
}

OmxComponentImpl::~OmxComponentImpl() {
  mRoles.clear();
  port_iterator it;
  for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
    delete it->second;
  }
  mPortMap.clear();
  if (mOmxEventQueue) {
    mOmxEventQueue->stop();
    delete mOmxEventQueue;
    mOmxEventQueue = NULL;
  }
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
  OMX_LOGD("Get component %s's parameter index %s", mName, OmxIndex2String(index));
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
      err = CheckOmxHeader(port_def);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("CheckOmxHeader error %d", err);
        return err;
      }
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
      OMX_LOGD();
      OMX_PARAM_COMPONENTROLETYPE *role =
          reinterpret_cast<OMX_PARAM_COMPONENTROLETYPE *>(params);
      err = CheckOmxHeader(role);
      if (OMX_ErrorNone != err) {
        OMX_LOGD();
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
  OMX_LOGD("Set component %s's parameter index %s", mName, OmxIndex2String(index));
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
        OMX_LOGD();
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


OMX_ERRORTYPE OmxComponentImpl::useBuffer(
    OMX_BUFFERHEADERTYPE **bufHdr,
    OMX_U32 portIndex,
    OMX_PTR appPrivate,
    OMX_U32 size,
    OMX_U8 *buffer) {
  MmuAutoLock lock(mLock);

  OMX_ERRORTYPE err =OMX_ErrorNone;
  OmxPortImpl *port = getPort(portIndex);
  if (NULL == port) {
    return OMX_ErrorBadPortIndex;
  }
  if (OMX_StateInvalid == mState) {
    OMX_LOGE("%s mState is OMX_StateInvalid", mName);
    return OMX_ErrorInvalidState;
  } else if (OMX_FALSE == port->isEnabled()) {
    OMX_LOGE("%s port %d is not enabled", mName, portIndex);
    return OMX_ErrorIncorrectStateOperation;
  } else if (mState != OMX_StateLoaded &&
      mState != OMX_StateWaitForResources) {
    OMX_LOGE("%s mState = %s(%d) is not right for use buffer",
        mName, OmxState2String(mState), mState);
    return OMX_ErrorIncorrectStateOperation;
  }

  OMX_LOGD("%s port %lu use buffer", mName, portIndex);
  err = port->useBuffer(bufHdr,appPrivate,size,buffer);
  if (port->mBufferList.size() == port->getBufferCount()) {
    port->setPopulated(OMX_TRUE);
    checkTransitions();
  }

  return err;
}

OMX_ERRORTYPE OmxComponentImpl::allocateBuffer(
    OMX_BUFFERHEADERTYPE **bufHdr,
    OMX_U32 portIndex,
    OMX_PTR appPrivate,
    OMX_U32 size) {
  MmuAutoLock lock(mLock);

  OMX_ERRORTYPE err = OMX_ErrorNone;
  OmxPortImpl *port = getPort(portIndex);
  if (NULL == port) {
    OMX_LOGE("%s port %d is not found", mName, portIndex);
    return OMX_ErrorBadPortIndex;
  }
  if (mState == OMX_StateInvalid) {
    OMX_LOGE("%s mState is OMX_StateInvalid", mName);
    return OMX_ErrorInvalidState;
  } else if (port->isEnabled() == OMX_FALSE) {
    OMX_LOGE("%s port %d is not enabled", mName, portIndex);
    return OMX_ErrorIncorrectStateOperation;
  } else if (mState != OMX_StateLoaded && mState != OMX_StateWaitForResources) {
    OMX_LOGE("%s mState = %s(%d) is not right for allocate buffer",
        mName, OmxState2String(mState), mState);
    return OMX_ErrorIncorrectStateOperation;
  }

  OMX_LOGD("%s port %lu allocate buffer", mName, portIndex);
  err = port->allocateBuffer(bufHdr, appPrivate, size);
  if (port->mBufferList.size() == port->getBufferCount()) {
    port->setPopulated(OMX_TRUE);
    checkTransitions();
  }

  return err;
}

OMX_ERRORTYPE OmxComponentImpl::freeBuffer(
    OMX_U32 portIndex,
    OMX_BUFFERHEADERTYPE *buffer) {
  MmuAutoLock lock(mLock);

  OMX_ERRORTYPE err =OMX_ErrorNone;
  OmxPortImpl *port = getPort(portIndex);
  if (NULL == port) {
    OMX_LOGE("%s Bad port index %lu", mName, portIndex);
    return OMX_ErrorBadPortIndex;
  }
#if 0 // TODO: check whether we can free buffer
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
#endif

  OMX_LOGD("%s port %lu free buffer %p", mName, portIndex, buffer);
  err = port->freeBuffer(buffer);
  port->setPopulated(OMX_FALSE);
  if (port->mBufferList.size() == 0) {
    OMX_LOGD("All buffer freed on port %lu", port->getPortIndex());
    checkTransitions();
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
  MmuAutoLock lock(mLock);

  OMX_ERRORTYPE err = OMX_ErrorNone;
  port_iterator it = mPortMap.find(buffer->nInputPortIndex);
  if (it == mPortMap.end()) {
    return OMX_ErrorBadPortIndex;
  }
  OmxPortImpl *port = it->second;
  if (!port->isEnabled()) {
    return OMX_ErrorIncorrectStateOperation;
  }

  //OMX_LOGD("%s empty buffer %p, size %d, flag %x, ts "TIME_FORMAT,
  //    mName, buffer, buffer->nFilledLen, buffer->nFlags, TIME_ARGS(buffer->nTimeStamp));
  port->pushBuffer(buffer);

  EventInfo info;
  info.SetInt32("what", kWhatEmptyThisBuffer);
  OmxEvent *event = new OmxEvent(this, &OmxComponentImpl::onOmxEvent, info);
  mOmxEventQueue->postEvent(event);
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::fillThisBuffer(
    OMX_BUFFERHEADERTYPE *buffer) {
  MmuAutoLock lock(mLock);

  OMX_ERRORTYPE err =OMX_ErrorNone;
  port_iterator it = mPortMap.find(buffer->nOutputPortIndex);
  if (it == mPortMap.end()) {
    return OMX_ErrorBadPortIndex;
  }
  OmxPortImpl *port = it->second;
  if (!port->isEnabled()) {
    return OMX_ErrorIncorrectStateOperation;
  }

  //OMX_LOGD("%s fill buffer %p", mName, buffer);
  port->pushBuffer(buffer);

  EventInfo info;
  info.SetInt32("what", kWhatFillThisBuffer);
  OmxEvent *event = new OmxEvent(this, &OmxComponentImpl::onOmxEvent, info);
  mOmxEventQueue->postEvent(event);
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
  OMX_LOGD("%s, %d: ret %x", __FUNCTION__, __LINE__, err);
  return err;
}

OMX_ERRORTYPE OmxComponentImpl::sendCommand(
    OMX_COMMANDTYPE cmd, OMX_U32 param, OMX_PTR cmdData) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOGD("%s: cmd %d, param %x, cmdData %x",
      mName, cmd, param, cmdData);
  if(mState == OMX_StateInvalid) {
    postEvent(OMX_EventError, OMX_ErrorInvalidState, (OMX_U32)NULL);
    return OMX_ErrorInvalidState;
  }

  EventInfo info;
  info.SetInt32("what", kWhatSendCommand);
  info.SetInt32("cmd", cmd);
  info.SetInt32("param", param);
  OmxEvent *event = new OmxEvent(this, &OmxComponentImpl::onOmxEvent, info);
  mOmxEventQueue->postEvent(event);

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

OMX_ERRORTYPE OmxComponentImpl::flushPort(OmxPortImpl *port) {
  OMX_LOGD("[%s : port %d], flush", mName, port->getPortIndex());
  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (port->isTunneled()) {
    // TODO: Handle tunnel mode
  } else {
    if (port->isInput()) {
      OMX_U32 count =10;
      while (port->getCachedBuffer() != NULL) {
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

void OmxComponentImpl::onPortEnableCompleted(
    OMX_U32 portIndex, OMX_BOOL enabled) {

}

OMX_ERRORTYPE OmxComponentImpl::onPortFlush(OMX_U32 portIndex, bool sendFlushComplete) {
  if (portIndex == OMX_ALL) {
    for (OMX_U32 i = 0; i < mPortMap.size(); i++) {
      onPortFlush(i, sendFlushComplete);
    }
    if (sendFlushComplete) {
      postEvent(OMX_EventCmdComplete, OMX_CommandFlush, OMX_ALL);
    }
    return OMX_ErrorNone;
  }

  port_iterator it = mPortMap.find(portIndex);
  if (it == mPortMap.end()) {
    OMX_LOGE("port %d is not found!", portIndex);
    return OMX_ErrorBadPortIndex;
  }
  OmxPortImpl *port = it->second;
  if (port->isInput()) {
    while (!port->isEmpty()) {
      postEmptyBufferDone(port->popBuffer());
      port->returnBuffer(port->getCachedBuffer());
    }
  } else {
    while (!port->isEmpty()) {
      postFillBufferDone(port->popBuffer());
      port->returnBuffer(port->getCachedBuffer());
    }
  }
  if (sendFlushComplete) {
    postEvent(OMX_EventCmdComplete, OMX_CommandFlush, portIndex);
  }

  return OMX_ErrorNone;
}

void OmxComponentImpl::checkTransitions() {
  if (mTargetState != mState) {
    bool transitionComplete = true;
    if ((mState == OMX_StateLoaded) && (mTargetState == OMX_StateIdle)) {
      port_iterator it;
      for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
        OmxPortImpl *port = it->second;
        if (port->isPopulated() == OMX_FALSE) {
          transitionComplete = false;
          break;
        }
      }
    } else if ((mState == OMX_StateIdle) && (mTargetState == OMX_StateLoaded)) {
      port_iterator it;
      for (it = mPortMap.begin(); it != mPortMap.end(); it++) {
        OmxPortImpl *port = it->second;
        OMX_U32 n = port->mBufferList.size();
        if (n > 0) {
          transitionComplete = false;
        }
      }
    }

    if (transitionComplete) {
      OMX_LOGD("%s OMX_CommandStateSet(%s), OMX_EventCmdComplete",
          mName, OmxState2String(mState));
      mState = mTargetState;
      postEvent(OMX_EventCmdComplete, OMX_CommandStateSet, mState);
    }
  }
}

OMX_ERRORTYPE OmxComponentImpl::onSetState(OMX_STATETYPE state) {
  OMX_LOGD("%s, state change, %s(%d) -> %s(%d)", mName,
      OmxState2String(mState), mState, OmxState2String(state), state);

  OMX_ERRORTYPE err = OMX_ErrorNone;
  if (mState == state) {
    OMX_LOGD("target state %s, same as current, just return", OmxState2String(mState));
    mStateInprogress = OMX_FALSE;
    return err;
  }

  err = isStateTransitionValid(mState, state);
  if (OMX_ErrorNone != err) {
    postEvent(OMX_EventError, err, (OMX_U32)NULL);
    return err;
  }

  if ((OMX_StateExecuting == mState) && (OMX_StateIdle == state)) {
    for (OMX_U32 i = 0; i < mPortMap.size(); i++) {
      // Return all buffers to client.
      onPortFlush(i, false);
    }
  }

  mTargetState = state;
  OMX_LOGD("%s, state change, %s(%d) -> %s(%d)", mName,
      OmxState2String(mState), mState, OmxState2String(mTargetState), mTargetState);
  checkTransitions();

  return err;
}

OMX_ERRORTYPE OmxComponentImpl::onProcessCommand(EventInfo &info) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  mint32 cmd = 0, param = 0;
  info.GetInt32("cmd", &cmd);
  info.GetInt32("param", &param);

  OMX_LOGD("%s: cmd %d, param %x", mName, cmd, param);

  switch (cmd) {
    case OMX_CommandStateSet: {
      OMX_LOGD("%s, OMX_CommandStateSet", mName);
      OMX_STATETYPE target_state = (OMX_STATETYPE)param;
      err = onSetState(target_state);
      break;
    }
    case OMX_CommandFlush: {
      OMX_LOGD("%s, OMX_CommandFlush, param = %d, OMX_ALL = %d", mName, param, OMX_ALL);
      onPortFlush(param, true);
      flush();
      break;
    }
    case OMX_CommandPortDisable: {
      OMX_LOGD("%s, OMX_CommandPortDisable", mName);
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
      OMX_LOGD("%s, OMX_CommandPortEnable", mName);
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
    default:
      err = OMX_ErrorUnsupportedIndex;
      break;
  }

  return err;
}

void OmxComponentImpl::onOmxEvent(EventInfo &info) {
  MmuAutoLock lock(mLock);

  mint32 msgType = 0;
  info.GetInt32("what", &msgType);
  //OMX_LOGD("%s: msgType = %d", mName, msgType);
  switch (msgType) {
    case kWhatSendCommand:
    {
      onProcessCommand(info);
      break;
    }
    case kWhatEmptyThisBuffer:
    case kWhatFillThisBuffer:
    {
      processBuffer();
      break;
    }
    default:
      OMX_LOGE("Unknown event %d!", msgType);
      break;
  }
}

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

}
