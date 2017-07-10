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

#ifndef OMX_CoreExt_h
#define OMX_CoreExt_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Each OMX header must include all required header files to allow the
 *  header to compile without errors.  The includes below are required
 *  for this header file to compile successfully
 */

#include <OMX_Core.h>

/** @ingroup comp */
typedef enum OMX_EVENTTYPE_EXT
{
  /**< component is reported an update of FPS every second */
  /**< data1 = (inputFps << 16) & displayedFps
   **< data2 = (droppedFps << 16) & lateFps */
  OMX_EventRendererFpsInfo = OMX_EventVendorStartUnused + 1,
  OMX_EventVideoResolutionChange,
  OMX_EventIDECODED, /**<The event for first I frame decoded done>*/
  /**<The event for first frame displayed done>*/
  /**< data1 indicates time stamp is valid or invalid: 0x80000000 is valid
  **<data2 is time stamp in 90K unit, can get second by (data2/90000)*/
  OMX_EventDISPLAYED,

  //for VideoProperties
  OMX_EventVideoFramerateChanged,
  OMX_EventVideoHeightChanged,
  OMX_EventVideoWidthChanged,
  OMX_EventVideoScanTypeChanged,
  OMX_EventVideoAFDChanged,
  OMX_EventVideoPresentChanged,
  OMX_EventVideoAspectRatioChanged,
  OMX_EventVideo3DFormatChanged,
  //end for VideoProperties

  //for dts cert
  OMX_EventAudioDtsInfo,
  OMX_EventAudioDtsPlaybackMode,
  OMX_EventAudioDtsChannelMask,
  OMX_EventAudioDtsRealChannelMask,

} OMX_EVENTTYPE_EXT;

typedef enum OMX_EXTRADATATYPE_EXT
{
  /**< The data payload contains information required for decryption */
  OMX_ExtraDataCryptoInfo = OMX_ExtraDataVendorStartUnused + 1,
  OMX_ExtraDataTimeInfo, /**< The data payload contains OMX_OTHER_EXTRADATA_TIME_INFO */
} OMX_EXTRADATATYPE_EXT;

typedef struct OMX_OTHER_EXTRADATA_TIME_INFO {
  OMX_TICKS nPTSFromContainer;  /**< Presentation timestamp from container in micro second.
                                     Negative value means that timestamp is unkown. */
  OMX_TICKS nDTSFromContainer;  /**< Decode timestamp from container in micro second.
                                     Negative value means that timestamp is unkown. */
  OMX_TICKS nMediatimeOffset;   /**< Offset to mediatime which is given via
                                       OMX_BUFFERHEADERTYPE.nTimeStamp */
} OMX_OTHER_EXTRADATA_TIME_INFO;

typedef struct OMX_RESOURCE_INFO {
  OMX_U32         nSize;
  OMX_VERSIONTYPE nVersion;
  OMX_U8          nResourceSize;
  OMX_U32         nResource[16];
} OMX_RESOURCE_INFO;

typedef enum OMX_COMMANDTYPE_EXT
{
  /**< send command to let component disconnect port */
  OMX_CommandDisconnectPort = OMX_CommandVendorStartUnused + 1,
  OMX_CommandClockPause,
  OMX_CommandClockStart,
  OMX_CommandClockStop,
  OMX_CommandDecode_ONE_Frame, /** <Decode one frame and auto stop */
  OMX_CommandSetStartDisplayPTS,
} OMX_COMMANDTYPE_EXT;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
/* File EOF */
