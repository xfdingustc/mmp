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

#ifndef ANDROID_RawDataSource_H
#define ANDROID_RawDataSource_H

extern "C" {
#include <stdint.h>
#include <libavformat/avio.h>
}

#include "MmpMediaDefs.h"

namespace mmp {

typedef enum
{
  DS_UNKNOWN = 0,
  DS_HLS_PLAYING_BANDWIDTH,
  DS_HLS_REALTIME_SPEED,
  DS_HLS_DOWNLOAD_PROGRESS_IN_PERCENT,
  DS_HLS_DOWNLOAD_PROGRESS_IN_BYTES,
  DS_HLS_DOWNLOAD_EOS,
  DS_HLS_DOWNLOAD_ERROR,
  DS_HLS_DOWNLOAD_PLAYLIST_ERROR,
  DS_HLS_DOWNLOAD_TS_ERROR,
  DS_HLS_GOT_IP_ADDRESS,
  DS_UNSUPPORT,
} DataSourceEvent;

class MRVLDataSourceCbTarget {
 public:
  virtual ~MRVLDataSourceCbTarget() {};
  virtual MediaResult OnDataSourceEvent(DataSourceEvent what, int extra) = 0;
};

// We take advantage of av_open_input_stream() of ffmpeg directly.
// ByteIOContext is used by ffmpeg to do read/write/seek
class RawDataSource
{
public:
  RawDataSource();
  virtual ~RawDataSource();

  void             registerCallback(MRVLDataSourceCbTarget *cb_target) { cb_target_ = cb_target; }
  bool             isPrepared() const { return prepared_; }
  AVIOContext*     getByteIOContext() { return context_; }

  virtual bool     isSeekable();
  virtual bool     seekToUs(int64_t seek_pos_us);
  virtual int64_t  seekToByte(int64_t offset, int whence);

  virtual uint32_t read(uint8_t* buf, int amt);

  virtual bool     stopDataSource();
  virtual bool     startDataSource() { return true; }

  virtual bool     getDurationUs(int64_t* duration_us);
  virtual bool     getLastSeekMediaTime(int64_t* media_time_ms) const { return false; }

  virtual AVInputFormat* getAVInputFormat();

  virtual bool isUrlHLS(const char* url) { return false; }

  static int     staticRead(void* thiz, uint8_t* buf, int amt);
  static int     staticWrite(void* thiz, uint8_t* buf, int amt);
  static int64_t staticSeek(void* thiz, int64_t offset, int whence);

protected:
  bool          prepared_;
  MRVLDataSourceCbTarget *cb_target_;

protected:
  //What we should provide to ffmpeg so that ffmpeg can get media data.
  AVIOContext* context_;
  uint8_t*      buffer_;
};

}
#endif

