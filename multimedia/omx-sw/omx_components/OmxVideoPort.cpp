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

#define LOG_TAG "OmxVideoPort"
#include "OmxVideoPort.h"

namespace mmp {

OmxVideoPort::OmxVideoPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  mDefinition.format.video.pNativeRender = NULL;
  mDefinition.format.video.nFrameWidth = 176;
  mDefinition.format.video.nFrameHeight = 144;
  mDefinition.format.video.nStride =  176;
  mDefinition.format.video.nSliceHeight =  144;
  mDefinition.format.video.nBitrate = 64000;
  mDefinition.format.video.xFramerate = 15 << 16;
  mDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
  mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
  mDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
  mDefinition.format.video.pNativeWindow = NULL;
  InitOmxHeader(mVideoParam);
  updateDomainParameter();
};

OmxVideoPort::~OmxVideoPort(void) {
}

/************************
*  OmxAvcPort
************************/

OmxAvcPort::OmxAvcPort(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.avc);
  mCodecParam.avc.nPortIndex = mDefinition.nPortIndex;
}

OmxAvcPort::OmxAvcPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingAVC;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = 15<<16;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.avc);
  mCodecParam.avc.nPortIndex = mDefinition.nPortIndex;
  mCodecParam.avc.eProfile = OMX_VIDEO_AVCProfileBaseline;
  mCodecParam.avc.eLevel = OMX_VIDEO_AVCLevel1;
}

OmxAvcPort::~OmxAvcPort() {
}


/************************
*  OmxH263Port
************************/
OmxH263Port::OmxH263Port(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingH263;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = 15<<16;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.h263);
  mCodecParam.h263.nPortIndex = mDefinition.nPortIndex;
  mCodecParam.h263.eProfile = OMX_VIDEO_H263ProfileBaseline;
  mCodecParam.h263.eLevel = OMX_VIDEO_H263Level10;
  mCodecParam.h263.bPLUSPTYPEAllowed = OMX_FALSE;
  mCodecParam.h263.bForceRoundingTypeToZero = OMX_TRUE;
}

OmxH263Port::OmxH263Port(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingH263;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = 15<<16;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.h263);
  mCodecParam.h263.nPortIndex = mDefinition.nPortIndex;
  mCodecParam.h263.eProfile = OMX_VIDEO_H263ProfileBaseline;
  mCodecParam.h263.eLevel = OMX_VIDEO_H263Level10;
  mCodecParam.h263.bPLUSPTYPEAllowed = OMX_FALSE;
  mCodecParam.h263.bForceRoundingTypeToZero = OMX_TRUE;
}

OmxH263Port::~OmxH263Port() {
}

/************************
*  OmxMpeg4Port
************************/
OmxMpeg4Port::OmxMpeg4Port(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = 15<<16;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.mpeg4);
  mCodecParam.mpeg4.nPortIndex = mDefinition.nPortIndex;
  mCodecParam.mpeg4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
  mCodecParam.mpeg4.eLevel = OMX_VIDEO_MPEG4Level1;
}

OmxMpeg4Port::OmxMpeg4Port(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  getVideoDefinition().eCompressionFormat = OMX_VIDEO_CodingMPEG4;
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = 15<<16;
  updateDomainParameter();
  InitOmxHeader(&mCodecParam.mpeg4);
  mCodecParam.mpeg4.nPortIndex = mDefinition.nPortIndex;
  mCodecParam.mpeg4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
  mCodecParam.mpeg4.eLevel = OMX_VIDEO_MPEG4Level1;
}

OmxMpeg4Port::~OmxMpeg4Port() {
}

#ifdef _ANDROID_
/************************
*  OmxVPXPort
************************/
OmxVPXPort::OmxVPXPort(OMX_DIRTYPE dir) {
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  //getVideoDefinition().eCompressionFormat =
  //    static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingVPX);
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = 15<<16;
  updateDomainParameter();
}

OmxVPXPort::OmxVPXPort(OMX_U32 index, OMX_DIRTYPE dir) {
  mDefinition.nPortIndex = index;
  mDefinition.eDir = dir;
  mDefinition.eDomain = OMX_PortDomainVideo;
  //getVideoDefinition().eCompressionFormat =
  //    static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingVPX);
  getVideoDefinition().eColorFormat = OMX_COLOR_FormatUnused;
  getVideoDefinition().nFrameWidth = 176;
  getVideoDefinition().nFrameHeight = 144;
  getVideoDefinition().nBitrate = 64000;
  getVideoDefinition().xFramerate = 15<<16;
  updateDomainParameter();
}

OmxVPXPort::~OmxVPXPort() {
}
#endif //_ANDROID_

}
