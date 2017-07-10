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



#include "StringConvertor.h"

using namespace std;

namespace berlin {

/*static*/
OMX_ERRORTYPE StringConvertor::Unicode2UTF8(OMX_U16* pUnicodeBuf, OMX_U32 iUnicodeBufLen,
    OMX_U8* pUTF8Buf, OMX_U32& iUTF8BufLen) {
  // TODO: This funtion is not implemented
  return OMX_ErrorNotImplemented;
}


/*static*/
OMX_ERRORTYPE StringConvertor::UTF82Unicode(OMX_U8* pUTF8Buf, OMX_U32 iUTF8BufLen,
    OMX_U16* pUnicodeBuf, OMX_U32& iUnicodeBufLen) {
  if (!pUTF8Buf || !pUnicodeBuf) {
    return OMX_ErrorUndefined;
  }

  OMX_U32 total_num = 0;

  OMX_U8 *p  = pUTF8Buf;

  for (OMX_U32 i = 0; i < iUTF8BufLen; i++) {
    if (*p >= 0x00 && *p <=0x7f) {
      p++;
      total_num++;
    } else if ((*p & (0xe0)) == 0xc0) {
      p++;
      p++;
      total_num++;
    } else if ((*p & (0xf0)) == 0xe0) {
      p++;
      p++;
      p++;
      total_num++;
    }
  }

  OMX_U32 result_size = 0;

  p = pUTF8Buf;
  OMX_U8 *tmp = (OMX_U8*)pUnicodeBuf;
  while (*p) {
    if (*p >= 0x00 && *p <= 0x7f) {
      *tmp = *p;
      tmp++;
      tmp++;
      result_size += 1;
    } else if ((*p & 0xe0)== 0xc0) {
      OMX_U16 t = 0;
      char t1 = 0;
      char t2 = 0;

      t1 = *p & (0x1f);
      p++;
      t2 = *p & (0x3f);

      *tmp = t2 | ((t1 & (0x03)) << 6);
      tmp++;
      *tmp = t1 >> 2;
      tmp++;
      result_size += 1;
    } else if ((*p & (0xf0))== 0xe0) {
      OMX_U16 t = 0;
      OMX_U16 t1 = 0;
      OMX_U16 t2 = 0;
      OMX_U16 t3 = 0;
      t1 = *p & (0x1f);
      p++;
      t2 = *p & (0x3f);
      p++;
      t3 = *p & (0x3f);

      *tmp = ((t2 & (0x03)) << 6) | t3;
      tmp++;
      *tmp = (t1 << 4) | (t2 >> 2);
      tmp++;
      result_size += 1;
    }
    p++;
  }
  iUnicodeBufLen = result_size;
  return OMX_ErrorNone;
}

}
