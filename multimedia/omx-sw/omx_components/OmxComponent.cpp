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

#define LOG_TAG "OmxComponent"
#include <OmxComponent.h>
#include <stdlib.h>

namespace mmp {

static OmxComponent *getComponent(OMX_HANDLETYPE hComponent) {
  return (OmxComponent *)
      ((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate;
}

static OMX_ERRORTYPE SendCommandWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_COMMANDTYPE Cmd,
    OMX_IN  OMX_U32 nParam1,
    OMX_IN  OMX_PTR pCmdData) {
  return getComponent(hComponent)->sendCommand(Cmd, nParam1, pCmdData);
}

static OMX_ERRORTYPE GetParameterWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nParamIndex,
    OMX_INOUT OMX_PTR pComponentParameterStructure) {
  return getComponent(hComponent)->getParameter(
      nParamIndex, pComponentParameterStructure);
}

static OMX_ERRORTYPE SetParameterWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_IN  OMX_PTR pComponentParameterStructure) {
  return getComponent(hComponent)->setParameter(
      nIndex, pComponentParameterStructure);
}

static OMX_ERRORTYPE GetConfigWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_INOUT OMX_PTR pComponentConfigStructure) {
  return getComponent(hComponent)->getConfig(nIndex, pComponentConfigStructure);
}

static OMX_ERRORTYPE SetConfigWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_IN  OMX_PTR pComponentConfigStructure) {
  return getComponent(hComponent)->setConfig(nIndex, pComponentConfigStructure);
}

static OMX_ERRORTYPE GetExtensionIndexWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_STRING cParameterName,
    OMX_OUT OMX_INDEXTYPE* pIndexType) {
  return getComponent(hComponent)->getExtensionIndex(cParameterName, pIndexType);
}

static OMX_ERRORTYPE GetStateWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_STATETYPE* pState) {
  return getComponent(hComponent)->getState(pState);
}

static OMX_ERRORTYPE UseBufferWrapper(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes,
    OMX_IN OMX_U8* pBuffer) {
  return getComponent(hComponent)->useBuffer(
      ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);
}

static OMX_ERRORTYPE AllocateBufferWrapper(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBuffer,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes) {
  return getComponent(hComponent)->allocateBuffer(
      ppBuffer, nPortIndex, pAppPrivate, nSizeBytes);
}

static OMX_ERRORTYPE FreeBufferWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_U32 nPortIndex,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer) {
  return getComponent(hComponent)->freeBuffer(nPortIndex, pBuffer);
}

static OMX_ERRORTYPE EmptyThisBufferWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer) {
  return getComponent(hComponent)->emptyThisBuffer(pBuffer);
}

static OMX_ERRORTYPE FillThisBufferWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer) {
  return getComponent(hComponent)->fillThisBuffer(pBuffer);
}

static OMX_ERRORTYPE ComponentDeInitWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent) {
  return getComponent(hComponent)->componentDeInit(hComponent);
}

static OMX_ERRORTYPE ComponentRoleEnumWrapper(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_U8 *cRole,
    OMX_IN OMX_U32 nIndex) {
  return getComponent(hComponent)->componentRoleEnum(cRole, nIndex);
}

static OMX_ERRORTYPE UseEGLImageWrapper(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN void* eglImage) {
  return getComponent(hComponent)->useEGLImage(
      ppBufferHdr,
      nPortIndex,
      pAppPrivate,
      eglImage);
}


static OMX_ERRORTYPE ComponentTunnelRequestWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_U32 nPort,
    OMX_IN  OMX_HANDLETYPE hTunneledComp,
    OMX_IN  OMX_U32 nTunneledPort,
    OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup) {
  return getComponent(hComponent)->componentTunnelRequest(
      nPort,
      hTunneledComp,
      nTunneledPort,
      pTunnelSetup);
}

static OMX_ERRORTYPE GetComponentVersionWrapper(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_STRING pComponentName,
    OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
    OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
    OMX_OUT OMX_UUIDTYPE* pComponentUUID) {
  return getComponent(hComponent)->getComponentVersion(
      pComponentName,
      pComponentVersion,
      pSpecVersion,
      pComponentUUID);
}

static OMX_ERRORTYPE SetCallbacksWrapper  (
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
    OMX_IN  OMX_PTR pAppData) {
  return getComponent(hComponent)->setCallbacks(
      pCallbacks,
      pAppData);
}

OMX_COMPONENTTYPE *OmxComponent::makeComponent(OmxComponent *base) {
  OMX_COMPONENTTYPE* result = new OMX_COMPONENTTYPE;
  if (!result) {
    return NULL;
  }

  result->nSize = sizeof(OMX_COMPONENTTYPE);
  result->nVersion.s.nVersionMajor = 1;
  result->nVersion.s.nVersionMinor = 0;
  result->nVersion.s.nRevision = 0;
  result->nVersion.s.nStep = 0;
  result->pComponentPrivate = base;
  result->pApplicationPrivate = NULL;

  // Map C functions to C++ functions.
  result->GetComponentVersion = GetComponentVersionWrapper;
  result->SendCommand = SendCommandWrapper;
  result->GetParameter = GetParameterWrapper;
  result->SetParameter = SetParameterWrapper;
  result->GetConfig = GetConfigWrapper;
  result->SetConfig = SetConfigWrapper;
  result->GetExtensionIndex = GetExtensionIndexWrapper;
  result->GetState = GetStateWrapper;
  result->ComponentTunnelRequest = ComponentTunnelRequestWrapper;
  result->UseBuffer = UseBufferWrapper;
  result->AllocateBuffer = AllocateBufferWrapper;
  result->FreeBuffer = FreeBufferWrapper;
  result->EmptyThisBuffer = EmptyThisBufferWrapper;
  result->FillThisBuffer = FillThisBufferWrapper;
  result->SetCallbacks = NULL;
  result->ComponentDeInit = ComponentDeInitWrapper;
  result->UseEGLImage = UseEGLImageWrapper;
  result->ComponentRoleEnum = ComponentRoleEnumWrapper;
  result->SetCallbacks = SetCallbacksWrapper;

  base->setComponentHandle(result);
  return result;
}

OmxComponent::OmxComponent(const OMX_CALLBACKTYPE *callbacks, OMX_PTR appData)
    : mCallbacks(callbacks),
      mAppData(appData),
      mComponentHandle(NULL) {
}

OmxComponent::OmxComponent() :
    mComponentHandle(NULL) {
}

OmxComponent::~OmxComponent() {
}

OMX_ERRORTYPE OmxComponent::setCallbacks (
    OMX_CALLBACKTYPE* callbacks,
    OMX_PTR appData) {
  mCallbacks = callbacks;
  mAppData = appData;
  return OMX_ErrorNone;
}

void OmxComponent::postEvent(OMX_EVENTTYPE event, OMX_U32 param1,
                             OMX_U32 param2) {
  (*mCallbacks->EventHandler)(
      mComponentHandle, mAppData, event, param1, param2, NULL);
}

void OmxComponent::postMark(OMX_PTR pMarkData) {
  (*mCallbacks->EventHandler)(
      mComponentHandle, mAppData, OMX_EventMark, 0, 0, pMarkData);
}

void OmxComponent::postFillBufferDone(OMX_BUFFERHEADERTYPE *bufHdr) {
  //OMX_LOGD("postFillBufferDone %p", bufHdr);
  (*mCallbacks->FillBufferDone)(
      mComponentHandle, mAppData, bufHdr);
}

void OmxComponent::postEmptyBufferDone(OMX_BUFFERHEADERTYPE *bufHdr) {
  //OMX_LOGD("postEmptyBufferDone %p", bufHdr);
  (*mCallbacks->EmptyBufferDone)(
      mComponentHandle, mAppData, bufHdr);
}

OMX_COMPONENTTYPE* OmxComponent::getComponentHandle(){
  return mComponentHandle;
}

void OmxComponent::setComponentHandle(OMX_COMPONENTTYPE *handle){
  mComponentHandle = handle;
}


}
