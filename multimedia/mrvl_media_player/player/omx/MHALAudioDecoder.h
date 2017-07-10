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

#ifndef MHAL_AUDIO_DECODER_H_
#define MHAL_AUDIO_DECODER_H_

#include "MHALDecoderComponent.h"

namespace mmp {

class MHALAudioDecoder : public MHALDecoderComponent {
  public:

    MHALAudioDecoder(MHALOmxCallbackTarget *cb_target);
    virtual ~MHALAudioDecoder();

    virtual OMX_ERRORTYPE OnOMXEvent(OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                OMX_U32 nData2, OMX_PTR pEventData);
    virtual OMX_ERRORTYPE FillBufferDone(OMX_BUFFERHEADERTYPE* pBuffer);

    virtual void pushCodecConfigData();
    virtual void fillThisBuffer(MmpBuffer *buf);

    static OMX_BOOL isAudioSupported(const char *mime_type_);

  private:
    virtual OMX_BOOL configPort();
    virtual const char* getComponentRole();

    virtual OMX_BOOL processData();

    void setAACFormat(const OMX_U32 inputPortIdx);
    void setMP3Format(const OMX_U32 inputPortIdx);
    void setMP2Format(const OMX_U32 inputPortIdx);
    void setMP1Format(const OMX_U32 inputPortIdx);
    void setAC3Format(const OMX_U32 inputPortIdx);
    void setVorbisFormat(const OMX_U32 inputPortIdx);
    void setG711Format(const OMX_U32 inputPortIdx);
    void setPCMFormat(const OMX_U32 inputPortIdx);
    void setWMAFormat(const OMX_U32 inputPortIdx);
    void setAMRFormat(const OMX_U32 inputPortIdx);
    void setRAFormat(const OMX_U32 inputPortIdx);
    void setDTSFormat(const OMX_U32 inputPortIdx);

    OMX_BOOL queueEOSToOMX();
    OMX_BOOL queueBufferToOMX(MmpBuffer* pkt);
};

}

#endif
