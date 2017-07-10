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

#define LOG_TAG "BerlinOmxAmpClock"
#include <BerlinOmxAmpClock.h>
#include <BerlinOmxAmpClockPort.h>

#define OMX_ROLE_CLOCK_BINARY "clock.binary"

namespace berlin {

OmxAmpClock::OmxAmpClock() {
  initVariables();
}

OmxAmpClock::OmxAmpClock(OMX_STRING name) {
  strncpy(mName, name, OMX_MAX_STRINGNAME_SIZE);
  initVariables();
}

OmxAmpClock::~OmxAmpClock() {
  if (mAmpHandle) {
    HRESULT err = SUCCESS;
    AMP_RPC(err, AMP_CLK_Destroy, mAmpHandle);
    AMP_FACTORY_Release(mAmpHandle);
    mAmpHandle = NULL;
  }
  OMX_LOGD("destroyed");
}

void OmxAmpClock::initVariables() {
  mClockState = OMX_TIME_ClockStateStopped;
  mScale = 0x10000;
  mMediaTimeOffset = 0;
  mStartTime = 0;
  mWaitMask = 0x3;
  mIsStopping = OMX_FALSE;
#ifdef OMX_CoreExt_h
  mShouldExit = OMX_FALSE;
  mThread = NULL;
  mThreadAttr = NULL;
  mFpsTimerLock = NULL;
  mFpsLoopLock = NULL;
  mFpsTimerStarted = OMX_FALSE;
#endif
}

OMX_ERRORTYPE OmxAmpClock::initRole() {
  if (strncmp(mName, kCompNamePrefix, kCompNamePrefixLength)) {
    return OMX_ErrorInvalidComponentName;
  }
  char *role_name = mName + kCompNamePrefixLength;
  if (!strncmp(role_name, OMX_ROLE_CLOCK_BINARY,
      strlen(OMX_ROLE_CLOCK_BINARY))) {
    addRole(OMX_ROLE_CLOCK_BINARY);
  } else {
    return OMX_ErrorInvalidComponentName;
  }
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpClock::initPort() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  mAudioPort = new OmxAmpClockPort(kClockPortStartNumber, OMX_DirOutput);
  mVideoPort = new OmxAmpClockPort(kClockPortStartNumber + 1, OMX_DirOutput);
  addPort(mAudioPort);
  addPort(mVideoPort);
  return err;
}

OMX_ERRORTYPE OmxAmpClock::getConfig(OMX_INDEXTYPE index, OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOGV("index %s, config %p", OmxIndex2String(index), config);
  switch (index) {
    case OMX_IndexConfigTimeScale: {
      OMX_TIME_CONFIG_SCALETYPE *scale =
          static_cast<OMX_TIME_CONFIG_SCALETYPE *>(config);
      err = CheckOmxHeader(scale);
      if (OMX_ErrorNone != err) {
        break;
      }
      scale->xScale = mScale;
      break;
    }
    case OMX_IndexConfigTimeClockState: {
      OMX_TIME_CONFIG_CLOCKSTATETYPE *state =
        static_cast<OMX_TIME_CONFIG_CLOCKSTATETYPE *>(config);
      err = CheckOmxHeader(state);
      if (OMX_ErrorNone != err) {
        break;
      }
      state->eState = mClockState;
      state->nStartTime = mStartTime;
      state->nOffset = mMediaTimeOffset;
      state->nWaitMask = mWaitMask;
      break;
    }
#ifndef OMX_SPEC_1_2_0_0_SUPPORT
    case OMX_IndexConfigTimeActiveRefClock:
      break;
#endif
    case OMX_IndexConfigTimeCurrentMediaTime: {
      OMX_TIME_CONFIG_TIMESTAMPTYPE *mediaTime =
        static_cast<OMX_TIME_CONFIG_TIMESTAMPTYPE *>(config);
      err = CheckOmxHeader(mediaTime);
      if (OMX_ErrorNone != err) {
        break;
      }
      err = getCurrentMediaTime(&mediaTime->nTimestamp);
      break;
    }
    case OMX_IndexConfigTimeCurrentWallTime:
      break;
    default:
      return OmxComponentImpl::getConfig(index, config);
  }
  return err;
}

OMX_ERRORTYPE OmxAmpClock::setConfig(OMX_INDEXTYPE index, OMX_PTR config) {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  OMX_LOGD("index %s", OmxIndex2String(index));
  switch (index) {
    case OMX_IndexConfigTimeScale: {
      OMX_TIME_CONFIG_SCALETYPE *scale =
          static_cast<OMX_TIME_CONFIG_SCALETYPE *>(config);
      err = CheckOmxHeader(scale);
      if (OMX_ErrorNone != err) {
        break;
      }
      mScale = scale->xScale;
      err = setScale(mScale);
      CHECKAMPERRLOG(err, "set scale");
      break;
    }
    case OMX_IndexConfigTimeClockState: {
      OMX_TIME_CONFIG_CLOCKSTATETYPE *state =
          static_cast<OMX_TIME_CONFIG_CLOCKSTATETYPE *>(config);
      err = CheckOmxHeader(state);
      if (OMX_ErrorNone != err) {
        break;
      }
      mClockState = state->eState;
      mStartTime = state->nStartTime;
      mMediaTimeOffset = state->nOffset;
      // mWaitMask = state->nWaitMask;
      OMX_LOGD("ClockState %s, startTime "TIME_FORMAT" Offset %llu",
          OmxClockState2String(mClockState), TIME_ARGS(mStartTime), mMediaTimeOffset);
      if (mClockState == OMX_TIME_ClockStateRunning) {
        setStartTime(mStartTime);
        setAmpState(AMP_EXECUTING);
      } else if (mClockState == OMX_TIME_ClockStateStopped) {
        setAmpState(AMP_IDLE);
      }
      break;
    }
#ifndef OMX_SPEC_1_2_0_0_SUPPORT
    case OMX_IndexConfigTimeActiveRefClock:
    case OMX_IndexConfigTimeCurrentAudioReference:
    case OMX_IndexConfigTimeCurrentVideoReference:
      break;
#endif
    case OMX_IndexConfigTimeMediaTimeRequest:
    case OMX_IndexConfigTimeClientStartTime:
      break;
#ifdef CLOCK_AV_SYNC_OPTION
    case OMX_IndexExtTimeClockDisableAVSync:
    case OMX_IndexExtTimeClockDisableASync:
    case OMX_IndexExtTimeClockDisableVSync:
    case OMX_IndexExtTimeClockEnableAVSync:
    case OMX_IndexExtTimeClockEnableASync:
    case OMX_IndexExtTimeClockEnableVSync:
      setAmpClockOption(index);
      break;
#endif
    default:
      return OmxComponentImpl::setConfig(index, config);
  }
  return err;
}

#ifdef CLOCK_AV_SYNC_OPTION
OMX_ERRORTYPE OmxAmpClock::setAmpClockOption(OMX_INDEXTYPE index) {
  HRESULT err = SUCCESS;
  if (mAmpHandle) {
    if (index == OMX_IndexExtTimeClockDisableAVSync) {
      AMP_RPC(err, AMP_CLK_SetAVSyncOption, mAmpHandle, AMP_CLK_PORT_IRRELEVANT,
          AMP_CLK_OPT_SYNC_OVERWRITE, 1, AMP_CLK_DISP, 0);
      CHECKAMPERRLOG(err, "disable avsync");
    } else if (index == OMX_IndexExtTimeClockDisableVSync) {
      OMX_LOGE("set config OMX_IndexExtTimeClockDisableVSync");
      AMP_RPC(err, AMP_CLK_SetAVSyncOption, mAmpHandle, 3,
          AMP_CLK_OPT_SYNC_OVERWRITE, 1, AMP_CLK_DISP, 0);
      CHECKAMPERRLOG(err, "disable vsync");
    } else if (index == OMX_IndexExtTimeClockEnableAVSync) {
      AMP_RPC(err, AMP_CLK_SetAVSyncOption, mAmpHandle, AMP_CLK_PORT_IRRELEVANT,
          AMP_CLK_OPT_SYNC_OVERWRITE, 0, AMP_CLK_DISP, 0);
      CHECKAMPERRLOG(err, "enable avsync");
    } else if (index == OMX_IndexExtTimeClockEnableVSync) {
      AMP_RPC(err, AMP_CLK_SetAVSyncOption, mAmpHandle, 3,
          AMP_CLK_OPT_SYNC_OVERWRITE, 0, AMP_CLK_DISP, 0);
      CHECKAMPERRLOG(err, "enable vsync");
    } else {
      OMX_LOGW("clock option %d haven't implemented", index);
    }
  }
  return static_cast<OMX_ERRORTYPE>(err);
}
#endif

OMX_ERRORTYPE OmxAmpClock::getCurrentMediaTime(OMX_TICKS *pts) {
  HRESULT err = SUCCESS;
  AMP_CLK_PTS amp_pts;
  kdMemset(&amp_pts, 0, sizeof(amp_pts));
  if (mAmpHandle) {
    AMP_RPC(err, AMP_CLK_GetSTC, mAmpHandle, &amp_pts);
    CHECKAMPERRLOG(err, "get CLK stc");
  }
  *pts = convert90KtoUs(amp_pts.m_uiHigh, amp_pts.m_uiLow);
  OMX_LOGV("CurrentTime "TIME_FORMAT " mv_pts %x-%x",
      TIME_ARGS(*pts), amp_pts.m_uiHigh, amp_pts.m_uiLow);
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpClock::setScale(OMX_S32 scale) {
  HRESULT err = SUCCESS;
  OMX_LOGD("SetScale 0x%x", scale);
  if (mAmpHandle && !mIsStopping) {
    AMP_RPC(err, AMP_CLK_SetClockRate, mAmpHandle, scale, 0x10000);
    CHECKAMPERRLOG(err, "set clock rate");
  }
// #ifdef OMX_CORE_EXT
#if 0
  if (mAudioPort->isTunneled()) {
    OMX_COMPONENTTYPE *omx_tunnel =
        static_cast<OMX_COMPONENTTYPE *>(mAudioPort->getTunnelComponent());
    if (omx_tunnel) {
      if (scale == 0) {
        OMX_SendCommand(omx_tunnel,
            static_cast<OMX_COMMANDTYPE>(OMX_CommandClockPause), 0, NULL);
      } else {
        OMX_SendCommand(omx_tunnel,
            static_cast<OMX_COMMANDTYPE>(OMX_CommandClockStart), 0, NULL);
      }
    }
  }
#endif
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpClock::setStartTime(OMX_TICKS startTime) {
  HRESULT err = SUCCESS;
  AMP_CLK_PTS pts;
  convertUsto90K(startTime, &pts.m_uiHigh, &pts.m_uiLow);
  if (mAmpHandle && !mIsStopping) {
    AMP_RPC(err, AMP_CLK_SetSTC, mAmpHandle, &pts);
    CHECKAMPERRLOG(err, "set CLK stc");
#ifdef OMX_CoreExt_h
    OMX_U64 currentTime = getFpsTimestamp();
    setUpFpsCalculator(currentTime);
#endif
  }
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpClock::setAmpState(AMP_STATE state) {
  HRESULT err = SUCCESS;
  OMX_LOGD("setAmpState %s", AmpState2String(state));
  if (mAmpHandle && !mIsStopping) {
    AMP_RPC(err, AMP_CLK_SetState, mAmpHandle, state);
  }
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpClock::prepare() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  AMP_FACTORY factory;
  err = AMP_GetFactory(&factory);
  CHECKAMPERRLOG(err, "get AMP factory");

  AMP_RPC(err, AMP_FACTORY_CreateComponent, factory, AMP_COMPONENT_CLK,
      1, &mAmpHandle);
  CHECKAMPERRLOG(err, "get clock compoenent");

  AMP_COMPONENT_CONFIG config;
  kdMemset(&config, 0 ,sizeof(config));
  config._d = AMP_COMPONENT_CLK;
  config._u.pCLK.mode = AMP_TUNNEL;
  config._u.pCLK.uiInputPortNum = 0;
  // To support passthru mode, need config 4 clock ports
  // 3 for audio and 1 for video
  config._u.pCLK.uiOutputPortNum = 4;
  config._u.pCLK.uiNotifierNum = 0;
  config._u.pCLK.eClockSource = AMP_CLK_SRC_VPP;
  config._u.pCLK.eAVSyncPolicy = AMP_CLK_POLICY_OMX;
  AMP_RPC(err, AMP_CLK_Open, mAmpHandle, &config);
  CHECKAMPERRLOG(err, "open CLK compoenent");

  // Disable audio/video port if it is not tunneled
  if (!mAudioPort->isTunneled()) {
    mAudioPort->disablePort();
  } else {
    // Clock always start state transistion first, so comment the below block
    // to leave connection after other components have finished preparation.
    /*
    OMX_COMPONENTTYPE *omx_tunnel =
          static_cast<OMX_COMPONENTTYPE *>(mAudioPort->getTunnelComponent());
    AMP_COMPONENT amp_tunnel = static_cast<OmxComponentImpl *>(
        omx_tunnel->pComponentPrivate)->getAmpHandle();
    if (amp_tunnel) {
      AMP_ConnectComp(mAmpHandle, 1, amp_tunnel, 0);
    }
    */
  }
  if (!mVideoPort->isTunneled()) {
    mVideoPort->disablePort();
  } else {
    /*
    OMX_COMPONENTTYPE *omx_tunnel =
          static_cast<OMX_COMPONENTTYPE *>(mVideoPort->getTunnelComponent());
    AMP_COMPONENT amp_tunnel = static_cast<OmxComponentImpl *>(
        omx_tunnel->pComponentPrivate)->getAmpHandle();
    if (amp_tunnel) {
      AMP_ConnectComp(mAmpHandle, 0, amp_tunnel, 1);
    }
    */
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpClock::release() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (NULL == mAmpHandle) {
    OMX_LOGD("mAmpHandle has been destroyed.");
    return static_cast<OMX_ERRORTYPE>(err);
  }
  // AREN connect and AREN disconnect
#ifdef OMX_CORE_EXT
  if (mAudioPort->isTunneled()) {
    OMX_COMPONENTTYPE *omx_tunnel =
          static_cast<OMX_COMPONENTTYPE *>(mAudioPort->getTunnelComponent());
    if (omx_tunnel) {
      OMX_PTR type = kdMalloc(3 + 1);
      if (type) {
        kdMemset(type, 0x0, 4);
        kdMemcpy(type, "clk", 3);
        OMX_SendCommand(omx_tunnel,
            static_cast<OMX_COMMANDTYPE>(OMX_CommandDisconnectPort), 0, type);
        kdFree(type);
      }
    }
  }
#endif
  if (mVideoPort->isTunneled()) {
    OMX_COMPONENTTYPE *omx_tunnel =
          static_cast<OMX_COMPONENTTYPE *>(mVideoPort->getTunnelComponent());
    if (NULL != omx_tunnel) {
      OmxComponentImpl *omx_vout = static_cast<OmxComponentImpl *>(reinterpret_cast<
          OMX_COMPONENTTYPE *>(omx_tunnel)->pComponentPrivate);
      if (NULL != omx_vout) {
        AMP_COMPONENT amp_vout = omx_vout->getAmpHandle();
        if (NULL != amp_vout) {
        // TODO: omx must make sure Vout's state as idling before Vout disconnects with CLK.
        // But now, omx can't control its state as idling on vout side because Mooplayer may call
        // vout to do resume before calling CLK release. So I just do a workaround there.
          AMP_RPC(err, AMP_VOUT_SetState, amp_vout, AMP_IDLE);
          CHECK_AMP_RETURN_VAL(err,"set Vout's state as idling before disconnecting with CLK");
          if (SUCCESS == err) {
          // Disconnect amp clock port 3 with amp vout port 1
            err = AMP_DisconnectComp(mAmpHandle, 3, amp_vout, 1);
            CHECKAMPERRLOG(err, "disconnect video port with CLK");
          }
        }
      } else {
        OMX_LOGD("omx vout component is already released.");
      }
    } else {
      OMX_LOGD("omx vout handle is already released.");
    }
  }

  AMP_RPC(err, AMP_CLK_Close, mAmpHandle);
  CHECKAMPERRLOG(err, "close CLK");

  AMP_RPC(err, AMP_CLK_Destroy, mAmpHandle);
  CHECKAMPERRLOG(err, "desctroy CLK");

  AMP_FACTORY_Release(mAmpHandle);
  mAmpHandle = NULL;
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpClock::preroll() {
  OMX_ERRORTYPE err = OMX_ErrorNone;
  return err;
}

OMX_ERRORTYPE OmxAmpClock::pause() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (mAmpHandle) {
    AMP_RPC(err, AMP_CLK_SetState, mAmpHandle, AMP_PAUSED);
    CHECKAMPERRLOG(err, "set CLK state to pause");
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpClock::resume() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  if (mAmpHandle) {
    AMP_RPC(err, AMP_CLK_SetState, mAmpHandle, AMP_EXECUTING);
    CHECKAMPERRLOG(err, "set CLK state to executing");
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpClock::start() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
#ifdef OMX_CoreExt_h
  mFpsTimerLock = kdThreadMutexCreate(KD_NULL);
  mFpsLoopLock = kdThreadMutexCreate(KD_NULL);
  if (mFpsTimerLock) {
    kdThreadMutexLock(mFpsTimerLock);
    mFpsTimer.start();
    mFpsTimerStarted = OMX_TRUE;
    kdThreadMutexUnlock(mFpsTimerLock);
  }
#endif
  setStartTime(mStartTime);
  if (mAmpHandle) {
    AMP_RPC(err, AMP_CLK_SetState, mAmpHandle, AMP_EXECUTING);
    CHECKAMPERRLOG(err, "set CLK state to executing");
  }
#ifdef OMX_CoreExt_h
  if (mVideoPort->isTunneled()) {
    OMX_LOGD("Create updating FPS thread");
    mThread = kdThreadCreate(mThreadAttr, threadEntry,(void *)this);
  }
#endif
  mClockState = OMX_TIME_ClockStateRunning;
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpClock::stop() {
  OMX_LOG_FUNCTION_ENTER;
  HRESULT err = SUCCESS;
  mIsStopping = OMX_TRUE;
#ifdef OMX_CoreExt_h
  mShouldExit = OMX_TRUE;
  if (mThread) {
    void *retval;
    KDint ret;
    ret = kdThreadJoin(mThread, &retval);
    OMX_LOGD("Stop updating FPS thread");
    mThread = NULL;
  }
  if (mFpsTimerLock) {
    kdThreadMutexFree(mFpsTimerLock);
    mFpsTimerLock = NULL;
  }
  if (mFpsLoopLock) {
    kdThreadMutexFree(mFpsLoopLock);
    mFpsLoopLock = NULL;
  }
#endif
  if (mAmpHandle) {
    AMP_RPC(err, AMP_CLK_SetState, mAmpHandle, AMP_IDLE);
    CHECKAMPERRLOG(err, "set CLK state to idle");
  }
  OMX_LOG_FUNCTION_EXIT;
  return static_cast<OMX_ERRORTYPE>(err);
}

OMX_ERRORTYPE OmxAmpClock::flush() {
  OMX_LOG_FUNCTION_ENTER;
  OMX_ERRORTYPE err = OMX_ErrorNone;

  OMX_LOG_FUNCTION_EXIT;
  return err;
}

#ifdef OMX_CoreExt_h
OMX_U64 OmxAmpClock::getFpsTimestamp() {
  if (mFpsTimerLock) {
    kdThreadMutexLock(mFpsTimerLock);
    OMX_U64 duration;
    if (mFpsTimerStarted) {
      mFpsTimer.stop();
      duration = mFpsTimer.durationUsecs();
      OMX_LOGV("Fps timestamp %lld", duration);
      kdThreadMutexUnlock(mFpsTimerLock);
      return duration;
    } else {
      kdThreadMutexUnlock(mFpsTimerLock);
      return 0;
    }
  } else {
    return 0;
  }
}

void OmxAmpClock::setUpFpsCalculator(OMX_U64 startTime) {
  kdThreadMutexLock(mFpsLoopLock);
  OMX_LOGD("setUpFpsCalculator startTime "TIME_FORMAT, TIME_ARGS(startTime));
  mInputFpsCalculator.start(startTime);
  mDisplayedFpsCalculator.start(startTime);
  mDroppedFpsCalculator.start(startTime);
  mDelayedFpsCalculator.start(startTime);
  kdThreadMutexUnlock(mFpsLoopLock);
}

void OmxAmpClock::doComputeFps() {
  HRESULT err = SUCCESS;
  while (!mShouldExit) {
    kdThreadMutexLock(mFpsLoopLock);
    if (mAmpHandle) {
      AMP_RND_STAT rendererStatistics;
      AMP_RPC(err, AMP_CLK_GetRndStatistics, mAmpHandle, 3, &rendererStatistics);
      if (SUCCESS != err) {
        OMX_LOGD("Get RenderStatistics err %d", err);
        kdThreadMutexUnlock(mFpsLoopLock);
        usleep(100000);
        continue;
      }
      OMX_LOGV("RenderStatus: Input=%d Ready=%d Display=%d Drop=%d Delay=%d Repeat=%d",
          rendererStatistics.uiNumVidInputFrames, rendererStatistics.uiNumVidReadyFrames,
          rendererStatistics.uiNumVidDisplayedFrames, rendererStatistics.uiNumVidDroppedFrames,
          rendererStatistics.uiNumVidDelayedFrames, rendererStatistics.uiNumVidRepeatedFrames);
      mInputFpsCalculator.setTotalFrameCount(rendererStatistics.uiNumVidInputFrames);
      mDisplayedFpsCalculator.setTotalFrameCount(rendererStatistics.uiNumVidDisplayedFrames);
      mDroppedFpsCalculator.setTotalFrameCount(rendererStatistics.uiNumVidDroppedFrames);
      mDelayedFpsCalculator.setTotalFrameCount(rendererStatistics.uiNumVidDelayedFrames);
    }
    OMX_U64 currentTime = getFpsTimestamp();
    OMX_BOOL isFpsUpdated = mInputFpsCalculator.checkCurrentTimeAndRefreshFps(currentTime);
    CHECK(isFpsUpdated == mDisplayedFpsCalculator.checkCurrentTimeAndRefreshFps(currentTime));
    CHECK(isFpsUpdated == mDroppedFpsCalculator.checkCurrentTimeAndRefreshFps(currentTime));
    CHECK(isFpsUpdated == mDelayedFpsCalculator.checkCurrentTimeAndRefreshFps(currentTime));
    if (isFpsUpdated) {
      OMX_U32 inputFrames = mDisplayedFpsCalculator.getLatestFps() +
          mDroppedFpsCalculator.getLatestFps();
      postEvent(static_cast<OMX_EVENTTYPE>(OMX_EventRendererFpsInfo),
          (inputFrames << 16) | mDisplayedFpsCalculator.getLatestFps(),
          (mDroppedFpsCalculator.getLatestFps() << 16)
          | mDelayedFpsCalculator.getLatestFps());
      OMX_LOGD("Update Fps: Input %d, Displayed %d, Dropped %d, Delayed %d",
          inputFrames, mDisplayedFpsCalculator.getLatestFps(),
          mDroppedFpsCalculator.getLatestFps(), mDelayedFpsCalculator.getLatestFps());
    }
    kdThreadMutexUnlock(mFpsLoopLock);
    usleep(100000);
  }
}

void * OmxAmpClock::threadEntry(void * args) {
  OmxAmpClock *clock = (OmxAmpClock *)args;
  prctl(PR_SET_NAME, "OmxComputeFps", 0, 0, 0);
  clock->doComputeFps();
  return NULL;
}
#endif

OMX_ERRORTYPE OmxAmpClock::componentDeInit(OMX_HANDLETYPE hComponent) {
  OmxComponent *comp = reinterpret_cast<OmxComponent *>(
      reinterpret_cast<OMX_COMPONENTTYPE *>(hComponent)->pComponentPrivate);
  OmxAmpClock *clock = static_cast<OmxAmpClock *>(comp);
  delete clock;
  return OMX_ErrorNone;
}

OMX_ERRORTYPE OmxAmpClock::createComponent(
    OMX_HANDLETYPE* handle,
    OMX_STRING componentName,
    OMX_PTR appData,
    OMX_CALLBACKTYPE* callBacks) {
  OmxAmpClock* comp = new OmxAmpClock(componentName);
  *handle = comp->makeComponent(comp);
  comp->setCallbacks(callBacks, appData);
  comp->componentInit();
  return OMX_ErrorNone;
}

extern "C" {
  OMX_API OMX_ERRORTYPE CreateComponentEntry(
      OMX_HANDLETYPE *handle,
      OMX_STRING componentName,
      OMX_PTR appData,
      OMX_CALLBACKTYPE *callBacks) {
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_LOGD("Omx Create Component %s", componentName);
    if (!strncmp(componentName, kCompNamePrefix, kCompNamePrefixLength)) {
      if (NULL != strstr(componentName, OMX_ROLE_CLOCK_BINARY)) {
        err = OmxAmpClock::createComponent(handle, componentName, appData,
            callBacks);
        return err;
      } else {
        OMX_LOGE("Amp component %s is not found!!!", componentName);
        err = OMX_ErrorInvalidComponentName;
        return err;
      }
    } else {
      OMX_LOGE("%s is not the Marvell Component!!!", componentName);
      err = OMX_ErrorInvalidComponentName;
      return err;
    }
  }
}

}  // namespace berlin
