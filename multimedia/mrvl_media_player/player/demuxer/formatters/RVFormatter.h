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

#ifndef REALVIDEO_FORMATTER_H_
#define REALVIDEO_FORMATTER_H_

extern "C" {
#include <libavformat/avformat.h>
#undef CodecType
}

#include "MRVLFormatter.h"

namespace mmp {

typedef struct _Common_packet_header {
  uint16_t start_code_1;
  uint8_t  start_code_2;
  uint8_t  packet_type;
  uint16_t reserved;
  uint8_t  marker_byte_1;
  uint16_t packet_payload_len_msb;
  uint8_t  marker_byte_2;
  uint16_t packet_payload_len_lsb;
  uint8_t  marker_byte_3;
  uint16_t packet_padding_len;
  uint8_t marker_byte_4;
} __attribute__ ((packed))RmPacketHeader;

class RVFormatter : public MRVLFormatter {
  public:
    RVFormatter(AVCodecContext *codec) {}
    virtual ~RVFormatter() {}

    virtual uint32_t computeNewESLen(
            const uint8_t* in, uint32_t inAllocLen) const;

    virtual int32_t formatES(
            const uint8_t* in, uint32_t inAllocLen, uint8_t* out,
            uint32_t outAllocLen) const;
};

}

#endif
