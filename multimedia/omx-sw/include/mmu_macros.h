/*
 **                Copyright 2014, MARVELL SEMICONDUCTOR, LTD.
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

#ifndef MMP_MACROS_H
#define MMP_MACROS_H

namespace mmp {

#ifndef NULL
#define NULL 0
#endif

#define _M_BOOL_EXPR(expr)                 \
 __extension__ ({                          \
   int _bool_var_;                         \
   if (expr)                               \
      _bool_var_ = 1;                      \
   else                                    \
      _bool_var_ = 0;                      \
   _bool_var_;                             \
 })
#define M_LIKELY(expr) (__builtin_expect (_M_BOOL_EXPR(expr), 1))
#define M_UNLIKELY(expr) (__builtin_expect (_M_BOOL_EXPR(expr), 0))

#define M_ASSERT_FATAL(expr,val) do {      \
  if (M_LIKELY(expr)) {                    \
  } else {                                 \
    return (val);                          \
  }                                        \
} while(0);

#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))
}
#endif
