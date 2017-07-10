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

#define LOG_TAG "BerlinOmxFpsCalculator"
#include <BerlinOmxLog.h>
#include <BerlinOmxFpsCalculator.h>

namespace berlin {

static const OMX_U64 ONE_SECOND_IN_MICRO_SECONDS = 1000000;
static const OMX_U32 INVALID_COUNT = 0xFFFFFFFF;

BerlinOmxFpsCalculator::BerlinOmxFpsCalculator()
    : mStarted(OMX_FALSE) {
  OMX_LOGV("constructor");
  mLock = kdThreadMutexCreate(KD_NULL);
}

BerlinOmxFpsCalculator::~BerlinOmxFpsCalculator() {
  OMX_LOGV("destructor");
  mStarted = OMX_FALSE;
  if (mLock) {
    kdThreadMutexFree(mLock);
    mLock = NULL;
  }
}

void BerlinOmxFpsCalculator::start(OMX_U64 timeUs) {
  OMX_LOGV("start %lld", timeUs);
  mTimeUsOfLatestFpsRefresh = timeUs;
  mPrevFrameCount = INVALID_COUNT;
  mLatestFps = 0;
  mStarted = OMX_TRUE;

  kdThreadMutexLock(mLock);
  mFrameCount = 0;
  kdThreadMutexUnlock(mLock);
}

OMX_BOOL BerlinOmxFpsCalculator::checkCurrentTimeAndRefreshFps(OMX_U64 curTimeUs) {
  if (!mStarted || INVALID_COUNT == mPrevFrameCount) {
    return OMX_FALSE;
  }

  OMX_U64 durationUs = curTimeUs - mTimeUsOfLatestFpsRefresh;
  OMX_LOGV("refreshFps %lld %lld", curTimeUs, durationUs);
  if (durationUs < ONE_SECOND_IN_MICRO_SECONDS) {
    return OMX_FALSE;
  }

  OMX_U32 framesCount = getFrameCount();
  OMX_U32 latestFramesCount = framesCount - mPrevFrameCount;
  mPrevFrameCount = framesCount;
  if (durationUs > ONE_SECOND_IN_MICRO_SECONDS * 11 / 10) {
    // If duration is larger than 1.1 sec, prints a warning message.
    OMX_LOGD("FPS refresh time %lld is too long.", durationUs);
  }
  mTimeUsOfLatestFpsRefresh = curTimeUs;
  mLatestFps = computeFps(latestFramesCount, durationUs);

  return OMX_TRUE;
}

void BerlinOmxFpsCalculator::incrementFrameCount() {
  OMX_LOGV("increaseDisplayedFrames");
  kdThreadMutexLock(mLock);
  if (INVALID_COUNT == mPrevFrameCount) {
    mPrevFrameCount = mFrameCount;
  }
  ++mFrameCount;
  kdThreadMutexUnlock(mLock);
}

void BerlinOmxFpsCalculator::setTotalFrameCount(OMX_U32 totalFrameCount) {
  kdThreadMutexLock(mLock);
  if (mFrameCount > totalFrameCount) {
    OMX_LOGD("The updated value %d should be equal to or larger than the previous values %d.",
        mFrameCount, totalFrameCount);
    // Frame counter should reset from this point.
    mPrevFrameCount = totalFrameCount;
  }
  mFrameCount = totalFrameCount;
  if (INVALID_COUNT == mPrevFrameCount) {
    mPrevFrameCount = mFrameCount;
  }
  kdThreadMutexUnlock(mLock);
}

OMX_U32 BerlinOmxFpsCalculator::getLatestFps() {
  OMX_LOGV("getLatestFps %d", mLatestFps);
  return mLatestFps;
}

OMX_U32 BerlinOmxFpsCalculator::computeFps(OMX_U64 frames, OMX_U64 durationUs) {
  // TODO(youngsang): this function needs unit tests.
  // 0.5 is added in order to round FPS.
  return static_cast<OMX_U32>((static_cast<double>(frames) * ONE_SECOND_IN_MICRO_SECONDS /
      durationUs) + 0.5);
}

OMX_U64 BerlinOmxFpsCalculator::getFrameCount() {
  OMX_LOGV("getRepeatedFrames");
  kdThreadMutexLock(mLock);
  OMX_U64 count = mFrameCount;
  kdThreadMutexUnlock(mLock);
  return count;
}

}  // namespace Berlin
