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


#ifndef MMU_THREAD_H
#define MMU_THREAD_H

#include "mmu_macros.h"
#include "mmu_types.h"

#include "pthread.h"

namespace mmu {

class MmuCondition;

class MmuMutex {
  public:
    inline              MmuMutex() { pthread_mutex_init(&gnu_mutex_, NULL); }
    inline              ~MmuMutex() { pthread_mutex_destroy(&gnu_mutex_); }

    inline    mbool     lock() {
                          if (M_UNLIKELY(-pthread_mutex_lock(&gnu_mutex_))) {
                            return false;
                          } else {
                            return true;
                          }
                        }
              void      unlock() { pthread_mutex_unlock(&gnu_mutex_); }

              mbool     trylock() {
                          if (M_UNLIKELY(-pthread_mutex_trylock(&gnu_mutex_))) {
                            return false;
                          } else {
                            return true;
                          }
                        }

  private:
    friend class MmuCondition;
    pthread_mutex_t     gnu_mutex_;

};


class MmuAutoLock {
  public:
    inline              MmuAutoLock(MmuMutex& mutex) : mmu_mutex_(mutex) { mmu_mutex_.lock(); }
    inline              MmuAutoLock(MmuMutex* mutex) : mmu_mutex_(*mutex) { mmu_mutex_.lock(); }
    inline              ~MmuAutoLock() { mmu_mutex_.unlock(); }
  private:
    MmuMutex&           mmu_mutex_;
};

class MmuCondition {
  public:
                        MmuCondition();
                        ~MmuCondition();
              mint32    wait(MmuMutex& mutex);
              mint32    waitrelative(MmuMutex& mutex, muint32 reltime_ns);
              void      signal();
              void      broadcast();
  private:
    pthread_cond_t      condition_;
};

typedef void  (*MmuTaskFunction)  (mpointer user_data);

class MmuTask {
  public:
                        MmuTask();
    virtual             ~MmuTask();

              mbool     Run();
              void      RequestExit();
              mbool     Join();

  protected:
              mbool     ExitPending() ;

  private:
    static    void*     _ThreadLoop(void* user);

  private:
    MmuTaskFunction     func_;
    mpointer            user_data_;
    MmuDestroyNotify    notify_;
    pthread_t           thread_id_;

    MmuMutex            lock_;
    volatile  mbool     exit_pending_;

    virtual   mbool     ThreadLoop() = 0;


};


}

#endif
