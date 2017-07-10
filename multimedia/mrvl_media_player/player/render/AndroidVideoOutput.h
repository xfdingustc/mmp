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

#ifndef ANDROID_VIDEO_OUTPUT_H_
#define ANDROID_VIDEO_OUTPUT_H_

#include "frameworks/mmp_frameworks.h"

#include <android/native_window.h>
#include <GraphicBuffer.h>
#include <HardwareAPI.h>
#include <list>

using namespace std;

using namespace android;

namespace mmp {

class VideoRenderSinkPad : public MmpPad {
  public:
    VideoRenderSinkPad(mchar* name, MmpPadDirection dir, MmpPadMode mode)
      : MmpPad(name, dir, mode) {
    }
    ~VideoRenderSinkPad() {};
    virtual MmpError ChainFunction(MmpBuffer* buf);
};

class AndroidNativeWindowRenderer : public MmpElement, public MmuTask {
  public:
    AndroidNativeWindowRenderer(const sp<ANativeWindow> &nativeWindow);
    virtual ~AndroidNativeWindowRenderer();

    void receiveFrame(MmpBuffer* buf);

  private:
    virtual mbool ThreadLoop();
    void render(MmpBuffer* buf);

    sp<ANativeWindow> mNativeWindow_;
    MmuMutex queue_mutex_;
    list<MmpBuffer*> mVideoQueue_;
};

}

#endif
