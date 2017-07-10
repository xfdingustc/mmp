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
#define LOG_TAG "OmxAudioRendererTest"

extern "C" {
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <libavformat/avformat.h>
}
#include <BerlinOmxCommon.h>
#include <gtest/gtest.h>

#include <vector>

#undef OMX_LOGV
#undef OMX_LOGD
#undef OMX_LOGI
#undef OMX_LOGW
#undef OMX_LOGE

#define OMX_LOGV(fmt, ...) \
  ALOGV("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OMX_LOGD(fmt, ...) \
  ALOGD("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OMX_LOGI(fmt, ...) \
  ALOGI("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OMX_LOGW(fmt, ...) \
  ALOGW("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OMX_LOGE(fmt, ...) \
  ALOGE("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

using namespace std;

static void dump_bytes(OMX_U8 *bits, size_t size) {
  static int first = 1;
  const char *mode = "a";
  if (first) {
    first = 0;
    mode = "w";
  }
  FILE *file = fopen("/data/omx/video_es.dat", mode);
  fwrite(bits, 1, size, file);
  fclose(file);
}

namespace berlin {
namespace test {

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

class OmxARenClient {
public:
  OMX_HANDLETYPE mCompHandle;
  OMX_STRING mCompName;
  OMX_CALLBACKTYPE decTestCallbacks;
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

  AVFormatContext *mAvFormatCtx;
  int mStreamIndex;
  char *mStreamName;
  int mSampleRate;
  int mChannels;
  enum AVSampleFormat mSampleFmt;

  OmxARenClient() {
    decTestCallbacks.EventHandler = decTestEventHandler;
    decTestCallbacks.EmptyBufferDone = decTestEmptyBufferDone;
    decTestCallbacks.FillBufferDone = decTestFillBufferDone;

    mAvFormatCtx = NULL;
    mStreamName = NULL;
    mSampleRate = 0;
    mChannels = 0;
    mSampleFmt = AV_SAMPLE_FMT_NONE;

    mStateSignal = new kdCondSignal();
    mEosSignal = new kdCondSignal();
    mInBufferLock = kdThreadMutexCreate(KD_NULL);
    mOutBufferLock = kdThreadMutexCreate(KD_NULL);
  }

  ~OmxARenClient() {
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

  static OMX_ERRORTYPE decTestEventHandler(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_EVENTTYPE eEvent, OMX_U32 Data1, OMX_U32 Data2,
      OMX_PTR pEventData) {
    OMX_LOGD("pAppData %p, event %x data %lX-%lX", pAppData,
        eEvent, Data1, Data2);
    OmxARenClient *ctx = reinterpret_cast<OmxARenClient *>(pAppData);
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

  static OMX_ERRORTYPE decTestEmptyBufferDone(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer) {
    OMX_LOGD("buffer %p", pBuffer);
    OmxARenClient *ctx = reinterpret_cast<OmxARenClient *>(pAppData);
    kdThreadMutexLock(ctx->mInBufferLock);
    OMX_BOOL isEOS =
        (pBuffer->nFlags & OMX_BUFFERFLAG_EOS) ? OMX_TRUE : OMX_FALSE;
    if (isEOS && ctx->mOutputEOS == OMX_FALSE) {
      OMX_LOGI("Receive EOS on output port");
      ctx->mEosSignal->setSignal();
      ctx->mOutputEOS = OMX_TRUE;
    }
    pBuffer->nFlags = 0;
    ctx->mInputBuffers.push_back(pBuffer);
    kdThreadMutexUnlock(ctx->mInBufferLock);
    if (ctx->mEOS == OMX_FALSE) {
      ctx->pushOneInputBuffer();
    }
    return OMX_ErrorNone;
  }

  static OMX_ERRORTYPE decTestFillBufferDone(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer) {
    OmxARenClient *ctx = reinterpret_cast<OmxARenClient *>(pAppData);
    OMX_LOGI("Frame %d, buffer %p, length %lu",
        ctx->mOutputFrameNum, pBuffer, pBuffer->nFilledLen);
    ctx->renderOutputData();
    ctx->mOutputFrameNum++;
    OMX_BOOL isEOS =
        (pBuffer->nFlags & OMX_BUFFERFLAG_EOS) ? OMX_TRUE : OMX_FALSE;
    pBuffer->nFilledLen = 0;
    pBuffer->nFlags = 0;
    if (isEOS && ctx->mOutputEOS == OMX_FALSE) {
      OMX_LOGI("Receive EOS on output port");
      ctx->mEosSignal->setSignal();
      ctx->mOutputEOS = OMX_TRUE;
    }
    if (!ctx->mOutputEOS) {
      kdThreadMutexLock(ctx->mOutBufferLock);
      ctx->mOutputBuffers.push_back(pBuffer);
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
    OMX_LOGI("%s() line %d, oParam.nPorts = %d, oParam.nStartPortNumber = %d",
        __FUNCTION__, __LINE__, oParam.nPorts, oParam.nStartPortNumber);
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
    // register all file formats
    av_register_all();
    int err = avformat_open_input(&mAvFormatCtx, mStreamName, NULL, NULL);
    ASSERT_GE(err, 0);
    err = avformat_find_stream_info(mAvFormatCtx, NULL);
    ASSERT_GE(err, 0);
    // select the video stream
    int idx = av_find_best_stream(mAvFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1,
        NULL, 0);
    // stream index should be >=0
    ASSERT_GE(idx, 0);
    mStreamIndex = idx;
    OMX_LOGD("Using stream %d", idx);

    AVCodecContext *codec_ctx = mAvFormatCtx->streams[mStreamIndex]->codec;
    mSampleRate = codec_ctx->sample_rate;
    mChannels = codec_ctx->channels;
    mSampleFmt = codec_ctx->sample_fmt;
    OMX_LOGD("codec_ctx->sample_rate = %d, codec_ctx->channels = %d, codec_ctx->sample_fmt = %d",
        mSampleRate, mChannels, mSampleFmt);
  }

  void closeStream() {
    if (NULL != mAvFormatCtx) {
      avformat_close_input(&mAvFormatCtx);
      mAvFormatCtx = NULL;
    }
  }

  void pushCodecConfigData() {
    OMX_ERRORTYPE ret;
    AVCodecContext *codec_ctx = mAvFormatCtx->streams[mStreamIndex]->codec;
    if (codec_ctx->extradata_size > 0) {
      vector<OMX_BUFFERHEADERTYPE *>::iterator it = mInputBuffers.begin();
      OMX_BUFFERHEADERTYPE *buf = *it;
      AVCodecContext *codec_ctx = mAvFormatCtx->streams[mStreamIndex]->codec;
      kdMemcpy(buf->pBuffer, codec_ctx->extradata, codec_ctx->extradata_size);
      buf->nFilledLen = codec_ctx->extradata_size;
      buf->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
      //dump_bytes(codec_ctx->extradata, codec_ctx->extradata_size);
      ret = OMX_EmptyThisBuffer(mCompHandle, buf);
      mInputBuffers.erase(it);
    }
  }

  void configPort() {
    OMX_LOGD("ENTER");
    OMX_ERRORTYPE ret;
    OMX_PORT_PARAM_TYPE oParam;

    InitOmxHeader(&oParam);
    ret = OMX_GetParameter(mCompHandle, OMX_IndexParamAudioInit, &oParam);
    OMX_LOGD("oParam.nStartPortNumber = %d, oParam.nPorts = %d",
        oParam.nStartPortNumber, oParam.nPorts);

    for (int i = oParam.nStartPortNumber; i < oParam.nPorts; i++) {
      OMX_LOGD("pcm config for port %d", i);
      OMX_AUDIO_PARAM_PCMMODETYPE profile;
      InitOmxHeader(&profile);

      profile.nPortIndex = i;
      ret = OMX_GetParameter(
          mCompHandle, OMX_IndexParamAudioPcm, &profile);
      ASSERT_EQ(ret, OMX_ErrorNone);

      profile.nSamplingRate = mSampleRate;
      profile.nChannels = mChannels;
      ret = OMX_SetParameter(
          mCompHandle, OMX_IndexParamAudioPcm, &profile);
      ASSERT_EQ(ret, OMX_ErrorNone);

      // Get param again to check whether set correctly.
      profile.nSamplingRate = 0;
      profile.nChannels = 0;
      ret = OMX_GetParameter(
          mCompHandle, OMX_IndexParamAudioPcm, &profile);
      ASSERT_EQ(ret, OMX_ErrorNone);
      OMX_LOGD("Got from comp: profile.nSamplingRate = %d, profile.nChannels = %d",
          profile.nSamplingRate, profile.nChannels);
      ASSERT_EQ(profile.nSamplingRate, mSampleRate);
      ASSERT_EQ(profile.nChannels, mChannels);
    }

    OMX_LOGD("EXIT");
    return;
  }

  void setVolume() {
    OMX_LOGD("ENTER");
    OMX_ERRORTYPE ret;
    OMX_PORT_PARAM_TYPE oParam;

    InitOmxHeader(&oParam);
    ret = OMX_GetParameter(mCompHandle, OMX_IndexParamAudioInit, &oParam);
    OMX_LOGD("oParam.nStartPortNumber = %d, oParam.nPorts = %d",
        oParam.nStartPortNumber, oParam.nPorts);

    for (int i = oParam.nStartPortNumber; i < oParam.nPorts; i++) {
      OMX_LOGD("volume config for port %d", i);
      OMX_AUDIO_CONFIG_VOLUMETYPE volume_conf;
      InitOmxHeader(&volume_conf);

      volume_conf.nPortIndex = i;
      ret = OMX_GetConfig(
          mCompHandle, OMX_IndexConfigAudioVolume, &volume_conf);
      ASSERT_EQ(ret, OMX_ErrorNone);

      volume_conf.sVolume.nValue = 50;
      volume_conf.sVolume.nMax = 100;
      volume_conf.sVolume.nMin = 0;
      ret = OMX_SetConfig(
          mCompHandle, OMX_IndexConfigAudioVolume, &volume_conf);
      ASSERT_EQ(ret, OMX_ErrorNone);

      // Get volume again to check whether set correctly.
      volume_conf.sVolume.nValue = 0;
      volume_conf.sVolume.nMax = 0;
      volume_conf.sVolume.nMin = 100;
      ret = OMX_GetConfig(
          mCompHandle, OMX_IndexConfigAudioVolume, &volume_conf);
      ASSERT_EQ(ret, OMX_ErrorNone);
      OMX_LOGD("volume_conf.sVolume.nValue = %d", volume_conf.sVolume.nValue);
      ASSERT_EQ(volume_conf.sVolume.nValue, 50);
      ASSERT_EQ(volume_conf.sVolume.nMax, 100);
      ASSERT_EQ(volume_conf.sVolume.nMin, 0);
    }

    OMX_LOGD("EXIT");
    return;
  }

  OMX_BOOL feedInputData() {
    OMX_ERRORTYPE ret;
    OMX_BOOL feeded = OMX_FALSE;
    int err = 0;
    AVPacket pkt;
    av_init_packet(&pkt);
    err = av_read_frame(mAvFormatCtx, &pkt);
    if (pkt.stream_index == mStreamIndex || (mEOS == OMX_FALSE && err < 0)) {
      vector<OMX_BUFFERHEADERTYPE *>::iterator it = mInputBuffers.begin();
      OMX_BUFFERHEADERTYPE *buf = *it;
      if (err < 0) {
        buf->nFlags = OMX_BUFFERFLAG_EOS;
        buf->nFilledLen = 0;
        mEOS = OMX_TRUE;
        OMX_LOGI("Input port got EOS");
      } else {
        kdMemcpy(buf->pBuffer, pkt.data, pkt.size);
        buf->nFilledLen = pkt.size;
        buf->nTimeStamp = (pkt.pts == AV_NOPTS_VALUE) ? pkt.dts : pkt.pts;
        OMX_LOGD("Push frame %d, pts %lld, size %lu", mInputFrameNum,
            buf->nTimeStamp, buf->nFilledLen);
      }
      //dump_bytes( pkt.data, pkt.size);
      ret = OMX_EmptyThisBuffer(mCompHandle, buf);
      mInputFrameNum++;
      mInputBuffers.erase(it);
      feeded = OMX_TRUE;
    }
    av_free_packet(&pkt);
    return feeded;
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
    OMX_ERRORTYPE err;
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

    // Waiting for command complete
    OMX_LOGD("Waiting for change state to Loaded");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to Loaded");
  }

  void prepare() {
    OMX_ERRORTYPE err;
    // Prepare
    err = OMX_GetHandle(&mCompHandle, mCompName, this, &decTestCallbacks);
    ASSERT_EQ(err, OMX_ErrorNone);
    mEOS = OMX_FALSE;
    mOutputEOS = OMX_FALSE;
    mInputFrameNum = 0;
    mOutputFrameNum = 0;
    //configOutputPort();
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
// class OmxARenClient


class OmxARenTest : public testing::Test {
public:
  OmxARenClient *mClient;
  OMX_STRING mStreamName;

  virtual void SetUp() {
    OMX_ERRORTYPE err = OMX_Init();
    ASSERT_EQ(OMX_ErrorNone, err);
    mClient = new OmxARenClient;
  }

  virtual void TearDown() {
    delete mClient;
    OMX_ERRORTYPE err = OMX_Deinit();
    ASSERT_EQ(OMX_ErrorNone, err);
  }

  void testPlay(OMX_IN OMX_STRING comp) {
    OMX_LOGE("ENTER");
    OMX_ERRORTYPE err;

    // Open stream with ffmpeg.
    mClient->mCompName = comp;
    mClient->mStreamName = mStreamName;
    mClient->openStream();

    // Create component.
    mClient->prepare();

    //Set parameters such as sample rate.
    mClient->configPort();
    mClient->setVolume();

    // Loded -> Idle
    mClient->testLoadedToIdle();

    // Idle -> Executing
    mClient->testIdleToExec();

    // Push buffer
    mClient->queueAllPortsBuffer(OMX_PortDomainAudio);

    // Begin transfer data.
    mClient->pushInputBuffer();

    // Wait EOS
    mClient->mEosSignal->waitSignal();

    // Executing -> Idle
    mClient->testExecToIdle();

    // Idle -> Loaded
    mClient->testIdleToLoaded();

    // Clean up
    mClient->cleanUp();
    mClient->closeStream();

    OMX_LOGE("EXIT");
  }

  void testStateTransition(OMX_IN OMX_STRING comp) {
    OMX_LOGD("Enter %s", __func__);
    mClient->mCompName = comp;
    mClient->mStreamName = mStreamName;
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
// class OmxARenTest


TEST_F(OmxARenTest, decodeTest) {
  char *compName = "OMX.Marvell.audio_renderer.pcm";
  mStreamName = "/data/WAV_PCM_64.0Kbps_8bit_8000Hz_1ch.wav";
  testPlay(compName);
}

GTEST_API_ int main(int argc, char **argv) {
  std::cout << "Running main() from gtest_main.cc\n";
  OMX_LOGD("Running main() from gtest_main.cc");

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

}  // namespace test
}  // namespace berlin
