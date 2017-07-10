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

#include "mmp_buffer.h"


namespace mmp {

MmpBuffer::MmpBuffer(muint32 realloc_size)
  : data(NULL),
    data_offset(0),
    size(0),
    index(0),
    pts(kInvalidTimeStamp),
    end_pts(kInvalidTimeStamp),
    flags_(MMP_BUFFER_FLAG_NONE) {
  if(realloc_size) {
    data = reinterpret_cast<muint8*>(malloc(realloc_size * sizeof(muint8)));
  }
}

MmpBuffer::~MmpBuffer() {
  Clear();
}

void MmpBuffer::Clear() {
  if (data && size) {
    free(data);
    data = NULL;
    size = 0;
    data_offset = 0;
  }

}

}
