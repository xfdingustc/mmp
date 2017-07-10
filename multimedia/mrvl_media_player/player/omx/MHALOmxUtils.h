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
#ifndef MHAL_OMX_UTILS_H_
#define MHAL_OMX_UTILS_H_

#include <media/stagefright/foundation/ADebug.h>

#include <OMX_Core.h>

extern "C" {

#if defined(OMX_VERSION_MAJOR) && defined(OMX_VERSION_MINOR) && \
        OMX_VERSION_MAJOR == 1 && OMX_VERSION_MINOR >= 2
#define OMX_SPEC_1_2_0_0_SUPPORT
#define OMX_StateInvalid OMX_StateReserved_0x00000000
#define OMX_ErrorInvalidState OMX_ErrorReserved_0x8000100A
#endif // OMX_VERSION >= 1.2

static const OMX_U32 kVideoPortStartNumber = 0x0;
static const OMX_U32 kAudioPortStartNumber = 0x0;
static const OMX_U32 kOtherPortStartNumber = 0x200;
static const OMX_U32 kClockPortStartNumber = kOtherPortStartNumber;
static const OMX_U32 kImagePortStartNumber = 0x300;
static const OMX_U32 kTextPortStartNumber = 0x400;

enum OMX_LOG_LEVEL {
  OMX_LOG_LEVEL_ERROR = 0,
  OMX_LOG_LEVEL_WARN,
  OMX_LOG_LEVEL_INFO,
  OMX_LOG_LEVEL_DEBUG,
  OMX_LOG_LEVEL_VERBOSE,
  OMX_LOG_LEVEL_RESERVE,
  OMX_LOG_LEVEL_MAX = 0x7FFFFFFF
};

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

const char* omxState2String(OMX_STATETYPE state);

}

namespace mmp {

template <typename _T>
void InitOmxHeader(_T* header) {
  // TODO: Should check whether it is suitable for all header
  memset(header, 0, sizeof(_T));
  header->nSize = sizeof(_T);
  header->nVersion.s.nVersionMajor = 1;
  header->nVersion.s.nVersionMinor = 0;
}

template <typename _T>
    void InitOmxHeader(_T& header) {
  // TODO: Should check whether it is suitable for all header
  memset(&header, 0, sizeof(_T));
  header.nSize = sizeof(_T);
  header->nVersion.s.nVersionMajor = 1;
  header->nVersion.s.nVersionMinor = 0;
}


typedef enum {
  EVENT_UNKNOWN = 0,
  EVENT_VIDEO_EOS,
  EVENT_AUDIO_EOS,
  EVENT_COMMAND_COMPLETE,
  EVENT_FPS_INFO,
  EVENT_ERROR,
  EVENT_VIDEO_RESOLUTION,
} MHAL_EVENT_TYPE;

typedef enum {
  OMX_TUNNEL = 0,
  OMX_NON_TUNNEL,
} OMX_TUNNEL_MODE;

class MHALOmxCallbackTarget {
 public:
  virtual ~MHALOmxCallbackTarget() {};
  virtual OMX_BOOL OnOmxEvent(MHAL_EVENT_TYPE event_code, OMX_U32 nData1 = 0,
              OMX_U32 nData2 = 0, OMX_PTR pEventData = NULL) = 0;
};

}
#endif
