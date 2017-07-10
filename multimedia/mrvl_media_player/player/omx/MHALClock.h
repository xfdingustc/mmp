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

#ifndef MHAL_CLOCK_H_
#define MHAL_CLOCK_H_

#include "MHALOmxComponent.h"

#include "mmp_clock.h"

namespace mmp {

class MHALClock : public MHALOmxComponent,
                  public MmpClock {
  public:

    MHALClock(MHALOmxCallbackTarget *cb_target);
    virtual ~MHALClock();

    virtual OMX_ERRORTYPE OnOMXEvent(OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                OMX_U32 nData2, OMX_PTR pEventData);

    void stopClock();
    void startClock(MmpClockTime startTimeUs);
    void setScale(mdouble scale);
    uint64_t getCurrentMediaTime();

    MmpClockTime  GetTime() { return getCurrentMediaTime();}

  private:
    virtual const char* getComponentRole();
};

}

#endif
