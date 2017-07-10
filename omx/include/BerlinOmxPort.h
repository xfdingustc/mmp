/*
**                Copyright 2012, MARVELL SEMICONDUCTOR, LTD.
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

#ifndef BERLIN_OMX_PORT_H_
#define BERLIN_OMX_PORT_H_

#include "OMX_Component.h"
#include "BerlinOmxCommon.h"

namespace berlin {

  static const OMX_U32 kVideoPortStartNumber = 0;
  // @todo libstagefright now using fixed input port 0 and output port 1,
  //           player would get the port num from component, would modify
  static const OMX_U32 kAudioPortStartNumber = 0x0;
  static const OMX_U32 kOtherPortStartNumber = 0x200;
  static const OMX_U32 kClockPortStartNumber = kOtherPortStartNumber;
  static const OMX_U32 kImagePortStartNumber = 0x300;
  static const OMX_U32 kTextPortStartNumber = 0x400;

  static const OMX_U32 kDefaultBufferSize = 1024 * 1024;

class OmxPort {
  public:
    // OmxPort();
    virtual ~OmxPort() {}

    virtual OMX_ERRORTYPE enablePort()=0;
    virtual OMX_ERRORTYPE disablePort()=0;
    virtual OMX_ERRORTYPE flushPort()=0;
    virtual OMX_ERRORTYPE markBuffer(OMX_MARKTYPE * mark) = 0;
    virtual OMX_ERRORTYPE allocateBuffer(
        OMX_BUFFERHEADERTYPE **bufHdr,
        OMX_PTR appPrivate,
        OMX_U32 size) = 0;
    virtual OMX_ERRORTYPE freeBuffer(OMX_BUFFERHEADERTYPE *buffer) = 0;
    virtual OMX_ERRORTYPE useBuffer(OMX_BUFFERHEADERTYPE **bufHdr,
                                    OMX_PTR appPrivate,
                                    OMX_U32 size,
                                    OMX_U8 *buffer) = 0;
    virtual void pushBuffer(OMX_BUFFERHEADERTYPE *buffer) = 0;
    virtual OMX_BUFFERHEADERTYPE* popBuffer() = 0;
    virtual void updateDomainParameter() = 0;
    virtual void updateDomainDefinition() = 0;

  private:

    // OMX_U32 mBufferCount;
    // union {
    //  OMX_AUDIO_PARAM_PORTFORMATTYPE audio;
    //  OMX_IMAGE_PARAM_PORTFORMATTYPE image;
    //  OMX_OTHER_PARAM_PORTFORMATTYPE other;
    //  OMX_VIDEO_PARAM_PORTFORMATTYPE video;
    // } mFormat;
  };
}
#endif   // BERLIN_OMX_PORT_H_
