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

#ifndef MMU_BASEOBJ_H
#define MMU_BASEOBJ_H

#include "mmu_thread.h"

namespace mmu {

class MmuBaseObj {
  public:
                          MmuBaseObj(const char* name = NULL);
    virtual               ~MmuBaseObj();

              void        Lock() { obj_mutex_.lock(); }
              void        Unlock() { obj_mutex_.unlock(); }

              mbool       SetOwner(MmuBaseObj* owner);
    inline    MmuBaseObj* GetOwner() { return owner_; }

    const     mchar*      GetName() { return name_; }

  private:
    MmuBaseObj*           owner_;

  protected:
    mchar*                name_;
    MmuMutex              obj_mutex_;
};

}

#endif
