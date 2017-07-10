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


#ifndef MMP_CAPS_H
#define MMP_CAPS_H

#include "mmu_baseobj.h"
#include "mmu_types.h"
#include "mmu_mvalue.h"
#include "mmu_hash.h"

#include <map>
//#include "mmp_structure.h"

using namespace std;
using namespace mmu;

namespace mmp {

class MmpCaps : public MmuBaseObj {
  public:
    explicit MmpCaps() : MmuBaseObj("default_caps") {};
    explicit MmpCaps(const mchar* media_type, const mchar* fieldname, ...);
    ~MmpCaps();

    MmpCaps& operator=(const MmpCaps &caps);

    void SetString(const mchar* field, const mchar* value);
    void SetInt32(const mchar* field, mint32 value);
    void SetData(const mchar* field, const void* value, mint32 size);

    mbool GetString(const mchar* field, const mchar** value);
    mbool GetInt32(const mchar* field, mint32* value);
    mbool GetData(const mchar* field, const void** value, mint32* size);


  private:
    mbool cleanItems();

    map<MmuHash, MValue*> items_;
};

}

#endif
