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

#ifndef ANDROID_HttpDataSource_H
#define ANDROID_HttpDataSource_H

#include <utils/threads.h>
#include <curl/curl.h>

#include <String8.h>
#include <KeyedVector.h>

#include "http_source/IReadCacheOwner.h"
#include "http_source/buffers.h"
#include "RawDataSource.h"

using namespace android;

namespace mmp {

class HttpDataSource : public RawDataSource, public IReadCacheOwner
{
public:
  explicit HttpDataSource(const char* url);
  virtual ~HttpDataSource();

  static bool DoStaticInit();
  static bool isUrlHttp(const char* url);

  //
  // RawDataSource
  //
  virtual bool     isSeekable() { return false; }
  virtual bool     seekToUs(int64_t seek_pos_us) { return false; }

  virtual int64_t  seekToByte(int64_t offset, int whence);
  virtual uint32_t read(uint8_t* buf, int amt);

  virtual bool     stopDataSource();
  virtual bool     startDataSource();

  virtual bool     getDurationUs(int64_t* duration_us) { return false; }
  virtual AVInputFormat* getAVInputFormat();

  //
  // IReadCacheOwner
  //
  virtual void OnConnectionReady(bool success);
  virtual void OnURLChange(const char *url);
  virtual bool getParamValue(const char* name, int &value) const;

  static const char* kParamSampleRate;
  static const char* kParamNumChannels;
  static const char* kAudioL16MIMEType;
  static const char* const kDefaultUserAgent;

private:
  typedef Mutex::Autolock Autolock;

private:
  bool DoSetup();
  void DoCleanup();

private:
  Mutex mutex_;
  bool reading_;
  bool stopping_;
  bool setup_done_;
  bool is_stopped_state_;
  Condition stopped_;
  int pipe_[2];

  ReadCache* data_cache_;
  char* source_url_;
//  BufferLevelTracker* buffer_tracker_;

  bool prepare_started_;
  bool prepare_finished_;
  bool file_len_valid_;
  uint64_t file_len_;
  uint64_t next_read_location_;

  uint8_t* tail_cache_;

  char* http_cookies_;
  char* http_user_agent_;
  KeyedVector<String8, String8>* http_headers_;

  static const uint32_t kDefaultMaxRAM;
  static const uint32_t kDefaultCacheBlockSize;
  static const uint32_t kTailCacheSize;

  static Mutex init_mutex_;
  static bool init_done_;

private:
  static bool beginsWith(const char *const str, const char *const prefix);
  static void DumpData(uint8_t *buffer, uint32_t size);
};

}

#endif

