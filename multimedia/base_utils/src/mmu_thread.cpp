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


#include "mmu_thread.h"

namespace mmu {

MmuCondition::MmuCondition() {
  pthread_cond_init(&condition_, NULL);
}

MmuCondition::~MmuCondition() {
  pthread_cond_destroy(&condition_);
}

mint32 MmuCondition::wait(MmuMutex& mutex) {
  return -pthread_cond_wait(&condition_, &mutex.gnu_mutex_);
}

mint32  MmuCondition::waitrelative(MmuMutex& mutex, muint32 reltime_ns) {
  struct timeval t;
  struct timespec ts;
  gettimeofday(&t, NULL);
  ts.tv_sec = t.tv_sec;
  ts.tv_nsec= t.tv_usec * 1000;
  ts.tv_sec += reltime_ns / 1000000000;
  ts.tv_nsec+= reltime_ns % 1000000000;
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_nsec -= 1000000000;
    ts.tv_sec  += 1;
  }
  return -pthread_cond_timedwait(&condition_, &mutex.gnu_mutex_, &ts);
}


void MmuCondition::signal() {
  pthread_cond_signal(&condition_);
}

void MmuCondition::broadcast() {
  pthread_cond_broadcast(&condition_);
}




MmuTask::MmuTask()
  : thread_id_(0),
    exit_pending_(false) {
}

MmuTask::~MmuTask() {
}

void* MmuTask::_ThreadLoop(void * user) {
  MmuTask* const self = static_cast<MmuTask*>(user);
  do {
    mbool result;
    if (!self->ExitPending()) {
      result = self->ThreadLoop();
    }

    {
      MmuAutoLock lock(self->lock_);
      if (result == false || self->exit_pending_) {
        break;
      }
    }
  } while(!self->exit_pending_);
  return NULL;
}

mbool MmuTask::Run() {
  MmuAutoLock lock(lock_);

  if (thread_id_ > 0) {
    return true;
  }

  mint32 result = pthread_create(&thread_id_, NULL, _ThreadLoop, this);

  if (result < 0) {
     thread_id_ = 0;
  }

  return true;
}

void MmuTask::RequestExit() {
  MmuAutoLock lock(lock_);
  exit_pending_ = true;
}


mbool MmuTask::Join() {

  if (thread_id_ > 0) {
    pthread_join(thread_id_, NULL);
    thread_id_ = 0;
  }

  return true;
}

mbool MmuTask::ExitPending() {
  MmuAutoLock lock(lock_);
  return exit_pending_;
}


}
