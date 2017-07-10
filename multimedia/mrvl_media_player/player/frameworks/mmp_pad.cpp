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

#include "mmp_pad.h"
#include "mmp_info.h"
#include "mmp_element.h"


namespace mmp {

MmpPad::MmpPad(mchar* name,  MmpPadDirection dir, MmpPadMode mode)
  : MmuBaseObj(name),
    direction_(dir),
    mode_(mode),
    peer_(NULL) {
}

MmpPad::~MmpPad() {
}

/* static */
MmpError MmpPad::Link(MmpPad* src_pad, MmpPad* sink_pad) {
  MmpError result;


  M_ASSERT_FATAL(src_pad, MMP_PAD_LINK_REFUSED);
  M_ASSERT_FATAL(sink_pad, MMP_PAD_LINK_REFUSED);

  M_ASSERT_FATAL(src_pad->IsSrc(),  MMP_PAD_LINK_WRONG_DIRECTION);
  M_ASSERT_FATAL(sink_pad->IsSink(), MMP_PAD_LINK_WRONG_DIRECTION);

  // Set peers before calling the link function
  {
    MmuAutoLock lock(src_pad->obj_mutex_);
    src_pad->peer_ = sink_pad;
  }
  {
    MmuAutoLock lock(sink_pad->obj_mutex_);
    sink_pad->peer_ = src_pad;
  }

  src_pad->LinkFunction();
  sink_pad->LinkFunction();

  return MMP_NO_ERROR;

done:
  return result;

}

MmpError MmpPad::LinkPrepare(MmpPad* peer) {
  // TODO: We need to lock the pad lock here

  return MMP_NO_ERROR;

}

MmpError MmpPad::Push(MmpBuffer* buf) {
  MmuAutoLock lock(obj_mutex_);
  MmpError ret = MMP_NO_ERROR;

  M_ASSERT_FATAL(IsSrc(), MMP_PAD_FLOW_ERROR);

  if (M_UNLIKELY(mode_ != MMP_PAD_MODE_PUSH)) {
    goto wrong_mode;
  }

  if (M_UNLIKELY(peer_ == NULL)) {
    goto not_linked;
  }

  ret = peer_->ChainFunction(buf);

  return ret;

no_chain_function:
wrong_mode:
  delete buf;
  return MMP_PAD_FLOW_ERROR;

not_linked:
  delete buf;
  return MMP_PAD_FLOW_NOT_LINKED;

}

MmpError MmpPad::PushEvent(MmpEvent* event) {
  MmuAutoLock lock(obj_mutex_);
  mbool ret = MMP_NO_ERROR;
  MmpElement *elem = NULL;

  if (M_UNLIKELY(peer_ == NULL)) {
    goto not_linked;
  }

  elem = reinterpret_cast<MmpElement*>(peer_->GetOwner());
  if (M_UNLIKELY(elem == NULL)) {
    goto not_linked;
  }

  ret = elem->SendEvent(event);

  return ret ? MMP_NO_ERROR : MMP_UNEXPECTED_ERROR;

not_linked:
  return MMP_PAD_FLOW_NOT_LINKED;
}

}
