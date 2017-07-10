/*
**                Copyright 2012, MARVELL SEMICONDUCTOR, LTD.
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


#include "IOnlineDebug.h"

namespace mmp {

const String16 IOnlineDebug::kServiceName("media.onlinedebug");

class BpOnlineDebug : public BpInterface<IOnlineDebug> {
 public:
  BpOnlineDebug(const sp<IBinder>& impl)
      : BpInterface<IOnlineDebug>(impl) {}

  virtual void SetBitMask(uint64_t bit_mask) {
    Parcel data, reply;
    data.writeInterfaceToken(IOnlineDebug::getInterfaceDescriptor());
    data.writeInt64(bit_mask);
    status_t err = remote()->transact(BnOnlineDebug::SET_BITMASK, data, &reply);
    //if (err != NO_ERROR)
      //return kServerDead;
    //return static_cast<AVResult>(reply.readInt32());
  }

  virtual uint64_t GetBitMask() {
    Parcel data, reply;
    data.writeInterfaceToken(IOnlineDebug::getInterfaceDescriptor());
    status_t err = remote()->transact(BnOnlineDebug::GET_BITMASK, data, &reply);
    if (err != NO_ERROR)
      return 0;
    return reply.readInt64();
  }
};

IMPLEMENT_META_INTERFACE(OnlineDebug, "android.media.OnlineDebug");
status_t BnOnlineDebug::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
  switch (code) {
    case SET_BITMASK: {
      CHECK_INTERFACE(IOnlineDebug, data, reply);
      uint64_t bit_mask = data.readInt64();
      SetBitMask(bit_mask);
      return NO_ERROR;
    }
    case GET_BITMASK: {
      CHECK_INTERFACE(IOnlineDebug, data, reply);
      uint64_t bit_mask = GetBitMask();
      reply->writeInt64(bit_mask);
      return NO_ERROR;
    }
    default: {
      return BBinder::onTransact(code, data, reply, flags);
    }
  }
}
}
