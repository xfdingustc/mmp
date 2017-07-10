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

#ifndef MMP_CLOCK_H
#define MMP_CLOCK_H

#include "mmu_types.h"
using namespace mmu;

namespace mmp {

typedef muint64  MmpClockTime;

typedef mint64  MmpClockTimeDiff;

#define MMP_CLOCK_TIME_NONE       ((MmpClockTime) -1)

#define MMP_CLOCK_TIME_IS_VALID(time)   (((MmpClockTime)(time)) != MMP_CLOCK_TIME_NONE)

class MmpClock {
  public:
    virtual                 ~MmpClock() {}
    virtual   MmpClockTime  GetTime() = 0;
    virtual   void  stopClock() = 0;
    virtual   void  startClock(MmpClockTime startTimeUs) = 0;
    virtual   void  setScale(mdouble scale) = 0;
};

}

#endif
