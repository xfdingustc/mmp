/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "AVCCFormatter"
#include <utils/Log.h>

#include "AVCCFormatter.h"
#include "MediaPlayerOnlineDebug.h"

#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/Utils.h>

namespace mmp {

extern uint64_t OnlineDebugBitMask;

// TODO: make this file independent from stagefright
AVCCFormatter::AVCCFormatter(AVCodecContext* codec)
  : mAVCCFound(false),
    mAVCC(NULL),
    mAVCCSize(0),
    mNALLengthSize(0) {
  parseCodecExtraData(codec);
}

AVCCFormatter::~AVCCFormatter() {
  delete[] mAVCC;
}

bool AVCCFormatter::parseCodecExtraData(AVCodecContext* codec) {
  DATA_READER_RUNTIME("ENTER");
  if (NULL == codec) {
    DATA_READER_ERROR("NULL codec context passed");
    return false;
  }

  if (codec->extradata_size < 7) {
    DATA_READER_ERROR("AVCC cannot be smaller than 7.");
    return false;
  }

  if (codec->extradata[0] != 1u) {
    DATA_READER_ERROR("We only support configurationVersion 1, but this is %u.",
        codec->extradata[0]);
    return false;
  }

  mAVCC = new uint8_t[codec->extradata_size];
  mAVCCSize = static_cast<uint32_t>(codec->extradata_size);
  memcpy(mAVCC, codec->extradata, mAVCCSize);

  // The number of bytes used to encode the length of a NAL unit.
  mNALLengthSize = 1 + (mAVCC[4] & 3);
  mAVCCFound = true;
  DATA_READER_RUNTIME("EXIT, AVCC is found.");
  return true;
}

size_t AVCCFormatter::parseNALSize(const uint8_t *data) const {
  switch (mNALLengthSize) {
    case 1:
      return *data;
    case 2:
      return U16_AT(data);
    case 3:
      return ((size_t)data[0] << 16) | U16_AT(&data[1]);
    case 4:
      return U32_AT(data);
  }

  // This cannot happen, mNALLengthSize springs to life by adding 1 to
  // a 2-bit integer.
  CHECK(!"Invalid NAL length size.");

  return 0;
}

uint32_t AVCCFormatter::computeNewESLen(
    const uint8_t* in, uint32_t inAllocLen) const {
  if (!mAVCCFound) {
    ALOGV("Can not compute new payload length, have not found an AVCC header yet.");
    return 0;
  }
  size_t srcOffset = 0;
  size_t dstOffset = 0;
  const size_t packetSize = static_cast<size_t>(inAllocLen);

  while (srcOffset < packetSize) {
    CHECK_LE(srcOffset + mNALLengthSize, packetSize);
    size_t nalLength = parseNALSize(&in[srcOffset]);
    srcOffset += mNALLengthSize;

    if (nalLength > packetSize - srcOffset) {
      DATA_READER_ERROR("Invalid nalLength (%zi) or packet size(%zi).",
          nalLength, packetSize);
      // Return what we've done so far, hoping that it could be recovered
      // later.
      return dstOffset;
    }

    if (nalLength == 0) {
      continue;
    }

    dstOffset += 4 + nalLength;
    srcOffset += nalLength;
  }
  CHECK_EQ(srcOffset, packetSize);
  return dstOffset;
}

int32_t AVCCFormatter::formatES(
    const uint8_t* in, uint32_t inAllocLen,
    uint8_t* out, uint32_t outAllocLen) const {
  CHECK(in != out);
  if (!mAVCCFound) {
    DATA_READER_ERROR("AVCC header has not been set.");
    return -1;
  }

  size_t srcOffset = 0;
  size_t dstOffset = 0;
  const size_t packetSize = static_cast<size_t>(inAllocLen);

  while (srcOffset < packetSize) {
    CHECK_LE(srcOffset + mNALLengthSize, packetSize);
    size_t nalLength = parseNALSize(&in[srcOffset]);
    srcOffset += mNALLengthSize;

    if (nalLength > packetSize - srcOffset) {
      DATA_READER_ERROR("Invalid nalLength (%zi) or packet size(%zi).", nalLength, packetSize);
      // Return what we've done so far, hoping that it could be recovered
      // later.
      return dstOffset;
    }

    if (nalLength == 0) {
      continue;
    }

    CHECK(dstOffset + 4 + nalLength <= outAllocLen);

    static const uint8_t kNALStartCode[4] =  { 0x00, 0x00, 0x00, 0x01 };
    memcpy(out + dstOffset, kNALStartCode, 4);
    dstOffset += 4;

    memcpy(&out[dstOffset], &in[srcOffset], nalLength);
    srcOffset += nalLength;
    dstOffset += nalLength;
  }
  CHECK_EQ(srcOffset, packetSize);
  return dstOffset;
}

}
