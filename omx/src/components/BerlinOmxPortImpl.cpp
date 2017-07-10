#define LOG_TAG "BerlinOmxPortImpl"
#include "BerlinOmxPortImpl.h"

//#include <exception>

namespace berlin {

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
  mBufferMutex = kdThreadMutexCreate(KD_NULL);
  //mBufferCond = kdThreadCondCreate(KD_NULL);
  mBufferSem = kdThreadSemCreate(KD_NULL);
  mMark.hMarkTargetComponent = NULL;
  mMark.pMarkData = NULL;
}


OmxPortImpl::~OmxPortImpl(void) {
  kdThreadMutexFree(mBufferMutex);
  //kdThreadCondFree(mBufferCond);
  kdThreadSemFree(mBufferSem);
}

OMX_ERRORTYPE OmxPortImpl::enablePort() {
  setEnabled(OMX_TRUE);

  //if(OMX_TRUE == mDefinition.bEnabled ) {
  //  return OMX_ErrorNone;
  //}
  // If port is tunneled, allocate buffers
  //if()

  // Non-tunnel mode, enabled until resource is ready.
  //for(OMX_U32 i= 0; i<mDefinition.nBufferCountActual; i++) {
  //
  //}
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxPortImpl::disablePort() {
  mDefinition.bEnabled = OMX_FALSE;
  mDefinition.bPopulated = OMX_FALSE;
  //if (mBufferList.size() != 0) {
  //  mInTransition = OMX_TRUE;
  //}
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
#if 0
  try
  {
    buf->pBuffer = new OMX_U8[size];
  }
  catch (std::bad_alloc)
  {
    delete buf;
    return OMX_ErrorInsufficientResources;
  }
#else
  buf->pBuffer = (OMX_U8*)malloc(size);
  if(buf->pBuffer == NULL){
    return OMX_ErrorInsufficientResources;
  }
#endif
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

///@todo need set more vars
void OmxPortImpl::setAudioParam(OMX_AUDIO_PARAM_PORTFORMATTYPE *audio_param) {
  mAudioParam.eEncoding = audio_param->eEncoding;
}

void OmxPortImpl::pushBuffer(OMX_BUFFERHEADERTYPE *buffer) {
  kdThreadMutexLock(mBufferMutex);
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
  //kdThreadCondSignal(mBufferCond);
  kdThreadMutexUnlock(mBufferMutex);
  kdThreadSemPost(mBufferSem);
}

void OmxPortImpl::returnBuffer(OMX_BUFFERHEADERTYPE * buffer) {
  kdThreadMutexLock(mBufferMutex);
  if (NULL != buffer) {
    setBufferOwner(buffer, OWN_BY_PLAYER);
  }
  if (mBuffer == buffer) {
    mBuffer = NULL;
  }
  kdThreadMutexUnlock(mBufferMutex);
}

OMX_BUFFERHEADERTYPE* OmxPortImpl::popBuffer() {
  //kdThreadSemWaitTimeout(mBufferSem, 1000);
  kdThreadSemWait(mBufferSem);
  if(mBufferQueue.size() == 0)
    return NULL;
  kdThreadMutexLock(mBufferMutex);
  //OMX_BUFFERHEADERTYPE* buffer = mBufferQueue.front();
  mBuffer = mBufferQueue.front();
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
  kdThreadMutexUnlock(mBufferMutex);
  return mBuffer;
}

void  OmxPortImpl::postBuffer() {
  kdThreadSemPost(mBufferSem);
}

OMX_BUFFERHEADERTYPE* OmxPortImpl::getBuffer() {
  kdThreadMutexLock(mBufferMutex);
  OMX_BUFFERHEADERTYPE* buffer = mBufferQueue.front();
  kdThreadMutexUnlock(mBufferMutex);
  return buffer;
}

bool OmxPortImpl::isEmpty() {
  kdThreadMutexLock(mBufferMutex);
  bool empty = mBufferQueue.empty();
  kdThreadMutexUnlock(mBufferMutex);
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

OMX_BUFFERHEADERTYPE * OmxPortImpl::returnCachedbuffer() {
  kdThreadMutexLock(mBufferMutex);
  vector<OmxBuffer*>::iterator it;
  for (it = mBufferList.begin(); it != mBufferList.end(); it++) {
    if (OWN_BY_LOWER == (*it)->mOwner) {
      kdThreadMutexUnlock(mBufferMutex);
      return (*it)->mBuffer;
    }
  }
  OMX_LOGD("No more lower owner buffer can been found.");
  kdThreadMutexUnlock(mBufferMutex);
  return NULL;
}

OMX_ERRORTYPE OmxPortImpl::flushPort() {
#if 0
  kdThreadMutexLock(mBufferMutex);
  while(!mBufferQueue.empty()) {
    OMX_BUFFERHEADERTYPE* buffer = mBufferQueue.front();
    mBufferQueue.pop();
  }
  kdThreadMutexUnlock(mBufferMutex);
#else
  mIsFlushing = OMX_TRUE;
#endif
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

} //namespace berlin
