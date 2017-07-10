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
#include "MediaPlayerOnlineDebug.h"

namespace mmp {

uint64_t OnlineDebugBitMask = DEFAULT_MASK;

MediaPlayerOnlineDebug::MediaPlayerOnlineDebug() {
  ONLINE_DEBUG_PRINT("DEFAULT_MASK = %b", DEFAULT_MASK);
  OnlineDebugBitMask |= DEFAULT_MASK;
}

void MediaPlayerOnlineDebug::SetBitMask(uint64_t bit_mask) {
  OnlineDebugBitMask = bit_mask;
}

uint64_t MediaPlayerOnlineDebug::GetBitMask() {
  return OnlineDebugBitMask;
}

sp<IBinder> createMediaPlayerOnlineDebug()
{
  ALOGD("%s, line %d: create MediaPlayerOnlineDebug\n", __FUNCTION__, __LINE__);
  return (new MediaPlayerOnlineDebug());
}

}
