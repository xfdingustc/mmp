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

#ifndef IONLINE_DEBUG_H
#define IONLINE_DEBUG_H

//#undef LOG_TAG
//#define LOG_TAG "MarvellMediaPlayer"

#include <utils/Log.h>
#include <utils/RefBase.h>
#include <binder/Binder.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>

using namespace android;

namespace mmp {


class IOnlineDebug : public IInterface {
  public:
   DECLARE_META_INTERFACE(OnlineDebug);

   static const String16 kServiceName;

   virtual void SetBitMask(uint64_t bit_mask) = 0;

   virtual uint64_t GetBitMask() = 0;
};

class BnOnlineDebug: public BnInterface<IOnlineDebug> {
 public:
  virtual status_t onTransact(uint32_t code,
                             const Parcel& data,
                             Parcel* reply,
                             uint32_t flags = 0);

  enum {
    SET_BITMASK = IBinder::FIRST_CALL_TRANSACTION,
    GET_BITMASK,
  };
};
}

#endif
