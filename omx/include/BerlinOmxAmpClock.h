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


#ifndef BERLIN_OMX_AMP_CLOCK_H_
#define BERLIN_OMX_AMP_CLOCK_H_


#include <BerlinOmxComponentImpl.h>
#include <BerlinOmxFpsCalculator.h>
#if PLATFORM_SDK_VERSION >= 19
#include <DurationTimers.h>
#else
#include <Timers.h>
#endif

#ifdef _ANDROID_
using namespace android;
#endif

namespace berlin {

class OmxAmpClock : public OmxComponentImpl {
public:
  OmxAmpClock();
  OmxAmpClock(OMX_STRING name);
  virtual ~OmxAmpClock();
  virtual OMX_ERRORTYPE componentDeInit(OMX_HANDLETYPE hComponent);
  virtual OMX_ERRORTYPE getConfig(OMX_INDEXTYPE index, OMX_PTR config);
  virtual OMX_ERRORTYPE setConfig(OMX_INDEXTYPE index, const OMX_PTR config);

  void initVariables();
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

#ifdef OMX_CoreExt_h
  OMX_U64 getFpsTimestamp();
  void setUpFpsCalculator(OMX_U64 startTime);
  void doComputeFps();
  static void * threadEntry(void *args);
#endif
#ifdef CLOCK_AV_SYNC_OPTION
  OMX_ERRORTYPE setAmpClockOption(OMX_INDEXTYPE index);
#endif

  static OMX_ERRORTYPE createComponent(
      OMX_HANDLETYPE* handle,
      OMX_STRING componentName,
      OMX_PTR appData,
      OMX_CALLBACKTYPE* callBacks);

private:
  OMX_ERRORTYPE getCurrentMediaTime(OMX_TICKS *pts);
  OMX_ERRORTYPE setScale(OMX_S32 scale);
  OMX_ERRORTYPE setStartTime(OMX_TICKS startTime);
  OMX_ERRORTYPE setAmpState(AMP_STATE state);
  OmxPortImpl *mVideoPort;
  OmxPortImpl *mAudioPort;
  OMX_S32 mScale;
  OMX_TIME_CLOCKSTATE mClockState;
  OMX_TICKS mStartTime;
  OMX_TICKS mMediaTimeOffset;
  OMX_U32 mWaitMask;
  OMX_TIME_REFCLOCKTYPE mActiveRefClock;
  OMX_TIME_SEEKMODETYPE mSeekMode;
  OMX_BOOL mIsStopping;
#ifdef OMX_CoreExt_h
  OMX_BOOL mShouldExit;
  KDThread *mThread;
  KDThreadAttr *mThreadAttr;
  KDThreadMutex *mFpsTimerLock;
  KDThreadMutex *mFpsLoopLock;
  DurationTimer mFpsTimer;
  OMX_BOOL mFpsTimerStarted;
  BerlinOmxFpsCalculator mInputFpsCalculator;
  BerlinOmxFpsCalculator mDisplayedFpsCalculator;
  BerlinOmxFpsCalculator mDroppedFpsCalculator;
  BerlinOmxFpsCalculator mDelayedFpsCalculator;
#endif
};

} // namespace berlin
#endif // BERLIN_OMX_AMP_CLOCK_H_


