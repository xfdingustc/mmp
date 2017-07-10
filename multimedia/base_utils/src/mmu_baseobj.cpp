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


#include "mmu_baseobj.h"

namespace mmu {

MmuBaseObj::MmuBaseObj(const char* name)
  : owner_(NULL) {
  if (name) {
    muint32 name_len = strlen(name) + 1;
    name_ = new mchar[name_len];
    strcpy(name_, name);
  }
}


MmuBaseObj::~MmuBaseObj() {
  if (name_) {
    delete name_;
    name_ = NULL;
  }
}

mbool MmuBaseObj::SetOwner(MmuBaseObj* owner) {
  MmuAutoLock lock(obj_mutex_);
  owner_ = owner;
  return true;
}

}
