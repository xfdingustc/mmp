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

#define LOG_TAG "OmxPortImpl"
#include "OmxPortImpl.h"


namespace mmp {

OmxPortImpl::OmxPortImpl(void) :
    mInTransition(OMX_FALSE),
    mIsTunneled(OMX_FALSE),
    mIsFlushing(OMX_FALSE),
    mBufferSupplier(OMX_BufferSupplyUnspecified),
    mBuffer(NULL) {
  InitOmxHeader(&mDefinition);
  mDefinition.bEnabled = OMX_TRUE;
  mDefinition.bPopulated = OMX_FALSE;
  mDefinition.bBuffersContiguous = OMX_FALSE;
  mDefinition.nBufferAlignment = 0;
  mDefinition.nBufferCountMin = 1;
  mDefinition.nBufferCountActual = 3;
  mDefinition.nBufferSize = kDefaultBufferSize;
  mMark.hMarkTargetComponent = NULL;
  mMark.pMarkData = NULL;
}


OmxPortImpl::~OmxPortImpl(void) {
}

OMX_ERRORTYPE OmxPortImpl::enablePort() {
  setEnabled(OMX_TRUE);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxPortImpl::disablePort() {
  mDefinition.bEnabled = OMX_FALSE;
  mDefinition.bPopulated = OMX_FALSE;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxPortImpl::allocateBuffer(
    OMX_BUFFERHEADERTYPE **bufHdr,
    OMX_PTR appPrivate,
    OMX_U32 size) {
  OMX_BUFFERHEADERTYPE * buf = new OMX_BUFFERHEADERTYPE;
  if(buf == NULL){
    return OMX_ErrorInsufficientResources;
  }
  InitOmxHeader(buf);
  buf->pBuffer = (OMX_U8*)malloc(size);
  if(buf->pBuffer == NULL){
    return OMX_ErrorInsufficientResources;
  }
  buf->nAllocLen = size;
  buf->nFilledLen = 0;
  buf->nOffset = 0;
  buf->nFlags = 0;
  buf->hMarkTargetComponent = NULL;
  buf->pMarkData = NULL;
  buf->nTickCount= 0;
  buf->nTimeStamp = 0;
  buf->pAppPrivate = appPrivate;
  buf->nInputPortIndex = (getDir()== OMX_DirInput)? getPortIndex() : 0;
  buf->nOutputPortIndex = (getDir()== OMX_DirOutput)? getPortIndex() : 0;
  buf->pInputPortPrivate = NULL;
  buf->pOutputPortPrivate = NULL;
  buf->pPlatformPrivate = NULL;
  OMX_LOGD("Port %d allocate buffer %p", getPortIndex(),buf);
  *bufHdr = buf;
  OmxBuffer * omx_buf = new OmxBuffer(buf, OMX_TRUE, OWN_BY_PLAYER);
  mBufferList.push_back(omx_buf);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxPortImpl::freeBuffer(OMX_BUFFERHEADERTYPE *buffer) {
  vector<OmxBuffer*>::iterator it;
  for (it = mBufferList.begin(); it != mBufferList.end(); it++) {
    if(buffer == (*it)->mBuffer) {
      break;
    }
  }
  if (it == mBufferList.end()) {
    OMX_LOGE("Can't find buffer %p",buffer);
    return OMX_ErrorBadParameter;
  }
  if((*it)->mIsAllocator) {
    free(buffer->pBuffer);
    buffer->pBuffer = NULL;
  }
  OMX_LOGD("Port %d free buffer %p\n",getPortIndex(), buffer);
  delete buffer;
  delete (*it);
  mBufferList.erase(it);
  setPopulated(OMX_FALSE);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxPortImpl::useBuffer(OMX_BUFFERHEADERTYPE **bufHdr,
    OMX_PTR appPrivate,
    OMX_U32 size,
    OMX_U8 *buffer) {
  OMX_BUFFERHEADERTYPE * buf = new OMX_BUFFERHEADERTYPE;
  if(buf == NULL){
    return OMX_ErrorInsufficientResources;
  }
  InitOmxHeader(buf);
  buf->pAppPrivate = appPrivate;
  buf->pBuffer = buffer;
  buf->nAllocLen = size;
  buf->nFilledLen = 0;
  buf->nOffset = 0;
  buf->nFlags = 0;
  buf->hMarkTargetComponent = NULL;
  buf->pMarkData = NULL;
  buf->nTickCount= 0;
  buf->nTimeStamp = 0;
  buf->nInputPortIndex = (getDir()==OMX_DirInput)? getPortIndex() : 0;
  buf->nOutputPortIndex = (getDir()==OMX_DirOutput)? getPortIndex() : 0;
  buf->pInputPortPrivate = NULL;
  buf->pOutputPortPrivate = NULL;
  buf->pPlatformPrivate = NULL;
  *bufHdr = buf;
  OmxBuffer* omx_buf = new OmxBuffer(buf, OMX_FALSE, OWN_BY_PLAYER);
  mBufferList.push_back(omx_buf);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxPortImpl::setVideoParam(OMX_VIDEO_PARAM_PORTFORMATTYPE *video_param) {
  mVideoParam.eColorFormat = video_param->eColorFormat;
  mVideoParam.eCompressionFormat = video_param->eCompressionFormat;
  mVideoParam.xFramerate = video_param->xFramerate;
  return OMX_ErrorNone;
};

OMX_ERRORTYPE OmxPortImpl::getVideoParam(OMX_VIDEO_PARAM_PORTFORMATTYPE *video_param) {
  video_param->eColorFormat = mVideoParam.eColorFormat;
  video_param->eCompressionFormat = mVideoParam.eCompressionFormat;
  video_param->xFramerate = mVideoParam.xFramerate;
  return OMX_ErrorNone;
};

// TODO: need set more vars
void OmxPortImpl::setAudioParam(OMX_AUDIO_PARAM_PORTFORMATTYPE *audio_param) {
  mAudioParam.eEncoding = audio_param->eEncoding;
}

void OmxPortImpl::pushBuffer(OMX_BUFFERHEADERTYPE *buffer) {
  MmuAutoLock lock(mBufferMutex);

  mBufferQueue.push(buffer);
  if (NULL != buffer) {
    setBufferOwner(buffer, OWN_BY_OMX);

    if (getDir() == OMX_DirInput) {
      if (mMark.hMarkTargetComponent != NULL) {
        buffer->hMarkTargetComponent = mMark.hMarkTargetComponent;
        buffer->pMarkData = mMark.pMarkData;
        mMark.hMarkTargetComponent = NULL;
        mMark.pMarkData = NULL;
      }
    }
  }
}

void OmxPortImpl::returnBuffer(OMX_BUFFERHEADERTYPE * buffer) {
  MmuAutoLock lock(mBufferMutex);

  if (NULL != buffer) {
    setBufferOwner(buffer, OWN_BY_PLAYER);
  }
  if (mBuffer == buffer) {
    mBuffer = NULL;
  }
}

OMX_BUFFERHEADERTYPE* OmxPortImpl::popBuffer() {
  MmuAutoLock lock(mBufferMutex);

  if(mBufferQueue.size() == 0)
    return NULL;
  mBuffer =  mBufferQueue.front();
  if (getDir() == OMX_DirOutput) {
   if (mMark.hMarkTargetComponent != NULL) {
      mBuffer->hMarkTargetComponent = mMark.hMarkTargetComponent;
      mBuffer->pMarkData = mMark.pMarkData;
      mMark.hMarkTargetComponent = NULL;
      mMark.pMarkData = NULL;
    }
  }
  mBufferQueue.pop();
  if (NULL != mBuffer) {
    setBufferOwner(mBuffer, OWN_BY_LOWER);
  }
  return mBuffer;
}

OMX_BUFFERHEADERTYPE* OmxPortImpl::getBuffer() {
  MmuAutoLock lock(mBufferMutex);
  OMX_BUFFERHEADERTYPE* buffer = mBufferQueue.front();
  return buffer;
}

bool OmxPortImpl::isEmpty() {
  MmuAutoLock lock(mBufferMutex);
  bool empty = mBufferQueue.empty();
  return empty;
}

void OmxPortImpl::setBufferOwner(OMX_BUFFERHEADERTYPE *buffer,
    BUFFER_OWNER owner) {
  vector<OmxBuffer*>::iterator it;
  for (it = mBufferList.begin(); it != mBufferList.end(); it++) {
    if(buffer == (*it)->mBuffer) {
      break;
    }
  }
  if (it == mBufferList.end()) {
    OMX_LOGE("Can't find buffer %p",buffer);
    return;
  }
  if ((OWN_BY_PLAYER == (*it)->mOwner && OWN_BY_OMX == owner) ||
      (OWN_BY_OMX == (*it)->mOwner && OWN_BY_LOWER == owner) ||
      (OWN_BY_LOWER == (*it)->mOwner && OWN_BY_PLAYER == owner)) {
    (*it)->mOwner = owner;
  } else {
    OMX_LOGD("The buffer %p owner type is not normal, old = %d  new = %d",
        buffer, (*it)->mOwner, owner);
    (*it)->mOwner = owner;
  }
}

OMX_ERRORTYPE OmxPortImpl::flushPort() {
  mIsFlushing = OMX_TRUE;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxPortImpl::flushComplete() {
  mIsFlushing = OMX_FALSE;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxPortImpl::markBuffer(OMX_MARKTYPE* mark) {
  mMark = *mark;
  return OMX_ErrorNone;
}

}
