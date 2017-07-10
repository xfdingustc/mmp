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
#define LOG_TAG "OmxEncTest"

extern "C" {
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <OMX_IVCommon.h>
}
#include <BerlinOmxCommon.h>
#include <gtest/gtest.h>

#include <vector>
using namespace std;

static void dump_bytes(OMX_U8 *bits, size_t size) {
  static int first = 1;
  const char *mode = "a";
  if (first) {
    first = 0;
    mode = "w";
  }
  FILE *file = fopen("/data/video_encoded.dat", mode);
  if (size > 0) {
    fwrite(bits, 1, size, file);
  }
  fclose(file);
}

namespace berlin {
namespace test {

OMX_U32 getFrameSize(OMX_U32 width, OMX_U32 height, OMX_COLOR_FORMATTYPE color) {
  switch(color) {
    case OMX_COLOR_FormatYCrYCb:
    case OMX_COLOR_FormatYCbYCr:
    case OMX_COLOR_FormatYUV422Planar:
      return width * height * 2;
    case OMX_COLOR_FormatYUV420Planar:
    case OMX_COLOR_FormatYUV420SemiPlanar:
      return width * height * 3 / 2;
    default:
      OMX_LOGW("Unsupport color format %d", color);
      return 0;
  }
}

typedef struct testVectorInfo {
  OMX_STRING streamName;
  OMX_U32 width;
  OMX_U32 height;
  OMX_COLOR_FORMATTYPE color;
}testVectorInfo;

static testVectorInfo gTestVector[] = {
  {"/data/test.yuv", 176, 144, OMX_COLOR_FormatYCbYCr},
  {"/data/paris_cif_420p.yuv", 352, 288, OMX_COLOR_FormatYUV420Planar},
  {"/data/car_cif_420sp.yuv", 352, 288, OMX_COLOR_FormatYUV420SemiPlanar}
};

class kdCondSignal {
public:
  kdCondSignal() {
    mLock = kdThreadMutexCreate(KD_NULL);
    mCond = kdThreadCondCreate(KD_NULL);
    mSignled = KD_FALSE;
  }

  ~kdCondSignal() {
    kdThreadMutexFree(mLock);
    kdThreadCondFree(mCond);
  }

  KDint waitSignal() {
    kdThreadMutexLock(mLock);
    if (mSignled) {
      return kdThreadMutexUnlock(mLock);
    } else {
      kdThreadCondWait(mCond, mLock);
      return kdThreadMutexUnlock(mLock);
    }
  }

  KDint setSignal() {
    kdThreadMutexLock(mLock);
    kdThreadCondSignal(mCond);
    mSignled = KD_TRUE;
    return kdThreadMutexUnlock(mLock);
  }

  KDint resetSignal() {
    kdThreadMutexLock(mLock);
    mSignled = KD_FALSE;
    return kdThreadMutexUnlock(mLock);
  }

private:
  KDThreadMutex *mLock;
  KDThreadCond *mCond;
  KDboolean mSignled;
};

class OmxEncClient {
public:
  OMX_HANDLETYPE mCompHandle;
  OMX_STRING mCompName;
  OMX_CALLBACKTYPE encTestCallbacks;
  OMX_BUFFERHEADERTYPE ***mBufHdr[4];
  vector<OMX_BUFFERHEADERTYPE *> mInputBuffers;
  vector<OMX_BUFFERHEADERTYPE *> mOutputBuffers;
  KDThreadMutex *mInBufferLock;
  KDThreadMutex *mOutBufferLock;
  int mInputFrameNum;
  int mOutputFrameNum;
  OMX_BOOL mEOS;
  OMX_BOOL mOutputEOS;
  kdCondSignal *mStateSignal;
  kdCondSignal *mEosSignal;
  FILE *mFd;
  OMX_U32 mFrameWidth;
  OMX_U32 mFrameHeight;
  OMX_COLOR_FORMATTYPE mColorFormat;
  char *mStreamName;

  OmxEncClient() {
    encTestCallbacks.EventHandler = encTestEventHandler;
    encTestCallbacks.EmptyBufferDone = encTestEmptyBufferDone;
    encTestCallbacks.FillBufferDone = encTestFillBufferDone;
    mFd = NULL;
    mStateSignal = new kdCondSignal();
    mEosSignal = new kdCondSignal();
    mInBufferLock = kdThreadMutexCreate(KD_NULL);
    mOutBufferLock = kdThreadMutexCreate(KD_NULL);
  }

  ~OmxEncClient() {
    cleanUp();
    closeStream();
    delete mStateSignal;
    delete mEosSignal;
    if (mInBufferLock) {
      kdThreadMutexFree(mInBufferLock);
    }
    if (mOutBufferLock) {
      kdThreadMutexFree(mOutBufferLock);
    }
  }

  static OMX_ERRORTYPE encTestEventHandler(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_EVENTTYPE eEvent, OMX_U32 Data1, OMX_U32 Data2,
      OMX_PTR pEventData) {
    OMX_LOGD("%s: pAppData %p, event %x data %lX-%lX", __func__, pAppData,
        eEvent, Data1, Data2);
    OmxEncClient *ctx = reinterpret_cast<OmxEncClient *>(pAppData);
    switch (eEvent) {
      case OMX_EventCmdComplete:
        if (Data1 == OMX_CommandStateSet) {
          ctx->mStateSignal->setSignal();
          OMX_LOGI("Complete change to state %lX", Data2);
        }
        break;
      default:
        break;
    }
    return OMX_ErrorNone;
  }

  static OMX_ERRORTYPE encTestEmptyBufferDone(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* bufHdr) {
    OMX_LOGD("%s: buffer %p", __func__, bufHdr);
    OmxEncClient *ctx = reinterpret_cast<OmxEncClient *>(pAppData);
    kdThreadMutexLock(ctx->mInBufferLock);
    bufHdr->nFlags = 0;
    ctx->mInputBuffers.push_back(bufHdr);
    kdThreadMutexUnlock(ctx->mInBufferLock);
    if (ctx->mEOS == OMX_FALSE) {
      ctx->pushOneInputBuffer();
    }
    return OMX_ErrorNone;
  }

  static OMX_ERRORTYPE encTestFillBufferDone(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* bufHdr) {
    OmxEncClient *ctx = reinterpret_cast<OmxEncClient *>(pAppData);
    OMX_LOGI("[%s]: Frame %d, buffer %p, length %lu", __func__,
        ctx->mOutputFrameNum, bufHdr, bufHdr->nFilledLen);
    dump_bytes(bufHdr->pBuffer, bufHdr->nFilledLen);
    ctx->mOutputFrameNum++;
    OMX_BOOL isEOS =
        (bufHdr->nFlags & OMX_BUFFERFLAG_EOS) ? OMX_TRUE : OMX_FALSE;
    bufHdr->nFilledLen = 0;
    bufHdr->nFlags = 0;
    if (isEOS && ctx->mOutputEOS == OMX_FALSE) {
      OMX_LOGI("Receive EOS on output port");
      ctx->mEosSignal->setSignal();
      ctx->mOutputEOS = OMX_TRUE;
    }
    if (!ctx->mOutputEOS) {
      kdThreadMutexLock(ctx->mOutBufferLock);
      ctx->mOutputBuffers.push_back(bufHdr);
      kdThreadMutexUnlock(ctx->mOutBufferLock);
      ctx->pushOutputBuffer();
    }
    return OMX_ErrorNone;
  }

  void allocateAllPortsBuffer(OMX_HANDLETYPE compHandle,
      OMX_PORTDOMAINTYPE domain) {
    OMX_ERRORTYPE err;
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
        OMX_LOGE("Incoreect domain %x", domain);
    }
    err = OMX_GetParameter(compHandle, param_index, &oParam);
    ASSERT_EQ(err, OMX_ErrorNone);
    if (oParam.nPorts > 0) {
      mBufHdr[domain] = new OMX_BUFFERHEADERTYPE **[oParam.nPorts];
    }
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);
    for (OMX_U32 i = oParam.nStartPortNumber, port = 0; i < oParam.nPorts;
        i++, port++) {
      def.nPortIndex = i;
      err = OMX_GetParameter(
          compHandle, OMX_IndexParamPortDefinition, &def);
      ASSERT_EQ(err, OMX_ErrorNone);
      mBufHdr[domain][port] =
          new OMX_BUFFERHEADERTYPE *[def.nBufferCountActual];
      for (OMX_U32 j = 0; j < def.nBufferCountActual; j++) {
        err = OMX_AllocateBuffer(compHandle, &mBufHdr[domain][port][j], i, NULL,
            def.nBufferSize);
        ASSERT_EQ(err, OMX_ErrorNone);
      }
    }
  }

  void getPortParam(OMX_HANDLETYPE compHandle, OMX_PORTDOMAINTYPE domain,
      OMX_PORT_PARAM_TYPE *param) {
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
        OMX_LOGE("Incoreect domain %x", domain);
        break;
    }
    ASSERT_NE(param_index, OMX_IndexComponentStartUnused);
    OMX_ERRORTYPE err = OMX_GetParameter(compHandle, param_index, param);
    ASSERT_EQ(err, OMX_ErrorNone);
  }

  void freeAllPortsBuffer(OMX_HANDLETYPE compHandle,
      OMX_PORTDOMAINTYPE domain) {
    OMX_ERRORTYPE err;
    OMX_PORT_PARAM_TYPE oParam;
    InitOmxHeader(&oParam);
    getPortParam(compHandle, domain, &oParam);
    if (oParam.nPorts <= 0) {
      return;
    }
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);
    for (OMX_U32 i = oParam.nStartPortNumber, port = 0; i < oParam.nPorts;
        i++, port++) {
      def.nPortIndex = i;
      err = OMX_GetParameter(compHandle, OMX_IndexParamPortDefinition, &def);
      ASSERT_EQ(err, OMX_ErrorNone);
      for (OMX_U32 j = 0; j < def.nBufferCountActual; j++) {
        err = OMX_FreeBuffer(compHandle, i, mBufHdr[domain][port][j]);
        ASSERT_EQ(err, OMX_ErrorNone);
      }
      delete[] mBufHdr[domain][port];
    }
    delete[] mBufHdr[domain];
  }

  void queueAllPortsBuffer(OMX_PORTDOMAINTYPE domain) {
    OMX_ERRORTYPE err;
    OMX_PORT_PARAM_TYPE oParam;
    InitOmxHeader(&oParam);
    getPortParam(mCompHandle, domain, &oParam);
    if (oParam.nPorts <= 0) {
      return;
    }
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);
    for (OMX_U32 i = oParam.nStartPortNumber, port = 0; i < oParam.nPorts;
        i++, port++) {
      def.nPortIndex = i;
      err = OMX_GetParameter(mCompHandle, OMX_IndexParamPortDefinition, &def);
      ASSERT_EQ(err, OMX_ErrorNone);
      for (OMX_U32 j = 0; j < def.nBufferCountActual; j++) {
        if (def.eDir == OMX_DirInput) {
          mInputBuffers.push_back(mBufHdr[domain][port][j]);
        } else {
          mOutputBuffers.push_back(mBufHdr[domain][port][j]);
        }
        ASSERT_EQ(err, OMX_ErrorNone);
      }
    }
  }
  void openStream() {
    mFd = fopen(mStreamName, "r");
  }

  void closeStream() {
    if (NULL != mFd) {
      fclose(mFd);
      mFd = NULL;
    }
  }

  void configInputPort() {
    OMX_ERRORTYPE ret;
    OMX_PORT_PARAM_TYPE oParam;
    InitOmxHeader(&oParam);
    ret = OMX_GetParameter(mCompHandle, OMX_IndexParamVideoInit, &oParam);
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);
    for (OMX_U32 i = oParam.nStartPortNumber; i < oParam.nPorts; i++) {
      def.nPortIndex = i;
      ret = OMX_GetParameter(mCompHandle, OMX_IndexParamPortDefinition, &def);
      ASSERT_EQ(ret, OMX_ErrorNone);
      if (def.eDir == OMX_DirInput) {
        OMX_LOGD("%d x %d, color 0x%x", mFrameWidth, mFrameHeight, mColorFormat);
        def.format.video.nFrameWidth = mFrameWidth;
        def.format.video.nFrameHeight = mFrameHeight;
        def.format.video.nStride = mFrameWidth;
        def.format.video.nSliceHeight = mFrameHeight;
        def.format.video.eColorFormat = mColorFormat;
        def.nBufferSize = getFrameSize(mFrameWidth, mFrameHeight, mColorFormat);
        ret = OMX_SetParameter(mCompHandle, OMX_IndexParamPortDefinition, &def);
        ASSERT_EQ(ret, OMX_ErrorNone);
      }
    }
  }

  OMX_BOOL feedInputData() {
    OMX_ERRORTYPE ret;
    size_t result;
    if (mEOS == OMX_FALSE ) {
      vector<OMX_BUFFERHEADERTYPE *>::iterator it = mInputBuffers.begin();
      OMX_BUFFERHEADERTYPE *buf = *it;
      result = fread(buf->pBuffer, 1, buf->nAllocLen, mFd);
      if (result <= 0) {
        buf->nFlags = OMX_BUFFERFLAG_EOS;
        buf->nFilledLen = 0;
        mEOS = OMX_TRUE;
        OMX_LOGI("Input port got EOS");
      } else {
        OMX_LOGD("Frame %d, size %d", mInputFrameNum, result);
        buf->nFilledLen = result;
        // buf->nTimeStamp = (pkt.pts == AV_NOPTS_VALUE) ? pkt.dts : pkt.pts;
      }
      ret = OMX_EmptyThisBuffer(mCompHandle, buf);
      mInputFrameNum++;
      mInputBuffers.erase(it);
    }
    return OMX_TRUE;
  }

  OMX_ERRORTYPE pushOneInputBuffer() {
    kdThreadMutexLock(mInBufferLock);
    if (mEOS == OMX_FALSE) {
      while (!mInputBuffers.empty() && !feedInputData()) {
      }
    }
    kdThreadMutexUnlock(mInBufferLock);
    return OMX_ErrorNone;
  }

  OMX_ERRORTYPE pushInputBuffer() {
    kdThreadMutexLock(mInBufferLock);
    if (mEOS == OMX_FALSE) {
      while (!mInputBuffers.empty()) {
        feedInputData();
      }
    }
    kdThreadMutexUnlock(mInBufferLock);
    return OMX_ErrorNone;
  }

  OMX_ERRORTYPE pushOneOutputBuffer() {
    kdThreadMutexLock(mOutBufferLock);
    OMX_ERRORTYPE err;
    vector<OMX_BUFFERHEADERTYPE *>::iterator it;
    if (!mOutputBuffers.empty()) {
      it = mOutputBuffers.begin();
      err = OMX_FillThisBuffer(mCompHandle, *it);
      mOutputBuffers.erase(it);
    }
    kdThreadMutexUnlock(mOutBufferLock);
    return OMX_ErrorNone;
  }

  OMX_ERRORTYPE pushOutputBuffer() {
    OMX_ERRORTYPE err = OMX_ErrorNone;
    kdThreadMutexLock(mOutBufferLock);
    vector<OMX_BUFFERHEADERTYPE *>::iterator it;
    while (!mOutputBuffers.empty()) {
      it = mOutputBuffers.begin();
      err = OMX_FillThisBuffer(mCompHandle, *it);
      mOutputBuffers.erase(it);
    }
    kdThreadMutexUnlock(mOutBufferLock);
    return err;
  }

  OMX_ERRORTYPE renderOutputData() {
    OMX_ERRORTYPE err = OMX_ErrorNone;
    return err;
  }

  void testLoadedToIdle() {
    OMX_ERRORTYPE err;
    OMX_STATETYPE state = OMX_StateInvalid;
    err = OMX_GetState(mCompHandle, &state);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_EQ(state, OMX_StateLoaded);
    mStateSignal->resetSignal();
    err = OMX_SendCommand(mCompHandle, OMX_CommandStateSet, OMX_StateIdle,
        NULL);
    ASSERT_EQ(err, OMX_ErrorNone);
    allocateAllPortsBuffer(mCompHandle, OMX_PortDomainAudio);
    allocateAllPortsBuffer(mCompHandle, OMX_PortDomainVideo);
    allocateAllPortsBuffer(mCompHandle, OMX_PortDomainImage);
    allocateAllPortsBuffer(mCompHandle, OMX_PortDomainOther);

    // Waiting for command complete
    OMX_LOGD("Waiting for state change to idle");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to idle");
  }

  void testIdleToExec() {
    OMX_ERRORTYPE err;
    OMX_STATETYPE state = OMX_StateInvalid;
    err = OMX_GetState(mCompHandle, &state);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_EQ(state, OMX_StateIdle);
    mStateSignal->resetSignal();
    err = OMX_SendCommand(mCompHandle, OMX_CommandStateSet, OMX_StateExecuting,
        NULL);
    ASSERT_EQ(err, OMX_ErrorNone);

    // Waiting for command complete
    OMX_LOGD("Waiting for change state to Executing");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to Executing");
  }

  void testExecToIdle() {
    OMX_ERRORTYPE err;
    OMX_STATETYPE state = OMX_StateInvalid;
    err = OMX_GetState(mCompHandle, &state);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_EQ(state, OMX_StateExecuting);
    mStateSignal->resetSignal();
    err = OMX_SendCommand(mCompHandle, OMX_CommandStateSet, OMX_StateIdle,
        NULL);
    ASSERT_EQ(err, OMX_ErrorNone);

    // Waiting for command complete
    OMX_LOGD("Waiting for state change to OMX_StateIdle");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to OMX_StateIdle");
  }

  void testIdleToLoaded() {
    OMX_ERRORTYPE err;
    OMX_STATETYPE state = OMX_StateInvalid;
    err = OMX_GetState(mCompHandle, &state);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_EQ(state, OMX_StateIdle);
    mStateSignal->resetSignal();
    err = OMX_SendCommand(mCompHandle, OMX_CommandStateSet, OMX_StateLoaded,
        NULL);
    ASSERT_EQ(err, OMX_ErrorNone);
    freeAllPortsBuffer(mCompHandle, OMX_PortDomainAudio);
    freeAllPortsBuffer(mCompHandle, OMX_PortDomainVideo);
    freeAllPortsBuffer(mCompHandle, OMX_PortDomainImage);
    freeAllPortsBuffer(mCompHandle, OMX_PortDomainOther);

    // Waiting for command complete
    OMX_LOGD("Waiting for change state to Loaded");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to Loaded");
  }

  void prepare() {
    OMX_ERRORTYPE err;
    // Prepare
    err = OMX_GetHandle(&mCompHandle, mCompName, this, &encTestCallbacks);
    ASSERT_EQ(err, OMX_ErrorNone);
    mEOS = OMX_FALSE;
    mOutputEOS = OMX_FALSE;
    mInputFrameNum = 0;
    mOutputFrameNum = 0;
    configInputPort();
  }

  void cleanUp() {
    OMX_ERRORTYPE err;
    if (NULL != mCompHandle) {
      err = OMX_FreeHandle(mCompHandle);
      ASSERT_EQ(err, OMX_ErrorNone);
      mCompHandle = NULL;
    }
  }
};
// class OmxEncClient
class OmxEncTest : public testing::Test {
public:
  OmxEncClient *mClient;
  OMX_STRING mStreamName;

  virtual void SetUp() {
    OMX_ERRORTYPE err = OMX_Init();
    ASSERT_EQ(OMX_ErrorNone, err);
    mClient = new OmxEncClient;
  }

  virtual void TearDown() {
    delete mClient;
    OMX_ERRORTYPE err = OMX_Deinit();
    ASSERT_EQ(OMX_ErrorNone, err);
  }

  void testEncode(OMX_IN OMX_STRING comp, OMX_S32 vect_idx) {
    OMX_ERRORTYPE err;
    mClient->mCompName = comp;
    mClient->mStreamName = gTestVector[vect_idx].streamName;
    mClient->mFrameWidth = gTestVector[vect_idx].width;
    mClient->mFrameHeight = gTestVector[vect_idx].height;
    mClient->mColorFormat = gTestVector[vect_idx].color;
    mClient->openStream();
    mClient->prepare();
    mClient->configInputPort();
    // Loded -> Idle
    mClient->testLoadedToIdle();
    // Idle -> Excuting
    mClient->testIdleToExec();
    // Push buffer
    mClient->queueAllPortsBuffer(OMX_PortDomainVideo);
    mClient->pushInputBuffer();
    mClient->pushOutputBuffer();
    // Wait EOS
    mClient->mEosSignal->waitSignal();
    mClient->testExecToIdle();
    // Idle -> Loaded
    mClient->testIdleToLoaded();
    // Clean up
    mClient->cleanUp();
    mClient->closeStream();
  }

  void testStateTransition(OMX_IN OMX_STRING comp, OMX_S32 vect_idx) {
    OMX_LOGD("Enter %s", __func__);
    mClient->mCompName = comp;
    mClient->mStreamName = gTestVector[vect_idx].streamName;
    mClient->mFrameWidth = gTestVector[vect_idx].width;
    mClient->mFrameHeight = gTestVector[vect_idx].height;
    mClient->mColorFormat = gTestVector[vect_idx].color;
    mClient->openStream();
    mClient->prepare();
    // Loded -> Idle
    mClient->testLoadedToIdle();
    // Idle -> Loaded
    mClient->testIdleToLoaded();
    // Free handle
    mClient->cleanUp();
    // Second run
    mClient->prepare();
    // Loded -> Idle
    mClient->testLoadedToIdle();
    // Idle -> Loaded
    mClient->testIdleToLoaded();
    mClient->cleanUp();
    mClient->closeStream();
    OMX_LOGD("Exit %s", __func__);
  }

};
// class OmxEncTest

class OmxEncMultiInstanceTest : public testing::Test {
public:
  friend class OmxEncClient;
  vector<OmxEncClient *> mClientVector;
  OMX_U32 mInstanceNum;

  virtual void SetUp() {
    OMX_ERRORTYPE err = OMX_Init();
    ASSERT_EQ(OMX_ErrorNone, err);
    mInstanceNum = 2;
  }

  virtual void TearDown() {
    OMX_ERRORTYPE err = OMX_Deinit();
    ASSERT_EQ(OMX_ErrorNone, err);
  }

  void toIdleTest() {
    for (OMX_U32 i = 0; i < mInstanceNum; i++) {
      OmxEncClient *test = new OmxEncClient;
      mClientVector.push_back(test);
      test->mStreamName = gTestVector[mInstanceNum].streamName;
      test->mFrameWidth = gTestVector[mInstanceNum].width;
      test->mFrameHeight = gTestVector[mInstanceNum].height;
      test->mColorFormat = gTestVector[mInstanceNum].color;
      test->mCompName = "OMX.Marvell.video_encoder.avc";
      test->openStream();
      test->prepare();
      test->testLoadedToIdle();
    }
    vector<OmxEncClient *>::iterator it;
    while (!mClientVector.empty()) {
      it = mClientVector.begin();
      (*it)->testIdleToLoaded();
      (*it)->cleanUp();
      (*it)->closeStream();
      delete (*it);
      mClientVector.erase(it);
    }
  }

  void toExcuteTest() {
    for (OMX_U32 i = 0; i < mInstanceNum; i++) {
      OmxEncClient *test = new OmxEncClient;
      mClientVector.push_back(test);
      test->mStreamName = gTestVector[mInstanceNum].streamName;
      test->mFrameWidth = gTestVector[mInstanceNum].width;
      test->mFrameHeight = gTestVector[mInstanceNum].height;
      test->mColorFormat = gTestVector[mInstanceNum].color;
      test->mCompName = "OMX.Marvell.video_encoder.avc";
      test->openStream();
      test->prepare();
      test->configInputPort();
      test->testLoadedToIdle();
      test->testIdleToExec();
    }
    vector<OmxEncClient *>::iterator it;
    while (!mClientVector.empty()) {
      it = mClientVector.begin();
      (*it)->testExecToIdle();
      (*it)->testIdleToLoaded();
      (*it)->cleanUp();
      (*it)->closeStream();
      delete (*it);
      mClientVector.erase(it);
    }
  }

  void testEncode(OMX_IN OMX_STRING comp) {
    for (OMX_U32 i = 0; i < mInstanceNum; i++) {
      OmxEncClient *test = new OmxEncClient;
      mClientVector.push_back(test);
      test->mStreamName = gTestVector[mInstanceNum].streamName;
      test->mFrameWidth = gTestVector[mInstanceNum].width;
      test->mFrameHeight = gTestVector[mInstanceNum].height;
      test->mColorFormat = gTestVector[mInstanceNum].color;
      test->mCompName = "OMX.Marvell.video_encoder.avc";
      test->openStream();
      test->prepare();
      test->configInputPort();
      test->testLoadedToIdle();
      test->testIdleToExec();
      test->queueAllPortsBuffer(OMX_PortDomainVideo);
      test->pushOutputBuffer();
      test->pushInputBuffer();
    }
    vector<OmxEncClient *>::iterator it;
    while (!mClientVector.empty()) {
      it = mClientVector.begin();
      (*it)->mEosSignal->waitSignal();
      (*it)->testExecToIdle();
      (*it)->testIdleToLoaded();
      (*it)->cleanUp();
      (*it)->closeStream();
      delete (*it);
      mClientVector.erase(it);
    }
  }
};
// class OmxEncMultiInstanceTest
#if 0
TEST_F(OmxEncMultiInstanceTest, multiToIdleTest) {
  toIdleTest();
}

// TODO: Enable multi enc test when amp side is ready
/*
 TEST_F(OmxEncMultiInstanceTest, mulitEncodeTest) {
 char *compName = "OMX.Marvell.video_encoder.avc";
 testEncode(compName, 1);
 }
 */
#endif

TEST_F(OmxEncTest, stateTest) {
  char *compName = "OMX.Marvell.video_encoder.avc";
  mStreamName = "/data/test.yuv";
  testStateTransition(compName , 1);
}

TEST_F(OmxEncTest,avcEncTest) {
  char *compName = "OMX.Marvell.video_encoder.avc";
  testEncode(compName, 0);
}

TEST_F(OmxEncTest, mpeg4EncTest) {
  char *compName = "OMX.Marvell.video_encoder.mpeg4";
  testEncode(compName, 1);
}

TEST_F(OmxEncTest, vp8EncTest) {
  char *compName = "OMX.Marvell.video_encoder.vp8";
  testEncode(compName, 0);
}

}  // namespace test
}  // namespace berlin
