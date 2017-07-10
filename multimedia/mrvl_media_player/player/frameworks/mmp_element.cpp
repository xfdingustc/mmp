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


#include "mmp_element.h"
#include "mmp_info.h"

namespace mmp {
MmpElement::MmpElement(const char* name)
  : MmuBaseObj(name),
    clock_(NULL) {
}

MmpElement::~MmpElement() {
  while (!src_pads_.empty()) {
    MmpPad *pad = *(src_pads_.begin());
    if (pad) {
      delete pad;
      pad = NULL;
    }
    src_pads_.erase(src_pads_.begin());
    MMP_DEBUG_OBJECT("one src pad is deleted, current total src pad numbers %d",
                     src_pads_.size());
  }

  while (!sink_pads_.empty()) {
    MmpPad *pad = *(sink_pads_.begin());
    if (pad) {
      delete pad;
      pad = NULL;
    }
    sink_pads_.erase(sink_pads_.begin());
    MMP_DEBUG_OBJECT("one sink pad is deleted, current total sink pad numbers %d",
                     sink_pads_.size());
  }
}

mbool MmpElement::AddPad(MmpPad* pad) {
  M_ASSERT_FATAL(pad, false);

  MmuAutoLock lock(obj_mutex_);

  pad->SetOwner(this);

  if (pad->IsSrc()) {
    src_pads_.push_back(pad);
    MMP_DEBUG_OBJECT("one src pad is added, current total src pad numbers %d",
                     src_pads_.size());
  } else if (pad->IsSink()) {
    sink_pads_.push_back(pad);
    MMP_DEBUG_OBJECT("one sink pad is added, current total sink pad numbers %d",
                     sink_pads_.size());
  }

  return true;
}

MmpPad* MmpElement::GetSrcPadByPrefix(const mchar* prefix, muint32 index) {
  MmuAutoLock lock(obj_mutex_);
  muint32 pad_index = 0;

  vector<MmpPad*>::iterator it;

  for (it = src_pads_.begin(); it != src_pads_.end(); it++) {
    MmpPad* pad = *it;
    if (strlen(pad->GetName()) < strlen(prefix)) {
      continue;
    }

    if (!strncmp(pad->GetName(), prefix, strlen(prefix)-1)) {
      if (pad_index++ == index) {
        return pad;
      }
    }
  }
  return NULL;
}

MmpPad* MmpElement::GetSinkPadByPrefix(const char* prefix, muint32 index) {
  MmuAutoLock lock(obj_mutex_);
  muint32 pad_index = 0;
  vector<MmpPad*>::iterator it;
  for (it = sink_pads_.begin(); it != sink_pads_.end(); it++) {
    MmpPad* pad = *it;
    if (!strncmp(pad->GetName(), prefix, strlen(prefix))) {
      if (pad_index++ == index) {
        return pad;
      }
    }
  }
  return NULL;
}

MmpPad* MmpElement::GetSrcPadByName(const char* name) {
  MmuAutoLock lock(obj_mutex_);
  muint32 pad_index = 0;
  vector<MmpPad*>::iterator it;
  for (it = sink_pads_.begin(); it != sink_pads_.end(); it++) {
    MmpPad* pad = *it;
    if (!strcmp(pad->GetName(), name)) {
      return pad;
    }
  }
  return NULL;
}

MmpPad* MmpElement::GetSinkPadByName(const char* name) {
  MmuAutoLock lock(obj_mutex_);
  muint32 pad_index = 0;
  vector<MmpPad*>::iterator it;
  for (it = sink_pads_.begin(); it != sink_pads_.end(); it++) {
    MmpPad* pad = *it;
    if (!strcmp(pad->GetName(), name)) {
      return pad;
    }
  }
  return NULL;
}


}
