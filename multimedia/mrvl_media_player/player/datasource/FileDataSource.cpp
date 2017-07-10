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

extern "C" {
#include <fcntl.h>
}

#include "MediaPlayerOnlineDebug.h"
#include "FileDataSource.h"

#undef  LOG_TAG
#define LOG_TAG "FileDataSource"

namespace mmp {

extern uint64_t OnlineDebugBitMask;

FileDataSource::FileDataSource(const char* url, bool* ret)
    : source_url_(NULL),
      source_offset_(0),
      constructed_from_fd_(false) {
  ALOGI("%s() line %d, begin to create FileDataSource", __FUNCTION__, __LINE__);

  commonInit();
  if (NULL != url) {
    source_url_ = strdup(url);
  }
  uint32_t tgt_size;
  if (!constructed_from_fd_) {

    // descriptor comes from NFS or SMB.
    source_fd_ = open(source_url_, O_RDONLY);
    if (source_fd_ <= 0) {
      ALOGE("Failed to open target file \"%s\" errno = %d (%s)",
           source_url_,
           errno,
           strerror(errno));
      goto bailout;
    }

    loff_t file_len;
    file_len = lseek64(source_fd_, 0, SEEK_END);
    if (file_len == ((loff_t)-1)) {
      ALOGE("Failed to determine length of file \"%s\"", source_url_);
      goto bailout;
    }
    lseek64(source_fd_, 0, SEEK_SET);
    file_len_ = static_cast<uint64_t>(file_len);
  } else {
    if (source_fd_ < 0) {
      ALOGE("Failed to dup initial file handle passed by framework!");
      goto bailout;
    }
    lseek64(source_fd_, explicit_offset_, SEEK_SET);
  }

  // Explicitly force a seek the first time someone reads.
  next_read_location_ = -1;
  prepared_ = true;
  *ret = true;
  return;

bailout:
  ALOGI("%s() line %d, FileDataSource is created %s error", __FUNCTION__,
      __LINE__, prepared_ ? "without" : "with");
  *ret = false;
}

FileDataSource::FileDataSource(int fd, uint64_t offset, uint64_t length) {
  ALOGI("%s() line %d, begin to create FileDataSource", __FUNCTION__, __LINE__);

  commonInit();
  if ((-1 != fd) && (length > 0)) {
    source_fd_ = dup(fd);
    explicit_offset_ = offset;
    explicit_length_ = length;
    constructed_from_fd_ = true;

    prepared_ = true;
    next_read_pos_ = -1;
  }

  ALOGI("%s() line %d, FileDataSource is created %s error", __FUNCTION__,
      __LINE__, prepared_ ? "without" : "with");
}

FileDataSource::~FileDataSource() {
  ALOGI("%s() line %d, begin to destroy FileSourceWrapper", __FUNCTION__, __LINE__);

  if (source_url_) {
    delete source_url_;
    source_url_ = NULL;
  }

  if (source_fd_ >= 0) {
    ::close(source_fd_);
    source_fd_ = -1;
  }

  ALOGI("%s() line %d, FileSourceWrapper is destroyed", __FUNCTION__, __LINE__);
}

bool FileDataSource::isSeekable() {
  ALOGV("%s() line %d", __FUNCTION__, __LINE__);
  return false;
}

bool FileDataSource::seekToUs(int64_t seek_pos_us) {
  ALOGV("%s() line %d", __FUNCTION__, __LINE__);
  return false;
}

// TODO: implement seek from ffmpeg
int64_t FileDataSource::seekToByte(int64_t offset, int whence) {
  int64_t target = -1;
  switch(whence) {
    case SEEK_SET:
      target = offset;
      break;

    case SEEK_CUR:
      target = static_cast<int64_t>(getCurPos() + offset);
      break;

    case SEEK_END:
      target = static_cast<int64_t>(getLength() + offset);
      break;

    case AVSEEK_SIZE:
      return static_cast<int64_t>(getLength());

    default:
      ALOGE("Invalid seek whence (%d) in ByteIOAdapter::Seek", whence);
  }

  if ((target < 0) || (static_cast<uint64_t>(target) > getLength())) {
    return -1;
  }

  next_read_pos_ = static_cast<uint64_t>(target);
  force_next_read_pos_ = true;
  return 0;
}

uint32_t FileDataSource::read(uint8_t* buf, int amt) {
  assert(buf);
  if (!amt)
    return 0;
  uint64_t pos = force_next_read_pos_ ? next_read_pos_ : getCurPos();
  ALOGV("%s() line %d, amt %d, pos %lld", __FUNCTION__, __LINE__, amt, pos);

  if (pos != next_read_location_)  {
    uint64_t target = explicit_offset_ + pos;
    loff_t res = lseek64(source_fd_, target, SEEK_SET);
    if (res == ((loff_t)-1)) {
      if (target == pos) {
        ALOGE("Failed to seek to position %llu in file \"%s\" (errno = %d)",
             pos, source_url_, errno);
      } else {
        ALOGE("Failed to seek to virtual position %llu (phys pos %llu) in file"
             " \"%s\" (errno = %d)", pos, target, source_url_, errno);
      }
      next_read_location_ = -1;
      return 0;
    }
    next_read_location_ = pos;
  }

  uint32_t tgt_amt = amt;
  if (constructed_from_fd_) {
    uint64_t read_end = pos + amt;
    if (read_end > explicit_length_) {
      uint64_t extra_amt = read_end - explicit_length_;
      if (extra_amt >= tgt_amt) {
        ALOGW("Read operation (%llu, %u) goes outside of explicit file range"
             " (%llu, %llu)", pos, amt, explicit_offset_, explicit_length_);
        return 0;
      }

      tgt_amt -= extra_amt;
    }
  }

  ssize_t amt_read = ::read(source_fd_, buf, tgt_amt);
  if (amt_read < 0) {
    ALOGE("Failed to read %d bytes from position %lld in file \"%s\""
         " (errno = %d)", amt, pos, source_url_, errno);
    return 0;
  }

  next_read_location_ = pos + amt_read;
  force_next_read_pos_ = false;

  return static_cast<uint32_t>(amt_read);
}

bool FileDataSource::stopDataSource() {
  ALOGV("%s() line %d", __FUNCTION__, __LINE__);
  return false;
}

bool FileDataSource::getDurationUs(int64_t* duration_us) {
  *duration_us = static_cast<uint64_t>(-1);
  ALOGV("%s() line %d", __FUNCTION__, __LINE__);
  return false;
}

AVInputFormat* FileDataSource::getAVInputFormat() {
  AVInputFormat *fmt;

  AVProbeData probe_data, *pd = &probe_data;
  int probe_data_size = 1024*256;

  memset(pd, 0, sizeof(AVProbeData));
  pd->buf_size = probe_data_size;
  pd->buf = (uint8_t*)malloc(pd->buf_size);
  if (pd->buf) {
    seekToByte(0, SEEK_SET);
    read(pd->buf, pd->buf_size);
    seekToByte(0, SEEK_SET);
  }
  fmt = av_probe_input_format(pd, 1);
  free(pd->buf);

  if (fmt) {
    ALOGI("%s() line %d, AVInputFormat %s", __FUNCTION__, __LINE__, fmt->name);
  } else {
    ALOGE("%s() line %d, AVInputFormat is not detected", __FUNCTION__, __LINE__);
  }

  return fmt;
}

void FileDataSource::commonInit() {
  source_url_ = NULL;
  source_fd_ = -1;
  file_len_ = 0;
  next_read_location_ = -1;
  explicit_offset_ = 0;
  explicit_length_ = 0;
  source_offset_ = 0;
  constructed_from_fd_ = false;
  force_next_read_pos_ = false;
  next_read_pos_ = -1;
}

uint64_t FileDataSource::getCurPos() const {
  return next_read_location_;
}

uint64_t FileDataSource::getLength() const {
  return constructed_from_fd_ ? explicit_length_ : file_len_;
}

}
