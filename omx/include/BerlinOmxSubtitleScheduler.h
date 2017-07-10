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

#ifndef BERLIN_OMX_SUBTITLE_SCHEDULER_H
#define BERLIN_OMX_SBUTITLE_SCHEDULER_H

#include <BerlinOmxComponentImpl.h>

namespace berlin {


class OmxSubtitleScheduler :  public OmxComponentImpl{
  public:
    OmxSubtitleScheduler();
    OmxSubtitleScheduler(OMX_STRING name);
    virtual ~OmxSubtitleScheduler();

    OMX_ERRORTYPE initRole();
    OMX_ERRORTYPE initPort();
    OMX_ERRORTYPE componentDeInit(OMX_HANDLETYPE hComponent);
    OMX_ERRORTYPE getParameter(OMX_INDEXTYPE index, OMX_PTR param);
    OMX_ERRORTYPE setParameter(OMX_INDEXTYPE index, const OMX_PTR param);
    OMX_ERRORTYPE getConfig(OMX_INDEXTYPE index, OMX_PTR config);
    OMX_ERRORTYPE setConfig(OMX_INDEXTYPE index, const OMX_PTR config);

    static OMX_ERRORTYPE createComponent(
        OMX_HANDLETYPE* handle,
        OMX_STRING componentName,
        OMX_PTR appData,
        OMX_CALLBACKTYPE* callBacks);


    OMX_ERRORTYPE prepare() { return OMX_ErrorNone; };
    OMX_ERRORTYPE preroll() { return OMX_ErrorNone; };
    OMX_ERRORTYPE start() { return OMX_ErrorNone; };
    OMX_ERRORTYPE pause() { return OMX_ErrorNone; };
    OMX_ERRORTYPE resume() { return OMX_ErrorNone; };
    OMX_ERRORTYPE stop() { return OMX_ErrorNone; };
    OMX_ERRORTYPE release() { return OMX_ErrorNone; };
    OMX_ERRORTYPE flush() { return OMX_ErrorNone; };


    //static void* threadEntry(void *args);
    //void decodeLoop();
  private:
    OmxPortImpl* mInputPort;
    OmxPortImpl* mOutputPort;

    //KDThread *mThread;
    //KDThreadAttr *mThreadAttr;
    //OMX_BOOL mShouldExit;
};



}

#endif

