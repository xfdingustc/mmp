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

#ifndef BERLIN_OMX_SUBTITLE_PORT_H_
#define BERLIN_OMX_SUBTITLE_PORT_H_

#include <BerlinOmxPortImpl.h>

namespace berlin {

class OmxSubtitlePort : public OmxPortImpl {
  public :
    OmxSubtitlePort() {};
    explicit OmxSubtitlePort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxSubtitlePort() {};

    void updateDomainParameter() { };
    void updateDomainDefinition() { };

};

class OmxSrtPort : public OmxSubtitlePort {
  public:
    OmxSrtPort() {};
    explicit OmxSrtPort(OMX_U32 index, OMX_DIRTYPE dir);
    virtual ~OmxSrtPort() {};
  private:
    OMX_U32 mInputBufferCount;
    static const OMX_U32 kNumInputBuffers = 4;
    static const OMX_U32 kNumOutputBuffers = 4;
};

}
#endif

