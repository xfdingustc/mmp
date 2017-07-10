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


#ifndef BERLIN_OMX_AUDIO_RENDERER_H_
#define BERLIN_OMX_AUDIO_RENDERER_H_

#include <BerlinOmxComponentImpl.h>
#include <BerlinOmxAudioPort.h>
#include <BerlinOmxAmpAudioPort.h>
#include <BerlinOmxAmpClockPort.h>

#ifdef _ANDROID_
#include <IAVSettings.h>
#include <OutputControl.h>
#include <SourceControl.h>
#include <utils/KeyedVector.h>
#endif

#ifdef _ANDROID_
using android::sp;
using marvell::AVSettingValue;
using marvell::IAVSettingObserver;
using marvell::IAVSettings;
using marvell::OutputControl;
using marvell::SourceControl;
#endif


namespace berlin {

#define AREN_PASSTHRU_OUTPUT_PORT_NUM 3
class OmxAmpAudioRenderer : public OmxComponentImpl {
  public:
    OmxAmpAudioRenderer();
    OmxAmpAudioRenderer(OMX_STRING name);
    virtual ~OmxAmpAudioRenderer();

    virtual OMX_ERRORTYPE getParameter(
        OMX_INDEXTYPE index, OMX_PTR params);
    virtual OMX_ERRORTYPE setParameter(
        OMX_INDEXTYPE index, const OMX_PTR params);

    virtual OMX_ERRORTYPE getConfig(
        OMX_INDEXTYPE index, OMX_PTR config);

    virtual OMX_ERRORTYPE setConfig(
        OMX_INDEXTYPE index, const OMX_PTR config);

    virtual OMX_ERRORTYPE componentDeInit(OMX_HANDLETYPE hComponent);

    virtual OMX_ERRORTYPE initRole();
    virtual OMX_ERRORTYPE initPort();
    virtual OMX_ERRORTYPE prepare();
    virtual OMX_ERRORTYPE preroll();
    virtual OMX_ERRORTYPE start();
    virtual OMX_ERRORTYPE pause();
    virtual OMX_ERRORTYPE resume();
    virtual OMX_ERRORTYPE stop();
    virtual OMX_ERRORTYPE release();
    virtual OMX_ERRORTYPE flush();

    OMX_ERRORTYPE pushAmpBd(AMP_PORT_IO port, UINT32 portindex, AMP_BD_ST *bd);
    OMX_ERRORTYPE registerBds(OmxAmpAudioPort *port);
    OMX_ERRORTYPE unregisterBds(OmxAmpAudioPort *port);

    void pushBufferLoop();
    UINT32 pushCbufBd(OMX_BUFFERHEADERTYPE *in_head);
    UINT32 requestCbufBd();
    void setPassThruMode();
    static void* threadEntry(void *args);

    static OMX_ERRORTYPE createComponent(
        OMX_HANDLETYPE *handle,
        OMX_STRING componentName,
        OMX_PTR appData,
        OMX_CALLBACKTYPE *callBacks);

    static HRESULT pushBufferDone(
        AMP_COMPONENT component,
        AMP_PORT_IO port_Io,
        UINT32 port_idx,
        AMP_BD_ST *bd,
        void *context);

    OmxPortImpl *mInputPort;
    OmxPortImpl *mClockPort;

  protected:
    virtual OMX_BOOL onPortDisconnect(OMX_BOOL disconnect, OMX_PTR type);
    virtual void onPortClockStart(OMX_BOOL start);

  private:
    // Define class that will receive the avsetting change notifications.
    class AVSettingObserver : public marvell::BnAVSettingObserver {
      public:
        AVSettingObserver(OmxAmpAudioRenderer& audiorenderer)
            :mAudioRenderer(audiorenderer){
        }
        void OnSettingUpdate(const char* name, const AVSettingValue& value);

      private:
        OmxAmpAudioRenderer& mAudioRenderer;
    };

    OMX_ERRORTYPE setAmpState(AMP_STATE state);
    OMX_ERRORTYPE getAmpState(AMP_STATE *state);
    OMX_ERRORTYPE connectSoundService();
    OMX_ERRORTYPE connectADecAudioPort(OMX_BOOL connect);
    OMX_ERRORTYPE connectClockPort(OMX_BOOL connect);
    HRESULT registerEvent(AMP_EVENT_CODE event);
    HRESULT unRegisterEvent(AMP_EVENT_CODE event);
    static HRESULT eventHandle(HANDLE hListener, AMP_EVENT *pEvent, VOID *pUserData);
    void initAQ();
    OMX_S32 getAudioChannel();
    OMX_ERRORTYPE setMixGain();

    AMP_FACTORY mFactory;
    HANDLE mSndSrvTunnel[AREN_PASSTHRU_OUTPUT_PORT_NUM];
    HANDLE mListener;

    KDThread *mThread;
    KDThreadAttr *mThreadAttr;
    OMX_BOOL mShouldExit;
    OMX_BOOL mPaused;
    KDThreadCond *mPauseCond;
    KDThreadMutex *mPauseLock;
    KDThreadMutex *mBufferLock;

    OMX_S32 mNumChannels;
    OMX_S32 mSamplingRate;
    OMX_S32 mNumData;
    OMX_S32 mBitPerSample;
    OMX_S32 mEndian;
    OMX_BS32 mVolume;
    OMX_BOOL mAudioMute;

    OMX_U32 mStreamPosition;
    OMX_U32 mInputFrameNum;
    OMX_U32 mInBDBackNum;
    OMX_BOOL mPassthru;

    //AQ Source Control
    sp<SourceControl> mSourceControl;
    sp<OutputControl> mOutputControl;
    sp<IAVSettingObserver> mObserver;
    int32_t mSourceId;
    int32_t mAudioChannel;
    int32_t mPhySourceGain;
    OMX_BOOL mIsSnsConnect;

    HANDLE mPool;
    OMX_U32 mPushedBdNum;
    OMX_U32 mReturnedBdNum;
    OMX_BUFFERHEADERTYPE *mCachedhead;
    OMX_BOOL mEOS;
    OMX_BOOL mMediaHelper;

    OMX_BOOL mArenAdecConnect;
    OMX_BOOL mArenClkConnect;
};

}

#endif
