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

#ifndef ANDROID_HttpLiveDataSource_H
#define ANDROID_HttpLiveDataSource_H

#include "LiveSession.h"
#include "LiveDataSource.h"

#include "RawDataSource.h"

using namespace android;

namespace mmp {

class HttpLiveDataSource : public RawDataSource, public HttpLiveCbTarget
{
public:
  HttpLiveDataSource(const char* url, const KeyedVector<String8, String8> *headers = NULL);
  virtual ~HttpLiveDataSource();

  bool     isSeekable();
  bool     seekToUs(int64_t seek_pos_us);
  int64_t  seekToByte(int64_t offset, int whence);

  uint32_t read(uint8_t* buf, int amt);

  bool     stopDataSource();
  bool     startDataSource();

  bool     getDurationUs(int64_t* duration_us);
  bool     getLastSeekMediaTime(int64_t* media_time_ms) const;
  AVInputFormat* getAVInputFormat();

  bool isUrlHLS(const char* url);

  bool setHTTPHeaders(const KeyedVector<String8, String8>* headers);

  bool OnHttpLiveEvent(HLSEvent what, int extra);

private:
  void* MonitorThreadEntry();
  static void* MonitorThread(void* thiz);

  char* source_url_;
  android::KeyedVector<android::String8, android::String8>* headers_;

  // To utilize stagefright http live streaming
  android::sp<android::LiveSession> live_session_;
  android::sp<android::LiveDataSource> data_source_;

  android::sp<android::ALooper> looper_;
  android::ALooper::handler_id looper_handler_id_;

  // read offset, keep in pace with the one in android::LiveDataSource
  off64_t source_offset_;

  bool during_download_;

  char ip_address[256];

  FILE *hls_dump_file_;
};

}
#endif

