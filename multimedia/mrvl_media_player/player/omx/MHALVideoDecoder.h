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

#ifndef MHAL_VIDEO_DECODER_H_

#define MHAL_VIDEO_DECODER_H_

#include "MHALDecoderComponent.h"

using namespace std;

namespace mmp {

// TODO: for unspported profile, there may be mosaic.
// Customer can choose whether to play the unsupported profiles.
//#define CHECKING_H264_PROFILE

// H264 profiles supported by BG2 vMeta
#define PROFILE_H264_BASELINE       66
#define PROFILE_H264_MAIN           77
#define PROFILE_H264_EXTENDED       88
#define PROFILE_H264_HIGH           100
#define PROFILE_H264_MULTIVIEW_HIGH 118
#define PROFILE_H264_STEREO_HIGH    128

class MHALVideoDecoder : public MHALDecoderComponent {
  public:

    MHALVideoDecoder(MHALOmxCallbackTarget *cb_target);
    virtual ~MHALVideoDecoder();

    virtual OMX_ERRORTYPE OnOMXEvent(OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                OMX_U32 nData2, OMX_PTR pEventData);
    virtual OMX_ERRORTYPE FillBufferDone(OMX_BUFFERHEADERTYPE* pBuffer);

    virtual void pushCodecConfigData();
    virtual void fillThisBuffer(MmpBuffer *buf);

  private:

    typedef struct {
      void *data;
      int size;
    }Config_buffer;

    virtual OMX_BOOL configPort();
    virtual const char* getComponentRole();

    virtual OMX_BOOL processData();

    void setVP6Format(const OMX_U32 inputPortIdx);
    void setRVFormat(const OMX_U32 inputPortIdx);
    void pushH264ConfigData();
    void freeH264ConfigData();

    void pushRVConfigData();

    void pushConfigData();

    OMX_BOOL queueEOSToOMX();
    OMX_BOOL queueBufferToOMX(MmpBuffer* pkt);

#ifdef CHECKING_H264_PROFILE
    OMX_BOOL isSupportedH264Profile(int profile);
#endif

    vector<Config_buffer *> mH264_config_buffer;
};

}

#endif
