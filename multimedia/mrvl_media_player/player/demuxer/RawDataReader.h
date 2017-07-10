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

#ifndef ANDROID_RAWDATAREADER_H
#define ANDROID_RAWDATAREADER_H

#include "MmpMediaDefs.h"
#include "frameworks/mmp_frameworks.h"

namespace mmp {

class MRVLDataReaderCbTarget {
 public:
  virtual ~MRVLDataReaderCbTarget() {}
  virtual bool OnSeekComplete(TimeStamp seek_time) = 0;
};

class RawDataReader : public MmpElement {
 public:
  RawDataReader(const char* name) : MmpElement(name) {}
  virtual ~RawDataReader() {}

  virtual MediaResult Probe(const char *url, MEDIA_FILE_INFO *file_info) = 0;
  virtual MediaResult Probe(const char *url, void *pb, void *fmt, MEDIA_FILE_INFO *pMediaInfo) = 0;
  virtual MediaResult Init() = 0;
  virtual MediaResult Start(double speed) = 0;
  virtual MediaResult Stop() = 0;
  virtual MediaResult Seek(uint64_t seek_pos_us) = 0;
  virtual MediaResult FlushDataReader() = 0;
  virtual MediaResult SetCurrentStream(int32_t video_idx, int32_t audio_idx) = 0;
  virtual bool isDataReaderEOS() = 0;
  virtual TimeStamp getLatestPts() = 0;
};

}
#endif
