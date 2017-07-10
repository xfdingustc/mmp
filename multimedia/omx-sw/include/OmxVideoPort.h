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

#ifndef OMX_VIDOE_PORT_H
#define OMX_VIDOE_PORT_H

#include "OmxPortImpl.h"

using namespace std;

namespace mmp {

struct ProfileLevelType{
  OMX_U32 eProfile;
  OMX_U32 eLevel;
};

struct VideoFormatType{
  OMX_VIDEO_CODINGTYPE eCompressionFormat;
  OMX_COLOR_FORMATTYPE eColorFormat;
  OMX_U32 xFramerate;
};

const OMX_U32 kDefaultVideoWidth = 1280;
const OMX_U32 kDefaultVideoHeight = 720;

class OmxVideoPort : public OmxPortImpl {
public :
  OmxVideoPort() {
    InitOmxHeader(&mVideoParam);
  };
  OmxVideoPort(OMX_U32 index, OMX_DIRTYPE dir);
  virtual ~OmxVideoPort();
  void updateDomainParameter() {
    mVideoParam.eColorFormat = getVideoDefinition().eColorFormat;
    mVideoParam.eCompressionFormat = getVideoDefinition().eCompressionFormat;
    mVideoParam.xFramerate = getVideoDefinition().xFramerate;
    mVideoParam.nPortIndex = getPortIndex();
  };
  void updateDomainDefinition() {
    mDefinition.format.video.eColorFormat = mVideoParam.eColorFormat;
    mDefinition.format.video.eCompressionFormat = mVideoParam.eCompressionFormat;
    mDefinition.format.video.xFramerate = mVideoParam.xFramerate;
  }

protected:
  union {
    OMX_VIDEO_PARAM_AVCTYPE avc;
    OMX_VIDEO_PARAM_MPEG4TYPE mpeg4;
    OMX_VIDEO_PARAM_H263TYPE h263;
    OMX_VIDEO_WMVFORMATTYPE wmv;
    OMX_VIDEO_PARAM_MPEG2TYPE mpeg2;
    OMX_VIDEO_PARAM_RVTYPE rv;
  } mCodecParam;
};

class OmxAvcPort : public OmxVideoPort {
public :
    OmxAvcPort(OMX_DIRTYPE dir);
    OmxAvcPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxAvcPort();
    inline OMX_VIDEO_PARAM_AVCTYPE& getCodecParam() {return mCodecParam.avc;};
    inline void setCodecParam(OMX_VIDEO_PARAM_AVCTYPE *codec_param) {
      mCodecParam.avc = *codec_param;
    }
 private:
    //OMX_VIDEO_PARAM_AVCTYPE mCodecParam;

  };

  class OmxH263Port : public OmxVideoPort {
 public :
    OmxH263Port() {
      mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
      InitOmxHeader(&mCodecParam.h263);
      //mCodecParam.eProfile = OMX_VIDEO_H263ProfileUnknown;
      //mCodecParam.eLevel = OMX_VIDEO_H263LevelUnknown;
      //mCodecParam.nPFrames = 10;
      //mCodecParam.nBFrames = 0;
      //mCodecParam.nGOBHeaderInterval = 11;
      mCodecParam.h263.nPortIndex = mDefinition.nPortIndex;
    }

    OmxH263Port(OMX_DIRTYPE dir);
    OmxH263Port(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxH263Port();
    inline OMX_VIDEO_PARAM_H263TYPE getCodecParam() {return mCodecParam.h263;};
    inline void setCodecParam(OMX_VIDEO_PARAM_H263TYPE *codec_param) {
      mCodecParam.h263 = *codec_param;
    }
 private:
    //OMX_VIDEO_PARAM_H263TYPE mCodecParam;
  };

  class OmxMpeg4Port : public OmxVideoPort {
 public :
    OmxMpeg4Port() {
      mDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
      InitOmxHeader(&mCodecParam.mpeg4);
      mCodecParam.mpeg4.nPortIndex = mDefinition.nPortIndex;
    }

    OmxMpeg4Port(OMX_DIRTYPE dir);
    OmxMpeg4Port(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxMpeg4Port();
    inline OMX_VIDEO_PARAM_MPEG4TYPE getCodecParam() {return mCodecParam.mpeg4;};
    inline void setCodecParam(OMX_VIDEO_PARAM_MPEG4TYPE *codec_param) {
      mCodecParam.mpeg4 = *codec_param;
    }
 private:
    //OMX_VIDEO_PARAM_MPEG4TYPE mCodecParam;
  };

#ifdef _ANDROID_
  class OmxVPXPort : public OmxVideoPort {
 public :
    OmxVPXPort() {
      //mDefinition.format.video.eCompressionFormat =
       //   static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingVPX);
    }
    OmxVPXPort(OMX_DIRTYPE dir);
    OmxVPXPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxVPXPort();
  };
#endif //_ANDROID_

}

#endif // BERLIN_OMX_VIDOE_PORT_H
