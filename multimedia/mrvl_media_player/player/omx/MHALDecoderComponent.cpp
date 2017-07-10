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

#include "MHALDecoderComponent.h"

#undef  LOG_TAG
#define LOG_TAG "MHALDecoderComponent"

namespace mmp {

MmpError MHALOmxSinkPad::ChainFunction(MmpBuffer* buf) {
  M_ASSERT_FATAL(buf, MMP_PAD_FLOW_ERROR);
  MmpError chain_ret = MMP_NO_ERROR;

  MHALDecoderComponent* comp = reinterpret_cast<MHALDecoderComponent*>(GetOwner());
  M_ASSERT_FATAL(comp, MMP_PAD_FLOW_ERROR);

  MediaResult ret = comp->FeedData(buf);
  if (kOutOfMemory == ret) {
    chain_ret = MMP_PAD_FLOW_BLOCKED;
  } else if (ret != kSuccess) {
    chain_ret = MMP_PAD_FLOW_ERROR;
  }

  return chain_ret;
}

MmpError MHALOmxSinkPad::LinkFunction() {
  M_ASSERT_FATAL(peer_, MMP_PAD_FLOW_NOT_LINKED);

  MmpCaps &peer_caps = peer_->GetCaps();

  MmpCaps &my_caps = GetCaps();

  // Call operator= to copy vaule.
  my_caps = peer_caps;

  return MMP_NO_ERROR;
}

MHALDecoderComponent::MHALDecoderComponent(OMX_PORTDOMAINTYPE domain)
  : mDomain_(domain),
    mBuffers_(NULL),
    mCanFeedData_(OMX_FALSE),
    mInPortEOS_(OMX_FALSE),
    mOutPortEOS_(OMX_FALSE),
    mShouldExit_(OMX_FALSE),
    mThreadId_(0),
    bWaitVideoIFrame_(OMX_FALSE) {
  OMX_LOGD("%s ENTER", compName);
  mInBufferLock_ = kdThreadMutexCreate(KD_NULL);
  mPacketListLock_ = kdThreadMutexCreate(KD_NULL);
  mDataList_ = new MmpBufferList;
  if (!mInBufferLock_ || !mPacketListLock_ || !mDataList_) {
    OMX_LOGE("Out of memory!");
  }

  mNativeWindow_ = NULL;

  MmpPad* sink_pad = static_cast<MmpPad*>(
      new MHALOmxSinkPad("sink", MmpPad::MMP_PAD_SINK, MmpPad::MMP_PAD_MODE_NONE));
  AddPad(sink_pad);

  OMX_LOGD("%s EXIT", compName);
}

MHALDecoderComponent::~MHALDecoderComponent() {
  OMX_LOGD("%s ENTER", compName);
  if (mInBufferLock_) {
    kdThreadMutexFree(mInBufferLock_);
  }
  if (mPacketListLock_) {
    kdThreadMutexFree(mPacketListLock_);
  }
  if (mDataList_) {
    delete mDataList_;
    mDataList_ = NULL;
  }
  OMX_LOGD("%s EXIT", compName);
}

void MHALDecoderComponent::flush() {
  OMX_LOGD("%s ENTER", compName);
  OMX_ERRORTYPE err = OMX_ErrorNone;

  kdThreadMutexLock(mPacketListLock_);

  // Clear EOS flag.
  mInPortEOS_ = OMX_FALSE;

  if (mDataList_) {
    mDataList_->flush();
  }

  if (OMX_FORMAT_MPEG_TS == mContainerType_) {
    bWaitVideoIFrame_ = OMX_TRUE;
  }

  kdThreadMutexUnlock(mPacketListLock_);

  MHALOmxComponent::flush();

  OMX_LOGD("%s EXIT", compName);
}

OMX_BOOL MHALDecoderComponent::allocateInputBuffers(OMX_U32 portIndex) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOmxHeader(&def);
  def.nPortIndex = portIndex;
  err = OMX_GetParameter(mCompHandle_, OMX_IndexParamPortDefinition, &def);
  if (OMX_ErrorNone != err) {
    OMX_LOGE("%s OMX_GetParameter(OMX_IndexParamPortDefinition) failed with error %d",
        compName, err);
    return OMX_FALSE;
  }

  OMX_LOGD("%s omx component's port %d needs %d buffers, buffer size %d", compName,
      portIndex, def.nBufferCountActual, def.nBufferSize);
  if (OMX_TUNNEL == omx_tunnel_mode_) {
    mBuffers_ = new OMX_U8 *[def.nBufferCountActual];
    if (!mBuffers_) {
      OMX_LOGE("Out of memory!");
      return OMX_FALSE;
    }
    for (OMX_U32 j = 0; j < def.nBufferCountActual; j++) {
      OMX_BUFFERHEADERTYPE *header = NULL;
      mBuffers_[j] = new OMX_U8[def.nBufferSize];
      if (!mBuffers_[j]) {
        OMX_LOGE("Out of memory!");
        return OMX_FALSE;
      }
      err = OMX_UseBuffer(mCompHandle_, &header, portIndex,
          NULL, def.nBufferSize, mBuffers_[j]);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("%s OMX_UseBuffer() the %dth failed with error %d", compName, j, err);
        return OMX_FALSE;
      }
      mInputBuffers_.push_back(header);
    }
  } else if (OMX_NON_TUNNEL == omx_tunnel_mode_) {
    // For non-tunnel mode, omx only support allocate buffer at the moment.
    for (OMX_U32 j = 0; j < def.nBufferCountActual; j++) {
      OMX_BUFFERHEADERTYPE *header = NULL;
      err = OMX_AllocateBuffer(mCompHandle_, &header, portIndex,
          NULL, def.nBufferSize);
      if (OMX_ErrorNone != err) {
        OMX_LOGE("%s OMX_UseBuffer() the %dth failed with error %d", compName, j, err);
        return OMX_FALSE;
      }
      mInputBuffers_.push_back(header);
    }
  }

  return OMX_TRUE;
}

OMX_BOOL MHALDecoderComponent::allocateOutputBuffers(OMX_U32 portIndex) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOmxHeader(&def);
  def.nPortIndex = portIndex;
  err = OMX_GetParameter(mCompHandle_, OMX_IndexParamPortDefinition, &def);
  if (OMX_ErrorNone != err) {
    OMX_LOGE("%s OMX_GetParameter(OMX_IndexParamPortDefinition) failed with error %d",
        compName, err);
    return OMX_FALSE;
  }

  OMX_LOGD("%s omx component's port %d needs %d buffers, buffer size %d", compName,
      portIndex, def.nBufferCountActual, def.nBufferSize);
  for (OMX_U32 j = 0; j < def.nBufferCountActual; j++) {
    OMX_BUFFERHEADERTYPE *header = NULL;
    err = OMX_AllocateBuffer(mCompHandle_, &header, portIndex,
        NULL, def.nBufferSize);
    if (OMX_ErrorNone != err) {
      OMX_LOGE("%s OMX_UseBuffer() the %dth failed with error %d", compName, j, err);
      return OMX_FALSE;
    }
    BufferInfo *info = new BufferInfo;
    if (!info) {
      OMX_LOGE("%s Out of memory!", compName);
      return OMX_FALSE;
    }
    info->header = header;
    info->graphicBuffer = NULL;
    info->buffer = new MmpBuffer((muint8*)(header->pBuffer));
    info->status = OWNED_BY_US;
    mOutputBuffers_.push_back(info);
  }

  return OMX_TRUE;
}

OMX_BOOL MHALDecoderComponent::getGraphicBufferUsage(OMX_U32 portIndex, OMX_U32* usage) {
  OMX_INDEXTYPE index;
  OMX_STRING name = const_cast<OMX_STRING>(
          "OMX.google.android.index.getAndroidNativeBufferUsage");
  OMX_ERRORTYPE err = OMX_GetExtensionIndex(mCompHandle_, name, &index);
  if (err != OMX_ErrorNone) {
    OMX_LOGE("%s OMX_GetExtensionIndex %s failed", compName, name);
    return OMX_FALSE;
  }

  OMX_VERSIONTYPE ver;
  ver.s.nVersionMajor = 1;
  ver.s.nVersionMinor = 0;
  ver.s.nRevision = 0;
  ver.s.nStep = 0;
  GetAndroidNativeBufferUsageParams params = {
      sizeof(GetAndroidNativeBufferUsageParams), ver, portIndex, 0,
  };

  err = OMX_GetParameter(mCompHandle_, index, &params);
  if (err != OMX_ErrorNone) {
    OMX_LOGE("%s OMX_GetAndroidNativeBufferUsage failed with error %d (0x%08x)",
        compName, err, err);
    return OMX_FALSE;
  }

  *usage = params.nUsage;
  return OMX_TRUE;
}

OMX_BOOL MHALDecoderComponent::useGraphicBuffer(
    OMX_U32 portIndex, sp<GraphicBuffer> graphicBuffer) {
  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_INDEXTYPE index;
  OMX_STRING name = const_cast<OMX_STRING>(
      "OMX.google.android.index.useAndroidNativeBuffer");
  OMX_ERRORTYPE err = OMX_GetExtensionIndex(mCompHandle_, name, &index);
  if (err != OMX_ErrorNone) {
    OMX_LOGE("%s OMX_GetExtensionIndex %s failed", compName, name);
    return OMX_FALSE;
  }

  OMX_BUFFERHEADERTYPE *header = NULL;

  OMX_VERSIONTYPE ver;
  ver.s.nVersionMajor = 1;
  ver.s.nVersionMinor = 0;
  ver.s.nRevision = 0;
  ver.s.nStep = 0;
  UseAndroidNativeBufferParams params = {
      sizeof(UseAndroidNativeBufferParams),
      ver,
      portIndex,
      NULL,
      &header,
      graphicBuffer,
  };

  ret = OMX_SetParameter(mCompHandle_, index, &params);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("%s OMX_UseAndroidNativeBuffer failed with error 0x%x", compName, err);
    return OMX_FALSE;
  }

  BufferInfo *info = new BufferInfo;
  if (!info) {
    OMX_LOGE("%s Out of memory!", compName);
    return OMX_FALSE;
  }
  info->header = header;
  info->graphicBuffer = graphicBuffer;
  info->buffer = new MmpBuffer((muint8*)(graphicBuffer.get()));
  info->status = OWNED_BY_US;
  mOutputBuffers_.push_back(info);

  return OMX_TRUE;
}

OMX_BOOL MHALDecoderComponent::enableGraphicBuffers(OMX_U32 portIndex, OMX_BOOL enable) {
  OMX_STRING name = const_cast<OMX_STRING>(
      "OMX.google.android.index.enableAndroidNativeBuffers");

  OMX_INDEXTYPE index;
  OMX_ERRORTYPE err = OMX_GetExtensionIndex(mCompHandle_, name, &index);
  if (err != OMX_ErrorNone) {
    if (enable) {
      OMX_LOGE("%s OMX_GetExtensionIndex %s failed", compName, name);
    }
    return OMX_FALSE;
  }

  OMX_VERSIONTYPE ver;
  ver.s.nVersionMajor = 1;
  ver.s.nVersionMinor = 0;
  ver.s.nRevision = 0;
  ver.s.nStep = 0;
  EnableAndroidNativeBuffersParams params = {
      sizeof(EnableAndroidNativeBuffersParams), ver, portIndex, enable,
  };

  err = OMX_SetParameter(mCompHandle_, index, &params);
  if (err != OMX_ErrorNone) {
    OMX_LOGE("%s OMX_EnableAndroidNativeBuffers failed with error %d (0x%08x)",
        compName, err, err);
    return OMX_FALSE;
  }

  return OMX_TRUE;
}

OMX_BOOL MHALDecoderComponent::allocateOutputBuffersFromNativeWindow(
    OMX_U32 portIndex) {
  if (mNativeWindow_ == NULL) {
    OMX_LOGE("%s mNativeWindow_ is NULL!", compName);
    return OMX_FALSE;
  }

  OMX_ERRORTYPE ret = OMX_ErrorNone;
  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOmxHeader(&def);
  def.nPortIndex = portIndex;
  ret = OMX_GetParameter(mCompHandle_, OMX_IndexParamPortDefinition, &def);
  if (OMX_ErrorNone != ret) {
    OMX_LOGE("%s OMX_GetParameter(OMX_IndexParamPortDefinition) failed with error %d",
        compName, ret);
    return OMX_FALSE;
  }

  OMX_LOGD("%s video nFrameWidth = %d, nFrameHeight = %d, eColorFormat = %d",
      compName, def.format.video.nFrameWidth, def.format.video.nFrameHeight,
      def.format.video.eColorFormat);
  status_t err = native_window_set_buffers_geometry(
      mNativeWindow_.get(),
      def.format.video.nFrameWidth,
      def.format.video.nFrameHeight,
      def.format.video.eColorFormat);
  if (err != 0) {
    OMX_LOGE("%s native_window_set_buffers_geometry failed: %s (%d)",
        compName, strerror(-err), -err);
    return OMX_FALSE;
  }

  err = native_window_set_scaling_mode(mNativeWindow_.get(),
      NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
  if (err != OK) {
    OMX_LOGE("%s set_scaling_mode failed: %s (%d)", compName, strerror(-err), -err);
    return OMX_FALSE;
  }

  OMX_U32 usage = 0;
  if (!getGraphicBufferUsage(portIndex, &usage)) {
    OMX_LOGW("%s querying usage flags from OMX IL component failed", compName);
    // XXX: Currently this error is logged, but not fatal.
    usage = 0;
  }

  OMX_LOGD("%s native_window_set_usage usage=0x%lx", compName, usage);
  err = native_window_set_usage(
      mNativeWindow_.get(), usage | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP);
  if (err != 0) {
    OMX_LOGE("%s native_window_set_usage failed: %s (%d)", compName, strerror(-err), -err);
    return OMX_FALSE;
  }

  int minUndequeuedBufs = 0;
  err = mNativeWindow_->query(mNativeWindow_.get(),
      NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &minUndequeuedBufs);
  if (err != 0) {
    OMX_LOGE("%s NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS query failed: %s (%d)",
        compName, strerror(-err), -err);
    return OMX_FALSE;
  }
  OMX_LOGD("%s minUndequeuedBufs = %d", compName, minUndequeuedBufs);

  if (def.nBufferCountActual < def.nBufferCountMin + minUndequeuedBufs) {
    OMX_U32 newBufferCount = def.nBufferCountMin + minUndequeuedBufs;
    def.nBufferCountActual = newBufferCount;
    ret = OMX_SetParameter(mCompHandle_, OMX_IndexParamPortDefinition, &def);
    if (OMX_ErrorNone != ret) {
      OMX_LOGE("%s setting nBufferCountActual to %lu failed: %d",
          compName, newBufferCount, ret);
      return OMX_FALSE;
    }
  }

  OMX_LOGD("%s allocating %lu buffers from a native window of size %lu on output port",
      compName, def.nBufferCountActual, def.nBufferSize);
  err = native_window_set_buffer_count(mNativeWindow_.get(), def.nBufferCountActual);
  if (err != 0) {
    OMX_LOGE("%s native_window_set_buffer_count failed: %s (%d)",
        compName, strerror(-err), -err);
    return OMX_FALSE;
  }

  for (OMX_U32 i = 0; i < def.nBufferCountActual; i++) {
    ANativeWindowBuffer* buf;
    err = native_window_dequeue_buffer_and_wait(mNativeWindow_.get(), &buf);
    if (err != 0) {
      OMX_LOGE("%s dequeueBuffer failed: %s (%d)", compName, strerror(-err), -err);
      break;
    }

    sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(buf, false));
    if (graphicBuffer == NULL) {
      OMX_LOGE("%s Out of memory!", compName);
      break;
    }
    OMX_LOGV("%s graphicBuffer.get() = %p, native window buf = %p",
        compName, graphicBuffer.get(), buf);
    useGraphicBuffer(portIndex, graphicBuffer);
  }

  for (OMX_U32 j = def.nBufferCountActual - minUndequeuedBufs; j < def.nBufferCountActual; j++) {
    BufferInfo *info = mOutputBuffers_[j];
    cancelBufferToNativeWindow(info);
  }
  return OMX_TRUE;
}

BufferInfo* MHALDecoderComponent::dequeueBufferFromNativeWindow() {
  // Dequeue the next buffer from the native window.
  ANativeWindowBuffer* buf;
  int fenceFd = -1;
  int err = native_window_dequeue_buffer_and_wait(mNativeWindow_.get(), &buf);
  if (err != 0) {
    OMX_LOGE("%s dequeueBuffer failed: %s (%d)", compName, strerror(-err), -err);
    return NULL;
  }

  for (OMX_U32 j = 0; j < mOutputBuffers_.size(); j++) {
    BufferInfo *info = mOutputBuffers_[j];
    if (info && info->graphicBuffer->handle == buf->handle) {
      info->status = OWNED_BY_US;
      return info;
    }
  }

  return NULL;
}

OMX_BOOL MHALDecoderComponent::cancelBufferToNativeWindow(BufferInfo *info) {
  if (!info) {
    OMX_LOGE("%s info is invalid!", compName);
    return OMX_FALSE;
  }
  OMX_LOGD("%s cancel GraphicBuffer %p", compName, info->graphicBuffer.get());
  status_t err = mNativeWindow_->cancelBuffer(
      mNativeWindow_.get(), info->graphicBuffer.get(), -1);
  if (err != 0) {
    OMX_LOGE("%s cancelBuffer failed: %s (%d)", compName, strerror(-err), -err);
    return OMX_FALSE;
  }
  info->status = OWNED_BY_NATIVE_WINDOW;
  return OMX_TRUE;
}

void MHALDecoderComponent::fillOmxBuffers() {
  for (OMX_U32 j = 0; j < mOutputBuffers_.size(); j++) {
    BufferInfo *info = mOutputBuffers_[j];
    if (OWNED_BY_US == info->status) {
      OMX_LOGI("%s OMX_FillThisBuffer %p", compName, info->header);
      OMX_FillThisBuffer(mCompHandle_, info->header);
      info->status = OWNED_BY_COMPONENT;
    }
  }
}

void MHALDecoderComponent::allocateOmxBuffers(OMX_PORTDOMAINTYPE domain) {
  OMX_LOGD("%s ENTER", compName);
  OMX_ERRORTYPE err = OMX_ErrorNone;

  OMX_PORT_PARAM_TYPE oParam;
  InitOmxHeader(&oParam);
  OMX_INDEXTYPE param_index = OMX_IndexComponentStartUnused;
  switch (domain) {
    case OMX_PortDomainAudio:
      param_index = OMX_IndexParamAudioInit;
      break;
    case OMX_PortDomainVideo:
      param_index = OMX_IndexParamVideoInit;
      break;
    case OMX_PortDomainImage:
      param_index = OMX_IndexParamImageInit;
      break;
    case OMX_PortDomainOther:
      param_index = OMX_IndexParamOtherInit;
      break;
    default:
      OMX_LOGE("%s Incoreect domain %x", compName, domain);
  }
  err = OMX_GetParameter(mCompHandle_, param_index, &oParam);
  if ((OMX_ErrorNone != err) || (oParam.nPorts <= 0)) {
    OMX_LOGE("%s OMX_GetParameter(%d) failed with error %d", compName, param_index, err);
    return;
  }

  allocateInputBuffers(oParam.nStartPortNumber);
  OMX_LOGD("%s omx_tunnel_mode_ = %s",
      compName, omx_tunnel_mode_ ? "OMX_NON_TUNNEL" : "OMX_TUNNEL");
  if (OMX_NON_TUNNEL == omx_tunnel_mode_) {
    // Allocate buffers on output port for non-tunnel mode.
    if (OMX_PortDomainAudio == domain) {
      allocateOutputBuffers(oParam.nStartPortNumber + 1);
    } else if (OMX_PortDomainVideo == domain) {
      allocateOutputBuffersFromNativeWindow(oParam.nStartPortNumber + 1);
    }
    // After the buffers are created, they can be sent to OMX.
    fillOmxBuffers();
  }

  OMX_LOGD("%s EXIT", compName);
}

void MHALDecoderComponent::freeOmxBuffers(OMX_PORTDOMAINTYPE domain) {
  OMX_LOGD("%s ENTER", compName);
  OMX_ERRORTYPE err = OMX_ErrorNone;

  OMX_PORT_PARAM_TYPE oParam;
  InitOmxHeader(&oParam);
  OMX_INDEXTYPE param_index = OMX_IndexComponentStartUnused;
  switch (domain) {
    case OMX_PortDomainAudio:
      param_index = OMX_IndexParamAudioInit;
      break;
    case OMX_PortDomainVideo:
      param_index = OMX_IndexParamVideoInit;
      break;
    case OMX_PortDomainImage:
      param_index = OMX_IndexParamImageInit;
      break;
    case OMX_PortDomainOther:
      param_index = OMX_IndexParamOtherInit;
      break;
    default:
      OMX_LOGE("%s Incoreect domain %x", compName, domain);
  }
  err = OMX_GetParameter(mCompHandle_, param_index, &oParam);
  if ((OMX_ErrorNone != err) || (oParam.nPorts <= 0)) {
    OMX_LOGE("%s OMX_GetParameter(%d) failed with error %d", compName, param_index, err);
    return;
  }

  OMX_PARAM_PORTDEFINITIONTYPE def;
  InitOmxHeader(&def);
  def.nPortIndex = oParam.nStartPortNumber;
  err = OMX_GetParameter(mCompHandle_, OMX_IndexParamPortDefinition, &def);
  if (OMX_ErrorNone != err) {
    OMX_LOGE("%s OMX_GetParameter(OMX_IndexParamPortDefinition) failed with "
        "error %d", compName, err);
    return;
  }

  // Free buffers on input port.
  for (list<OMX_BUFFERHEADERTYPE *>::iterator it = mInputBuffers_.begin();
      it != mInputBuffers_.end(); ++it) {
    OMX_BUFFERHEADERTYPE *header = *(it);
    err = OMX_FreeBuffer(mCompHandle_, oParam.nStartPortNumber, header);
    if (OMX_ErrorNone != err) {
      OMX_LOGE("%s OMX_FreeBuffer() failed with error %d", compName, err);
      continue;
    }
  }
  if (mBuffers_) {
    for (OMX_U32 j = 0; j < def.nBufferCountActual; j++) {
      OMX_LOGE("%s j = %d, def.nBufferCountActual = %d", compName, j, def.nBufferCountActual);
      delete mBuffers_[j];
      mBuffers_[j] = NULL;
    }
    delete[] mBuffers_;
    mBuffers_ = NULL;
  }
  mInputBuffers_.clear();

  // Free buffers on output port if in non-tunnel mode.
  if (OMX_NON_TUNNEL == omx_tunnel_mode_) {
    for (vector<BufferInfo *>::iterator it = mOutputBuffers_.begin();
        it != mOutputBuffers_.end(); ++it) {
      BufferInfo *info = *(it);
      if (info) {
        // Cancel the buffer if it belongs to an ANativeWindow.
        if ((OMX_PortDomainVideo == mDomain_) && (OWNED_BY_US == info->status)) {
          cancelBufferToNativeWindow(info);
        }
        // Delete MmpBuffer.
        info->buffer->data = NULL;
        info->buffer->size = 0;
        delete info->buffer;
        // Free OMX buffer;
        err = OMX_FreeBuffer(mCompHandle_, oParam.nStartPortNumber + 1, info->header);
        if (OMX_ErrorNone != err) {
          OMX_LOGE("%s OMX_FreeBuffer() failed with error %d", compName, err);
          continue;
        }
      }
      delete info;
    }
    mOutputBuffers_.clear();
  }

  OMX_LOGD("%s EXIT", compName);
}

OMX_BOOL MHALDecoderComponent::allocateResource() {
  allocateOmxBuffers(mDomain_);

  // Create working thread.
  OMX_LOGD("%s Create working thread.", compName);
  int res = pthread_create(&mThreadId_, NULL, WorkingThreadEntry, this);
  if (res) {
    OMX_LOGE("%s failed to create its work thread with err %d", compName, res);
  }

  if (OMX_FORMAT_MPEG_TS == mContainerType_) {
    bWaitVideoIFrame_ = OMX_TRUE;
  }

  return MHALOmxComponent::allocateResource();
}

OMX_BOOL MHALDecoderComponent::releaseResource() {
  freeOmxBuffers(mDomain_);
  if (mThreadId_ > 0) {
    mShouldExit_ = OMX_TRUE;
    OMX_LOGD("%s Wait for working thread to join.", compName);
    void* thread_ret;
    pthread_join(mThreadId_, &thread_ret);
    mThreadId_ = 0;
  }
  return MHALOmxComponent::releaseResource();
}

MediaResult MHALDecoderComponent::FeedData(MmpBuffer *buf) {
  bool ret = false;
  int queued_bytes = 0;

  M_ASSERT_FATAL(buf, kUnexpectedError);

  kdThreadMutexLock(mPacketListLock_);

  if (mDataList_) {
    if (mDataList_->canQueueMore(buf->size)) {
      ret = mDataList_->pushBack(buf);
    } else {
      kdThreadMutexUnlock(mPacketListLock_);
      return kOutOfMemory;
    }
  } else {
    // mDataList_ is not ready, give it a chance to retry.
    kdThreadMutexUnlock(mPacketListLock_);
    return kOutOfMemory;
  }

  kdThreadMutexUnlock(mPacketListLock_);

  return ret ? kSuccess : kUnexpectedError;
}

int MHALDecoderComponent::getBufferLevel(void) {
  int level = 0;
  uint32_t fullness = 0;
  uint32_t capacity = 0;

  if (mDataList_) {
    fullness = mDataList_->getBufferedSize();
    capacity = mDataList_->getCapacity();

    if (capacity) {
      level = (fullness * 100) / capacity;
    }
  }

  return level;
}

OMX_ERRORTYPE MHALDecoderComponent::EmptyBufferDone(OMX_BUFFERHEADERTYPE* pBuffer) {
  kdThreadMutexLock(mInBufferLock_);
  OMX_BOOL isEOS =
      (pBuffer->nFlags & OMX_BUFFERFLAG_EOS) ? OMX_TRUE : OMX_FALSE;
  if (isEOS) {
    OMX_LOGI("%s Receive EOS on output port", compName);
    //ctx->mEosSignal->setSignal();
    //ctx->mOutputEOS = OMX_TRUE;
  }

  // To avoid a same buffer be used twice.
  OMX_BOOL bufferReturned = OMX_FALSE;
  if (!mInputBuffers_.empty()) {
    for (list<OMX_BUFFERHEADERTYPE *>::iterator it = mInputBuffers_.begin();
        it != mInputBuffers_.end(); ++ it ) {
      OMX_BUFFERHEADERTYPE *buf = *it;
      if (pBuffer == buf) {
        bufferReturned = OMX_TRUE;
        break;
      }
    }
  }
  if (bufferReturned) {
    OMX_LOGW("%s Buffer %p has already been returned!!!", compName, pBuffer);
  } else {
    pBuffer->nFlags = 0;
    mInputBuffers_.push_back(pBuffer);
  }
  kdThreadMutexUnlock(mInBufferLock_);
  return OMX_ErrorNone;
}

MmpCaps* MHALDecoderComponent::getPadCaps() {
  MmpPad* video_sink_pad = GetSinkPad();
  if (!video_sink_pad) {
    OMX_LOGE("%s No sink pad found!", compName);
    return NULL;
  }

  MmpCaps &caps = video_sink_pad->GetCaps();

  return &caps;
}

mbool MHALDecoderComponent::SendEvent(MmpEvent* event) {
  M_ASSERT_FATAL(event, false);

  switch (event->GetEventType()) {
    case MmpEvent::MMP_EVENT_BUFFER_RENDERED: {
      MmpBuffer *buf = (MmpBuffer *)(event->GetEventData());
      fillThisBuffer(buf);
      break;
    }
    default:
      return true;
  }

  return true;
}

void* MHALDecoderComponent::WorkingThreadLoop() {
  const int sleeptime = 10 * 1000; // 10ms
  OMX_BOOL result = OMX_TRUE;

  while(!mShouldExit_) {
    result = processData();

    if (!result) {
      // Nothing to do, sleep 10ms.
      usleep(sleeptime);
    }
  }

  mShouldExit_ = OMX_FALSE;
  OMX_LOGD("%s Quit working thread now.", compName);
  return NULL;
}

void* MHALDecoderComponent::WorkingThreadEntry(void* thiz){
  if (thiz == NULL) {
    return NULL;
  }
  MHALDecoderComponent *pMHALOmxComponent = reinterpret_cast<MHALDecoderComponent*>(thiz);
  // Give our thread a name to assist in debugging.
  prctl(PR_SET_NAME, "MHALOMXThread", 0, 0, 0);
  return pMHALOmxComponent->WorkingThreadLoop();
}

}
