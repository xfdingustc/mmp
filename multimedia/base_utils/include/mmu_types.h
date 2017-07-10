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


#ifndef MMU_TYPES_H
#define MMU_TYPES_H

namespace mmu {

typedef bool                    mbool;

typedef char                    mchar;
typedef wchar_t                 mwchar;
typedef signed char             mint8;
typedef signed short            mint16;
typedef signed long             mint32;
typedef signed long long        mint64;

typedef unsigned char           muchar;
typedef unsigned char           muint8;
typedef unsigned short          muint16;
typedef unsigned long           muint32;
typedef unsigned long long      muint64;

typedef unsigned long long      moffset;

typedef float   mfloat;
typedef double  mdouble;

typedef void*   mpointer;
typedef const void* mconstpointer;

typedef enum {
  M_TYPE_NONE     = 0,
  M_TYPE_CHAR,
  M_TYPE_INT16,
  M_TYPE_UINT16,
  M_TYPE_INT32,
  M_TYPE_UINT32,
  M_TYPE_INT64,
  M_TYPE_UINT64,
  M_TYPE_FLOAT,
  M_TYPE_PTR,
  M_TYPE_BOOL,
} MType;

typedef void            (*MmuDestroyNotify)       (mpointer       data);

}

#endif
