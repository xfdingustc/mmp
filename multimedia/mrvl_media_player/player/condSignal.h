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

#ifndef MRVL_CONDITION_SIGNAL_H_
#define MRVL_CONDITION_SIGNAL_H_

#include <KD/kdplatform.h>
#include <KD/kd.h>

namespace mmp {

class kdCondSignal {
public:
  kdCondSignal() {
    mLock = kdThreadMutexCreate(KD_NULL);
    mCond = kdThreadCondCreate(KD_NULL);
    mSignled = KD_FALSE;
  }

  ~kdCondSignal() {
    kdThreadMutexFree(mLock);
    kdThreadCondFree(mCond);
  }

  KDint waitSignal() {
    kdThreadMutexLock(mLock);
    if (mSignled) {
      return kdThreadMutexUnlock(mLock);
    } else {
      kdThreadCondWait(mCond, mLock);
      return kdThreadMutexUnlock(mLock);
    }
  }

  KDint setSignal() {
    kdThreadMutexLock(mLock);
    kdThreadCondSignal(mCond);
    mSignled = KD_TRUE;
    return kdThreadMutexUnlock(mLock);
  }

  KDint resetSignal() {
    kdThreadMutexLock(mLock);
    mSignled = KD_FALSE;
    return kdThreadMutexUnlock(mLock);
  }

private:
  KDThreadMutex *mLock;
  KDThreadCond *mCond;
  KDboolean mSignled;
};

}

#endif
