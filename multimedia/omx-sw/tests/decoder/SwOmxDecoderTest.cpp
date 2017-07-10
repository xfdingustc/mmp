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
#define LOG_TAG "OmxVideoDecoderTest"

extern "C" {
#include <OMX_Core.h>
#include <OMX_Component.h>
#include <libavformat/avformat.h>
}
#include <BerlinOmxCommon.h>
#include <gtest/gtest.h>
#include <mmu_thread_utils.h>
#include <EventQueue.h>

#include <list>
using namespace std;

namespace mmp {
namespace test {

static OMX_BOOL getOMXChannelMapping(size_t numChannels, OMX_AUDIO_CHANNELTYPE map[]) {
    switch (numChannels) {
        case 1:
            map[0] = OMX_AUDIO_ChannelCF;
            break;
        case 2:
            map[0] = OMX_AUDIO_ChannelLF;
            map[1] = OMX_AUDIO_ChannelRF;
            break;
        case 3:
            map[0] = OMX_AUDIO_ChannelLF;
            map[1] = OMX_AUDIO_ChannelRF;
            map[2] = OMX_AUDIO_ChannelCF;
            break;
        case 4:
            map[0] = OMX_AUDIO_ChannelLF;
            map[1] = OMX_AUDIO_ChannelRF;
            map[2] = OMX_AUDIO_ChannelLR;
            map[3] = OMX_AUDIO_ChannelRR;
            break;
        case 5:
            map[0] = OMX_AUDIO_ChannelLF;
            map[1] = OMX_AUDIO_ChannelRF;
            map[2] = OMX_AUDIO_ChannelCF;
            map[3] = OMX_AUDIO_ChannelLR;
            map[4] = OMX_AUDIO_ChannelRR;
            break;
        case 6:
            map[0] = OMX_AUDIO_ChannelLF;
            map[1] = OMX_AUDIO_ChannelRF;
            map[2] = OMX_AUDIO_ChannelCF;
            map[3] = OMX_AUDIO_ChannelLFE;
            map[4] = OMX_AUDIO_ChannelLR;
            map[5] = OMX_AUDIO_ChannelRR;
            break;
        case 7:
            map[0] = OMX_AUDIO_ChannelLF;
            map[1] = OMX_AUDIO_ChannelRF;
            map[2] = OMX_AUDIO_ChannelCF;
            map[3] = OMX_AUDIO_ChannelLFE;
            map[4] = OMX_AUDIO_ChannelLR;
            map[5] = OMX_AUDIO_ChannelRR;
            map[6] = OMX_AUDIO_ChannelCS;
            break;
        case 8:
            map[0] = OMX_AUDIO_ChannelLF;
            map[1] = OMX_AUDIO_ChannelRF;
            map[2] = OMX_AUDIO_ChannelCF;
            map[3] = OMX_AUDIO_ChannelLFE;
            map[4] = OMX_AUDIO_ChannelLR;
            map[5] = OMX_AUDIO_ChannelRR;
            map[6] = OMX_AUDIO_ChannelLS;
            map[7] = OMX_AUDIO_ChannelRS;
            break;
        default:
            return OMX_FALSE;
    }

    return OMX_TRUE;
}

class CondSignal {
public:
  CondSignal() {
    mSignled = OMX_FALSE;
  }

  ~CondSignal() {
  }

  void waitSignal() {
    MmuAutoLock lock(mLock);
    if (!mSignled) {
      mCond.wait(mLock);
    }
  }

  void setSignal() {
    MmuAutoLock lock(mLock);
    mCond.signal();
    mSignled = OMX_TRUE;
  }

  void resetSignal() {
    MmuAutoLock lock(mLock);
    mSignled = OMX_FALSE;
  }

private:
  MmuMutex mLock;
  MmuCondition mCond;
  OMX_BOOL mSignled;
};


class OmxDecClient {
public:
  enum {
    kWhatSendCommand = 100,
    kWhatEmptyThisBuffer,
    kWhatFillThisBuffer,
    kWhatCommandComplete,
    kWhatEmptyBufferDone,
    kWhatFillBufferDone,
  };

  OMX_HANDLETYPE mCompHandle;
  OMX_STRING mCompName;
  OMX_CALLBACKTYPE decTestCallbacks;
  OMX_PORTDOMAINTYPE mDomain;

  list<OMX_BUFFERHEADERTYPE *> mInputBuffers;
  list<OMX_BUFFERHEADERTYPE *> mOutputBuffers;
  MmuMutex mBufferLock;
  int mInputFrameNum;
  int mOutputFrameNum;

  OMX_BOOL mEOS;
  OMX_BOOL mOutputEOS;

  CondSignal *mStateSignal;
  CondSignal *mEosSignal;

  AVFormatContext *mAvFormatCtx;
  int mStreamIndex;
  char *mStreamName;

  OmxEventQueue *mOmxEventQueue;

  FILE *fd_input;
  FILE *fd_output;

  class OmxEvent : public OmxEventQueue::Event {
    public:
      OmxEvent(OmxDecClient *comp,
            void (OmxDecClient::*method)(EventInfo &info),
            EventInfo &info)
        : Event(info),
          mComp(comp),
          mMethod(method) {
      }

      virtual ~OmxEvent() {}

    protected:
      virtual void fire(OmxEventQueue *queue, int64_t /* now_us */) {
          (mComp->*mMethod)(mEventInfo);
      }

    private:
      OmxDecClient *mComp;
      void (OmxDecClient::*mMethod)(EventInfo &info);

      OmxEvent(const OmxEvent &);
      OmxEvent &operator=(const OmxEvent &);
  };

  OmxDecClient() {
    decTestCallbacks.EventHandler = decTestEventHandler;
    decTestCallbacks.EmptyBufferDone = decTestEmptyBufferDone;
    decTestCallbacks.FillBufferDone = decTestFillBufferDone;
    mAvFormatCtx = NULL;
    mStateSignal = new CondSignal();
    mEosSignal = new CondSignal();

    mOmxEventQueue = new OmxEventQueue("OmxVidDecTest");
    mOmxEventQueue->start();

    fd_input = fopen("/data/omx/test_dec_input.dat", "w+");
    fd_output = fopen("/data/omx/test_dec_output.dat", "w+");
  }

  ~OmxDecClient() {
    cleanUp();
    closeStream();
    delete mStateSignal;
    delete mEosSignal;

    if (mOmxEventQueue) {
      mOmxEventQueue->stop();
      delete mOmxEventQueue;
      mOmxEventQueue = NULL;
    }

    if (fd_input) {
      fclose(fd_input);
      fd_input = NULL;
    }

    if (fd_output) {
      fclose(fd_output);
      fd_output = NULL;
    }
  }

  void dump_data(FILE *file, OMX_U8 *data, size_t size) {
    if (file) {
    fwrite(data, 1, size, file);
    fflush(file);
    }
  }

  static OMX_ERRORTYPE decTestEventHandler(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_EVENTTYPE eEvent, OMX_U32 Data1, OMX_U32 Data2,
      OMX_PTR pEventData) {
    OMX_LOGD("%s: pAppData %p, event %x data %lX-%lX", __func__, pAppData,
        eEvent, Data1, Data2);
    OmxDecClient *ctx = reinterpret_cast<OmxDecClient *>(pAppData);
    switch (eEvent) {
      case OMX_EventCmdComplete:
        if (Data1 == OMX_CommandStateSet) {
          // Send data in another thread.
          EventInfo info;
          info.SetInt32("what", kWhatCommandComplete);
          info.SetInt32("cmd", OMX_CommandStateSet);
          info.SetInt32("param", Data2);
          OmxEvent *event = new OmxEvent(ctx, &OmxDecClient::onOmxEvent, info);
          ctx->mOmxEventQueue->postEvent(event);
        }
        break;
      default:
        break;
    }
    return OMX_ErrorNone;
  }

  static OMX_ERRORTYPE decTestEmptyBufferDone(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer) {
    OMX_LOGD("buffer %p empty done", pBuffer);
    OmxDecClient *ctx = reinterpret_cast<OmxDecClient *>(pAppData);
    if (!ctx) {
      OMX_LOGE("OmxDecClient not found!");
      return OMX_ErrorBadParameter;
    }

    // Send data in another thread.
    EventInfo info;
    info.SetInt32("what", kWhatEmptyBufferDone);
    info.SetPointer("BufferHeader", pBuffer);
    OmxEvent *event = new OmxEvent(ctx, &OmxDecClient::onOmxEvent, info);
    ctx->mOmxEventQueue->postEvent(event);

    return OMX_ErrorNone;
  }

  static OMX_ERRORTYPE decTestFillBufferDone(OMX_HANDLETYPE hComponent,
      OMX_PTR pAppData, OMX_BUFFERHEADERTYPE* pBuffer) {
    OmxDecClient *ctx = reinterpret_cast<OmxDecClient *>(pAppData);
    if (!ctx) {
      OMX_LOGE("OmxDecClient not found!");
      return OMX_ErrorBadParameter;
    }
    OMX_LOGI("decoded frame %d, buffer %p, length %lu filled",
        ctx->mOutputFrameNum, pBuffer, pBuffer->nFilledLen);

    // Send data in another thread.
    EventInfo info;
    info.SetInt32("what", kWhatFillBufferDone);
    info.SetPointer("BufferHeader", pBuffer);
    OmxEvent *event = new OmxEvent(ctx, &OmxDecClient::onOmxEvent, info);
    ctx->mOmxEventQueue->postEvent(event);

    return OMX_ErrorNone;
  }

  void onOmxEvent(EventInfo &info) {
    MmuAutoLock lock(mBufferLock);

    mint32 msgType = 0;
    info.GetInt32("what", &msgType);
    //OMX_LOGD("msgType = %d", msgType);

    switch (msgType) {
      case kWhatEmptyThisBuffer:
      {
        if (!mEOS && !mInputBuffers.empty()) {
          OMX_BUFFERHEADERTYPE *buf = *(mInputBuffers.begin());
          if (!buf) {
            OMX_LOGE("OMX_BUFFERHEADERTYPE is not found!");
            return;
          }
          feedInputData(buf);
          mInputBuffers.erase(mInputBuffers.begin());
        }
        break;
      }

      case kWhatFillThisBuffer:
      {
        if (!mOutputEOS && !mOutputBuffers.empty()) {
          OMX_BUFFERHEADERTYPE *buf = *(mOutputBuffers.begin());
          if (!buf) {
            OMX_LOGE("OMX_BUFFERHEADERTYPE is not found!");
            return;
          }
          OMX_ERRORTYPE err = OMX_FillThisBuffer(mCompHandle, buf);
          ASSERT_EQ(err, OMX_ErrorNone);
          mOutputBuffers.erase(mOutputBuffers.begin());
        }
        break;
      }

      case kWhatEmptyBufferDone:
      {
        OMX_BUFFERHEADERTYPE *buf = NULL;
        info.GetPointer("BufferHeader", (void **)&buf);
        OMX_LOGD("buf = %p empty done", buf);
        if (!buf) {
          OMX_LOGE("OMX_BUFFERHEADERTYPE is not found!");
          return;
        }
        mInputBuffers.push_back(buf);
        pushOneInputBuffer();
        break;
      }

      case kWhatFillBufferDone:
      {
        OMX_BUFFERHEADERTYPE *buf = NULL;
        info.GetPointer("BufferHeader", (void **)&buf);
        OMX_LOGD("buf = %p fill done", buf);
        if (!buf) {
          OMX_LOGE("OMX_BUFFERHEADERTYPE is not found!");
          return;
        }
        renderOutputData(buf);
        mOutputFrameNum++;
        OMX_BOOL isEOS = (buf->nFlags & OMX_BUFFERFLAG_EOS) ? OMX_TRUE : OMX_FALSE;
        buf->nFilledLen = 0;
        buf->nFlags = 0;
        mOutputBuffers.push_back(buf);
        if (isEOS && mOutputEOS == OMX_FALSE) {
          OMX_LOGI("Receive EOS on output port");
          // Notify test to next step.
          mEosSignal->setSignal();
          mOutputEOS = OMX_TRUE;
        }
        if (!mOutputEOS) {
          pushOneOutputBuffer();
        }
        break;
      }

      case kWhatCommandComplete:
      {
        mint32 state = 0;
        info.GetInt32("param", &state);
        OMX_LOGI("Complete change to state %s(%d)",
            OmxState2String((OMX_STATETYPE)state), state);
        mStateSignal->setSignal();
        break;
      }

      default:
        OMX_LOGE("Unknown event %d!", msgType);
        break;
    }
  }

  void allocateAllPortsBuffer(OMX_HANDLETYPE compHandle, OMX_PORTDOMAINTYPE domain) {
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
        OMX_LOGE("Incorect domain %x", domain);
    }
    err = OMX_GetParameter(compHandle, param_index, &oParam);
    ASSERT_EQ(err, OMX_ErrorNone);

    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);

    // Allocate input port buffers
    def.nPortIndex = oParam.nStartPortNumber;
    err = OMX_GetParameter(compHandle, OMX_IndexParamPortDefinition, &def);
    ASSERT_EQ(err, OMX_ErrorNone);
    for (OMX_U32 j = 0; j < def.nBufferCountActual; j++) {
      OMX_BUFFERHEADERTYPE *header = NULL;
      err = OMX_AllocateBuffer(compHandle, &header, oParam.nStartPortNumber, NULL,
          def.nBufferSize);
      ASSERT_EQ(err, OMX_ErrorNone);
      mInputBuffers.push_back(header);
      OMX_LOGD("input port def.nBufferSize = %d, a new header %p, nAllocLen = %d",
          def.nBufferSize, header, header->nAllocLen);
    }
    OMX_LOGD("input port def.nBufferCountActual = %d, mInputBuffers.size() = %d",
        def.nBufferCountActual, mInputBuffers.size());
    ASSERT_EQ(def.nBufferCountActual, mInputBuffers.size());

    // Allocate output port buffers
    def.nPortIndex = oParam.nStartPortNumber + 1;
    err = OMX_GetParameter(compHandle, OMX_IndexParamPortDefinition, &def);
    ASSERT_EQ(err, OMX_ErrorNone);
    for (OMX_U32 j = 0; j < def.nBufferCountActual; j++) {
      OMX_BUFFERHEADERTYPE *header = NULL;
      err = OMX_AllocateBuffer(compHandle, &header, oParam.nStartPortNumber + 1, NULL,
          def.nBufferSize);
      ASSERT_EQ(err, OMX_ErrorNone);
      mOutputBuffers.push_back(header);
      OMX_LOGD("output port def.nBufferSize = %d, a new header %p", def.nBufferSize, header);
    }
    OMX_LOGD("output port def.nBufferCountActual = %d, mOutputBuffers.size() = %d",
        def.nBufferCountActual, mOutputBuffers.size());
    ASSERT_EQ(mOutputBuffers.size(), def.nBufferCountActual);
  }

  void freeAllPortsBuffer(OMX_HANDLETYPE compHandle, OMX_PORTDOMAINTYPE domain) {
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
        OMX_LOGE("Incorect domain %x", domain);
    }
    err = OMX_GetParameter(compHandle, param_index, &oParam);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_GE(oParam.nPorts, 0);

    // Free buffers on input port.
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);
    def.nPortIndex = oParam.nStartPortNumber;
    err = OMX_GetParameter(compHandle, OMX_IndexParamPortDefinition, &def);
    ASSERT_EQ(err, OMX_ErrorNone);
    OMX_LOGD("input port def.nBufferCountActual = %d, mInputBuffers.size() = %d",
        def.nBufferCountActual, mInputBuffers.size());
    ASSERT_EQ(mInputBuffers.size(), def.nBufferCountActual);
    for (list<OMX_BUFFERHEADERTYPE *>::iterator it = mInputBuffers.begin();
        it != mInputBuffers.end(); ++it) {
      OMX_BUFFERHEADERTYPE *header = *(it);
      err = OMX_FreeBuffer(compHandle, oParam.nStartPortNumber, header);
      ASSERT_EQ(err, OMX_ErrorNone);
    }
    mInputBuffers.clear();

    // Free buffers on output port.
    def.nPortIndex = oParam.nStartPortNumber + 1;
    err = OMX_GetParameter(compHandle, OMX_IndexParamPortDefinition, &def);
    ASSERT_EQ(err, OMX_ErrorNone);
    OMX_LOGD("output port def.nBufferCountActual = %d, mOutputBuffers.size() = %d",
        def.nBufferCountActual, mOutputBuffers.size());
    ASSERT_EQ(mOutputBuffers.size(), def.nBufferCountActual);
    for (list<OMX_BUFFERHEADERTYPE *>::iterator it = mOutputBuffers.begin();
        it != mOutputBuffers.end(); ++it) {
      OMX_BUFFERHEADERTYPE *header = *(it);
      err = OMX_FreeBuffer(compHandle, oParam.nStartPortNumber + 1, header);
      ASSERT_EQ(err, OMX_ErrorNone);
    }
    mOutputBuffers.clear();
  }

  void openStream() {
    // register all file formats
    av_register_all();
    int err = avformat_open_input(&mAvFormatCtx, mStreamName, NULL, NULL);
    ASSERT_GE(err, 0);
    err = avformat_find_stream_info(mAvFormatCtx, NULL);
    ASSERT_GE(err, 0);

    int idx = -1;
    if (OMX_PortDomainVideo == mDomain) {
      // select the video stream
      idx = av_find_best_stream(mAvFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
      // stream index should be >=0
      ASSERT_GE(idx, 0);
    } else if (OMX_PortDomainAudio == mDomain) {
      // select the video stream
      idx = av_find_best_stream(mAvFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
      // stream index should be >=0
      ASSERT_GE(idx, 0);
    }
    mStreamIndex = idx;
    OMX_LOGD("Using stream %d", idx);
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
      list<OMX_BUFFERHEADERTYPE *>::iterator it = mInputBuffers.begin();
      OMX_BUFFERHEADERTYPE *buf = *it;
      if (!buf) {
        OMX_LOGE("buf is not found!");
        return;
      }
      AVCodecContext *codec_ctx = mAvFormatCtx->streams[mStreamIndex]->codec;
      if (!codec_ctx) {
        OMX_LOGE("codec_ctx is not found!");
        return;
      }
      if (codec_ctx->extradata && codec_ctx->extradata_size) {
        memcpy(buf->pBuffer, codec_ctx->extradata, codec_ctx->extradata_size);
        buf->nFilledLen = codec_ctx->extradata_size;
        buf->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
        //dump_data(fd_input, codec_ctx->extradata, codec_ctx->extradata_size);
        ret = OMX_EmptyThisBuffer(mCompHandle, buf);
        OMX_LOGD("push codec config data in buf %p to omx", buf);
        mInputBuffers.erase(it);
      }
    }
  }

  void setupRawAudioFormat(OMX_U32 portIndex, int32_t sampleRate, int32_t numChannels) {
    OMX_ERRORTYPE err = OMX_ErrorNone;

    // port definition
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);
    def.nPortIndex = portIndex;
    err = OMX_GetParameter(mCompHandle, OMX_IndexParamPortDefinition, &def);
    ASSERT_EQ(err, OMX_ErrorNone);

    def.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    err = OMX_SetParameter(mCompHandle, OMX_IndexParamPortDefinition, &def);
    ASSERT_EQ(err, OMX_ErrorNone);

    // pcm param
    OMX_AUDIO_PARAM_PCMMODETYPE pcmParams;
    InitOmxHeader(&pcmParams);
    pcmParams.nPortIndex = portIndex;
    err = OMX_GetParameter(mCompHandle, OMX_IndexParamAudioPcm, &pcmParams);
    ASSERT_EQ(err, OMX_ErrorNone);

    OMX_LOGD("numChannels = %d, sampleRate = %d, bitPerSample = 16", numChannels, sampleRate);
    pcmParams.nChannels = numChannels;
    pcmParams.eNumData = OMX_NumericalDataSigned;
    pcmParams.bInterleaved = OMX_TRUE;
    pcmParams.nBitPerSample = 16;
    pcmParams.nSamplingRate = sampleRate;
    pcmParams.ePCMMode = OMX_AUDIO_PCMModeLinear;

    if (getOMXChannelMapping(numChannels, pcmParams.eChannelMapping) != OMX_TRUE) {
      OMX_LOGE("numChannels %d is not compatible for omx channel mapping", numChannels);
    }

    err = OMX_SetParameter(mCompHandle, OMX_IndexParamAudioPcm, &pcmParams);
    ASSERT_EQ(err, OMX_ErrorNone);
  }

  void configOutputPort(OMX_PORTDOMAINTYPE domain) {
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    AVCodecContext *codec_ctx = mAvFormatCtx->streams[mStreamIndex]->codec;

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
        OMX_LOGE("Incorect domain %x", domain);
    }
    ret = OMX_GetParameter(mCompHandle, param_index, &oParam);
    ASSERT_EQ(ret, OMX_ErrorNone);

    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOmxHeader(&def);
    for (OMX_U32 i = oParam.nStartPortNumber; i < oParam.nPorts; i++) {
      def.nPortIndex = i;
      ret = OMX_GetParameter(mCompHandle, OMX_IndexParamPortDefinition, &def);
      ASSERT_EQ(ret, OMX_ErrorNone);
      if (def.eDir == OMX_DirOutput) {
        if (OMX_PortDomainVideo == mDomain) {
          OMX_LOGD("Change output: width %d, height %d", codec_ctx->coded_width,
              codec_ctx->coded_height);
          def.format.video.nFrameWidth = codec_ctx->coded_width;
          def.format.video.nFrameHeight = codec_ctx->coded_height;
          def.format.video.nStride = codec_ctx->coded_width;
          def.format.video.nSliceHeight = codec_ctx->coded_height;
          def.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
          def.nBufferSize = codec_ctx->coded_width * codec_ctx->coded_height * 3 / 2;
          ret = OMX_SetParameter(mCompHandle, OMX_IndexParamPortDefinition, &def);
          ASSERT_EQ(ret, OMX_ErrorNone);
        } else if (OMX_PortDomainAudio == mDomain) {
          setupRawAudioFormat(i, codec_ctx->sample_rate, codec_ctx->channels);
        }
      }
    }
  }

  OMX_BOOL feedInputData(OMX_BUFFERHEADERTYPE *buf) {
    OMX_ERRORTYPE ret;
    OMX_BOOL feeded = OMX_FALSE;
    int err = 0;
    AVPacket pkt;

    av_init_packet(&pkt);
    while (true) {
      err = av_read_frame(mAvFormatCtx, &pkt);
      if (err < 0) {
        buf->nFlags = OMX_BUFFERFLAG_EOS;
        buf->nFilledLen = 0;
        mEOS = OMX_TRUE;
        OMX_LOGI("Input port got EOS");
        break;
      }

      if (pkt.stream_index == mStreamIndex) {
        memcpy(buf->pBuffer, pkt.data, pkt.size);
        buf->nFilledLen = pkt.size;
        buf->nTimeStamp = (pkt.pts == AV_NOPTS_VALUE) ? pkt.dts : pkt.pts;
        OMX_LOGD("Push frame %d, pts %lld, size %lu", mInputFrameNum,
            buf->nTimeStamp, buf->nFilledLen);
        break;
      }

      av_free_packet(&pkt);
    }

    //dump_data(fd_input, pkt.data, pkt.size);
    ret = OMX_EmptyThisBuffer(mCompHandle, buf);
    mInputFrameNum++;
    feeded = OMX_TRUE;
    av_free_packet(&pkt);

    return feeded;
  }

  OMX_ERRORTYPE pushOneInputBuffer() {
    // Send data in another thread.
    EventInfo info;
    info.SetInt32("what", kWhatEmptyThisBuffer);
    OmxEvent *event = new OmxEvent(this, &OmxDecClient::onOmxEvent, info);
    mOmxEventQueue->postEvent(event);
    return OMX_ErrorNone;
  }

  OMX_ERRORTYPE pushInputBuffer() {
    MmuAutoLock lock(mBufferLock);
    if (mEOS == OMX_FALSE) {
      for (list<OMX_BUFFERHEADERTYPE *>::iterator it = mInputBuffers.begin();
          it != mInputBuffers.end(); ++it) {
        pushOneInputBuffer();
      }
    }
    OMX_LOGD("push all buffers to omx");
    return OMX_ErrorNone;
  }

  OMX_ERRORTYPE renderOutputData(OMX_BUFFERHEADERTYPE *buf) {
    OMX_ERRORTYPE err = OMX_ErrorNone;
    // Dump the decoded data to check whether OMX decoder works correctly.
    if (buf) {
      //dump_data(fd_output, buf->pBuffer, buf->nFilledLen);
    }
    return err;
  }

  OMX_ERRORTYPE pushOneOutputBuffer() {
    // Send data in another thread.
    EventInfo info;
    info.SetInt32("what", kWhatFillThisBuffer);
    OmxEvent *event = new OmxEvent(this, &OmxDecClient::onOmxEvent, info);
    mOmxEventQueue->postEvent(event);
    return OMX_ErrorNone;
  }

  OMX_ERRORTYPE pushOutputBuffer() {
    MmuAutoLock lock(mBufferLock);
    OMX_ERRORTYPE err = OMX_ErrorNone;
    for (list<OMX_BUFFERHEADERTYPE *>::iterator it = mOutputBuffers.begin();
        it != mOutputBuffers.end(); ++it) {
      pushOneOutputBuffer();
    }
    OMX_LOGD("push all buffers to omx");
    return err;
  }

  void testLoadedToIdle() {
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_STATETYPE state = OMX_StateInvalid;

    err = OMX_GetState(mCompHandle, &state);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_EQ(state, OMX_StateLoaded);

    mStateSignal->resetSignal();

    err = OMX_SendCommand(mCompHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    ASSERT_EQ(err, OMX_ErrorNone);

    allocateAllPortsBuffer(mCompHandle, mDomain);

    // Waiting for command complete
    OMX_LOGD("Waiting for state change to idle");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to idle");
  }

  void testIdleToExec() {
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_STATETYPE state = OMX_StateInvalid;

    err = OMX_GetState(mCompHandle, &state);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_EQ(state, OMX_StateIdle);

    mStateSignal->resetSignal();
    err = OMX_SendCommand(mCompHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    ASSERT_EQ(err, OMX_ErrorNone);

    // Waiting for command complete
    OMX_LOGD("Waiting for change state to Executing");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to Executing");
  }

  void testExecToIdle() {
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_STATETYPE state = OMX_StateInvalid;

    err = OMX_GetState(mCompHandle, &state);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_EQ(state, OMX_StateExecuting);

    mStateSignal->resetSignal();
    err = OMX_SendCommand(mCompHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
    ASSERT_EQ(err, OMX_ErrorNone);

    // Waiting for command complete
    OMX_LOGD("Waiting for state change to OMX_StateIdle");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to OMX_StateIdle");
  }

  void testIdleToLoaded() {
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_STATETYPE state = OMX_StateInvalid;

    err = OMX_GetState(mCompHandle, &state);
    ASSERT_EQ(err, OMX_ErrorNone);
    ASSERT_EQ(state, OMX_StateIdle);

    mStateSignal->resetSignal();
    err = OMX_SendCommand(mCompHandle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    ASSERT_EQ(err, OMX_ErrorNone);

    freeAllPortsBuffer(mCompHandle, mDomain);

    // Waiting for command complete
    OMX_LOGD("Waiting for change state to Loaded");
    mStateSignal->waitSignal();
    OMX_LOGD("State changed to Loaded");
  }

  void prepare() {
    OMX_ERRORTYPE err = OMX_ErrorNone;
    // Prepare
    err = OMX_GetHandle(&mCompHandle, mCompName, this, &decTestCallbacks);
    ASSERT_EQ(err, OMX_ErrorNone);
    mEOS = OMX_FALSE;
    mOutputEOS = OMX_FALSE;
    mInputFrameNum = 0;
    mOutputFrameNum = 0;
    configOutputPort(mDomain);
  }

  void cleanUp() {
    OMX_ERRORTYPE err = OMX_ErrorNone;
    if (NULL != mCompHandle) {
      err = OMX_FreeHandle(mCompHandle);
      ASSERT_EQ(err, OMX_ErrorNone);
      mCompHandle = NULL;
    }
  }
};

// class OmxDecClient
class OmxDecTest : public testing::Test {
public:
  OmxDecClient *mClient;
  OMX_STRING mStreamName;

  virtual void SetUp() {
    OMX_ERRORTYPE err = OMX_Init();
    ASSERT_EQ(OMX_ErrorNone, err);
    mClient = new OmxDecClient;
  }

  virtual void TearDown() {
    delete mClient;
    OMX_ERRORTYPE err = OMX_Deinit();
    ASSERT_EQ(OMX_ErrorNone, err);
  }

  void testDecode(OMX_IN OMX_STRING comp, OMX_PORTDOMAINTYPE domain) {
    OMX_ERRORTYPE err;
    mClient->mCompName = comp;
    mClient->mStreamName = mStreamName;
    mClient->mDomain = domain;

    mClient->openStream();
    mClient->prepare();

    // Loded -> Idle
    mClient->testLoadedToIdle();
    // Idle -> Excuting
    mClient->testIdleToExec();

    // Push buffer
    mClient->pushOutputBuffer();
    mClient->pushCodecConfigData();
    mClient->pushInputBuffer();
    // Wait EOS
    mClient->mEosSignal->waitSignal();

    mClient->testExecToIdle();
    // Idle -> Loaded
    mClient->testIdleToLoaded();

    // Clean up
    mClient->cleanUp();
    mClient->closeStream();
  }

  void testStateTransition(OMX_IN OMX_STRING comp, OMX_PORTDOMAINTYPE domain) {
    OMX_LOGD("Enter %s", __func__);
    mClient->mCompName = comp;
    mClient->mStreamName = mStreamName;
    mClient->mDomain = domain;

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
// class OmxDecTest


TEST_F(OmxDecTest, decStateTest) {
  char *compName = "comp.sw.video_decoder.avc";
  mStreamName = "/data/test.mp4";
  testStateTransition(compName, OMX_PortDomainVideo);
}

TEST_F(OmxDecTest, videodecodeTest) {
  char *compName = "comp.sw.video_decoder.avc";
  mStreamName = "/data/test.mp4";
  testDecode(compName, OMX_PortDomainVideo);
}

TEST_F(OmxDecTest, audiodecodeTest) {
  char *compName = "comp.sw.audio_decoder.aac";
  mStreamName = "/data/test.mp4";
  testDecode(compName, OMX_PortDomainAudio);
}

GTEST_API_ int main(int argc, char **argv) {
  std::cout << "Running main() from gtest_main.cc\n";
  OMX_LOGD("Running main() from gtest_main.cc");

  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

}  // namespace test
}
