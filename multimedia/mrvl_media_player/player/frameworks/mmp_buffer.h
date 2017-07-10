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

#ifndef MARVELL_ESPACKET_H
#define MARVELL_ESPACKET_H
#include <stdio.h>

#include "MmpMediaDefs.h"

#include "mmu_types.h"

using namespace mmu;
namespace mmp {

const uint64_t kInvalidTimeStamp = 0xFFFFFFFFFFFFFFFFLL;

class MmpBuffer {
  public:
    explicit  MmpBuffer(muint32 realloc_size = 0);
    explicit  MmpBuffer(muint8 *buf)
      : data(buf),
        data_offset(0),
        size(0),
        index(0),
        pts(kInvalidTimeStamp),
        end_pts(kInvalidTimeStamp),
        flags_(MMP_BUFFER_FLAG_NONE){}
    ~MmpBuffer();

    void Clear();

    typedef   enum {
      MMP_BUFFER_FLAG_NONE        = 0,
      MMP_BUFFER_FLAG_KEY_FRAME   = (1 << 0),
      MMP_BUFFER_FLAG_EOS         = (1 << 1),
    } MmpBufferFlags;

    inline    void      SetFlag(MmpBufferFlags flag) { flags_ |= flag; };
    inline    void      UnsetFlag(MmpBufferFlags flag) { flags_ &= ~flag; };
    inline    mbool     IsFlagSet(MmpBufferFlags flag) { return (flags_ & flag); };
    inline    muint32   GetFlag() { return flags_; };

  public:

    muint8*             data;
    muint32             data_offset;
    mint32              size;
    muint32             index;
    TimeStamp           pts;
    TimeStamp           end_pts;

  private:
    muint32             flags_;

};





}
#endif
