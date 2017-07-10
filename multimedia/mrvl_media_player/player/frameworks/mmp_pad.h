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

#ifndef MMP_PAD_H
#define MMP_PAD_H




#include "mmu_baseobj.h"
#include "mmu_macros.h"
#include "mmu_types.h"

#include "mmp_buffer.h"
#include "mmp_errors.h"
#include "mmp_caps.h"
#include "mmp_event.h"

using namespace mmu;

namespace mmp {


class MmpPad : public MmuBaseObj {
  public:
    typedef enum {
      MMP_PAD_UNKNOWN,
      MMP_PAD_SRC,
      MMP_PAD_SINK,
    } MmpPadDirection;

    typedef enum {
      MMP_PAD_MODE_NONE,
      MMP_PAD_MODE_PUSH,
      MMP_PAD_MODE_PULL,
    } MmpPadMode;

  public:
                        MmpPad() {}
    explicit            MmpPad(mchar* name, MmpPadDirection dir, MmpPadMode mode);
    virtual             ~MmpPad();

    static    MmpError  Link(MmpPad* src_pad, MmpPad* sink_pad);
              mbool     IsLinked() { return (peer_); };

              mbool     IsSrc() { return (direction_ == MMP_PAD_SRC); }
              mbool     IsSink() { return (direction_ == MMP_PAD_SINK); }

              MmpError  Pull(MmpBuffer* buf, moffset offset, muint32 size) {return MMP_ERROR_IO;}
              MmpError  Push(MmpBuffer* buf);

    virtual   MmpError  ChainFunction(MmpBuffer* buf) { return MMP_NO_ERROR; }
    virtual   MmpError  LinkFunction() { return MMP_NO_ERROR; };

              MmpCaps&  GetCaps() { return caps_; }

              MmpError  PushEvent(MmpEvent* event);

  private:
              MmpError  LinkPrepare(MmpPad* peer);

  protected:
    MmpPad*             peer_;

  private:
    MmpPadDirection     direction_;
    MmpPadMode          mode_;


    MmpCaps             caps_;

};


}

#endif
