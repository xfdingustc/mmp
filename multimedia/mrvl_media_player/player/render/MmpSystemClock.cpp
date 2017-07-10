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

#include "MmpSystemClock.h"
#include <utils/Log.h>

#undef  LOG_TAG
#define LOG_TAG "MmpSystemClock"

namespace mmp {

MmpSystemClock::MmpSystemClock()
  : mStartTimeUs_(0),
    mMediaTimeUs_(0),
    mRate_(0.0) {
}

MmpSystemClock::~MmpSystemClock() {
  ALOGD("MmpSystemClock is destroyed.");
}

MmpClockTime MmpSystemClock::GetTime() {
  struct timeval sys_time;
  gettimeofday(&sys_time, NULL);
  MmpClockTime timpe_past_us = (sys_time.tv_sec - sysTimeWhenStart_.tv_sec) * 1000000
      + (sys_time.tv_usec - sysTimeWhenStart_.tv_usec);
  return mStartTimeUs_ + timpe_past_us * mRate_;
}

void MmpSystemClock::stopClock() {
  mRate_ = 0.0;
}

void MmpSystemClock::startClock(MmpClockTime startTimeUs) {
  gettimeofday(&sysTimeWhenStart_, NULL);
  mStartTimeUs_ = startTimeUs;
}

void MmpSystemClock::setScale(mdouble scale) {
  ALOGD("Set clock rate to %f", scale);
  mRate_ = scale;
}

}
