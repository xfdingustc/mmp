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


//#define LOG_NDEBUG 0
#define LOG_TAG "RVFormatter"
#include <utils/Log.h>

#include "RVFormatter.h"
#include "MediaPlayerOnlineDebug.h"

namespace mmp {

extern uint64_t OnlineDebugBitMask;

typedef struct rv_segment_struct {
  uint32_t bIsValid;
  uint32_t ulOffset;
} rv_segment;

uint32_t RVFormatter::computeNewESLen(
    const uint8_t* in, uint32_t inAllocLen) const {
  uint32_t out_buf_size = 0;
  uint32_t buf_size = inAllocLen;

  if (!in) {
    return 0;
  }

  // Get segment number;
  uint32_t numSegments = in[0];
  uint32_t mPacketHeaderLen = sizeof(RmPacketHeader);

  // Calc the output buffer size;
  out_buf_size = buf_size - (numSegments + 1) * 8 - 1 +
      (numSegments + 2) * mPacketHeaderLen;

  return out_buf_size;
}

int32_t RVFormatter::formatES(
    const uint8_t* in, uint32_t inAllocLen,
    uint8_t* out_buf, uint32_t outAllocLen) const {
  uint32_t buf_size = inAllocLen;

  if (!in || !out_buf || (in == out_buf)) {
    return 0;
  }

  RmPacketHeader* common_hdr = new RmPacketHeader;
  if (!common_hdr) {
    DATA_READER_ERROR("Out of memory!");
    return 0;
  }
  common_hdr->start_code_1 = 0;
  common_hdr->start_code_2 = 1;
  common_hdr->packet_type = 0x10;
  common_hdr->reserved = 0;
  common_hdr->marker_byte_1 = 0x88;
  common_hdr->packet_payload_len_msb = 0;
  common_hdr->marker_byte_2 = 0x88;
  common_hdr->packet_payload_len_lsb = 0x1800;
  common_hdr->marker_byte_3 = 0x88;
  common_hdr->packet_padding_len = 0;
  common_hdr->marker_byte_4 = 0x88;

  common_hdr->packet_type = 0x12;
  common_hdr->packet_payload_len_msb = 0x0;
  common_hdr->packet_payload_len_lsb = 0x0;
  common_hdr->reserved = 1;

  // Get segment number;
  uint32_t numSegments = in[0];
  uint32_t mPacketHeaderLen = sizeof(RmPacketHeader);

  memcpy(out_buf, common_hdr, mPacketHeaderLen);
  out_buf += mPacketHeaderLen;

  rv_segment *pSegment = reinterpret_cast<rv_segment *>(const_cast<uint8_t *>(in) + 1);
  uint32_t offset = 1 + (numSegments + 1) * sizeof(rv_segment);
  in += offset;

  for (int i = 0; i <= numSegments; i++) {
    int segmentSize, temp1;
    common_hdr->packet_type = 0x11;
    if(!pSegment[i].bIsValid) {
      common_hdr->reserved = 1;
    } else {
      common_hdr->reserved = 0;
    }

    if(i == numSegments) {
      segmentSize = buf_size - pSegment[i].ulOffset - offset;
    } else {
      segmentSize = pSegment[i + 1].ulOffset - pSegment[i].ulOffset;
    }

    /*malloc out buf*/
    temp1 = segmentSize >> 16;
    common_hdr->packet_payload_len_msb = ((temp1 & 0xff) << 8) + (temp1 >> 8);
    temp1 = segmentSize & 0x0ffff;
    common_hdr->packet_payload_len_lsb = ((temp1 & 0xff) << 8) + (temp1 >> 8);
    /*copy to out buf*/
    memcpy (out_buf, common_hdr, mPacketHeaderLen);
    memcpy (out_buf + mPacketHeaderLen, in + pSegment[i].ulOffset, segmentSize);
    out_buf += mPacketHeaderLen + segmentSize;
  }

  delete common_hdr;

  return outAllocLen;
}

}
