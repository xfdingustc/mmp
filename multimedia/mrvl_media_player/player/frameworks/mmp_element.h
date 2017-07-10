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

#ifndef MMP_ELEMENT_H
#define MMP_ELEMENT_H

#include "mmp_clock.h"
#include "mmp_event.h"
#include "mmp_pad.h"
#include <vector>

using namespace mmu;

namespace mmp {

class MmpElement : public MmuBaseObj {
  public:
    explicit            MmpElement(const char* name = NULL);
    virtual             ~MmpElement();

              mbool     AddPad(MmpPad* pad);

    inline    muint32   GetSrcPadCount() { return src_pads_.size(); }
    inline    muint32   GetSinkPadCount() { return sink_pads_.size(); }

    inline    MmpPad*   GetSrcPad(muint32 index = 0) { return src_pads_[index]; }
    inline    MmpPad*   GetSinkPad(muint32 index = 0) { return sink_pads_[index]; }

              MmpPad*   GetSrcPadByPrefix(const char* prefix, muint32 index = 0);
              MmpPad*   GetSinkPadByPrefix(const char* prefix, muint32 index = 0);

              MmpPad*   GetSrcPadByName(const char* name);
              MmpPad*   GetSinkPadByName(const char* name);

    inline    void      SetClock(MmpClock* clock) { clock_ = clock; }

    virtual   mbool     SendEvent(MmpEvent* event) { return true; }

  protected:
              MmpClock* GetClock() { return clock_; }

  private:
    vector<MmpPad*>     src_pads_;
    vector<MmpPad*>     sink_pads_;

  protected:
    MmpClock*           clock_;

};

}
#endif
