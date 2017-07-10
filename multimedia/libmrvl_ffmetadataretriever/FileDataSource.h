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

#ifndef MARVELL_FileDataSource_H
#define MARVELL_FileDataSource_H

#include "RawDataSource.h"

namespace android {

class FileDataSource : public RawDataSource
{
public:
  explicit FileDataSource(const char* url, bool* ret);
  FileDataSource(int fd, uint64_t offset, uint64_t length);
  virtual ~FileDataSource();

  bool     isSeekable();
  bool     seekToUs(int64_t seek_pos_us);
  int64_t  seekToByte(int64_t offset, int whence);

  uint32_t read(uint8_t* buf, int amt);

  bool     stopDataSource();

  bool     getDurationUs(int64_t* duration_us);
  AVInputFormat* getAVInputFormat();


private:
  void commonInit();
  uint64_t getLength() const;
  uint64_t getCurPos() const;

  char*    source_url_;
  uint64_t file_len_;
  off64_t  source_offset_;
  int      source_fd_;
  uint64_t next_read_location_;
  uint64_t explicit_offset_;
  uint64_t explicit_length_;
  bool     constructed_from_fd_;
  bool     force_next_read_pos_;
  uint64_t next_read_pos_;

};

}
#endif

