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

#ifndef MMP_SYSTEM_CLOCK_H_
#define MMP_SYSTEM_CLOCK_H_

#include "frameworks/mmp_frameworks.h"

namespace mmp {

class MmpSystemClock : public MmpClock {
  public:
    MmpSystemClock();
    virtual ~MmpSystemClock();

    virtual MmpClockTime GetTime();
    virtual void stopClock();
    virtual void startClock(MmpClockTime startTimeUs);
    virtual void setScale(mdouble scale);

  private:
    MmpClockTime mStartTimeUs_;
    MmpClockTime mMediaTimeUs_;
    mdouble mRate_;
    struct timeval sysTimeWhenStart_;
};

}

#endif
