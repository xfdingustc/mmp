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

#ifndef ANDROID_THREADUTILS_H
#define ANDROID_THREADUTILS_H

namespace android {

class PipeEvent {
 public:
   PipeEvent();
  ~PipeEvent();

  bool successfulInit() const {
    return ((pipe_[0] >= 0) && (pipe_[1] >= 0));
  }

  int getWakeupHandle() const { return pipe_[0]; }

  // Block until the event fires; returns true if the event fired and false if
  // the wait timed out.  Timeout is expressed in milliseconds; negative values
  // mean wait forever.
  bool wait(int timeout = -1);

  void clearPendingEvents();
  void setEvent();

 private:
  int pipe_[2];
};

}  // namespace android

#endif  // ANDROID_THREADUTILS_H
