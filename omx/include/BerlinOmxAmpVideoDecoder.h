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


#ifndef BERLIN_OMX_AMP_VIDEO_DECODER_H_
#define BERLIN_OMX_AMP_VIDEO_DECODER_H_

#include <BerlinOmxAmpVideoPort.h>
#include <BerlinOmxComponentImpl.h>
#include <BerlinOmxVoutProxy.h>
#include <CryptoInfo.h>
extern "C" {
#include <mediahelper.h>
}

#ifdef _ANDROID_
#include <cutils/properties.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <utils/KeyedVector.h>
#include <HardwareAPI.h>
#include <SourceControl.h>
#endif

#ifdef _ANDROID_
using namespace android;
using marvell::SourceControl;
using marvell::AVSettingValue;
using marvell::IAVSettingObserver;
using marvell::IAVSettings;
using marvell::AVSettingsHelper;
#endif

namespace berlin {

#define OMX_ROLE_VIDEO_DECODER_H263  "video_decoder.h263"
#define OMX_ROLE_VIDEO_DECODER_AVC   "video_decoder.avc"
#define OMX_ROLE_VIDEO_DECODER_AVC_SECURE   "video_decoder.avc.secure"
#define OMX_ROLE_VIDEO_DECODER_MPEG2 "video_decoder.mpeg2"
#define OMX_ROLE_VIDEO_DECODER_MPEG4 "video_decoder.mpeg4"
#define OMX_ROLE_VIDEO_DECODER_RV    "video_decoder.rv"
#define OMX_ROLE_VIDEO_DECODER_WMV   "video_decoder.wmv"
#define OMX_ROLE_VIDEO_DECODER_WMV_SECURE   "video_decoder.wmv.secure"
#define OMX_ROLE_VIDEO_DECODER_VC1   "video_decoder.vc1"
#define OMX_ROLE_VIDEO_DECODER_VC1_SECURE   "video_decoder.vc1.secure"
#define OMX_ROLE_VIDEO_DECODER_MJPEG   "video_decoder.mjpeg"
#ifdef _ANDROID_
#define OMX_ROLE_VIDEO_DECODER_VPX   "video_decoder.vpx"
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
#define OMX_ROLE_VIDEO_DECODER_VP8   "video_decoder.vp8"
#define OMX_ROLE_VIDEO_DECODER_MSMPEG4   "video_decoder.msmpeg4"
#define OMX_ROLE_VIDEO_DECODER_HEVC   "video_decoder.hevc"
#endif

struct ProfileLevelType{
  OMX_U32 eProfile;
  OMX_U32 eLevel;
};

struct VideoFormatType{
  OMX_VIDEO_CODINGTYPE eCompressionFormat;
  OMX_COLOR_FORMATTYPE eColorFormat;
  OMX_U32 xFramerate;
};

struct AdaptivePlaybackStatus{
  OMX_BOOL bEnable;
  OMX_U32 bMaxWidth;
  OMX_U32 bMaxHeight;
};

typedef struct VdecStreamInfo {
  OMX_U32     nMaxWidth; /* Reslution width,*/
  OMX_U32     nMaxHeight; /* Reslution height,*/
  OMX_U32     mMaxNumDisBuf; /* Display buf Num,*/
  OMX_BOOL    nIsIntlSeq;/* Interlace or sequence*/
  OMX_U32     nFrameRateNum;/* Frame rate's Numerator  */
  OMX_U32     nFrameRateDen;/* Frame rate's denominator*/
  OMX_U32     nSarWidth;
  OMX_U32     nSarHeight;
  OMX_U32     nFrameRate;/* (nFrameRateNum * 1000)/nFrameRateDen */
  OMX_BOOL    nIsFirstIFrameMg;
  OMX_U32     nStereoMode; /* 3D info, such as FramePacking, SideBySide, TopBottom and etc.*/
}VdecStreamInfo;

static const ProfileLevelType kAvcProfileLevel[] = {
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1},
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel12},
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel3},
  {OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel22},
  {OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel31},
  {OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel4},
  {OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel51},
  {OMX_VIDEO_AVCProfileExtended, OMX_VIDEO_AVCLevel51}
};

static const ProfileLevelType kMpeg4ProfileLevel[] = {
  {OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0},
  {OMX_VIDEO_MPEG4ProfileMain, OMX_VIDEO_MPEG4Level5},
  {OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level5}
};

static const ProfileLevelType kMpeg2ProfileLevel[] = {
  {OMX_VIDEO_MPEG2ProfileSimple, OMX_VIDEO_MPEG2LevelML},
  {OMX_VIDEO_MPEG2ProfileMain, OMX_VIDEO_MPEG2LevelML}
};

static const ProfileLevelType kH263ProfileLevel[] = {
  {OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level10}
};

static const ProfileLevelType kVp8ProfileLevel[] = {
  {OMX_VIDEO_VP8ProfileMain, OMX_VIDEO_VP8Level_Version0},
  {OMX_VIDEO_VP8ProfileMain, OMX_VIDEO_VP8Level_Version1},
  {OMX_VIDEO_VP8ProfileMain, OMX_VIDEO_VP8Level_Version2},
  {OMX_VIDEO_VP8ProfileMain, OMX_VIDEO_VP8Level_Version3}
};

static const VideoFormatType kFormatSupport[] = {
  {OMX_VIDEO_CodingAVC, OMX_COLOR_FormatUnused, 15 <<16},
  {OMX_VIDEO_CodingMPEG4, OMX_COLOR_FormatUnused, 15 <<16},
  {OMX_VIDEO_CodingH263, OMX_COLOR_FormatUnused, 15 <<16},
  {OMX_VIDEO_CodingMPEG2, OMX_COLOR_FormatUnused, 15 <<16},
#ifdef _ANDROID_
  {static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingVPX),
       OMX_COLOR_FormatUnused, 15 <<16},
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
  {OMX_VIDEO_CodingVP8, OMX_COLOR_FormatUnused, 15 <<16},
  {OMX_VIDEO_CodingVC1, OMX_COLOR_FormatUnused, 15 <<16},
  {static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingMSMPEG4),
      OMX_COLOR_FormatUnused, 15 <<16},
#endif
  {OMX_VIDEO_CodingWMV, OMX_COLOR_FormatUnused, 15 <<16},
  {OMX_VIDEO_CodingMJPEG, OMX_COLOR_FormatUnused, 15 <<16},
};

typedef struct CodecSpeficDataType {
  void * data;
  unsigned int size;
} CodecSpeficDataType;

class OmxAmpVideoDecoder : public OmxComponentImpl {
  public:
    OmxAmpVideoDecoder();
    OmxAmpVideoDecoder(OMX_STRING name);
    virtual ~OmxAmpVideoDecoder();
    virtual OMX_ERRORTYPE initRole();
    virtual OMX_ERRORTYPE initPort();
    virtual OMX_ERRORTYPE componentDeInit(OMX_HANDLETYPE hComponent);
    virtual OMX_ERRORTYPE getConfig(OMX_INDEXTYPE index, OMX_PTR config);
    virtual OMX_ERRORTYPE getParameter(OMX_INDEXTYPE index, const OMX_PTR params);
    virtual OMX_ERRORTYPE setParameter(OMX_INDEXTYPE index, const OMX_PTR params);
    virtual OMX_ERRORTYPE prepare();
    virtual OMX_ERRORTYPE preroll();
    virtual OMX_ERRORTYPE start();
    virtual OMX_ERRORTYPE pause();
    virtual OMX_ERRORTYPE resume();
    virtual OMX_ERRORTYPE stop();
    virtual OMX_ERRORTYPE release();
    virtual OMX_ERRORTYPE flush();
    void pushBufferLoop();
    static void* threadEntry(void *args);
    static HRESULT pushBufferDone(
        AMP_COMPONENT component,
        AMP_PORT_IO port_Io,
        UINT32 port_idx,
        AMP_BD_ST *bd,
        void *context);
    static HRESULT vdecUserDataCb(
        AMP_COMPONENT component,
        AMP_PORT_IO ePortIo,
        UINT32 uiPortIdx,
        struct AMP_BD_ST *hBD,
        void *context);
    static OMX_ERRORTYPE createAmpVideoDecoder(
        OMX_HANDLETYPE *handle,
        OMX_STRING componentName,
        OMX_PTR appData,
        OMX_CALLBACKTYPE *callBacks);

  protected:
#ifdef VIDEO_DECODE_ONE_FRAME_MODE
    virtual void onPortVideoDecodeMode(
        OMX_U32 enable = 1, OMX_S32 mode = -1);
#endif
    virtual void onPortEnableCompleted(OMX_U32 portIndex, OMX_BOOL enabled);

  private:
    OMX_ERRORTYPE getAmpVideoDecoder();
    OMX_ERRORTYPE destroyAmpVideoDecoder();
    OMX_ERRORTYPE getAmpDisplayService();
    OMX_ERRORTYPE destroyAmpDisplayService();
    OMX_ERRORTYPE getAmpDmxComponent();
    OMX_ERRORTYPE destroyAmpDmxComponent();
    OMX_ERRORTYPE clearAmpDmxPort();
    OMX_ERRORTYPE setDmxChannel(OMX_BOOL on);
    OMX_ERRORTYPE configTvpVersion();
    OMX_BOOL isVideoParamSupport(OMX_VIDEO_PARAM_PORTFORMATTYPE* video_param);
    OMX_BOOL isProfileLevelSupport(OMX_VIDEO_PARAM_PROFILELEVELTYPE* prof_lvl);
    OMX_ERRORTYPE setAmpState(AMP_STATE state);
    OMX_ERRORTYPE getAmpState(AMP_STATE *state);
    OMX_ERRORTYPE registerBds(OmxAmpVideoPort *port);
    OMX_ERRORTYPE unregisterBds(OmxAmpVideoPort *port);
    OMX_ERRORTYPE pushAmpBd(AMP_PORT_IO port, UINT32 portindex, AMP_BD_ST *bd);
    OMX_ERRORTYPE resetPtsForFirstFrame(OMX_BUFFERHEADERTYPE *in_head);
    OMX_ERRORTYPE parseExtraData(OMX_BUFFERHEADERTYPE *in_head, CryptoInfo **pCryptoInfo);
    OMX_ERRORTYPE processPRctrl(OMX_BUFFERHEADERTYPE *in_head,
        CryptoInfo **pCryptoInfo, OMX_U32 padding_size);
    OMX_ERRORTYPE processWVctrl(OMX_BUFFERHEADERTYPE *in_head,
        CryptoInfo **pCryptoInfo, OMX_U32 padding_size);
    OMX_ERRORTYPE pushSecureDataToAmp(OMX_BUFFERHEADERTYPE *in_head, CryptoInfo **pCryptoInfo);
    OMX_ERRORTYPE writeDataToCbuf(OMX_BUFFERHEADERTYPE *in_head, OMX_BOOL isCaching);
    OMX_ERRORTYPE pushCbufDataToAmp();
    OMX_ERRORTYPE DecryptSecureDataToCbuf(OMX_BUFFERHEADERTYPE *in_head, CryptoInfo **pCryptoInfo,
        OMX_U32 cbuf_offset);
    HRESULT processInputDone(AMP_BD_ST *bd);
    HRESULT processOutputDone(AMP_BD_ST *bd);
    void updateAmpBuffer(AmpBuffer *amp_buffer, buffer_handle_t handle);
    OMX_S32 bindPQSource();
    OMX_S32 getVideoPlane();
    HRESULT registerEvent(AMP_EVENT_CODE event);
    HRESULT unRegisterEvent(AMP_EVENT_CODE event);
    static HRESULT eventHandle(HANDLE hListener, AMP_EVENT *pEvent,
      VOID *pUserData);
    void postVideoPresenceNotification(OMX_VIDEO_PRESENCE videoPresence);
    void notifySourceVideoInfoChanged();
    class AVSettingObserver : public marvell::BnAVSettingObserver {
      public:
        AVSettingObserver(OmxAmpVideoDecoder *vdec)
            :mVdec(vdec){
        }
        void OnSettingUpdate(const char* name, const AVSettingValue& value);
      private:
        OmxAmpVideoDecoder *mVdec;
    };

  public:
    OmxAmpVideoPort *mInputPort;
    OmxAmpVideoPort *mOutputPort;
    OMX_U32 mStreamPosition;
    OMX_U32 mInputFrameNum;
    OMX_U32 mOutputFrameNum;
    OMX_U32 mInBDBackNum;
    OMX_U32 mOutBDPushNum;
    TimeStampRecover *mTsRecover;

  private:
    OMX_BOOL mEOS;
    OMX_BOOL mOutputEOS;
    OMX_BOOL mPaused;
    const ProfileLevelType *mProfileLevelPtr;
    OMX_U32 mProfileLevelNum;
    KDThread *mThread;
    KDThreadAttr *mThreadAttr;
    KDThreadCond *mPauseCond;
    KDThreadMutex *mPauseLock;
    KDThreadMutex *mBufferLock;
    OMX_MARKTYPE mMark;
    CodecExtraData *mExtraData;
    SecureCodecExtraData *mSecureExtraData;
    OMX_BOOL mUseNativeBuffers;
    OMX_BOOL mShouldExit;
    OMX_BOOL mInited;
    OMX_U32 mProfile;
    OMX_U32 mLevel;
    HANDLE mListener;
    //PQ Source Control
    sp<SourceControl> mSourceControl;
    int32_t mSourceId;
    int32_t mVideoPlaneId;
    VdecStreamInfo mVdecInfo;
    sp<IAVSettingObserver> mObserver;
    //Stream In Mode
    HANDLE mPool;
    OMX_U32 mPushedBdNum;
    OMX_U32 mReturnedBdNum;
    OMX_U32 mAddedPtsTagNum;
    OMX_BUFFERHEADERTYPE *mCachedhead;
    CryptoInfo *mCryptoInfo;
    OMX_BOOL mMediaHelper;
    AMP_COMPONENT mDMXHandle;
    AMP_DMX_INPUT_ST mInputObj;
    AMP_DMX_CHNL_ST mChnlObj;
    OMX_BOOL mSchemeIdSend;
    OMX_U64 mDataCount;
    OMX_BOOL mIsWMDRM;
    UINT32 mShmHandle; // only used for WMDRM cbuf shm handle
    AdaptivePlaybackStatus mAdaptivePlayback;
    OMX_BOOL mStartPushOutBuf;
    OMX_BOOL mHasRegisterOutBuf;

    TEEC_Context mContext;

    float mSar;
    float mPar;
    float mDar;

    int32_t mAFD;
    OMX_VIDEO_PRESENCE mVideoPresence;

    AMP_DISP mDispHandle;
};

} // namespace berlin
#endif //BERLIN_OMX_AMP_VIDEO_DECODER_H_


