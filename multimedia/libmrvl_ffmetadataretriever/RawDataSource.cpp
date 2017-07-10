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

#include <utils/Log.h>

#include "RawDataSource.h"

#undef  LOG_TAG
#define LOG_TAG "RawDataSource"

namespace android{
static uint32_t kAdapterBufferSize = 32768;

RawDataSource::RawDataSource()
    : prepared_(false),
      cb_target_(NULL),
      buffer_(NULL){
  ALOGI("%s() line %d, begin to create RawDataSource", __FUNCTION__, __LINE__);

  uint32_t tgt_size = kAdapterBufferSize + FF_INPUT_BUFFER_PADDING_SIZE;
  buffer_ = static_cast<uint8_t*>(av_mallocz(tgt_size));
  if (NULL != buffer_) {
    context_ = avio_alloc_context(buffer_,
                               static_cast<int>(kAdapterBufferSize),
                               0,  // write_flag = false.
                               static_cast<void*>(this),
                               staticRead, //ffmpeg call it to read data from HLS
                               staticWrite,
                               staticSeek);
  }

  ALOGI("%s() line %d, RawDataSource is created", __FUNCTION__, __LINE__);
}

RawDataSource::~RawDataSource() {
  ALOGI("%s() line %d, begin to destroy RawDataSource", __FUNCTION__, __LINE__);
  if (NULL != buffer_) {
    av_free(buffer_);
    buffer_ = NULL;
  }
  ALOGI("%s() line %d, RawDataSource is destroyed", __FUNCTION__, __LINE__);
}

bool RawDataSource::stopDataSource() {
  ALOGI("%s() line %d", __FUNCTION__, __LINE__);
  return false;
}

uint32_t RawDataSource::read(uint8_t* buf, int amt) {
  ALOGD("%s() line %d", __FUNCTION__, __LINE__);
  return amt;
}

int64_t RawDataSource::seekToByte(int64_t offset, int whence) {
  ALOGD("%s() line %d", __FUNCTION__, __LINE__);
  return -1;
}

bool RawDataSource::isSeekable() {
  ALOGD("%s() line %d", __FUNCTION__, __LINE__);
  return false;
}

bool RawDataSource::seekToUs(int64_t seek_pos_us) {
  ALOGI("%s() line %d", __FUNCTION__, __LINE__);
  return false;
}

bool RawDataSource::getDurationUs(int64_t* duration_us) {
  *duration_us = static_cast<uint64_t>(-1);
  ALOGI("%s() line %d", __FUNCTION__, __LINE__);
  return false;
}


AVInputFormat* RawDataSource::getAVInputFormat() {
  ALOGD("%s() line %d", __FUNCTION__, __LINE__);
  return NULL;
}

/*static function*/
int RawDataSource::staticRead(void* thiz, uint8_t* buf, int amt) {
  return static_cast<RawDataSource*>(thiz)->read(buf, amt);
}

/*static function*/
int RawDataSource::staticWrite(void* thiz, uint8_t* buf, int amt) {
  // We don't need write function at the moment.
  return -1;
}

/*static function*/
int64_t RawDataSource::staticSeek(void* thiz, int64_t offset, int whence) {
  return static_cast<RawDataSource*>(thiz)->seekToByte(offset, whence);
}

}

