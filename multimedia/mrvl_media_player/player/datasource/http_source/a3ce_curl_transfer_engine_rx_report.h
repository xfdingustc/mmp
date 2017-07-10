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


#ifndef ANDROID_A3CE_CURL_TRANSFER_ENGINE_RX_REPORT_H
#define ANDROID_A3CE_CURL_TRANSFER_ENGINE_RX_REPORT_H

#include <stdint.h>

namespace android {

class A3ceCURLTransferEngineRXReport {
 public:
  A3ceCURLTransferEngineRXReport(uint32_t recent_realtime_byterate,
                                uint32_t recent_windowed_byterate,
                                uint32_t recent_window_size) {
    recent_realtime_byterate_  = recent_realtime_byterate;
    recent_windowed_byterate_  = recent_windowed_byterate;
    recent_window_size_        = recent_window_size;
  }

  uint32_t RecentRealtimeByterate() const { return recent_realtime_byterate_; }
  uint32_t RecentWindowedByterate() const { return recent_windowed_byterate_; }
  uint32_t RecentWindowSize() const { return recent_window_size_; }

 private:
  uint32_t recent_realtime_byterate_;
  uint32_t recent_windowed_byterate_;
  uint32_t recent_window_size_;
};

}  // namespace android
#endif  // ANDROID_A3CE_CURL_TRANSFER_ENGINE_RX_REPORT_H
