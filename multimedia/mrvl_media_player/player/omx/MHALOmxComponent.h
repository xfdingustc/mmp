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

#ifndef MHAL_OMX_COMPONENT_H_
#define MHAL_OMX_COMPONENT_H_

#include "BerlinOmxProxy.h"
#include "MHALOmxUtils.h"

#include "MmpMediaDefs.h"
#include "condSignal.h"

#include "frameworks/mmp_frameworks.h"

extern "C" {
#include <stdio.h>
#include <sys/prctl.h>
}

#include <vector>
#include <list>

using namespace std;

namespace mmp {

class MHALOmxComponent : public MmpElement {
  public:

    MHALOmxComponent();
    virtual ~MHALOmxComponent();

    virtual OMX_ERRORTYPE OnOMXEvent(OMX_EVENTTYPE eEvent, OMX_U32 nData1,
                OMX_U32 nData2, OMX_PTR pEventData);

    virtual OMX_ERRORTYPE EmptyBufferDone(OMX_BUFFERHEADERTYPE* pBuffer);

    virtual OMX_ERRORTYPE FillBufferDone(OMX_BUFFERHEADERTYPE* pBuffer);

    OMX_BOOL setUp(OMX_TUNNEL_MODE mode = OMX_TUNNEL);
    OMX_COMPONENTTYPE* getCompHandle() { return mCompHandle_; }
    OMX_BOOL setOmxStateAsync(OMX_STATETYPE state);
    OMX_BOOL setOmxState(OMX_STATETYPE state);
    OMX_BOOL flushAsync();
    OMX_BOOL setupTunnel(OMX_U32 myPort, OMX_COMPONENTTYPE *comp, OMX_U32 compPort);
    OMX_BOOL setContainerType(OMX_MEDIACONTAINER_FORMATTYPE container);

    static OMX_ERRORTYPE OnEvent(
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_IN OMX_PTR pAppData,
            OMX_IN OMX_EVENTTYPE eEvent,
            OMX_IN OMX_U32 nData1,
            OMX_IN OMX_U32 nData2,
            OMX_IN OMX_PTR pEventData);

    static OMX_ERRORTYPE OnEmptyBufferDone(
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_IN OMX_PTR pAppData,
            OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);

    static OMX_ERRORTYPE OnFillBufferDone(
            OMX_OUT OMX_HANDLETYPE hComponent,
            OMX_OUT OMX_PTR pAppData,
            OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer);

  protected:
    virtual OMX_BOOL configPort();
    virtual const char* getComponentRole() = 0;

    virtual void flush();

    virtual OMX_BOOL allocateResource() { return OMX_TRUE; }
    virtual OMX_BOOL releaseResource() { return OMX_TRUE; }

    OMX_BOOL allocateNode(const char *componentRole);

    MHALOmxCallbackTarget *mCallbackTarget_;

    char compName[64];
    BerlinOmxProxy *mOMX_;
    OMX_COMPONENTTYPE *mCompHandle_;
    kdCondSignal *mStateSignal_;
    kdCondSignal *mFlushSignal_;

    OMX_U32 mNumPorts_;
    OMX_U32 mFlushCount_;

    OMX_BOOL mSyncCmd_;

    OMX_MEDIACONTAINER_FORMATTYPE mContainerType_;

    OMX_TUNNEL_MODE omx_tunnel_mode_;
};

}

#endif
