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

#ifndef MHAL_DECODER_COMPONENT_H_
#define MHAL_DECODER_COMPONENT_H_

#include <android/native_window.h>
#include <GraphicBuffer.h>
#include <HardwareAPI.h>

#include "MHALOmxComponent.h"
#include "frameworks/mmp_frameworks.h"

using namespace android;
namespace mmp {

class MHALOmxSinkPad : public MmpPad {
  public:
    MHALOmxSinkPad(mchar* name, MmpPadDirection dir, MmpPadMode mode)
      : MmpPad(name, dir, mode) {
    }
    virtual               ~MHALOmxSinkPad() {};
    virtual   MmpError    ChainFunction(MmpBuffer* buf);
    virtual   MmpError    LinkFunction();
};

enum BufferStatus {
  OWNED_BY_US,
  OWNED_BY_COMPONENT,
  OWNED_BY_NATIVE_WINDOW,
};

typedef struct BufferInfo {
  OMX_BUFFERHEADERTYPE *header;
  sp<GraphicBuffer>     graphicBuffer;
  MmpBuffer            *buffer;
  BufferStatus          status;
} BufferInfo;

class MHALDecoderComponent : public MHALOmxComponent {
  public:

    MHALDecoderComponent(OMX_PORTDOMAINTYPE domain);
    virtual ~MHALDecoderComponent();

    virtual mbool SendEvent(MmpEvent* event);

    virtual void pushCodecConfigData() = 0;
    virtual void fillThisBuffer(MmpBuffer *buf) = 0;

    virtual OMX_ERRORTYPE EmptyBufferDone(OMX_BUFFERHEADERTYPE* pBuffer);

    MediaResult FeedData(MmpBuffer *pkt);
    int getBufferLevel(void);

    void canSendBufferToOmx(OMX_BOOL canfeed) { mCanFeedData_ = canfeed; }
    void setNativeWindow(sp<ANativeWindow> window) { mNativeWindow_ = window; }

  protected:
    virtual void flush();
    virtual OMX_BOOL allocateResource();
    virtual OMX_BOOL releaseResource();

    virtual OMX_BOOL processData() { return OMX_FALSE; }

    MmpCaps* getPadCaps();

    void allocateOmxBuffers(OMX_PORTDOMAINTYPE domain);
    void freeOmxBuffers(OMX_PORTDOMAINTYPE domain);
    void fillOmxBuffers();

    OMX_BOOL allocateInputBuffers(OMX_U32 portIndex);
    OMX_BOOL allocateOutputBuffers(OMX_U32 portIndex);
    OMX_BOOL allocateOutputBuffersFromNativeWindow(OMX_U32 portIndex);

    OMX_BOOL enableGraphicBuffers(OMX_U32 portIndex, OMX_BOOL enable);
    OMX_BOOL getGraphicBufferUsage(OMX_U32 portIndex, OMX_U32* usage);
    OMX_BOOL useGraphicBuffer(OMX_U32 portIndex, sp<GraphicBuffer> graphicBuffer);
    OMX_BOOL cancelBufferToNativeWindow(BufferInfo *info);
    BufferInfo* dequeueBufferFromNativeWindow();

    void* WorkingThreadLoop();
    static void* WorkingThreadEntry(void* thiz);

    OMX_PORTDOMAINTYPE mDomain_;

    KDThreadMutex *mPacketListLock_;
    MmpBufferList *mDataList_;

    KDThreadMutex *mInBufferLock_;
    list<OMX_BUFFERHEADERTYPE *> mInputBuffers_;
    OMX_U8 **mBuffers_;

    OMX_BOOL mCanFeedData_;
    OMX_BOOL mInPortEOS_;
    OMX_BOOL mOutPortEOS_;

    // Working thread
    OMX_BOOL mShouldExit_;
    pthread_t mThreadId_;

    // Some *.ts streams does not start with I-Frame.
    // Don't send these frames to video pipeline to avoid mosaic.
    OMX_BOOL bWaitVideoIFrame_;

    vector<BufferInfo *> mOutputBuffers_;

    sp<ANativeWindow> mNativeWindow_;
};

}

#endif
