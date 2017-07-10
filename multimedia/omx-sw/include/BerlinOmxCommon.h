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


#ifndef BERLIN_OMX_COMMON_H_
#define BERLIN_OMX_COMMON_H_
#include <OMX_Types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <OMX_Component.h>
#include <BerlinOmxLog.h>

#if defined(OMX_VERSION_MAJOR) && defined(OMX_VERSION_MINOR) && \
        OMX_VERSION_MAJOR == 1 && OMX_VERSION_MINOR >= 2
#define OMX_SPEC_1_2_0_0_SUPPORT
#include <OMX_AudioExt.h>
#include <OMX_CoreExt.h>
#include <OMX_IndexExt.h>
#include <OMX_VideoExt.h>
#include <OMX_IVCommonExt.h>
#include <OMX_RoleNameMrvlExt.h>
#define OMX_StateInvalid OMX_StateReserved_0x00000000
#define OMX_ErrorInvalidState OMX_ErrorReserved_0x8000100A
#endif // OMX_VERSION >= 1.2

#define NELM(array) (sizeof(array)/sizeof(array[0]))

namespace mmp {
enum OMX_VENDOR_ERRORTYPE {
  OMX_ErrorVendorStartUnused,
  OMX_ErrorParameterChange,
  OMX_ErrorVendorMax = 0x7FFFFFFF
};

typedef union OmxPortFormat {
  OMX_AUDIO_PARAM_PORTFORMATTYPE audio;
  OMX_IMAGE_PARAM_PORTFORMATTYPE image;
  OMX_OTHER_PARAM_PORTFORMATTYPE other;
  OMX_VIDEO_PARAM_PORTFORMATTYPE video;
} OmxPortFormat;

#ifdef _ANDROID_
static const OMX_U8 kSpecVersionMajor = 1;
static const OMX_U8 kSpecVersionMinor = 0;
static const OMX_U8 kSpecRevision = 0;
static const OMX_U8 kSpecStep = 0;
static const OMX_U32 kSpecVersion = 0x00000001;
#else // _ANDROID_
#ifdef OMXIL_1_2_
static const OMX_U8 kSpecVersionMajor = 1;
static const OMX_U8 kSpecVersionMinor = 2;
static const OMX_U8 kSpecRevision = 0;
static const OMX_U8 kSpecStep = 0;
static const OMX_U32 kSpecVersion = 0x00000201;
#else // OMXIL_1_2_
static const OMX_U8 kSpecVersionMajor = 1;
static const OMX_U8 kSpecVersionMinor = 1;
static const OMX_U8 kSpecRevision = 2;
static const OMX_U8 kSpecStep = 0;
static const OMX_U32 kSpecVersion = 0x00020101;
#endif // OMXIL_1_2_
#endif // _ANDROID_

static const OMX_U8 kCompVersionMajor = 1;
static const OMX_U8 kCompVersionMinor = 0;
static const OMX_U8 kCompRevision = 0;
static const OMX_U8 kCompStep = 0;
static const OMX_U32 kCompVersion = 0x00000001;
static const char* kCompNamePrefix = "comp.sw.";
static const OMX_U32 kCompNamePrefixLength = 8;

template <typename _T>
_T Max(_T a, _T b) {
  return (a > b ? a : b);
}

template <typename _T>
_T Min(_T a, _T b) {
  return (a < b ? a : b);
}

template <typename _T>
OMX_ERRORTYPE CheckOmxHeader(_T* header) {
  if(header->nSize != sizeof(_T)) {
    // TODO: remove this line after debug
    OMX_U32 header_size = sizeof(_T);
    // LOGW("Header size if wrong");
    return OMX_ErrorBadParameter;
  }
  if(header->nVersion.s.nVersionMajor != kSpecVersionMajor||
     header->nVersion.s.nVersionMinor != kSpecVersionMinor) {
    // LOGW( "Version mismatch\n");
    return OMX_ErrorVersionMismatch;
  }
  return OMX_ErrorNone;
}

template <typename _T>
void InitOmxHeader(_T* header) {
  // TODO: Should check whether it is suitable for all header
  memset(header, 0, sizeof(_T));
  header->nSize = sizeof(_T);
  header->nVersion.nVersion = kSpecVersion;
}

template <typename _T>
    void InitOmxHeader(_T& header) {
  // TODO: Should check whether it is suitable for all header
  memset(&header, 0, sizeof(_T));
  header.nSize = sizeof(_T);
  header.nVersion.nVersion = kSpecVersion;
}

inline void setSpecVersion(OMX_VERSIONTYPE* specVersion) {
  specVersion->s.nVersionMajor = kSpecVersionMajor;
  specVersion->s.nVersionMinor = kSpecVersionMinor;
  specVersion->s.nRevision = kSpecRevision;
  specVersion->s.nStep = kSpecStep;
}

inline void setComponentVersion(OMX_VERSIONTYPE* componentVersion) {
  componentVersion->nVersion = kCompVersion;
}

}
#endif // BERLIN_OMX_COMMON_H_
