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

/**
 * @file BerlinOmxAmpAudioDecoder.h
 * @author  csheng@marvell.com
 * @date 2013.01.17
 * @brief  Define the class OmxAmpAudioDecoder
 */

#ifndef BERLIN_OMX_AMP_AUDIO_DECODER_H_
#define BERLIN_OMX_AMP_AUDIO_DECODER_H_

#include <BerlinOmxAmpAudioPort.h>
#include <BerlinOmxComponentImpl.h>
#include <BerlinOmxAmpAudioRenderer.h>
#include <CryptoInfo.h>

extern "C" {
#include <mediahelper.h>
}

#ifdef _ANDROID_
#include <utils/KeyedVector.h>
#include <SourceControl.h>
#include <source_constants.h>
#endif

#ifdef _ANDROID_
using marvell::AudioStreamInfo;
using marvell::SourceControl;
#endif

namespace berlin {

#define OMX_ROLE_AUDIO_DECODER_AAC  "audio_decoder.aac"
#define OMX_ROLE_AUDIO_DECODER_AAC_LEN 17

#define OMX_ROLE_AUDIO_DECODER_AAC_SECURE  "audio_decoder.aac.secure"
#define OMX_ROLE_AUDIO_DECODER_AAC_SECURE_LEN 24

#define OMX_ROLE_AUDIO_DECODER_MP1  "audio_decoder.mp1"
#define OMX_ROLE_AUDIO_DECODER_MP1_LEN 17

#define OMX_ROLE_AUDIO_DECODER_MP2  "audio_decoder.mp2"
#define OMX_ROLE_AUDIO_DECODER_MP2_LEN 17

#define OMX_ROLE_AUDIO_DECODER_MP3  "audio_decoder.mp3"
#define OMX_ROLE_AUDIO_DECODER_MP3_LEN 17

#define OMX_ROLE_AUDIO_DECODER_VORBIS  "audio_decoder.vorbis"
#define OMX_ROLE_AUDIO_DECODER_VORBIS_LEN 20

#define OMX_ROLE_AUDIO_DECODER_G711  "audio_decoder.g711"
#define OMX_ROLE_AUDIO_DECODER_G711_LEN 18

#define OMX_ROLE_AUDIO_DECODER_AC3  "audio_decoder.ac3"
#define OMX_ROLE_AUDIO_DECODER_AC3_LEN 17

#define OMX_ROLE_AUDIO_DECODER_AC3_SECURE  "audio_decoder.ac3.secure"
#define OMX_ROLE_AUDIO_DECODER_AC3_SECURE_LEN 24

#define OMX_ROLE_AUDIO_DECODER_DTS  "audio_decoder.dts"
#define OMX_ROLE_AUDIO_DECODER_DTS_LEN 17

#define OMX_ROLE_AUDIO_DECODER_WMA_SECURE  "audio_decoder.wma.secure"
#define OMX_ROLE_AUDIO_DECODER_WMA_SECURE_LEN 24

#define OMX_ROLE_AUDIO_DECODER_WMA  "audio_decoder.wma"
#define OMX_ROLE_AUDIO_DECODER_WMA_LEN 17

#define OMX_ROLE_AUDIO_DECODER_AMR  "audio_decoder.amr"
#define OMX_ROLE_AUDIO_DECODER_AMR_LEN 17

#define OMX_ROLE_AUDIO_DECODER_AMRNB  "audio_decoder.amrnb"
#define OMX_ROLE_AUDIO_DECODER_AMRNB_LEN 19

#define OMX_ROLE_AUDIO_DECODER_AMRWB  "audio_decoder.amrwb"
#define OMX_ROLE_AUDIO_DECODER_AMRWB_LEN 19

#define OMX_ROLE_AUDIO_DECODER_RAW  "audio_decoder.raw"
#define OMX_ROLE_AUDIO_DECODER_RAW_LEN 17

#define OMX_ROLE_AUDIO_DECODER_RA  "audio_decoder.ra"
#define OMX_ROLE_AUDIO_DECODER_RA_LEN 16

static const AudioFormatType kAudioFormatSupport[] = {
  {OMX_AUDIO_CodingAAC},
  {OMX_AUDIO_CodingMP3},
  {OMX_AUDIO_CodingVORBIS},
  {OMX_AUDIO_CodingG711},
#ifdef OMX_AudioExt_h
  {static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingAC3)},
  {static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingDTS)},
  {static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingMP2)},
  {static_cast<OMX_AUDIO_CODINGTYPE>(OMX_AUDIO_CodingMP1)},
#endif
  {OMX_AUDIO_CodingWMA},
  {OMX_AUDIO_CodingAMR},
};


typedef struct CodecSpeficDataType {
  void * data;
  unsigned int size;
} CodecSpeficDataType;

class OmxAmpAudioDecoder : public OmxComponentImpl {
  public:
    OmxAmpAudioDecoder();
    explicit OmxAmpAudioDecoder(OMX_STRING name);
    virtual ~OmxAmpAudioDecoder();

    virtual OMX_ERRORTYPE componentDeInit(
        OMX_HANDLETYPE hComponent);
    virtual OMX_ERRORTYPE getParameter(OMX_INDEXTYPE index, OMX_PTR params);
    virtual OMX_ERRORTYPE setParameter(OMX_INDEXTYPE index,
        const OMX_PTR params);
    virtual OMX_ERRORTYPE initPort();
    virtual OMX_ERRORTYPE initRole();
    virtual OMX_ERRORTYPE prepare();
    virtual OMX_ERRORTYPE preroll();
    virtual OMX_ERRORTYPE start();
    virtual OMX_ERRORTYPE pause();
    virtual OMX_ERRORTYPE resume();
    virtual OMX_ERRORTYPE stop();
    virtual OMX_ERRORTYPE release();
    virtual OMX_ERRORTYPE flush();
    void pushBufferLoop();
    OMX_ERRORTYPE DecryptSecureDataToCbuf(OMX_BUFFERHEADERTYPE *in_head,
        CryptoInfo *cryptoInfo, AMP_SHM_HANDLE handle, OMX_U32 cbuf_offset);
    UINT32 pushCbufBd(OMX_BUFFERHEADERTYPE *in_head, CryptoInfo *cryptoInfo);
    UINT32 requestCbufBd();

    static void* threadEntry(void *args);
    static HRESULT pushBufferDone(
        AMP_COMPONENT component,
        AMP_PORT_IO port_Io,
        UINT32 port_idx,
        AMP_BD_ST *bd,
        void *context);
    static OMX_ERRORTYPE createAmpAudioDecoder(
        OMX_HANDLETYPE *handle,
        OMX_STRING componentName,
        OMX_PTR appData,
        OMX_CALLBACKTYPE *callBacks);

  protected:
    virtual void onPortFlushCompleted(OMX_U32 portIndex);
    virtual void onPortEnableCompleted(OMX_U32 portIndex, OMX_BOOL enabled);

  private:
    OMX_ERRORTYPE getAmpAudioDecoder();
    OMX_ERRORTYPE destroyAmpAudioDecoder();
    OMX_ERRORTYPE setAmpState(AMP_STATE state);
    OMX_ERRORTYPE getAmpState(AMP_STATE *state);
    OMX_ERRORTYPE pushAmpBd(AMP_PORT_IO port, UINT32 portindex, AMP_BD_ST *bd);
    OMX_ERRORTYPE registerBds(OmxAmpAudioPort *port);
    OMX_ERRORTYPE unregisterBds(OmxAmpAudioPort *port);
    OMX_S32 getAmpDecType(OMX_AUDIO_CODINGTYPE type);
    OMX_BOOL isAudioParamSupport(OMX_AUDIO_PARAM_PORTFORMATTYPE* audio_param);
    OMX_ERRORTYPE setADECParameter(OMX_S32 adec_type);
    OMX_ERRORTYPE clearAmpPort();
    OMX_ERRORTYPE configDmxPort();
    void initAQ(OMX_S32 adec_type);
    OMX_S32 getAudioChannel();
    HRESULT registerEvent(AMP_EVENT_CODE event);
    HRESULT unRegisterEvent(AMP_EVENT_CODE event);
    static HRESULT eventHandle(HANDLE hListener, AMP_EVENT *pEvent, VOID *pUserData);
    OMX_ERRORTYPE resetPtsForFirstFrame(OMX_BUFFERHEADERTYPE *in_head);
    OMX_ERRORTYPE setWMAParameter(OMX_U8 *extra_data = NULL,
        OMX_U32 extra_size = 0);
    void initParameter();
    OMX_ERRORTYPE getAmpAudioInfo(int adecAudioType, AMP_ADEC_AUD_INFO* audinfo);
    OMX_U32 getAudioProfileLevel(OMX_U32& profile, OMX_U32& level);

  private:
    // Define class that will receive the avsetting change notifications.
    class AVSettingObserver : public marvell::BnAVSettingObserver {
      public:
        AVSettingObserver(OmxAmpAudioDecoder& audioDecoder)
            :mAudioDecoder(audioDecoder){
        }
        void OnSettingUpdate(const char* name, const AVSettingValue& value);
      private:
        OmxAmpAudioDecoder& mAudioDecoder;
    };

  public:
    OMX_U32 mStreamPosition;
    OMX_U32 mInputFrameNum;
    OMX_U32 mOutputFrameNum;
    OMX_U32 mInBDBackNum;
    OMX_U32 mOutBDPushNum;
    OmxPortImpl *mInputPort;
    OmxPortImpl *mOutputPort;
    OMX_BOOL mShouldExit;
    OMX_BOOL mInited;
    OMX_BOOL mCodecSpecficDataSent;
    OMX_BOOL mOutputConfigured;
    OMX_BOOL mOutputConfigChangedComplete;
    OMX_BOOL mOutputConfigInit;
    vector<CodecSpeficDataType> mCodecSpeficData;

  private:
    static const OMX_U32 kDefaultSampleRate = 44100;
    static const OMX_U8 kDefaultChannels = 2;
    OMX_BOOL mEOS;
    OMX_BOOL mOutputEOS;
    OMX_BOOL mDelayOutputEOS;
    OMX_U8 mPushCount;
    OMX_BOOL mPaused;
    KDThread *mThread;
    KDThreadAttr *mThreadAttr;
    KDThreadCond *mPauseCond;
    KDThreadMutex *mPauseLock;
    KDThreadMutex *mBufferLock;
    OMX_MARKTYPE mMark;
    AMP_FACTORY mFactory;
    OMX_BOOL mIsADTS;
    OMX_TICKS mTimeStampUs;
    AMP_COMPONENT mDMXHandle;
    OMX_BOOL mAudioDrm;
    OMX_U64 mAudioLength;
    AMP_DMX_CHNL_ST mChnlObj;
    AMP_DMX_INPUT_ST mInputObj;
    AMP_SHM_HANDLE mhInputBuf;
    OMX_U8 mDrm_type;
    OMX_BOOL mWMDRM;
    OMX_BOOL mSchemeIdSend;
    HANDLE mListener;

    // AQ Source Control
    sp<SourceControl> mSourceControl;
    int32_t mSourceId;
    int32_t mAudioChannel;
    int32_t mPhyDRCMode;
    int32_t mPhyChmap;

    // DCV Cert
    int32_t mStereoMode;
    int32_t mDualMode;

    HANDLE mPool;
    OMX_U32 mPushedBdNum;
    OMX_U32 mReturnedBdNum;
    OMX_BUFFERHEADERTYPE *mCachedhead;
    OMX_BOOL mMediaHelper;

    //MS11:DDC Cert
    sp<OutputControl> mOutputControl;
    sp<IAVSettingObserver> mObserver;
};
}
#endif  // BERLIN_OMX_AMP_AUDIO_DECODER_H_
