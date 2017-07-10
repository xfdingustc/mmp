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


#ifndef MMP_INFO_H
#define MMP_INFO_H

#ifdef ANDROID
#include <utils/Log.h>
#define LOG_TAG "MMPFrameworks"
#endif

namespace mmp {

#ifdef ANDROID
#define MMP_ERROR_OBJECT(str, ...) \
  do { \
    ALOGE("%s %s() line %d: "str, GetName(), __FUNCTION__, __LINE__, ##__VA_ARGS__); \
  } while(0);

#define MMP_INFO_OBJECT(str, ...) \
  do { \
    ALOGI("%s %s() line %d: "str, GetName(), __FUNCTION__, __LINE__, ##__VA_ARGS__); \
  } while(0);

#define MMP_DEBUG_OBJECT(str, ...) \
  do { \
    ALOGD("%s %s() line %d: "str, GetName(), __FUNCTION__, __LINE__, ##__VA_ARGS__); \
  } while(0);

#define MMP_WARNING_OBJECT(str, ...) \
  do { \
    ALOGW("%s %s() line %d: "str, GetName(), __FUNCTION__, __LINE__, ##__VA_ARGS__); \
  } while(0);

#define MMP_VERBOSE_OBJECT(str, ...) \
    do { \
      ALOGV("%s %s() line %d: "str, GetName(), __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while(0);

#define MMP_ERROR(str, ...) \
  do { \
    ALOGE("%s() line %d: "str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
  } while(0);


#endif
}

#endif
