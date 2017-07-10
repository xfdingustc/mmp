/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OMX_VideoExt_h
#define OMX_VideoExt_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Each OMX header shall include all required header files to allow the
 * header to compile without errors.  The includes below are required
 * for this header file to compile successfully
 */
#include <OMX_Core.h>

/** Enum for video codingtype extensions */
typedef enum OMX_VIDEO_CODINGEXTTYPE {
  OMX_VIDEO_ExtCodingUnused = OMX_VIDEO_CodingKhronosExtensions,
  OMX_VIDEO_CodingVPX,        /**< Google VPX, formerly known as On2 VP8 */
  OMX_VIDEO_CodingVP6,        /**< VP6 */
  OMX_VIDEO_CodingMSMPEG4,    /**< MSMPEG4 */
  OMX_VIDEO_CodingSorenson,  /**< Sorenson codec*/
  OMX_VIDEO_CodingHEVC,       /** < HEVC/H265 */
} OMX_VIDEO_CODINGEXTTYPE;

/** VP6 Versions */
typedef enum OMX_VIDEO_VP6FORMATTYPE {
  OMX_VIDEO_VP6FormatUnused = 0x01,   /**< Format unused or unknown */
  OMX_VIDEO_VP6      = 0x02,   /**< On2 VP6 */
  OMX_VIDEO_VP6F     = 0x04,   /**< On2 VP6 (Flash version) */
  OMX_VIDEO_VP6A     = 0x08,   /**< On2 VP6 (Flash version, with alpha channel) */
  OMX_VIDEO_VP6FormatKhronosExtensions = 0x6F000000,
  /**< Reserved region for introducing Khronos Standard Extensions */
  OMX_VIDEO_VP6FormatVendorStartUnused = 0x7F000000,
  /**< Reserved region for introducing Vendor Extensions */
  OMX_VIDEO_VP6FormatMax    = 0x7FFFFFFF
} OMX_VIDEO_VP6FORMATTYPE;

/**
  * VP6 Params
  *
  * STRUCT MEMBERS:
  *  nSize      : Size of the structure in bytes
  *  nVersion   : OMX specification version information
  *  nPortIndex : Port that this structure applies to
  *  eFormat    : VP6 format
  */
typedef struct OMX_VIDEO_PARAM_VP6TYPE {
  OMX_U32 nSize;
  OMX_VERSIONTYPE nVersion;
  OMX_U32 nPortIndex;
  OMX_VIDEO_VP6FORMATTYPE eFormat;
} OMX_VIDEO_PARAM_VP6TYPE;

//for VideoProperties

typedef enum OMX_VIDEO_3D_FORMAT {
  OMX_VIDEO_3D_FORMAT_NONE,
  OMX_VIDEO_3D_FORMAT_FP,
  OMX_VIDEO_3D_FORMAT_SBS,
  OMX_VIDEO_3D_FORMAT_TB,
  OMX_VIDEO_3D_FORMAT_FS
} OMX_VIDEO_3D_FORMAT;

typedef enum OMX_VIDEO_ASPECT_RATIO {
  OMX_VIDEO_ASPECT_RATIO_UNKNOWN,
  OMX_VIDEO_ASPECT_RATIO_1_1,
  OMX_VIDEO_ASPECT_RATIO_4_3,
  OMX_VIDEO_ASPECT_RATIO_16_9
} OMX_VIDEO_ASPECT_RATIO;

typedef enum OMX_VIDEO_SCAN_TYPE {
  OMX_VIDEO_SCAN_TYPE_UNKNOWN,
  OMX_VIDEO_SCAN_TYPE_INTERLACED,
  OMX_VIDEO_SCAN_TYPE_PROGRESSIVE
} OMX_VIDEO_SCAN_TYPE;

typedef enum OMX_VIDEO_PRESENCE {
  OMX_VIDEO_PRESENCE_FALSE,
  OMX_VIDEO_PRESENCE_TRUE
} OMX_VIDEO_PRESENCE;


/**<possible values are 0x0-0xf, for the meaning of each code, please refer to AFD spec */
typedef enum OMX_VIDEO_AFD_CODE {
  OMX_VIDEO_AFD_CODE_0000,
  OMX_VIDEO_AFD_CODE_0001,
  OMX_VIDEO_AFD_CODE_0010,
  OMX_VIDEO_AFD_CODE_0011,
  OMX_VIDEO_AFD_CODE_0100,
  OMX_VIDEO_AFD_CODE_0101,
  OMX_VIDEO_AFD_CODE_0110,
  OMX_VIDEO_AFD_CODE_0111,
  OMX_VIDEO_AFD_CODE_1000,
  OMX_VIDEO_AFD_CODE_1001,
  OMX_VIDEO_AFD_CODE_1010,
  OMX_VIDEO_AFD_CODE_1011,
  OMX_VIDEO_AFD_CODE_1100,
  OMX_VIDEO_AFD_CODE_1101,
  OMX_VIDEO_AFD_CODE_1110,
  OMX_VIDEO_AFD_CODE_1111
} OMX_VIDEO_AFD_CODE;

typedef struct OMX_VIDEO_PROPERTIES {
  int width;
  int height;
  int framerate;
  OMX_VIDEO_SCAN_TYPE scanType;
  OMX_VIDEO_ASPECT_RATIO aspectRatio;
} OMX_VIDEO_PROPERTIES;

//end for VideoProperties

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OMX_VideoExt_h */
