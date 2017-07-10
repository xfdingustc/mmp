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


#ifndef OMX_VIDEO_DECODER_H_
#define OMX_VIDEO_DECODER_H_


#include <OmxComponentImpl.h>
#include <OmxVideoPort.h>
#include <FFMpegVideoDecoder.h>

namespace mmp {

static const ProfileLevelType kAvcProfileLevel[] = {
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1},
  {OMX_VIDEO_AVCProfileMain, OMX_VIDEO_AVCLevel22},
  {OMX_VIDEO_AVCProfileHigh, OMX_VIDEO_AVCLevel51},
  {OMX_VIDEO_AVCProfileExtended, OMX_VIDEO_AVCLevel51}
};

static const VideoFormatType kFormatSupport[] = {
  {OMX_VIDEO_CodingAVC, OMX_COLOR_FormatUnused, 15 <<16},
  {OMX_VIDEO_CodingMPEG4, OMX_COLOR_FormatUnused, 15 <<16},
  {OMX_VIDEO_CodingH263, OMX_COLOR_FormatUnused, 60 <<16},
  {OMX_VIDEO_CodingMPEG2, OMX_COLOR_FormatUnused, 15 <<16},
#ifdef _ANDROID_
  //{static_cast<OMX_VIDEO_CODINGTYPE>(OMX_VIDEO_CodingVPX),
  //     OMX_COLOR_FormatUnused, 15 <<16},
#endif
#ifdef OMX_SPEC_1_2_0_0_SUPPORT
  {OMX_VIDEO_CodingVP8, OMX_COLOR_FormatUnused, 15 <<16},
  {OMX_VIDEO_CodingVC1, OMX_COLOR_FormatUnused, 15 <<16},
#endif
  {OMX_VIDEO_CodingWMV, OMX_COLOR_FormatUnused, 15 <<16},
  {OMX_VIDEO_CodingMJPEG, OMX_COLOR_FormatUnused, 15 <<16},
};



class OmxVideoDecoder : public OmxComponentImpl {
  public:
    OmxVideoDecoder(OMX_STRING name);
    virtual ~OmxVideoDecoder();

    virtual OMX_ERRORTYPE componentDeInit(OMX_HANDLETYPE hComponent);
    virtual OMX_ERRORTYPE getParameter(OMX_INDEXTYPE index, OMX_PTR params);
    virtual OMX_ERRORTYPE setParameter(OMX_INDEXTYPE index,
        const OMX_PTR params);

  private:
    virtual OMX_ERRORTYPE initRole();
    virtual OMX_ERRORTYPE initPort();

    virtual OMX_ERRORTYPE processBuffer();
    virtual OMX_ERRORTYPE flush();

    OMX_ERRORTYPE processOutputBuffer();
    OMX_ERRORTYPE processInputBuffer();

    OMX_BOOL isVideoParamSupport(OMX_VIDEO_PARAM_PORTFORMATTYPE* video_param);
    OMX_BOOL isProfileLevelSupport(OMX_VIDEO_PARAM_PROFILELEVELTYPE* prof_lvl);

    inline OMX_VIDEO_CODINGTYPE getCodecType() {
      return mInputPort->getVideoDefinition().eCompressionFormat;
    };

    typedef struct CodecSpeficDataType {
      void * data;
      unsigned int size;
    } CodecSpeficDataType;

    OmxPortImpl *mInputPort;
    OmxPortImpl *mOutputPort;
    OMX_BOOL mInited;
    OMX_BOOL mCodecSpecficDataSent;
    vector<CodecSpeficDataType> mCodecSpeficData;

    bool mUseNativeBuffers;

    OMX_U32 mProfile;
    OMX_U32 mLevel;

    OMX_BOOL mEOS;
    OMX_U32  mHasDecodedFrame;
    OMX_MARKTYPE mMark;

    FFMpegVideoDecoder* mFFmpegDecoder;
  };
}
#endif


