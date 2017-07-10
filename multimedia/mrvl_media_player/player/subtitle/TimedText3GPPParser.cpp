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

#include "TimedText3GPPParser.h"

#undef  LOG_TAG
#define LOG_TAG "TimedText3GPPParser"

namespace mmp {


TimedText3GPPParser::TimedText3GPPParser() {
}

TimedText3GPPParser::~TimedText3GPPParser() {
}

list<MmpBuffer*> TimedText3GPPParser::ParsePacket(MmpBuffer* pkt) {
  list<MmpBuffer*> subtitles;
  subtitles.clear();

  if (pkt && pkt->size > 2) {
    MmpBuffer* buf = new MmpBuffer;
    buf->pts = pkt->pts;
    buf->end_pts = pkt->end_pts;
    buf->size = pkt->size - 2;
    buf->data = (muint8 *)malloc(buf->size + 1);
    memset(buf->data, 0x00, buf->size + 1);
    memcpy(buf->data, pkt->data + 2, buf->size);

    subtitles.push_back(buf);
  }

  delete pkt;
  return subtitles;
}

}

