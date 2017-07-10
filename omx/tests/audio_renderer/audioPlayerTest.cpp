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

enum {
  kPortIndexInput = 0,
  kPortIndexOutput = 1,
  kPortIndexClock = 0x200
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

#undef  LOG_TAG
#define LOG_TAG "OmxAudDecoderClient"

class OmxAudDecoderClient {
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

  OmxAudDecoderClient() {
    OMX_LOGD("ENTER");
    decTestCallbacks.EventHandler = decTestEventHandler;
    decTestCallbacks.EmptyBufferDone = decTestEmptyBufferDone;
    decTestCallbacks.FillBufferDone = decTestFillBufferDone;
    mAvFormatCtx = NULL;
    mStateSignal = new kdCondSignal();
    mEosSignal = new kdCondSignal();
    mInBufferLock = kdThreadMutexCreate(KD_NULL);
    mOutBufferLock = kdThreadMutexCreate(KD_NULL);
    OMX_LOGD("EXIT");
  }
  ~OmxAudDecoderClient() {
    OMX_LOGD("ENTER");
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
    OMX_LOGD("EXIT");
  }
  static OMX_ERRORTYPE decTestEventHandler(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_EVENTTYPE eEvent, OMX_U32 Data1, OMX_U32 Data2,
      OMX_PTR pEventData) {
    OMX_LOGD("ENTER");
    OMX_LOGD("pAppData %p, event %x data %lX-%lX", pAppData,
        eEvent, Data1, Data2);
    OmxAudDecoderClient *ctx = reinterpret_cast<OmxAudDecoderClient *>(pAppData);
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
    OMX_LOGD("EXIT");
    return OMX_ErrorNone;
  }

  static OMX_ERRORTYPE decTestEmptyBufferDone(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer) {
    OMX_LOGD("ENTER");
    OMX_LOGD("buffer %p", pBuffer);
    OmxAudDecoderClient *ctx = reinterpret_cast<OmxAudDecoderClient *>(pAppData);
    kdThreadMutexLock(ctx->mInBufferLock);
    pBuffer->nFlags = 0;
    ctx->mInputBuffers.push_back(pBuffer);
    kdThreadMutexUnlock(ctx->mInBufferLock);
    if (ctx->mEOS == OMX_FALSE) {
      ctx->pushOneInputBuffer();
    }
    OMX_LOGD("EXIT");
    return OMX_ErrorNone;
  }

  static OMX_ERRORTYPE decTestFillBufferDone(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer) {
    OMX_LOGD("ENTER");
    OmxAudDecoderClient *ctx = reinterpret_cast<OmxAudDecoderClient *>(pAppData);
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
    OMX_LOGD("EXIT");
    return OMX_ErrorNone;
  }

  void allocateAllPortsBuffer(OMX_HANDLETYPE compHandle,
      OMX_PORTDOMAINTYPE domain) {
    OMX_LOGD("ENTER");
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
      if (i != oParam.nStartPortNumber) {
        OMX_LOGD("allocate the input buffers only.");
        continue;
      }
      def.nPortIndex = i;
      err = OMX_GetParameter(
          compHandle, OMX_IndexParamPortDefinition, &def);
      ASSERT_EQ(err, OMX_ErrorNone);
      mBufHdr[domain][port] =
          new OMX_BUFFERHEADERTYPE *[def.nBufferCountActual];
      for (OMX_U32 j = 0; j < def.nBufferCountActual; j++) {
        err = OMX_AllocateBuffer(compHandle, &mBufHdr[domain][port][j], i, NULL,
            def.nBufferSize);
        OMX_LOGD("err = 0x%x, j = %d, def.nBufferCountActual = %d", err, j, def.nBufferCountActual);
        ASSERT_EQ(err, OMX_ErrorNone);
      }
    }
    OMX_LOGD("EXIT");
  }

  void getPortParam(OMX_HANDLETYPE compHandle, OMX_PORTDOMAINTYPE domain,
      OMX_PORT_PARAM_TYPE *param) {
    OMX_LOGD("ENTER");
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
    OMX_LOGD("EXIT");
  }

  void freeAllPortsBuffer(OMX_HANDLETYPE compHandle,
      OMX_PORTDOMAINTYPE domain) {
    OMX_LOGD("ENTER");
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
      if (i != oParam.nStartPortNumber) {
        OMX_LOGD("free the input buffers only.");
        continue;
      }
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
    OMX_LOGD("EXIT");
  }

  void queueAllPortsBuffer(OMX_PORTDOMAINTYPE domain) {
    OMX_LOGD("ENTER");
    OMX_ERRORTYPE err;
    OMX_PORT_PARAM_TYPE oParam;
    InitOmxHeader(&oParam);
    getPortParam(mCompHandle, domain, &oParam);
    if (oParam.nPorts <= 0) {
      OMX_LOGD("EXIT");
      return;
    }
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);
    for (OMX_U32 i = oParam.nStartPortNumber, port = 0; i < oParam.nPorts;
        i++, port++) {
      if (i != oParam.nStartPortNumber) {
        OMX_LOGD("queue the input buffers only.");
        continue;
      }
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
    OMX_LOGD("EXIT");
  }
  void openStream() {
    OMX_LOGD("ENTER");
    // register all file formats
    av_register_all();
    int err = avformat_open_input(&mAvFormatCtx, mStreamName, NULL, NULL);
    ASSERT_GE(err, 0);
    err = avformat_find_stream_info(mAvFormatCtx, NULL);
    ASSERT_GE(err, 0);
    // select the video or audio stream
    int idx  = av_find_best_stream(mAvFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

    // stream index should be >=0
    ASSERT_GE(idx, 0);
    mStreamIndex = idx;
    OMX_LOGD("Using stream %d", idx);
    OMX_LOGD("EXIT");
  }

  void closeStream() {
    OMX_LOGD("ENTER");
    if (NULL != mAvFormatCtx) {
      avformat_close_input(&mAvFormatCtx);
      mAvFormatCtx = NULL;
    }
    OMX_LOGD("EXIT");
  }

  void pushCodecConfigData() {
    OMX_LOGD("ENTER");
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
    OMX_LOGD("EXIT");
  }

  void setPCMFormat(const OMX_U32 inputPortIdx) {
    OMX_LOGD("ENTER");
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    AVCodecContext *codec_ctx = mAvFormatCtx->streams[mStreamIndex]->codec;

    OMX_AUDIO_PARAM_PCMMODETYPE param;
    InitOmxHeader(&param);
    param.nPortIndex = inputPortIdx;
    ret = OMX_GetParameter(mCompHandle, OMX_IndexParamAudioPcm, &param);
    if (OMX_ErrorNone != ret) {
      OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioPcm) with error %d", ret);
      return;
    }

    param.eNumData = OMX_NumericalDataSigned;
    param.eEndian = OMX_EndianLittle;
    param.bInterleaved = OMX_TRUE;
    param.ePCMMode = OMX_AUDIO_PCMModeLinear;
    param.nSamplingRate = codec_ctx->sample_rate;
    param.nChannels = codec_ctx->channels;
    switch (codec_ctx->codec_id) {
      case CODEC_ID_PCM_U8:
        param.nBitPerSample = 8;
        param.eNumData = OMX_NumericalDataUnsigned;
        break;
      case CODEC_ID_PCM_S16LE:
        param.nBitPerSample = 16;
        break;
      case CODEC_ID_PCM_S16BE:
        param.nBitPerSample = 16;
        param.eEndian = OMX_EndianBig;
        break;
      case CODEC_ID_PCM_S24LE:
        param.nBitPerSample = 24;
        break;
        break;
      case CODEC_ID_PCM_S24BE:
        param.nBitPerSample = 24;
        param.eEndian = OMX_EndianBig;
        break;
      default:
        param.nBitPerSample = 16;
    }
    OMX_LOGE("param.nBitPerSample = %d, SamplingRate = %d, Channels = %d",
        param.nBitPerSample, param.nSamplingRate, param.nChannels);

    ret = OMX_SetParameter(mCompHandle, OMX_IndexParamAudioPcm, &param);
    if (OMX_ErrorNone != ret) {
      OMX_LOGE("Failed to OMX_SetParameter(OMX_IndexParamAudioPcm) with error %d", ret);
      return;
    }

    OMX_LOGD("EXIT");
  }

  void configOutputPort() {
    OMX_LOGD("ENTER");
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    AVCodecContext *codec_ctx = mAvFormatCtx->streams[mStreamIndex]->codec;
    OMX_PORT_PARAM_TYPE oParam;
    InitOmxHeader(&oParam);
    if (strstr(mCompName, "video_decoder")) {
      ret = OMX_GetParameter(mCompHandle, OMX_IndexParamVideoInit, &oParam);
    } else if (strstr(mCompName, "audio_decoder")) {
      if (CODEC_ID_AAC == codec_ctx->codec_id) {
        ret = OMX_GetParameter(mCompHandle, OMX_IndexParamAudioInit, &oParam);
        if (strstr(mCompName, "aac")) {
          OMX_LOGD("aac config\n");
          OMX_AUDIO_PARAM_AACPROFILETYPE profile;
          InitOmxHeader(&profile);
          profile.nPortIndex = 0;

          ret = OMX_GetParameter(
              mCompHandle, OMX_IndexParamAudioAac, &profile);
          ASSERT_EQ(ret, OMX_ErrorNone);
          profile.nChannels = codec_ctx->channels;
          profile.nSampleRate = codec_ctx->sample_rate;

          profile.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4FF;

          ret = OMX_SetParameter(
              mCompHandle, OMX_IndexParamAudioAac, &profile);

          ASSERT_EQ(ret, OMX_ErrorNone);
        }
      } else if ((CODEC_ID_PCM_S16LE == codec_ctx->codec_id) ||
          (CODEC_ID_PCM_S16BE == codec_ctx->codec_id) ||
          (CODEC_ID_PCM_U8 == codec_ctx->codec_id) ||
          (CODEC_ID_PCM_S24LE == codec_ctx->codec_id) ||
          (CODEC_ID_PCM_S24BE == codec_ctx->codec_id)) {
        InitOmxHeader(&oParam);
        ret = OMX_GetParameter(mCompHandle, OMX_IndexParamAudioInit, &oParam);
        if (OMX_ErrorNone != ret) {
          OMX_LOGE("Failed to OMX_GetParameter(OMX_IndexParamAudioInit) with error %d", ret);
          return;
        }
        OMX_LOGD("oParam.nStartPortNumber = %d, oParam.nPorts = %d",
            oParam.nStartPortNumber, oParam.nPorts);
        setPCMFormat(oParam.nStartPortNumber);
      }
      OMX_LOGD("EXIT");
      return;
    } else {
      OMX_LOGE("Invalid component name %s\n", mCompName);
    }
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);
    for (OMX_U32 i = oParam.nStartPortNumber; i < oParam.nPorts; i++) {
      def.nPortIndex = i;
      ret = OMX_GetParameter(mCompHandle, OMX_IndexParamPortDefinition, &def);
      ASSERT_EQ(ret, OMX_ErrorNone);
      if (def.eDir == OMX_DirOutput) {
        OMX_LOGD("Change output: width %d, height %d", codec_ctx->coded_width,
            codec_ctx->coded_height);
        def.format.video.nFrameWidth = codec_ctx->coded_width;
        def.format.video.nFrameHeight = codec_ctx->coded_height;
        def.format.video.nStride = codec_ctx->coded_width;
        def.format.video.nSliceHeight = codec_ctx->coded_height;
        def.format.video.eColorFormat = OMX_COLOR_FormatYUV422Planar;
        def.nBufferSize = codec_ctx->coded_width * codec_ctx->coded_height * 2;
        ret = OMX_SetParameter(mCompHandle, OMX_IndexParamPortDefinition, &def);
        ASSERT_EQ(ret, OMX_ErrorNone);
      }
    }
    OMX_LOGD("EXIT");
  }

  #define INT64_C(c) (c ## LL)
  #define UINT64_C(c) (c ## ULL)

  #define TIME_BASE_NUMER_1MHZ UINT64_C(1000000)
  #define TIME_BASE_NUMER_1KHZ UINT64_C(1000)

  #define INT64_MAX  9223372036854775807LL

  uint64_t GetTimeStampInUs(int64_t time_stamp, AVRational *pAVRational) {
    // If the pAVRational->num is too big, we have to use float type, otherwise it will overflow
    if (pAVRational->num > INT64_MAX / (int64_t)TIME_BASE_NUMER_1MHZ / 1000000) {
      return ((float)pAVRational->num / pAVRational->den) * TIME_BASE_NUMER_1MHZ * time_stamp;
    }

    if (INT64_MAX / TIME_BASE_NUMER_1MHZ >=
       (time_stamp < 0) ? time_stamp * -1 : time_stamp) {
     return (time_stamp * TIME_BASE_NUMER_1MHZ * pAVRational->num) /
         static_cast<int64_t>(pAVRational->den);
    } else {
     return (time_stamp * TIME_BASE_NUMER_1KHZ * pAVRational->num) /
         static_cast<int64_t>(pAVRational->den * UINT64_C(1000000));
    }
  }

  OMX_BOOL feedInputData() {
    OMX_LOGD("ENTER");
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_BOOL feeded = OMX_FALSE;
    AVStream *current_stream = NULL;
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
        //buf->nTimeStamp = (pkt.pts == AV_NOPTS_VALUE) ? pkt.dts : pkt.pts;
        current_stream = mAvFormatCtx->streams[mStreamIndex];
        buf->nTimeStamp = GetTimeStampInUs(pkt.pts, &(current_stream->time_base));
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
    OMX_LOGD("EXIT");
    return feeded;
  }

  OMX_ERRORTYPE pushOneInputBuffer() {
    OMX_LOGD("ENTER");
    kdThreadMutexLock(mInBufferLock);
    if (mEOS == OMX_FALSE) {
      while (!mInputBuffers.empty() && !feedInputData()) {
      }
    }
    kdThreadMutexUnlock(mInBufferLock);
    OMX_LOGD("EXIT");
    return OMX_ErrorNone;
  }

  OMX_ERRORTYPE pushInputBuffer() {
    OMX_LOGD("ENTER");
    kdThreadMutexLock(mInBufferLock);
    if (mEOS == OMX_FALSE) {
      while (!mInputBuffers.empty()) {
        feedInputData();
      }
    }
    kdThreadMutexUnlock(mInBufferLock);
    OMX_LOGD("EXIT");
    return OMX_ErrorNone;
  }

  OMX_ERRORTYPE pushOneOutputBuffer() {
    OMX_LOGD("ENTER");
    kdThreadMutexLock(mOutBufferLock);
    OMX_ERRORTYPE err;
    vector<OMX_BUFFERHEADERTYPE *>::iterator it;
    if (!mOutputBuffers.empty()) {
      it = mOutputBuffers.begin();
      err = OMX_FillThisBuffer(mCompHandle, *it);
      mOutputBuffers.erase(it);
    }
    kdThreadMutexUnlock(mOutBufferLock);
    OMX_LOGD("EXIT");
    return OMX_ErrorNone;
  }

  OMX_ERRORTYPE pushOutputBuffer() {
    OMX_LOGD("ENTER");
    OMX_ERRORTYPE err;
    kdThreadMutexLock(mOutBufferLock);
    vector<OMX_BUFFERHEADERTYPE *>::iterator it;
    while (!mOutputBuffers.empty()) {
      it = mOutputBuffers.begin();
      err = OMX_FillThisBuffer(mCompHandle, *it);
      mOutputBuffers.erase(it);
    }
    kdThreadMutexUnlock(mOutBufferLock);
    OMX_LOGD("EXIT");
    return err;
  }
  OMX_ERRORTYPE renderOutputData() {
    OMX_LOGD("ENTER");
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_LOGD("EXIT");
    return err;
  }

  void testLoadedToIdle() {
    OMX_LOGD("ENTER");
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

    OMX_LOGD("EXIT");
  }

  void testIdleToExec() {
    OMX_LOGD("ENTER");
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

    OMX_LOGD("EXIT");
  }

  void testExecToIdle() {
    OMX_LOGD("ENTER");
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

    OMX_LOGD("EXIT");
  }

  void testIdleToLoaded() {
    OMX_LOGD("ENTER");
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

    OMX_LOGD("EXIT");
  }

  int64_t getStreamStartTime() {
    OMX_LOGD("mAvFormatCtx->start_time = %lld us", mAvFormatCtx->start_time);
    return mAvFormatCtx->start_time;
  }

  void prepare() {
    OMX_LOGD("ENTER");
    OMX_ERRORTYPE err;
    // Prepare
    err = OMX_GetHandle(&mCompHandle, mCompName, this, &decTestCallbacks);
    ASSERT_EQ(err, OMX_ErrorNone);
    mEOS = OMX_FALSE;
    mOutputEOS = OMX_FALSE;
    mInputFrameNum = 0;
    mOutputFrameNum = 0;
    //configOutputPort();
    OMX_LOGD("EXIT");
  }
  void cleanUp() {
    OMX_LOGD("ENTER");
    OMX_ERRORTYPE err;
    if (NULL != mCompHandle) {
      err = OMX_FreeHandle(mCompHandle);
      ASSERT_EQ(err, OMX_ErrorNone);
      mCompHandle = NULL;
    }
    OMX_LOGD("EXIT");
  }
};
// class OmxAudDecoderClient


#undef  LOG_TAG
#define LOG_TAG "OmxAudRendererClient"

class OmxAudRendererClient {
public:
  OMX_HANDLETYPE mCompHandle;
  OMX_STRING mCompName;
  OMX_CALLBACKTYPE aRenTestCallbacks;
  kdCondSignal *mStateSignal;

  OmxAudRendererClient() {
    aRenTestCallbacks.EventHandler = aRenTestEventHandler;
    mStateSignal = new kdCondSignal();
  }

  ~OmxAudRendererClient() {
    delete mStateSignal;
  }

  void prepare() {
    OMX_ERRORTYPE err;
    // Prepare
    err = OMX_GetHandle(&mCompHandle, mCompName, this, &aRenTestCallbacks);
    ASSERT_EQ(err, OMX_ErrorNone);
  }

  void cleanUp() {
    OMX_ERRORTYPE err;
    if (NULL != mCompHandle) {
      err = OMX_FreeHandle(mCompHandle);
      ASSERT_EQ(err, OMX_ErrorNone);
      mCompHandle = NULL;
    }
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

    // Waiting for command complete
    OMX_LOGD("Waiting for change state to Loaded");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to Loaded");
  }

  static OMX_ERRORTYPE aRenTestEventHandler(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_EVENTTYPE eEvent, OMX_U32 Data1, OMX_U32 Data2,
      OMX_PTR pEventData) {
    OMX_LOGD("pAppData %p, event %x data %lX-%lX", pAppData,
        eEvent, Data1, Data2);
    OmxAudRendererClient *ctx = reinterpret_cast<OmxAudRendererClient *>(pAppData);
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
};

#undef  LOG_TAG
#define LOG_TAG "OmxClockClient"

class OmxClockClient {
public:
  OMX_HANDLETYPE mCompHandle;
  OMX_STRING mCompName;
  OMX_CALLBACKTYPE clockTestCallbacks;
  kdCondSignal *mStateSignal;

  OmxClockClient() {
    clockTestCallbacks.EventHandler = aClockTestEventHandler;
    mStateSignal = new kdCondSignal();
  }

  ~OmxClockClient() {
    delete mStateSignal;
  }

  void prepare() {
    OMX_ERRORTYPE err;
    // Prepare
    err = OMX_GetHandle(&mCompHandle, mCompName, this, &clockTestCallbacks);
    ASSERT_EQ(err, OMX_ErrorNone);
  }

  void cleanUp() {
    OMX_ERRORTYPE err;
    if (NULL != mCompHandle) {
      err = OMX_FreeHandle(mCompHandle);
      ASSERT_EQ(err, OMX_ErrorNone);
      mCompHandle = NULL;
    }
  }

  void startClock(int64_t startTimeUs) {
    OMX_LOGD("ENTER, startTimeUs = %lld us", startTimeUs);
    OMX_TIME_CONFIG_CLOCKSTATETYPE conf;
    InitOmxHeader(&conf);
    conf.eState = OMX_TIME_ClockStateRunning;
    conf.nStartTime = static_cast<OMX_TICKS>(startTimeUs);
    conf.nOffset = static_cast<OMX_TICKS>(0);
    OMX_ERRORTYPE err = OMX_SetConfig(mCompHandle, OMX_IndexConfigTimeClockState, &conf);
    if (err != OMX_ErrorNone) {
      // TODO: error handling
      OMX_LOGE("OMX_SetConfig(OMX_IndexConfigTimeClockState) failed with error %d", err);
    }
    OMX_LOGD("EXIT");
  }

  void setScale(double scale) {
    OMX_LOGD("ENTER, scale = %f", scale);
    OMX_TIME_CONFIG_SCALETYPE conf;
    InitOmxHeader(&conf);
    conf.xScale = static_cast<OMX_S32>(scale * 0x10000);
    if (conf.xScale != 0x0 && conf.xScale != 0x10000) {
      // TODO: Support trick play.
      OMX_LOGE("Trick play is not supported: scale = %lf", scale);
      return;
    }
    OMX_ERRORTYPE err = OMX_SetConfig(mCompHandle, OMX_IndexConfigTimeScale, &conf);
    if (err != OMX_ErrorNone) {
      // TODO: error handling
      OMX_LOGE("OMX_SetConfig(OMX_IndexConfigTimeScale) failed with error %d", err);
    }
    OMX_LOGD("EXIT");
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

    // Waiting for command complete
    OMX_LOGD("Waiting for change state to Loaded");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to Loaded");
  }

  static OMX_ERRORTYPE aClockTestEventHandler(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_EVENTTYPE eEvent, OMX_U32 Data1, OMX_U32 Data2,
      OMX_PTR pEventData) {
    OMX_LOGD("pAppData %p, event %x data %lX-%lX", pAppData,
        eEvent, Data1, Data2);
    OmxClockClient *ctx = reinterpret_cast<OmxClockClient *>(pAppData);
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
};


#undef  LOG_TAG
#define LOG_TAG "OmxAudTunnelTest"

class OmxAudTunnelTest : public testing::Test {
public:
  OmxAudDecoderClient *mAudDecClient;
  OmxAudRendererClient *mAudRenClient;
  OmxClockClient *mOmxClockClient;
  OMX_STRING mStreamName;

  virtual void SetUp() {
    OMX_ERRORTYPE err = OMX_Init();
    ASSERT_EQ(OMX_ErrorNone, err);
    mAudDecClient = new OmxAudDecoderClient;
    mAudRenClient = new OmxAudRendererClient;
    mOmxClockClient = new OmxClockClient;
  }

  virtual void TearDown() {
    delete mAudDecClient;
    delete mAudRenClient;
    delete mOmxClockClient;
    OMX_ERRORTYPE err = OMX_Deinit();
    ASSERT_EQ(OMX_ErrorNone, err);
  }

  void testAudTunnel(OMX_IN OMX_STRING comp1, OMX_IN OMX_STRING comp2,
      OMX_IN OMX_STRING comp3) {
    OMX_LOGD("################## ENTER");
    OMX_ERRORTYPE err;

    mAudDecClient->mStreamName = mStreamName;
    mAudDecClient->openStream();

    OMX_LOGD("##################");
    mOmxClockClient->mCompName = comp3;
    mOmxClockClient->prepare();

    OMX_LOGD("##################");
    mAudDecClient->mCompName = comp1;
    mAudDecClient->prepare();
    mAudDecClient->configOutputPort();

    OMX_LOGD("##################");
    mAudRenClient->mCompName = comp2;
    mAudRenClient->prepare();

    OMX_LOGD("##################");
    OMX_SetupTunnel(mAudDecClient->mCompHandle, kPortIndexOutput,
        mAudRenClient->mCompHandle, kPortIndexInput);
    OMX_SetupTunnel(mOmxClockClient->mCompHandle, kPortIndexClock,
        mAudRenClient->mCompHandle, kPortIndexClock);

    OMX_LOGD("##################");
    // Loded -> Idle
    mOmxClockClient->testLoadedToIdle();
    mAudDecClient->testLoadedToIdle();
    mAudRenClient->testLoadedToIdle();

    OMX_LOGD("##################");
    // Idle -> Executing
    mAudDecClient->testIdleToExec();
    mAudRenClient->testIdleToExec();
    mOmxClockClient->testIdleToExec();
    mOmxClockClient->startClock(mAudDecClient->getStreamStartTime());
    mOmxClockClient->setScale(1.0);

    OMX_LOGD("##################");
    // Push buffer
    mAudDecClient->queueAllPortsBuffer(OMX_PortDomainAudio);
    mAudDecClient->pushCodecConfigData();
    mAudDecClient->pushInputBuffer();

    OMX_LOGD("##################");
    // Wait EOS
    mAudDecClient->mEosSignal->waitSignal();

    OMX_LOGD("##################");
    // Executing -> Idle
    mOmxClockClient->testExecToIdle();
    mAudDecClient->testExecToIdle();
    mAudRenClient->testExecToIdle();

    OMX_LOGD("##################");
    // Idle -> Loaded
    mOmxClockClient->testIdleToLoaded();
    mAudDecClient->testIdleToLoaded();
    mAudRenClient->testIdleToLoaded();

    OMX_LOGD("##################");
    // Clean up
    mOmxClockClient->cleanUp();
    mAudDecClient->cleanUp();
    mAudRenClient->cleanUp();

    OMX_LOGD("##################");
    mAudDecClient->closeStream();

    OMX_LOGD("################## EXIT");
  }
};
// class OmxAudTunnelTest

TEST_F(OmxAudTunnelTest, pipelineTest) {
  char *compName1 = "OMX.Marvell.audio_decoder.raw";
  char *compName2 = "OMX.Marvell.audio_renderer.pcm";
  char *compName3 = "OMX.Marvell.clock.binary";
  mStreamName = "/data/test.wav";
  testAudTunnel(compName1, compName2, compName3);
}

GTEST_API_ int main(int argc, char **argv) {
  std::cout << "Running main() from gtest_main.cc\n";
  OMX_LOGD("Running main() from gtest_main.cc");

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

}  // namespace test
}  // namespace berlin
