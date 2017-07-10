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


#ifndef OMX_AUDIO_DECODER_H_
#define OMX_AUDIO_DECODER_H_


#include <OmxComponentImpl.h>
#include <OmxAudioPort.h>
#include <FFMpegAudioDecoder.h>

namespace mmp {

class OmxAudioDecoder : public OmxComponentImpl {
  public:
    OmxAudioDecoder(OMX_STRING name);
    virtual ~OmxAudioDecoder();

    virtual OMX_ERRORTYPE componentDeInit(OMX_HANDLETYPE hComponent);
    virtual OMX_ERRORTYPE getParameter(OMX_INDEXTYPE index, OMX_PTR params);
    virtual OMX_ERRORTYPE setParameter(OMX_INDEXTYPE index,
        const OMX_PTR params);

  private:
    virtual OMX_ERRORTYPE initRole();
    virtual OMX_ERRORTYPE initPort();

    virtual OMX_ERRORTYPE processBuffer();
    virtual OMX_ERRORTYPE flush();

    inline OMX_AUDIO_CODINGTYPE getCodecType() {
      return mInputPort->getAudioDefinition().eEncoding;
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

    OMX_BOOL mEOS;
    OMX_MARKTYPE mMark;

    FFMpegAudioDecoder* mFFmpegDecoder;
  };
}
#endif


