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


#ifndef BERLIN_OMX_LOG_H_
#define BERLIN_OMX_LOG_H_

#ifdef _ANDROID_
#include <media/stagefright/foundation/ADebug.h>
#endif

#include <OMX_Core.h>
#include <OMX_Other.h>

extern "C" {

  enum OMX_LOG_LEVEL {
    OMX_LOG_LEVEL_ERROR = 0,
    OMX_LOG_LEVEL_WARN,
    OMX_LOG_LEVEL_INFO,
    OMX_LOG_LEVEL_DEBUG,
    OMX_LOG_LEVEL_VERBOSE,
    OMX_LOG_LEVEL_RESERVE,
    OMX_LOG_LEVEL_MAX = 0x7FFFFFFF
  };

  static const char* level_string[] = {
    "E",
    "W",
    "I",
    "D",
    "V",
    "R"
  };

#ifndef OMX_LOG_TAG
#define OMX_LOG_TAG "BelinOmx"
#endif

#ifdef _ANDROID_
#define OMX_LOGV(fmt, ...) \
  ALOGV("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OMX_LOGD(fmt, ...) \
  ALOGD("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OMX_LOGI(fmt, ...) \
  ALOGI("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OMX_LOGW(fmt, ...) \
  ALOGW("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OMX_LOGE(fmt, ...) \
  ALOGE("%s() line %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define OMX_LOGV(fmt...) do {omx_log_printf(OMX_LOG_TAG, OMX_LOG_LEVEL_VERBOSE,fmt);} while(0)
#define OMX_LOGD(fmt...) do {omx_log_printf(OMX_LOG_TAG, OMX_LOG_LEVEL_DEBUG, fmt);} while(0)
#define OMX_LOGI(fmt...) do {omx_log_printf(OMX_LOG_TAG, OMX_LOG_LEVEL_INFO, fmt);} while(0)
#define OMX_LOGW(fmt...) do {omx_log_printf(OMX_LOG_TAG, OMX_LOG_LEVEL_WARN, fmt);} while(0)
#define OMX_LOGE(fmt...) do {omx_log_printf(OMX_LOG_TAG, OMX_LOG_LEVEL_ERROR, fmt);} while(0)


#endif
#define OMX_LOG_FUNCTION_ENTER   OMX_LOGD("Function Enter")
#define OMX_LOG_FUNCTION_EXIT    OMX_LOGD("Function Exit")

void omx_log_printlog(char *tag, OMX_LOG_LEVEL level, ...);

void omx_log_printf(const char *tag, OMX_LOG_LEVEL level, const char *fmt,...);

#define TIME_SECOND 1000000LL
#define TIME_FORMAT "%u:%02u:%02u.%06u"
#define TIME_ARGS(t)    \
     (uint32_t)((t) / (TIME_SECOND * 60 * 60)), \
     (uint32_t)(((t) / (TIME_SECOND * 60)) % 60), \
     (uint32_t)(((t) / TIME_SECOND) % 60), \
     (uint32_t)((t) % TIME_SECOND)

OMX_STRING GetOMXBufferFlagDescription(OMX_U32 flag);

OMX_STRING OmxState2String(OMX_STATETYPE state);
OMX_STRING OmxIndex2String(OMX_INDEXTYPE index);
OMX_STRING OmxClockState2String(OMX_TIME_CLOCKSTATE state);
OMX_STRING OmxDir2String(OMX_DIRTYPE dir);

}
#endif // BERLIN_OMX_LOG_H_
