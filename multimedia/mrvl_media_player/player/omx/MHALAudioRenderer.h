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

#ifndef MHAL_AUDIO_RENDERER_H_

#define MHAL_AUDIO_RENDERER_H_

#include "MHALOmxComponent.h"

namespace mmp {

class MHALAudioRenderer : public MHALOmxComponent {
  public:

    MHALAudioRenderer(MHALOmxCallbackTarget *cb_target);
    virtual ~MHALAudioRenderer();

    virtual OMX_ERRORTYPE OnOMXEvent(OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                OMX_U32 nData2, OMX_PTR pEventData);

    OMX_BOOL setVolume(float leftVolume, float rightVolume);
    OMX_BOOL disablePort(const OMX_U32 portIdx);

  private:
    virtual const char* getComponentRole();
};

}

#endif
