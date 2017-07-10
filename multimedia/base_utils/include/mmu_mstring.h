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

#ifndef MMU_STRING_H
#define MMU_STRING_H

#include "mmu_types.h"

namespace mmu {

class MString {
  public:
    MString();
    MString(const mchar* str);
    MString(const mchar* str, muint32 size);
    ~MString();

    void Clear();
    void Trim();
    void Erase(muint32 start, muint32 n);


    void SetTo(const mchar *s);
    void SetTo(const mchar* s, muint32 size);

    muint32 GetSize() { return size_; }
    const mchar* GetString() { return data_; }

    mbool Empty() { return size_ == 0;}

    void Append(mchar c) { Append(&c, 1); };
    void Append(const mchar *s);
    void Append(const mchar* s, muint32 size);

  private:
    void makeMutable();
  private:
    mchar*    data_;
    muint32   size_;
    muint32   alloc_size_;
};

}
#endif
