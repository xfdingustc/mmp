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

#include "MHALOmxComponent.h"
#include "MHALOmxUtils.h"

#undef  LOG_TAG
#define LOG_TAG "MHALOmxComponent"

namespace mmp {

static OMX_CALLBACKTYPE sCallbacks = {
  MHALOmxComponent::OnEvent,
  MHALOmxComponent::OnEmptyBufferDone,
  MHALOmxComponent::OnFillBufferDone,
};

MHALOmxComponent::MHALOmxComponent()
  : mCallbackTarget_(NULL),
    mCompHandle_(NULL),
    mNumPorts_(0),
    mFlushCount_(0),
    mSyncCmd_(OMX_FALSE),
    mContainerType_(OMX_FORMATMax),
    omx_tunnel_mode_(OMX_TUNNEL),
    MmpElement("mhal-omx-comp") {
  memset(compName, 0x00, 64);
  OMX_LOGD("%s ENTER", compName);
  mOMX_ = BerlinOmxProxy::getInstance();

  mStateSignal_ = new kdCondSignal();
  mFlushSignal_ = new kdCondSignal();
  if (!mStateSignal_ || !mFlushSignal_) {
    OMX_LOGE("Out of memory!");
  }

  OMX_LOGD("%s EXIT", compName);
}

MHALOmxComponent::~MHALOmxComponent() {
  OMX_LOGD("%s ENTER", compName);
  delete mStateSignal_;
  delete mFlushSignal_;
  if (mOMX_ && mCompHandle_) {
    mOMX_->destroyComponentInstance(mCompHandle_);
    mCompHandle_ = NULL;
  }
  OMX_LOGD("%s EXIT", compName);
}

OMX_BOOL MHALOmxComponent::setUp(OMX_TUNNEL_MODE mode) {
  omx_tunnel_mode_ = mode;
  if (allocateNode(getComponentRole())) {
    if (!configPort()) {
      OMX_LOGE("%s failed to config component!", compName);
      return OMX_FALSE;
    }
  } else {
    OMX_LOGE("%s failed to allocate node", compName);
    return OMX_FALSE;
  }

  return OMX_TRUE;
}

OMX_BOOL MHALOmxComponent::configPort() {
  return OMX_TRUE;
}

OMX_BOOL MHALOmxComponent::allocateNode(const char *componentRole) {
  if (!componentRole) {
    OMX_LOGE("can't allocate component without role.");
    return OMX_FALSE;
  }
  const char *compName = mOMX_->findComponentFromRole(componentRole);
  OMX_LOGD("compName = %s", compName);
  if (!compName) {
    OMX_LOGD("compName is not found!!!");
    return OMX_FALSE;
  }
  mOMX_->makeComponentInstance(compName, &sCallbacks, this, &mCompHandle_);
  return OMX_TRUE;
}

OMX_BOOL MHALOmxComponent::setContainerType(OMX_MEDIACONTAINER_FORMATTYPE container) {
  OMX_LOGD("%s ENTER", compName);
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  mContainerType_ = container;

  // Set container type to OMX component.
  if (OMX_FORMATMax != container) {
    OMX_MEDIACONTAINER_INFOTYPE param;
    InitOmxHeader(&param);
    param.eFmtType = container;
    ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamMediaContainer, &param);
    if (OMX_ErrorNone != ret) {
      OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamMediaContainer) with error %d", ret);
      return OMX_FALSE;
    }
  }

  OMX_LOGD("%s EXIT", compName);
  return OMX_TRUE;
}

OMX_BOOL MHALOmxComponent::setOmxState(OMX_STATETYPE state) {
  OMX_LOGD("%s ENTER, set state %s(%d)", compName, omxState2String(state), state);
  OMX_ERRORTYPE err = OMX_ErrorNone;

  if ((state < OMX_StateLoaded) || (state > OMX_StateWaitForResources)) {
    OMX_LOGE("%s invalid OMX state %d", compName, state);
    return OMX_FALSE;
  }

  OMX_STATETYPE original_state = OMX_StateInvalid;
  err = OMX_GetState(mCompHandle_, &original_state);
  if (OMX_ErrorNone != err) {
    OMX_LOGE("%s OMX_GetState() failed with error %d", compName, err);
    return OMX_FALSE;
  }
  OMX_LOGI("%s original state is %s", compName, omxState2String(original_state));
  if (state == original_state) {
    OMX_LOGI("%s omx component already in %s", compName, omxState2String(state));
    return OMX_TRUE;
  }

  mSyncCmd_ = OMX_TRUE;
  mStateSignal_->resetSignal();
  err = OMX_SendCommand(mCompHandle_, OMX_CommandStateSet, state, NULL);
  if (OMX_ErrorNone != err) {
    OMX_LOGE("%s OMX_SendCommand(OMX_CommandStateSet) to OMX_StateIdle failed "
        "with error %d", compName, err);
    return OMX_FALSE;
  }

  // Resouce operations.
  if ((OMX_StateLoaded == original_state) && (OMX_StateIdle == state)) {
    allocateResource();
  }
  if ((OMX_StateIdle == original_state) && (OMX_StateLoaded == state)) {
    releaseResource();
  }

  mStateSignal_->waitSignal();
  mSyncCmd_ = OMX_FALSE;
  OMX_LOGD("%s EXIT", compName);
  return OMX_TRUE;
}

OMX_BOOL MHALOmxComponent::setOmxStateAsync(OMX_STATETYPE state) {
  OMX_LOGD("%s ENTER, set state %s(%d)", compName, omxState2String(state), state);
  OMX_ERRORTYPE err = OMX_ErrorNone;

  if ((state < OMX_StateLoaded) || (state > OMX_StateWaitForResources)) {
    OMX_LOGE("%s invalid OMX state %d", compName, state);
    return OMX_FALSE;
  }

  OMX_STATETYPE original_state = OMX_StateInvalid;
  err = OMX_GetState(mCompHandle_, &original_state);
  if (OMX_ErrorNone != err) {
    OMX_LOGE("%s OMX_GetState() failed with error %d", compName, err);
    return OMX_FALSE;
  }
  OMX_LOGI("%s original state is %s", compName, omxState2String(original_state));
  if (state == original_state) {
    OMX_LOGI("%s omx component already in %s", compName, omxState2String(state));
    if (mCallbackTarget_) {
      mCallbackTarget_->OnOmxEvent(EVENT_COMMAND_COMPLETE, OMX_CommandStateSet);
    }
    return OMX_TRUE;
  }

  err = OMX_SendCommand(mCompHandle_, OMX_CommandStateSet, state, NULL);
  if (OMX_ErrorNone != err) {
    OMX_LOGE("%s OMX_SendCommand(OMX_CommandStateSet) to OMX_StateIdle failed "
        "with error %d", compName, err);
    return OMX_FALSE;
  }

  // Resouce operations.
  if ((OMX_StateLoaded == original_state) && (OMX_StateIdle == state)) {
    allocateResource();
  }
  if ((OMX_StateIdle == original_state) && (OMX_StateLoaded == state)) {
    releaseResource();
  }

  OMX_LOGD("%s EXIT", compName);
  return OMX_TRUE;
}

OMX_BOOL MHALOmxComponent::flushAsync() {
  OMX_LOGD("%s ENTER", compName);
  flush();
  OMX_LOGD("%s EXIT", compName);
  return OMX_TRUE;
}

void MHALOmxComponent::flush() {
  OMX_LOGD("%s ENTER", compName);
  OMX_ERRORTYPE err = OMX_ErrorNone;

  // Flush each omx component's port.
  mFlushCount_ = mNumPorts_;

  err = OMX_SendCommand(mCompHandle_, OMX_CommandFlush, OMX_ALL, NULL);
  if (OMX_ErrorNone != err) {
    OMX_LOGE("%s OMX_SendCommand(OMX_CommandFlush) failed with error %d", compName, err);
    return;
  }

  OMX_LOGD("%s EXIT", compName);
}

OMX_BOOL MHALOmxComponent::setupTunnel(OMX_U32 myPort, OMX_COMPONENTTYPE *comp, OMX_U32 compPort) {
  OMX_ERRORTYPE err = OMX_ErrorNone;

  err = mOMX_->setupTunnel(mCompHandle_, myPort, comp, compPort);
  if (OMX_ErrorNone != err) {
    OMX_LOGE("%s setupTunnel() between component %p and %p failed with error %d",
        compName, mCompHandle_, comp, err);
    return OMX_FALSE;
  }

  return OMX_TRUE;
}

OMX_ERRORTYPE MHALOmxComponent::OnOMXEvent(OMX_EVENTTYPE eEvent,
    OMX_U32 nData1, OMX_U32 nData2, OMX_PTR pEventData) {
  switch (eEvent) {
    case OMX_EventCmdComplete:
      if (OMX_CommandStateSet == nData1) {
        OMX_LOGI("%s Complete change to state %s(%d)",
            compName, omxState2String((OMX_STATETYPE)nData2), nData2);
        if (mSyncCmd_) {
          mStateSignal_->setSignal();
        } else {
          if (mCallbackTarget_) {
            mCallbackTarget_->OnOmxEvent(EVENT_COMMAND_COMPLETE, nData1);
          }
        }
      }
      if (OMX_CommandFlush == nData1) {
        if (mFlushCount_ > 0) {
          mFlushCount_--;
        }
        if (0 == mFlushCount_) {
          OMX_LOGI("%s flush completed.", compName);
          if (mCallbackTarget_) {
            mCallbackTarget_->OnOmxEvent(EVENT_COMMAND_COMPLETE, nData1);
          }
        }
      }
      break;
    default:
      break;
  }

  return OMX_ErrorNone;
}

OMX_ERRORTYPE MHALOmxComponent::EmptyBufferDone(OMX_BUFFERHEADERTYPE* pBuffer) {
  OMX_LOGD("%s", compName);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE MHALOmxComponent::FillBufferDone(OMX_BUFFERHEADERTYPE* pBuffer) {
  OMX_LOGD("%s", compName);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE MHALOmxComponent::OnEvent(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_EVENTTYPE eEvent,
    OMX_IN OMX_U32 nData1,
    OMX_IN OMX_U32 nData2,
    OMX_IN OMX_PTR pEventData) {
  MHALOmxComponent *comp = reinterpret_cast<MHALOmxComponent *>(pAppData);
  if (!comp) {
    OMX_LOGE("MHALOmxComponent is not found!");
    return OMX_ErrorBadParameter;
  }

  comp->OnOMXEvent(eEvent, nData1, nData2, pEventData);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE MHALOmxComponent::OnEmptyBufferDone(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_PTR pAppData,
    OMX_IN OMX_BUFFERHEADERTYPE* pBuffer) {
  MHALOmxComponent *comp = reinterpret_cast<MHALOmxComponent *>(pAppData);
  if (!comp) {
    OMX_LOGE("MHALOmxComponent is not found!");
    return OMX_ErrorBadParameter;
  }

  comp->EmptyBufferDone(pBuffer);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE MHALOmxComponent::OnFillBufferDone(
    OMX_OUT OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_PTR pAppData,
    OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer) {
  MHALOmxComponent *comp = reinterpret_cast<MHALOmxComponent *>(pAppData);
  if (!comp) {
    OMX_LOGE("MHALOmxComponent is not found!");
    return OMX_ErrorBadParameter;
  }

  comp->FillBufferDone(pBuffer);
  return OMX_ErrorNone;
}

}
